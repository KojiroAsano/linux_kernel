#ifndef LIB_H_
#define LIB_H_

// ============================================================================
// lib.h — 標準ライブラリの無いカーネルで使う、最低限のメモリ操作関数
// ============================================================================
// 普通のC言語プログラムなら<string.h>にあるものと同じ名前・同じ意味の
// 関数だが、OS自作では標準ライブラリが使えないので、これらは全部
// lib.asmでアセンブリとして自前実装している。

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

#endif
