#!/bin/bash
set -euo pipefail
# ↑ どれかのコマンドが失敗したら即座にスクリプトを止める。
#   これが無いと、例えばloader.asmの構文エラーでnasmが失敗しても
#   後続のddがそのまま実行され、boot.imgが黙って壊れた状態で
#   ビルド「成功」してしまう(実際に一度これで原因不明のハングを踏んだ)。

# 1. カーネル側を先にビルドして実サイズを確定させる。
#    (loader.asm/boot.asmはカーネルの実サイズを知る必要があるので、
#    先にこちらを済ませておく)
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

# 2. kernel.binの実サイズからセクタ数を計算する。手で決め打ちしないので、
#    今後カーネルが育っても自動的に追従し、読み込み不足で壊れることがない。
KERNEL_SIZE=$(stat -c%s kernel.bin)
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))

# boot(1) + loader(5, boot.asm側で固定) + kernel(KERNEL_SECTORS) + 余白10
TOTAL_SECTORS=$((1 + 5 + KERNEL_SECTORS + 10))

echo "kernel.bin: ${KERNEL_SIZE} bytes -> KERNEL_SECTORS=${KERNEL_SECTORS}, boot.img=${TOTAL_SECTORS} sectors"

# 3. 計算したセクタ数をnasmの-Dでloader.asm/boot.asmに渡す。
#    (-Dを付けずに単体アセンブルした場合は、それぞれのファイル内の
#    %ifndefフォールバック値が使われる)
nasm -f bin -o loader.bin loader.asm -DKERNEL_SECTORS=$KERNEL_SECTORS
nasm -f bin -o boot.bin boot.asm -DTOTAL_SECTORS=$TOTAL_SECTORS

rm -f boot.img
dd if=/dev/zero of=boot.img bs=512 count=$TOTAL_SECTORS

dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc

qemu-system-x86_64 \
  -drive format=raw,file=boot.img \
  -cpu qemu64,+pdpe1gb \
  -m 512M \
  -no-reboot \
  -d int,cpu_reset
