// ============================================================================
// syscall.c — システムコールの実装とディスパッチ
// ============================================================================

#include "syscall.h"
#include "print.h"
#include "process.h"
#include "keyboard.h"
#include "memory.h"
#include "debug.h"
#include "stddef.h"

// システムコール番号→実装関数、のディスパッチテーブル。
// ユーザー側(syscall.asm)のmov eax,N; int 0x80のNと、ここの添字が対応する。
static SYSTEMCALL system_calls[10];

// ============================================================================
// sys_write — argptr[0](バッファの先頭アドレス)をargptr[1]バイト分、
// 画面へ書き出す
// ============================================================================
static int sys_write(int64_t *argptr)
{
    write_screen((char*)argptr[0], (int)argptr[1], 0xf);
    return (int)argptr[1];
}

// ============================================================================
// sys_sleep — argptr[0]ティック分、呼び出し元プロセスを寝かせる
// ============================================================================
// 1回のsleep(-1)ではタイマー割り込みが1回来るたびに起こされるだけなので
// (process.cのsleep/wake_up参照)、指定ティック数に達するまでループで
// 何度も寝直す。
static int sys_sleep(int64_t* argptr)
{
    uint64_t old_ticks;
    uint64_t ticks;
    uint64_t sleep_ticks = argptr[0];

    ticks = get_ticks();
    old_ticks = ticks;

    while (ticks - old_ticks < sleep_ticks) {
       sleep(-1);
       ticks = get_ticks();
    }

    return 0;
}

// ============================================================================
// sys_exit — 呼び出し元プロセスを終了させる
// ============================================================================
static int sys_exit(int64_t *argptr)
{
    exit();
    return 0;
}

// ============================================================================
// sys_wait — 終了済みプロセスの回収役として居座る(process.cのwait参照)
// ============================================================================
static int sys_wait(int64_t *argptr)
{
    wait();
    return 0;
}

// ============================================================================
// sys_keyboard_read — キーボードのリングバッファから1文字取り出す
// ============================================================================
// バッファが空ならread_key_buffer()内部でsleep()するので、この呼び出し
// 自体はキー入力があるまでブロックする。
static int sys_keyboard_read(int64_t *argptr)
{
    return read_key_buffer();
}

// ============================================================================
// sys_get_total_memory — e820から分かった、使用可能な総メモリ量(MB)を返す
// ============================================================================
static int sys_get_total_memory(int64_t *argptr)
{
    return get_total_memory();
}

// ============================================================================
// sys_kill — argptr[0]で指定したPIDのプロセスを強制終了させる
// ============================================================================
// 戻り値は0=成功(見つけて終了させた)、-1=失敗(該当PIDが無い)。
// 自分自身のPIDを指定した場合はkill_process内部でexit()に合流するので、
// この関数からは戻ってこない(戻り値を返す機会が無い)。
static int sys_kill(int64_t *argptr)
{
    int pid = (int)argptr[0];
    return kill_process(pid) ? 0 : -1;
}

// ============================================================================
// init_system_call — ディスパッチテーブルを組み立てる
// ============================================================================
void init_system_call(void)
{
    system_calls[0] = sys_write;
    system_calls[1] = sys_sleep;
    system_calls[2] = sys_exit;
    system_calls[3] = sys_wait;
    system_calls[4] = sys_keyboard_read;
    system_calls[5] = sys_get_total_memory;
    system_calls[6] = sys_kill;
}

// ============================================================================
// system_call — trap.cのhandler()から、int 0x80が来るたびに呼ばれる
// ============================================================================
// tf->raxがシステムコール番号、tf->rdiが引数の個数、tf->rsiが引数配列
// へのポインタ(呼び出し規約はsyscall.hのコメント参照)。戻り値は
// tf->raxに書き戻す(iretqでリング3に戻った時、ユーザー側からは
// RAXの値として見える)。
void system_call(struct TrapFrame *tf)
{
    int64_t i = tf->rax;
    int64_t param_count = tf->rdi;
    int64_t *argptr = (int64_t*)tf->rsi;

    if (param_count < 0 || i > 6 || i < 0) {
        tf->rax = -1;   // 範囲外の番号は、静かに失敗を返すだけにする
        return;
    }

    ASSERT(system_calls[i] != NULL);
    tf->rax = system_calls[i](argptr);
}
