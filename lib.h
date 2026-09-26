#ifndef LIB_H_
#define LIB_H_

#include "stdbool.h"

// ============================================================================
// lib.h — 標準ライブラリの無いカーネルで使う、最低限のメモリ操作関数
// ============================================================================
// 普通のC言語プログラムなら<string.h>にあるものと同じ名前・同じ意味の
// 関数だが、OS自作では標準ライブラリが使えないので、これらは全部
// lib.asmでアセンブリとして自前実装している。

// ============================================================================
// struct List / struct HeadList — プロセスをつなぐための単方向連結リスト
// ============================================================================
// process.cの「実行可能待ち」「スリープ中」「終了済み」などの一覧を
// 管理するのに使う、汎用の連結リスト。struct Processの先頭に
// struct List *nextを置くことで、struct Process*をそのままstruct List*
// として扱える(C言語でよく使われる「構造体の先頭に共通フィールドを
// 置いて、擬似的な継承のように使う」というテクニック)。
struct List {
    struct List *next;
};

// HeadListは「リストの先頭(next)」と「リストの末尾(tail)」の両方を
// 覚えておくことで、末尾への追加(append_list_tail)をO(1)でできるように
// したもの。
struct HeadList {
    struct List *next;
    struct List *tail;
};

// bufferの先頭からsizeバイト分を、全部valueで埋める。
void memset(void *buffer, char value, int size);

// srcからdestへ、sizeバイトコピーする。srcとdestの範囲が重なっていても
// 正しくコピーできる(重なりに応じて、コピーする向きを自動で選ぶ実装)。
void memmove(void *dest, const void *src, int size);

// srcからdestへ、sizeバイトコピーする。範囲が重なっていないことが
// 前提(重なっている場合の動作はmemmoveを使うこと)。
void memcpy(void *dest, const void *src, int size);

// s1とs2の先頭sizeバイトを比較する。
// 【注意】C標準ライブラリのmemcmpとは戻り値の意味が逆で、
// この実装は全部一致していれば1、1箇所でも違えば0を返す
// (詳しくはlib.asmのコメント参照)。今のところどこからも
// 呼ばれていない。
int memcmp(const void *s1, const void *s2, int size);

// ============================================================================
// 連結リスト操作(lib.c)
// ============================================================================
// itemをlistの末尾に追加する。
void append_list_tail(struct HeadList *list, struct List *item);

// listの先頭を取り除いて返す(空ならNULL)。
struct List* remove_list_head(struct HeadList *list);

// listが空かどうかを返す。
bool is_list_empty(struct HeadList *list);

// listの中から、waitフィールド(struct Processの2番目のint)がwaitと
// 一致する最初の要素を取り除いて返す(process.cのwake_up()が使う)。
struct List* remove_list(struct HeadList *list, int wait);

// listの中から、pidフィールドがpidと一致する要素を取り除いて返す
// (process.cのkill_process()が使う)。
struct List* remove_list_by_pid(struct HeadList *list, int pid);

#endif
