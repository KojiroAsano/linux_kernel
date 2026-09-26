// ============================================================================
// print.c(ユーザーランド版) — ユーザープログラム用のprintf
// ============================================================================
// カーネル側のprint.c(printk)とほぼ同じ組み立てロジックだが、最後の
// 出力先だけが違う。カーネル側はVGAメモリへ直接書き込むが、こちらは
// リング3で動いているのでVGAメモリへ直接触ることができず(触ろうと
// すれば#GP/#PFになる)、syscall.asmのwriteu()でカーネルにシステム
// コールを依頼し、代わりに書いてもらう。
// ============================================================================

#include "stdint.h"
#include "stdarg.h"

extern int writeu(char *buffer, int buffer_size);   // syscall.asm

static int udecimal_to_string(char *buffer, int position, uint64_t digits)
{
    char digits_map[10] = "0123456789";
    char digits_buffer[25];
    int size = 0;

    do {
        digits_buffer[size++] = digits_map[digits % 10];
        digits /= 10;
    } while (digits != 0);

    for (int i = size-1; i >= 0; i--) {
        buffer[position++] = digits_buffer[i];
    }

    return size;
}

static int decimal_to_string(char *buffer, int position, int64_t digits)
{
    int size = 0;

    if (digits < 0) {
        digits = -digits;
        buffer[position++] = '-';
        size = 1;
    }

    size += udecimal_to_string(buffer, position, (uint64_t)digits);
    return size;
}

static int hex_to_string(char *buffer, int position, uint64_t digits)
{
    char digits_buffer[25];
    char digits_map[16] = "0123456789ABCDEF";
    int size = 0;

    do {
        digits_buffer[size++] = digits_map[digits % 16];
        digits /= 16;
    } while (digits != 0);

    for (int i = size-1; i >= 0; i--) {
        buffer[position++] = digits_buffer[i];
    }

    buffer[position++] = 'H';

    return size+1;
}

static int read_string(char *buffer, int position, const char *string)
{
    int index = 0;

    for (index = 0; string[index] != '\0'; index++) {
        buffer[position++] = string[index];
    }

    return index;
}

// ============================================================================
// printf — ユーザープログラムから使う、書式付き出力
// ============================================================================
// %x(16進数)・%u(符号なし10進数)・%d(符号あり10進数)・%s(文字列)・
// %c(1文字)に対応。カーネル側のprintkと同じく、いったんbufferへ
// 全部組み立ててから、最後にまとめてwriteu()(=システムコール経由で
// write_screen)へ渡す。
//
// 【%cの引数について】呼び出し側でchar型の変数をそのまま渡しても、
// 可変長引数では自動的にint(32bit)へ昇格される。x86-64のSystem V
// 呼び出し規約では、可変長引数の整数はどのサイズでも8バイト分の
// レジスタ/スタック領域に(上位ビットを含めて正しく)格納されるため、
// 他の書式指定子と同じくva_arg(args, int64_t)で読み出しても
// 問題なく値を取り出せる(この関数の他の書式指定子と同じ前提)。
int printf(const char *format, ...)
{
    char buffer[1024];
    int buffer_size = 0;
    int64_t integer = 0;
    char *string = 0;
    va_list args;

    va_start(args,format);

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            buffer[buffer_size++] = format[i];
        }
        else {
            switch (format[++i]) {
                case 'x':
                    integer = va_arg(args, int64_t);
                    buffer_size += hex_to_string(buffer, buffer_size, (uint64_t)integer);
                    break;

                case 'u':
                    integer = va_arg(args, int64_t);
                    buffer_size += udecimal_to_string(buffer, buffer_size, (uint64_t)integer);
                    break;

                case 'd':
                    integer = va_arg(args, int64_t);
                    buffer_size += decimal_to_string(buffer, buffer_size, integer);
                    break;

                case 's':
                    string = va_arg(args, char*);
                    buffer_size += read_string(buffer, buffer_size, string);
                    break;

                case 'c':
                    integer = va_arg(args, int64_t);
                    buffer[buffer_size++] = (char)integer;
                    break;

                default:
                    buffer[buffer_size++] = '%';
                    i--;
            }
        }
    }

    buffer_size = writeu(buffer, buffer_size);
    va_end(args);

    return buffer_size;
}
