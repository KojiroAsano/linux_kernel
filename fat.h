#ifndef _FAT_H_
#define _FAT_H_

#include "stdint.h"
#include "stdbool.h"

// ============================================================================
// fat.h — FAT12/16ファイルシステムの読み取り
// ============================================================================
// FATファイルシステムは、ディスクの先頭(LBA0)にある「BPB(BIOS
// Parameter Block)」というブートセクタの中の構造体に、ディスクの
// ジオメトリ(1セクタ何バイトか・1クラスタ何セクタか・FATがいくつ
// あるか等)が書かれている。まずこれを読んで解析しないと、
// ルートディレクトリやファイルの中身がディスクのどこにあるか
// 計算できない。
// ============================================================================

// fat_read_bpb()が、BPBを解析した結果をまとめて返すための構造体。
// FAT自体のバイト単位の生のレイアウト(struct FatBPB)はfat.cの中に
// 隠してあり、外からは常にこの「使いやすく整形済み」の形で受け取る。
struct FatInfo {
    uint16_t bytes_per_sector;    // 1セクタのバイト数(通常512)
    uint8_t  sectors_per_cluster;  // 1クラスタのセクタ数
    uint32_t total_sectors;         // ディスク全体のセクタ数

    uint32_t fat_start_lba;    // FAT(ファイルアロケーションテーブル)本体の開始LBA
    uint32_t sectors_per_fat;   // FAT1個あたりのセクタ数
    uint8_t  num_fats;           // FATの個数(通常2、片方が壊れた時の予備)

    uint32_t root_dir_start_lba;   // ルートディレクトリの開始LBA
    uint32_t root_dir_sectors;      // ルートディレクトリが占めるセクタ数
    uint16_t root_entry_count;       // ルートディレクトリのエントリ数(通常224)

    uint32_t data_start_lba;   // データ領域(実際のファイルの中身)の開始LBA
};

// driveの指定したディスクのLBA0からBPBを読み込み、解析結果をfat_infoへ
// 書き込む。成功したらtrue。
bool fat_read_bpb(int drive, struct FatInfo *fat_info);

#endif
