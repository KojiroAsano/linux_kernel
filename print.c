// ============================================================================
// print.c — カーネル用の画面出力(printk)
// ============================================================================
// OSにはまだprintf/malloc/文字列関数などの標準ライブラリが無いので、
// 画面に文字を出すための最低限の仕組みを全部自前で用意している。
// VGAテキストモード(0xb8000番地から、2バイトで1文字)に直接書き込む
// ことで、文字を表示している。
// ============================================================================

#include "stdint.h"
#include "stdarg.h"   // 可変長引数(va_list, va_start, va_arg, va_end)を使うため
#include "print.h"
#include "lib.h"       // memset, memcpy
#include "memory.h"     // P2V()

// 画面バッファの状態を覚えておく変数。P2V(0xb8000)としているのは、
// higher-halfカーネルでは物理アドレスをそのまま使えず、対応する
// 仮想アドレスに変換する必要があるため(memory.h参照)。
static struct ScreenBuffer screen_buffer = {(char*)P2V(0xb8000), 0, 0};

// ============================================================================
// udecimal_to_string — 符号なし整数を10進数の文字列に変換する
// ============================================================================
// digitsを10で割った余りを1桁ずつ取り出していくが、この方法だと
// 「下の桁から」順番に出てくる(例えば123なら3,2,1の順)。なので、
// いったんdigits_bufferに逆順のまま溜めてから、最後に後ろから
// 読み出すことで正しい順番(1,2,3)にして書き込んでいる。
static int udecimal_to_string(char *buffer, int position, uint64_t digits)
{
    char digits_map[10] = "0123456789";  // 数字→文字への変換表
    char digits_buffer[25];               // 1桁ずつ、逆順に溜めておく一時領域
    int size = 0;                          // 溜めた桁数

    do {
        digits_buffer[size++] = digits_map[digits % 10];  // 一番下の桁を文字にして溜める
        digits /= 10;                                       // 1桁分捨てる
    } while (digits != 0);   // do-whileなので、digitsが最初から0でも
                              // 最低1回は実行される(「0」という1文字は
                              // 出力される)

    // 逆順に溜めたdigits_bufferを、後ろから前へなめることで正しい順番にして出力
    for (int i = size-1; i >= 0; i--) {
        buffer[position++] = digits_buffer[i];   // 1文字ずつ本来のバッファへ書き込む
    }

    return size;   // 実際に書き込んだ文字数
}

// ============================================================================
// decimal_to_string — 符号あり整数を10進数の文字列に変換する
// ============================================================================
static int decimal_to_string(char *buffer, int position, int64_t digits)
{
    int size = 0;   // 追加で書き込んだ文字数(符号分)

    if (digits < 0) {   // 負の数かどうか確認
        digits = -digits;         // 負の数は符号を反転させてから処理
        buffer[position++] = '-';  // 先に'-'を書いておく
        size = 1;
    }

    // 絶対値になった数を、符号なし変換関数にそのまま任せる
    size += udecimal_to_string(buffer, position, (uint64_t)digits);
    return size;   // '-'の分も含めた合計文字数
}

// ============================================================================
// hex_to_string — 整数を16進数の文字列に変換する(末尾に'H'を付ける)
// ============================================================================
// 考え方はudecimal_to_stringと同じで、16で割った余りを1桁ずつ
// (0-9はそのまま、10-15はA-Fに)変換して溜め、逆順にして出力する。
static int hex_to_string(char *buffer, int position, uint64_t digits)
{
    char digits_buffer[25];               // 1桁ずつ、逆順に溜めておく一時領域
    char digits_map[16] = "0123456789ABCDEF";  // 数字→文字への変換表
    int size = 0;                           // 溜めた桁数

    do {
        digits_buffer[size++] = digits_map[digits % 16];  // 一番下の桁を文字にして溜める
        digits /= 16;                                       // 1桁分捨てる
    } while (digits != 0);   // digitsが0になるまで繰り返す

    for (int i = size-1; i >= 0; i--) {   // 逆順に溜めた桁を、正しい順番で書き込む
        buffer[position++] = digits_buffer[i];
    }

    buffer[position++] = 'H';   // 「これは16進数ですよ」の印として末尾に'H'を付ける

    return size+1;   // 数字の桁数+'H'の1文字
}

// ============================================================================
// read_string — 文字列をそのままバッファへコピーする(%s用)
// ============================================================================
static int read_string(char *buffer, int position, const char *string)
{
    int index = 0;   // 何文字コピーしたか

    // '\0'(文字列の終わりを表す番兵)に出会うまでコピーし続ける
    for (index = 0; string[index] != '\0'; index++) {
        buffer[position++] = string[index];   // 1文字コピー
    }

    return index;   // コピーした文字数
}

// ============================================================================
// write_screen — 組み立てた文字列を、実際にVGAテキストメモリへ書き込む
// ============================================================================
static void write_screen(const char *buffer, int size, struct ScreenBuffer *sb, char color)
{
    int column = sb->column;   // 今のカーソル位置(列)を取り出す
    int row = sb->row;         // 今のカーソル位置(行)を取り出す

    for (int i = 0; i < size; i++) {   // 渡された文字列を1文字ずつ処理する
        if (row >= 25) {
            // 画面の一番下(25行目)を超えてしまう場合は、いわゆる
            // 「スクロール」をする: 2行目から25行目までの内容を
            // まるごと1行分上へずらし(memcpy)、一番下の行を
            // クリアする(memset)。それから改めてrowを1つ戻す。
            memcpy(sb->buffer,sb->buffer+LINE_SIZE,LINE_SIZE*24);   // 1行分上へずらす
            memset(sb->buffer+LINE_SIZE*24,0,LINE_SIZE);             // 最終行をクリア
            row--;                                                    // 行位置を1つ戻す
        }

        if (buffer[i] == '\n') {
            // 改行文字は画面には表示せず、カーソル位置を次の行の先頭へ動かすだけ
            column = 0;   // 列を先頭に戻す
            row++;        // 次の行へ
        }
        else {
            // 文字コードと色の2バイトを、該当する画面上の位置へ書き込む。
            // column*2+row*LINE_SIZE が「その文字が画面上の何バイト目か」
            // を表す計算(1文字2バイト、1行LINE_SIZEバイトなので)。
            sb->buffer[column*2+row*LINE_SIZE] = buffer[i];   // 文字コードを書き込む
            sb->buffer[column*2+row*LINE_SIZE+1] = color;     // 色を書き込む

            column++;   // 次の列へ進める

            if (column >= 80) {   // 右端まで来たら次の行へ折り返す
                column=0;   // 列を先頭に戻す
                row++;      // 次の行へ
            }
        }
    }

    // 今回書いた分の最終的なカーソル位置を覚えておく(次回のprintk呼び出しで続きから書けるように)
    sb->column = column;   // 最終的な列位置を保存
    sb->row = row;         // 最終的な行位置を保存
}

// ============================================================================
// printk — カーネル版のprintf
// ============================================================================
// 書式文字列(format)の中の%x, %u, %d, %sを、可変長引数(args)から
// 受け取った値に置き換えながら、いったんbufferに全部組み立ててから、
// 最後にまとめてwrite_screen()で画面へ出力する。
int printk(const char *format, ...)
{
    // 【注意】このbufferは固定1024バイトで、書き込む位置(buffer_size)の
    // 上限チェックを一切していない。もし合計で1024バイトを超えるような
    // 長い書式文字列・長い%s文字列を渡すと、このスタック上の配列の外側を
    // 書き換えてしまう(スタックバッファオーバーフロー)。今のところ
    // カーネル内の呼び出しはどれも短い文字列しか渡していないので実害は
    // 出ていないが、本来はbuffer_sizeがsizeof(buffer)を超えないよう、
    // 各所で上限チェックを入れるべき箇所。
    char buffer[1024];    // 組み立てた文字列を溜めておく一時バッファ
    int buffer_size = 0;  // bufferに今何バイト書き込んだか
    int64_t integer = 0;  // %x/%u/%dの値を受け取る一時変数
    char *string = 0;     // %sの値を受け取る一時変数
    va_list args;         // 可変長引数を1つずつ読み出すためのカーソル

    va_start(args,format);   // 可変長引数を読み取る準備

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            // 普通の文字はそのままバッファにコピー
            buffer[buffer_size++] = format[i];
        }
        else {
            // '%'の次の1文字を見て、どの書式指定かを判定する
            switch (format[++i]) {
                case 'x':   // 16進数
                    integer = va_arg(args, int64_t);   // 次の引数を取り出す
                    buffer_size += hex_to_string(buffer, buffer_size, (uint64_t)integer);  // 変換して追記
                    break;

                case 'u':   // 符号なし10進数
                    integer = va_arg(args, int64_t);   // 次の引数を取り出す
                    buffer_size += udecimal_to_string(buffer, buffer_size, (uint64_t)integer);  // 変換して追記
                    break;

                case 'd':   // 符号あり10進数
                    integer = va_arg(args, int64_t);   // 次の引数を取り出す
                    buffer_size += decimal_to_string(buffer, buffer_size, integer);  // 変換して追記
                    break;

                case 's':   // 文字列
                    string = va_arg(args, char*);   // 次の引数(文字列)を取り出す
                    buffer_size += read_string(buffer, buffer_size, string);  // そのまま追記
                    break;

                default:
                    // 知らない書式指定子だった場合は、'%'をそのまま出力して、
                    // 次の1文字は普通の文字として再度読み直す(i--で1つ戻す)
                    buffer[buffer_size++] = '%';
                    i--;
            }
        }
    }

    // 組み立てた文字列を、色0xf(白)でまとめて画面へ出力する
    write_screen(buffer, buffer_size, &screen_buffer, 0xf);
    va_end(args);   // 可変長引数の読み取りを終了する

    return buffer_size;   // 実際に出力した文字数
}
