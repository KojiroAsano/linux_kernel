#ifndef _LIB_H_
#define _LIB_H_

#include "stdint.h"

// ============================================================================
// lib.h(ユーザーランド版) — このプログラムから使える関数の一覧
// ============================================================================
// printf以外は全部、実体がlib/syscall.asm・lib/lib.asmにある
// (カーネルへのシステムコール、またはmemset等のアセンブリ実装)。

int printf(const char *format, ...);   // 画面へ書式付き出力する(lib/print.c)

void sleepu(uint64_t ticks);            // 指定ティック数だけ眠る
void exitu(void);                        // このプロセスを終了する
void waitu(void);                         // 終了済みプロセスの回収役として居座る
unsigned char keyboard_readu(void);        // キー入力を1文字取得する(無ければ待つ)
int get_total_memoryu(void);                // 使用可能な物理メモリの合計(MB)を取得する
int killu(int pid);                          // 指定したPIDのプロセスを強制終了させる

void memset(void* buffer, char value, int size);
void memmove(void* dst, void* src, int size);
void memcpy(void* dst, void* src, int size);
int memcmp(void* src1, void* src2, int size);

#endif
