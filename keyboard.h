#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include "stdint.h"

// ============================================================================
// struct KeyboardBuffer — キー入力を溜めておくリングバッファ
// ============================================================================
// frontから読み出し、endへ書き込む。front==endなら空。
// (end+1)%size==frontならいっぱい(この時は新しい入力を捨てる)。
struct KeyboardBuffer {
    char buffer[500];
    int front;
    int end;
    int size;
};

// keyboard_read()が、直前のスキャンコードの意味を覚えておくためのフラグ。
#define E0_SIGN (1 << 0)     // 拡張スキャンコード(0xE0で始まる2バイト目待ち)
#define SHIFT (1 << 1)        // Shiftキーが押されている
#define CAPS_LOCK (1 << 2)     // CapsLockが有効

char read_key_buffer(void);     // リングバッファから1文字取り出す(syscall.cから呼ばれる)
void keyboard_handler(void);     // IRQ1(ベクタ33)が来るたびに呼ばれる(trap.cから)
unsigned char in_byte(uint16_t port);  // 指定ポートから1バイト読む(trap.asm)

#endif
