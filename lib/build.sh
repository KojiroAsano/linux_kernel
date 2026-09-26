#!/bin/bash
# ============================================================================
# lib/build.sh — ユーザーランド共通ライブラリ(lib.a)を組み立てる
# ============================================================================
# syscall.asm(システムコールの呼び出しスタブ)・lib.asm(memset等)・
# print.c(printf)をそれぞれアセンブル/コンパイルし、1つの静的
# ライブラリlib.aにまとめる。user1/user2/user3のbuild.shは、この
# lib.aをコピーしてきて自分のプログラム本体とリンクする。
# ============================================================================
set -euo pipefail

nasm -f elf64 -o syscall.o syscall.asm
nasm -f elf64 -o lib.o lib.asm
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c print.c
ar rcs lib.a print.o syscall.o lib.o
