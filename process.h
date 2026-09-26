#ifndef _PROCESS_H_
#define _PROCESS_H_

// ============================================================================
// process.h — プロセス管理・スケジューラ
// ============================================================================
// このカーネルは「プロセス」を、以下の3つを1セットにしたものとして
// 扱う:
//   1. リング3で実行されるユーザーコード(loader.asmがディスクから
//      読み込んでおいた、独立したプログラム)
//   2. そのプロセス専用のページテーブル(setup_uvm/setup_kvmで組み立てる)
//   3. そのプロセス専用のカーネルスタック(割り込み処理やswap()による
//      コンテキストスイッチに使う)
// スケジューラはこれらを「実行可能(READY)」「実行中(RUNNING)」
// 「スリープ中(SLEEP)」「終了済み(KILLED)」の状態で管理し、タイマー
// 割り込みのたびにyield()を呼んで次のプロセスへ切り替える
// (プリエンプティブなラウンドロビン方式)。
// ============================================================================

#include "trap.h"
#include "lib.h"

// ============================================================================
// struct Process — 1つのプロセスの情報
// ============================================================================
// 先頭のstruct List *nextは、lib.cの汎用連結リスト関数へ
// struct Process*をそのまま渡せるようにするためのもの
// (struct Listのコメント参照)。
struct Process {
    struct List *next;    // 連結リスト用(ready_list/wait_list/kill_listのどれかに繋がる)
    int pid;               // プロセスID(1から順に採番)
    int state;              // PROC_*のいずれか(下のdefine参照)
    int wait;                // sleep()に渡された「待っている理由」の値。
                              // wake_up()が同じ値を指定して起こす。
    uint64_t context;         // このプロセスが最後にswap()で「眠った」時点の
                              // カーネルスタックのRSP。次に選ばれた時、
                              // ここからswap()で復帰する。
    uint64_t page_map;         // このプロセス専用のページテーブル(PML4)の仮想アドレス
    uint64_t stack;              // このプロセス専用のカーネルスタック(2MBページ)の先頭
    struct TrapFrame *tf;         // stack上に置かれた、リング3へ入るための
                                   // 「偽の割り込みフレーム」(下のset_process_entry参照)
};

// ============================================================================
// struct TSS — TSS(Task State Segment)をC言語側から書き換えるための構造体
// ============================================================================
// kernel.asmが定義している実際のTSS(ラベルTss)と、バイト単位で完全に
// 同じレイアウトになるように作ってある。process.cのset_tss()は、
// この構造体を通してkernel.asmのTssのrsp0フィールドを直接書き換える
// (extern struct TSS Tss;としてkernel.asm側のシンボルを参照する)。
struct TSS {
    uint32_t res0;    // オフセット0-3: 予約
    uint64_t rsp0;     // オフセット4-11: リング3から割り込み/例外が入った時に
                        // 切り替える先のカーネルスタック。プロセスを切り替える
                        // たびにset_tss()がここを書き換える。
    uint64_t rsp1;       // オフセット12-19: 未使用
    uint64_t rsp2;        // オフセット20-27: 未使用
    uint64_t res1;         // オフセット28-35: 予約
    uint64_t ist1;           // オフセット36-43: IST1(kernel.asmのDFStackTop、ダブルフォルト用)
    uint64_t ist2;             // オフセット44-51: 未使用
    uint64_t ist3;               // オフセット52-59: 未使用
    uint64_t ist4;                 // オフセット60-67: 未使用
    uint64_t ist5;                   // オフセット68-75: 未使用
    uint64_t ist6;                     // オフセット76-83: 未使用
    uint64_t ist7;                       // オフセット84-91: 未使用
    uint64_t res2;                         // オフセット92-99: 予約
    uint16_t res3;                           // オフセット100-101: 予約
    uint16_t iopb;                             // オフセット102-103: I/Oマップベース
} __attribute__((packed));

// ============================================================================
// struct ProcessControl — スケジューラ全体の状態
// ============================================================================
struct ProcessControl {
    struct Process *current_process;   // 今CPUで動いているプロセス
    struct HeadList ready_list;         // 実行可能(次に動かせる)プロセスの一覧
    struct HeadList wait_list;           // スリープ中のプロセスの一覧
    struct HeadList kill_list;            // 終了したがまだ後片付け(wait())されていない一覧
};

#define STACK_SIZE (2*1024*1024)   // プロセス1個あたりのカーネルスタックのサイズ(2MB、kalloc単位と同じ)
#define NUM_PROC 10                 // 同時に存在できるプロセスの最大数

// プロセスの状態
#define PROC_UNUSED 0    // process_table上の未使用スロット
#define PROC_INIT 1       // 作成中(まだREADYになっていない)
#define PROC_RUNNING 2      // 今CPUで実行中
#define PROC_READY 3          // 実行可能(ready_listで待機中)
#define PROC_SLEEP 4            // スリープ中(wait_listで待機中)
#define PROC_KILLED 5             // 終了済み(kill_listでwait()による後片付け待ち)

// ============================================================================
// process.cで定義されている関数
// ============================================================================
void init_process(void);   // process_table上に最初の3プロセスを作る
void launch(void);          // 最初のプロセスへ制御を渡す(KMainから一度だけ呼ぶ)
void pstart(struct TrapFrame *tf);   // tfのiretqでリング3へジャンプする(trap.asm)
void yield(void);            // 今のプロセスをready_listの末尾に戻し、次のプロセスへ切り替える
void swap(uint64_t *prev, uint64_t next);   // カーネルスタックのコンテキストスイッチ本体(trap.asm)
void sleep(int wait);         // 今のプロセスをwait理由でスリープさせる
void wake_up(int wait);        // wait理由でスリープしているプロセスを全部起こす
void exit(void);                // 今のプロセスを終了させる
void wait(void);                 // 終了したプロセスをkill_listから回収し続ける(いわゆるinitプロセス役)
bool kill_process(int pid);       // PIDを指定して、他のプロセスを強制終了させる

#endif
