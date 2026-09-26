#include "lib.h"

#define LINE_MAX 128

// ============================================================================
// read_line — キーボードから1行読み取る
// ============================================================================
// keyboard_readu()を1文字ずつ呼び、バッファに溜めていく。
//   ・Enter('\n')     → 行を確定させ、null終端して返す
//   ・Backspace('\b') → バッファの末尾を1つ消す(バッファが空なら何もしない)
//   ・それ以外          → バッファに追加する(buffer_sizeを超える分は無視する)
// 押されたキーはその場でprintf("%c",ch)を使ってそのまま画面へエコー
// 表示する。バックスペースの画面上の消去自体はwrite_screen(print.c)側が
// 「行の先頭では何もしない」を含めてすでに処理してくれるので、ここでは
// 単に'\b'をそのまま出力するだけでよい。
// 戻り値は読み取った文字数(null終端は含まない)。
static int read_line(char *buffer, int buffer_size)
{
    int len = 0;
    char ch;

    while (1) {
        ch = keyboard_readu();   // 1文字取得(無ければ内部でブロックする)

        if (ch == '\n') {
            printf("%c", ch);     // 改行を画面にも反映する
            buffer[len] = '\0';    // 呼び出し側が普通の文字列として扱えるようにする
            return len;
        }
        else if (ch == '\b') {
            if (len > 0) {
                len--;
                printf("%c", ch);   // 画面側の1文字消去はwrite_screenが担当
            }
        }
        else {
            if (len < buffer_size - 1) {   // null終端の1バイト分は必ず残す
                buffer[len++] = ch;
                printf("%c", ch);            // 入力した文字をそのまま表示(エコー)
            }
            // バッファが満杯の場合は、echoも含めて黙って捨てる
            // (keyboard.cのリングバッファが満杯時に捨てるのと同じ考え方)
        }
    }
}

// ============================================================================
// starts_with — sがprefixで始まっているかどうかを調べる
// ============================================================================
// C標準ライブラリのstrncmp相当が無いので、1文字ずつ自分で比較する。
// prefixの終端('\0')まで全部一致すれば真。途中でsの方が短く尽きた
// 場合は、s[i]=='\0'とprefix[i](まだ'\0'でない)が食い違うので
// 正しく偽になる。
static int starts_with(const char *s, const char *prefix)
{
    int i = 0;

    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) {
            return 0;
        }
        i++;
    }

    return 1;
}

// ============================================================================
// parse_uint — 文字列先頭の10進数を整数に変換する(atoi相当)
// ============================================================================
// 数字でない文字(空白や'\0'など)が出てきた時点で打ち切る。
static int parse_uint(const char *s)
{
    int value = 0;

    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }

    return value;
}

// ============================================================================
// run_command — 1行分のコマンド文字列を解析して実行する
// ============================================================================
static void run_command(char *line)
{
    if (starts_with(line, "kill ")) {
        // "kill "の5文字はもう判定済みなので、その続きから数字を読む。
        int pid = parse_uint(line + 5);

        if (killu(pid) == 0) {
            printf("killed pid %d\n", pid);
        }
        else {
            printf("no such process: %d\n", pid);
        }
    }
    else if (starts_with(line, "mem")) {
        printf("total memory: %u MB\n", get_total_memoryu());
    }
    else if (line[0] == '\0') {
        // 何も入力せずEnterだけ押した場合は何もしない
    }
    else {
        printf("unknown command: %s\n", line);
    }
}

int main(void)
{
    char line[LINE_MAX];

    while (1) {
        printf("> ");
        read_line(line, LINE_MAX);
        run_command(line);
    }

    return 0;
}
