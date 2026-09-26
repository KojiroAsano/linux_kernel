#ifndef _SYSCALL_H_
#define _SYSCALL_H_

// ============================================================================
// syscall.h — システムコール(リング3からリング0への機能呼び出し)
// ============================================================================
// ユーザープログラムは`int 0x80`命令でカーネルへ処理を依頼する
// (trap.asmのsysint、trap.cのIDT登録参照)。呼び出し規約は:
//   RAX = システムコール番号
//   RDI = 引数の個数
//   RSI = 引数配列(int64_t*)へのポインタ
// になるようユーザー側のスタブ(userランドのsyscall.asm)が積んでから
// int 0x80を実行する。
// ============================================================================

#include "trap.h"

// 1個のシステムコール実装が持つべき形。argptr[0], argptr[1], ...で
// 引数を受け取り、戻り値をintで返す。
typedef int (*SYSTEMCALL)(int64_t *argptr);

void init_system_call(void);            // system_calls[]テーブルを組み立てる
void system_call(struct TrapFrame *tf);  // tf->raxの番号に応じて実際に呼び出す(trap.cのhandlerから呼ばれる)

#endif
