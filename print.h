#ifndef PRINT_H_
#define PRINT_H_

#define LINE_SIZE 160

struct ScreenBuffer
{
    /* data */
    char* buffer;
    int column;
    int row;
};

int printk(const char* format, ...); // 画面に文字列を出力する関数

#endif 