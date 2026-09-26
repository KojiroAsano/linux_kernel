// ============================================================================
// fat.c — FAT12/16ファイルシステムの読み取り(BPB解析)
// ============================================================================

#include "fat.h"
#include "ata.h"

// ============================================================================
// struct FatBPB — ディスクのLBA0(ブートセクタ)にある、生のBPBレイアウト
// ============================================================================
// バイト単位のオフセットは仕様で厳密に決まっている。__attribute__((packed))
// を付けて、コンパイラが余白(パディング)を挟まないようにしてある
// (memory.hのstruct E820と同じ理由)。
// FAT32には(この構造体には無い)追加のフィールドがあるが、今回は
// FAT12/16だけを対象にしているので含めていない。
struct FatBPB {
    uint8_t  jump[3];              // オフセット0-2: ブート時のジャンプ命令(中身は使わない)
    uint8_t  oem_name[8];            // オフセット3-10: フォーマットしたツールの名前など
    uint16_t bytes_per_sector;         // オフセット11-12: 1セクタのバイト数
    uint8_t  sectors_per_cluster;        // オフセット13: 1クラスタのセクタ数
    uint16_t reserved_sectors;             // オフセット14-15: FATより前の予約セクタ数(ブートセクタ自身を含む)
    uint8_t  num_fats;                       // オフセット16: FATの個数
    uint16_t root_entry_count;                 // オフセット17-18: ルートディレクトリのエントリ数
    uint16_t total_sectors_16;                   // オフセット19-20: 総セクタ数(小さい場合はここ、0ならtotal_sectors_32を見る)
    uint8_t  media_descriptor;                     // オフセット21: メディア種別
    uint16_t sectors_per_fat;                        // オフセット22-23: FAT12/16でのFAT1個あたりのセクタ数
    uint16_t sectors_per_track;                        // オフセット24-25: (CHS用、今回は使わない)
    uint16_t num_heads;                                  // オフセット26-27: (CHS用、今回は使わない)
    uint32_t hidden_sectors;                               // オフセット28-31: パーティション開始までの隠しセクタ数
    uint32_t total_sectors_32;                               // オフセット32-35: 総セクタ数(大きい場合はこちら)
} __attribute__((packed));

// ============================================================================
// fat_read_bpb — BPBを読み、扱いやすい形へ解析する
// ============================================================================
bool fat_read_bpb(int drive, struct FatInfo *fat_info)
{
    unsigned char sector[512];
    struct FatBPB *bpb;

    if (!ata_read_sector(drive, 0, sector)) {
        return false;   // ディスクそのものが読めなかった
    }

    bpb = (struct FatBPB*)sector;

    // --- BPBの生の値を、そのままコピーする部分 ---
    fat_info->bytes_per_sector = bpb->bytes_per_sector;
    fat_info->sectors_per_cluster = bpb->sectors_per_cluster;
    fat_info->num_fats = bpb->num_fats;
    fat_info->root_entry_count = bpb->root_entry_count;
    fat_info->sectors_per_fat = bpb->sectors_per_fat;

    // 総セクタ数は、16bitフィールドに収まっていればそちら、
    // 0(収まらなかった)なら32bitフィールドを見る、という仕様。
    fat_info->total_sectors = (bpb->total_sectors_16 != 0)
                                ? bpb->total_sectors_16
                                : bpb->total_sectors_32;

    // --- ここから、生の値を組み合わせて計算する部分 ---

    // FAT本体は、予約セクタ(ブートセクタを含む)の直後から始まる。
    fat_info->fat_start_lba = bpb->reserved_sectors;

    // ルートディレクトリは、FATが(num_fats個)全部並んだ直後から始まる。
    fat_info->root_dir_start_lba = fat_info->fat_start_lba
                                    + (uint32_t)fat_info->num_fats * fat_info->sectors_per_fat;

    // ルートディレクトリの1エントリは32バイト固定なので、
    // 「エントリ数×32バイト」を「1セクタのバイト数」で切り上げ除算すれば、
    // ルートディレクトリが占めるセクタ数が求まる。
    fat_info->root_dir_sectors = ((uint32_t)fat_info->root_entry_count * 32
                                   + fat_info->bytes_per_sector - 1)
                                  / fat_info->bytes_per_sector;

    // データ領域(実際のファイルの中身)は、ルートディレクトリの直後から始まる。
    fat_info->data_start_lba = fat_info->root_dir_start_lba + fat_info->root_dir_sectors;

    return true;
}
