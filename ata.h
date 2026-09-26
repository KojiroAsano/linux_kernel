#ifndef _ATA_H_
#define _ATA_H_

#include "stdint.h"
#include "stdbool.h"

// ============================================================================
// ata.h — ATA(IDE)ディスクへの、BIOSを使わない生の読み書き
// ============================================================================
// loader.asmはBIOSのINT 13hでディスクを読んでいたが、これはリアルモード
// でしか使えない。カーネル(ロングモード)側から実行中にディスクへ
// アクセスするには、ATAコントローラのI/Oポートへ直接読み書きする、
// 自前のドライバが必要になる。ここではプライマリATAバス(0x1F0-0x1F7)
// だけを対象にした、PIO(Programmed I/O、DMAを使わない一番シンプルな
// 方式)の最小限の実装。同じプライマリバスに、マスタ・スレーブの
// 2台のドライブがぶら下がる想定(build.shのQEMU起動オプション参照:
// 1台目のboot.imgがマスタ、2台目のfat.imgがスレーブ)。
// ============================================================================

#define ATA_DRIVE_MASTER 0   // boot.img(今の起動に使っているディスク)
#define ATA_DRIVE_SLAVE  1   // fat.img(FATファイルシステム用の2台目)

// 1セクタ(512バイト)分、driveの指定したlbaの位置からbufferへ読み込む。
// 成功したらtrue、ドライブがエラーを返したらfalseを返す。
bool ata_read_sector(int drive, uint64_t lba, void *buffer);

// 1セクタ(512バイト)分、bufferの内容をdriveの指定したlbaの位置へ書き込む。
// 成功したらtrue、ドライブがエラーを返したらfalseを返す。
bool ata_write_sector(int drive, uint64_t lba, const void *buffer);

#endif
