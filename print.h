#ifndef PRINT_H_
#define PRINT_H_

// VGAテキストモードの画面は、横80文字×縦25行。1文字あたり2バイト
// (1バイト目=文字コード、2バイト目=色)使うので、1行分は 80*2=160バイト。
#define LINE_SIZE 160

// 画面のどこまで書いたかを覚えておくための構造体。
struct ScreenBuffer
{
    /* data */
    char* buffer;  // VGAテキストメモリの先頭アドレス(通常は仮想0xb8000番地)
    int column;    // 現在のカーソル位置(列、0-79)
    int row;       // 現在のカーソル位置(行、0-24)
};

// printk — カーネル版のprintf。書式文字列と可変長引数を受け取り、
// 画面に文字列を組み立てて表示する(print.c参照)。使える書式指定子は
// %x(16進数)・%u(符号なし10進数)・%d(符号あり10進数)・%s(文字列)の4つ。
int printk(const char* format, ...); // 画面に文字列を出力する関数

#endif
