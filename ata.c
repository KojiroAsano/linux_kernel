// ============================================================================
// ata.c — ATA PIOドライバ(プライマリバス・マスタドライブ、LBA28)
// ============================================================================
// ATAコントローラは、決まった番号のI/Oポート(プライマリバスなら
// 0x1F0〜0x1F7)へレジスタ形式で読み書きすることで操作する。
// PIOモードでの1セクタ読み書きの流れは、規格上おおまかに:
//   1. ドライブがビジー(BSY)でなくなるのを待つ
//   2. ドライブ・LBA・セクタ数をレジスタにセットする
//   3. コマンド(READ/WRITE)を送る
//   4. 転送準備完了(DRQ)を待つ
//   5. データポートから256ワード(=512バイト)読み書きする
// という手順になっている。
// ============================================================================

#include "ata.h"
#include "io.h"

// --- プライマリATAバスのI/Oポート番号 ---
#define ATA_DATA         0x1F0   // データ(16bit、read/write)。実際の512バイトはここを256回読み書きする
#define ATA_SECTOR_COUNT 0x1F2   // 読み書きするセクタ数
#define ATA_LBA_LOW      0x1F3   // LBAのbit0-7
#define ATA_LBA_MID      0x1F4   // LBAのbit8-15
#define ATA_LBA_HIGH     0x1F5   // LBAのbit16-23
#define ATA_DRIVE_HEAD   0x1F6   // ドライブ選択(bit4)・LBAモード指定・LBAのbit24-27
#define ATA_STATUS       0x1F7   // ステータス(read)
#define ATA_COMMAND      0x1F7   // コマンド(write)。ステータスと同じポート番号だが向きが逆

// --- ステータスレジスタ(ATA_STATUS)の各ビット ---
#define ATA_SR_ERR (1 << 0)   // 直前のコマンドでエラーが発生した
#define ATA_SR_DRQ (1 << 3)   // データの読み書き準備ができている
#define ATA_SR_BSY (1 << 7)   // ドライブがコマンド処理中(この間は他のレジスタに触れてはいけない)

// --- コマンド(ATA_COMMANDへ送る値) ---
#define ATA_CMD_READ_SECTORS  0x20   // セクタ読み込み(リトライあり、28bit LBA)
#define ATA_CMD_WRITE_SECTORS 0x30   // セクタ書き込み(リトライあり、28bit LBA)

#define SECTOR_SIZE 512

// ============================================================================
// wait_bsy_clear — ドライブのBSY(ビジー)ビットが下がるのを待つ
// ============================================================================
// BSYが立っている間は、ドライブが直前のコマンドをまだ処理中で、
// 他のレジスタへの書き込み・読み込みが保証されない。次の操作を
// 始める前に必ずこれを待つ。
static void wait_bsy_clear(void)
{
    while (in_byte(ATA_STATUS) & ATA_SR_BSY) {
        // 何もせず、BSYが下がるまでひたすら読み直す(ビジーウェイト)。
        // PIOモードは仕組み上こうする以外に方法が無い。
    }
}

// ============================================================================
// wait_drq_or_error — データ転送の準備(DRQ)ができるか、エラーになるまで待つ
// ============================================================================
// 戻り値trueならDRQが立った(データ転送してよい)、falseならERRが
// 立った(ドライブがエラーを報告した)。
static bool wait_drq_or_error(void)
{
    unsigned char status;

    while (1) {
        status = in_byte(ATA_STATUS);

        if (status & ATA_SR_ERR) {
            return false;   // ドライブがエラーを報告した(存在しないLBAを指定した等)
        }
        if (status & ATA_SR_DRQ) {
            return true;    // データ転送の準備ができた
        }
    }
}

// ============================================================================
// select_and_setup — ドライブ・LBA・セクタ数をレジスタにセットする
// ============================================================================
// 0xE0は「マスタドライブを選ぶ・LBAモードを使う」という固定ビット
// (bit5とbit7は仕様上常に1、bit6=1でLBAモード、bit4=0でマスタドライブ)。
// そこにLBAの上位4bit(bit24-27)をORして書き込む。
static void select_and_setup(uint64_t lba, uint8_t sector_count)
{
    out_byte(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    out_byte(ATA_SECTOR_COUNT, sector_count);
    out_byte(ATA_LBA_LOW, (uint8_t)(lba & 0xFF));
    out_byte(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    out_byte(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
}

// ============================================================================
// ata_read_sector — 1セクタ(512バイト)読み込む
// ============================================================================
bool ata_read_sector(uint64_t lba, void *buffer)
{
    uint16_t *buf16 = (uint16_t*)buffer;

    wait_bsy_clear();               // 前のコマンドが残っていないことを確認
    select_and_setup(lba, 1);        // 1セクタ分のLBA・ドライブを指定
    out_byte(ATA_COMMAND, ATA_CMD_READ_SECTORS);   // 読み込みコマンドを送る

    wait_bsy_clear();                // コマンド受理直後、処理が終わってBSYが下がるのを待つ
    if (!wait_drq_or_error()) {
        return false;                  // ドライブがエラーを報告した
    }

    // データポートから256ワード(256*2=512バイト)読み出す。
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        buf16[i] = in_word(ATA_DATA);
    }

    return true;
}

// ============================================================================
// ata_write_sector — 1セクタ(512バイト)書き込む
// ============================================================================
bool ata_write_sector(uint64_t lba, const void *buffer)
{
    const uint16_t *buf16 = (const uint16_t*)buffer;

    wait_bsy_clear();
    select_and_setup(lba, 1);
    out_byte(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);   // 書き込みコマンドを送る

    wait_bsy_clear();
    if (!wait_drq_or_error()) {
        return false;
    }

    // データポートへ256ワード書き込む。
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        out_word(ATA_DATA, buf16[i]);
    }

    return true;
}
