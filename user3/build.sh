#!/bin/bash
# ============================================================================
# user3/build.sh — このユーザープログラムをuser3.binとして組み立てる
# ============================================================================
# lib.a(共通ライブラリ)はこのフォルダに既にコピーされている前提
# (トップレベルのbuild.shが、lib/でビルドしてからここへコピーする)。
# ============================================================================
set -euo pipefail

nasm -f elf64 -o start.o start.asm
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c main.c
ld -nostdlib -Tlink.lds -o user start.o main.o lib.a
objcopy -O binary user user3.bin
