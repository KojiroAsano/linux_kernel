#!/bin/bash
set -euo pipefail
# ↑ どれかのコマンドが失敗したら即座にスクリプトを止める。
#   これが無いと、例えばloader.asmの構文エラーでnasmが失敗しても
#   後続のddがそのまま実行され、boot.imgが黙って壊れた状態で
#   ビルド「成功」してしまう(実際に一度これで原因不明のハングを踏んだ)。

nasm -f bin -o boot.bin boot.asm
nasm -f bin -o loader.bin loader.asm
nasm -f elf64 -o kernel.o kernel.asm
nasm -f elf64 -o trapa.o trap.asm
nasm -f elf64 -o liba.o lib.asm
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c main.c
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c trap.c
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c print.c 
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c debug.c
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c memory.c

ld -nostdlib -T link.lds -o kernel kernel.o main.o trapa.o trap.o liba.o print.o debug.o memory.o
objcopy -O binary kernel kernel.bin





rm -f boot.img
dd if=/dev/zero of=boot.img bs=512 count=100

dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc


# kernel（5セクタ）
# dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc


qemu-system-x86_64 \
  -drive format=raw,file=boot.img \
  -cpu qemu64,+pdpe1gb \
  -m 512M \
  -no-reboot \
  -d int,cpu_reset