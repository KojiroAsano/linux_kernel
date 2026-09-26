#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

// ============================================================================
// struct E820 — BIOSから取得した「メモリ領域1個分」の情報
// ============================================================================
// loader.asmが起動時にBIOSのINT 15h(EAX=0xe820)を使って取得し、
// 物理アドレス0x9008以降に並べて書き込んでおいたデータと、
// 1対1で対応する構造体。__attribute__((packed))を付けているのは、
// アセンブリ側が実際に書き込むバイト列の並びと、この構造体の
// メモリ上の並びを隙間なくぴったり一致させるため(packedが無いと、
// コンパイラが8バイト境界に揃えるための余白を勝手に挟むことがある)。
struct E820 {
    uint64_t address;  // この領域の開始物理アドレス
    uint64_t length;   // この領域のバイト数
    uint32_t type;     // 種類。1=Usable(OSが自由に使ってよい空きメモリ)。
                        // 2以降はACPI予約領域やNVSなど、様々な理由で
                        // 使ってはいけない領域(詳しくはACPI仕様を参照)。
} __attribute__((packed));

// e820から取り出した「使ってよいメモリ領域」だけを、後で使いやすい形で
// 覚えておくための構造体(init_memory()が使う)。
struct FreeMemRegion {
    uint64_t address;  // 領域の開始物理アドレス
    uint64_t length;   // 領域のバイト数
};

// ============================================================================
// struct Page — 空きページの連結リスト用のノード
// ============================================================================
// kalloc()/kfree()は「空いている物理ページを、単方向の連結リストとして
// つなげておく」という、シンプルな方式でメモリを管理している。
// あるページが「空いている」間は、そのページの先頭8バイトを
// 「次の空きページのアドレス」として間借りして使う、という発想
// (空いているページの中身は誰も気にしないので、そこにリストの
// ポインタを直接書き込んでしまって構わない)。
struct Page {
    struct Page* next;
};

// ============================================================================
// ページテーブル関連の型・定数・マクロ
// ============================================================================
// x86-64のページテーブルは本来PML4→PDPT→PD→PTの4階層あるが、このOSは
// 「1GBページ」を使うことでPML4→PDPTの2階層だけで済ませている
// (loader.asmのページテーブル設定を参照)。PDE/PD/PDPTRという名前は
// 付いているが、実体はどれも「ページテーブルの1エントリ分の生の64bit
// 値(物理アドレス+フラグビット)」を指すポインタとして使っている。
typedef uint64_t PDE;    // ページテーブルの1エントリの値そのもの
typedef PDE* PD;          // ページテーブル(エントリの配列)の先頭を指すポインタ
typedef PD* PDPTR;         // さらに1段上の階層のテーブルを指すポインタ

// ページテーブルエントリの下位ビットに立てるフラグ。
#define PTE_P 1        // Present: このエントリは有効
#define PTE_W 2        // Writable: 書き込み可能
#define PTE_U 4         // User: リング3からもアクセス可能
#define PTE_ENTRY 0x80    // PS(Page Size): これは2MBページである、という印

// カーネルが仮想アドレス空間のどこから始まるか。上位半分
// (0xffff800000000000以降)を丸ごとカーネル用に使う設計
// (higher-halfカーネルと呼ばれる方式)。
#define KERNEL_BASE 0xffff800000000000

// このOSが物理メモリを配る時の最小単位。2MB。
// (x86-64の「2MBページ」の大きさに合わせてある)
#define PAGE_SIZE (2*1024*1024)

// --- アドレス計算用マクロ ---
// PA_UP(v):   vをPAGE_SIZEの倍数に「切り上げる」
// PA_DOWN(v): vをPAGE_SIZEの倍数に「切り下げる」
// どちらも、下位21bit(2MB=2^21なので)を一旦0にしてから、
// 必要なら1ページ分足す、という計算をビット演算で行っている。
#define PA_UP(v) ((((uint64_t)v + PAGE_SIZE-1) >> 21) << 21)
#define PA_DOWN(v) (((uint64_t)v >> 21) << 21)

// P2V (Physical to Virtual) / V2P (Virtual to Physical):
// このOSのhigher-halfマッピングでは、「物理アドレスpの内容」は
// 「仮想アドレス KERNEL_BASE+p」からも見える(loader.asmが設定した、
// 物理0～1GBと仮想KERNEL_BASE～+1GBの二重マッピング)。なので物理→仮想の
// 変換は単純にKERNEL_BASEを足すだけ、仮想→物理はその逆に引くだけで
// 計算できる。
#define P2V(p) ((uint64_t)(p) + KERNEL_BASE)
#define V2P(v) ((uint64_t)(v) - KERNEL_BASE)

// ページテーブルエントリから、フラグビットを取り除いた「実際の
// アドレス部分」だけを取り出す。下位ビットをいったん0にしてから
// 戻すことで、アドレス部分だけを残している。
#define PDE_ADDR(p) (((uint64_t)p >> 12) << 12)   // 4KB境界に切り捨て
#define PTE_ADDR(p) (((uint64_t)p >> 21) << 21)   // 2MB境界に切り捨て

// --- memory.cで定義されている関数 ---
void init_memory(void);   // e820を解析し、空きメモリ領域をkalloc用に登録する
void init_kvm(void);        // カーネル専用のページテーブルを新しく作り、切り替える
void switch_vm(uint64_t map);  // CR3を書き換えて、使うページテーブルを切り替える
void* kalloc(void);          // 空きページを1つ取り出す(連結リストの先頭を取る)
void kfree(uint64_t v);       // 使い終わったページを空きリストに戻す
bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute);
                                // 仮想アドレス範囲[v,e)を、物理アドレスpaから
                                // 順にマッピングする(ページテーブルを実際に
                                // 組み立てる本体)
void load_cr3(uint64_t map);    // CR3レジスタを書き換える(trap.asm、アセンブリ側の実体)
void free_vm(uint64_t map);     // ページテーブルを解放する
void free_pages(uint64_t map, uint64_t vstart, uint64_t vend);  // ページテーブルのマッピングを解除し、物理ページを解放する
bool setup_uvm(uint64_t map, uint64_t start, int size);  // ユーザープロセス用のページテーブルを組み立てる
uint64_t setup_kvm(void);  // カーネル専用のページテーブルを組み立てる
uint64_t get_total_memory(void);  // 使用可能な物理メモリの合計をMB単位で返す

#endif
