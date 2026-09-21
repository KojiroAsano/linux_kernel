#!/bin/bash
set -euo pipefail
# ↑ どれかのコマンドが失敗したら即座にスクリプトを止める。
#   これが無いと、例えばloader.asmの構文エラーでnasmが失敗しても
#   後続のddがそのまま実行され、boot.imgが黙って壊れた状態で
#   ビルド「成功」してしまう(実際に一度これで原因不明のハングを踏んだ)。

# ============================================================================
# 1. カーネル側を先にビルドして実サイズを確定させる。
#    (loader.asm/boot.asmはカーネルの実サイズを知る必要があるので、
#    先にこちらを済ませておく)
# ============================================================================

# --- アセンブリファイルのアセンブル ---
# -f elf64: 出力形式をELF64(Linuxなどで使われる、gcc/ldが扱える形式)にする。
#           boot.asm/loader.asmとは違い、これらはOS本体を組み立てる
#           部品なので、後でC言語のオブジェクトファイルとldでリンクする
#           必要があり、そのためにこの形式でアセンブルする。
nasm -f elf64 -o kernel.o kernel.asm   # kernel.asm → kernel.o
nasm -f elf64 -o trapa.o trap.asm       # trap.asm → trapa.o
nasm -f elf64 -o liba.o lib.asm          # lib.asm → liba.o

# --- C言語ファイルのコンパイル ---
# -std=c99:            C99規格でコンパイルする
# -mcmodel=large:       higher-half(0xffff800000000000のような、非常に
#                       大きい)アドレスにあるデータを正しく参照できる
#                       ようにする。通常の(small)コードモデルだと、
#                       アドレスが32bitに収まる前提で最適化されてしまい、
#                       このカーネルのアドレス配置では正しく動かない。
# -ffreestanding:        「OSが無い環境向け」のコンパイルであることを
#                       gccに伝える。標準ライブラリ(libc)が無い前提になり、
#                       mainではなくKMainがエントリポイントであることも
#                       この指定と合わせて意味を持つ。
# -fno-stack-protector:  gccが自動で挿入する「スタック破壊検出」の仕組みを
#                       無効化する。この仕組みは内部で__stack_chk_failと
#                       いう関数呼び出しに依存しているが、そんな関数は
#                       このカーネルには存在しないので、付けたままだと
#                       リンクエラーになる。
# -mno-red-zone:         「レッドゾーン」(関数のスタックの少し下側を、
#                       割り込みなどに邪魔されない前提で自由に使ってよい、
#                       という通常のユーザープログラム向けの最適化)を
#                       無効化する。カーネルでは割り込みがいつ発生するか
#                       分からず、その割り込みハンドラ自身がスタックを
#                       使うため、この前提が成り立たない。
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c main.c    # main.c → main.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c trap.c     # trap.c → trap.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c print.c     # print.c → print.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c debug.c      # debug.c → debug.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c memory.c      # memory.c → memory.o

# --- リンク ---
# -nostdlib: 標準ライブラリをリンクしない(存在しないので)
# -T link.lds: メモリ配置のルールとしてlink.ldsを使う
# 全部の.oファイルを1つの実行ファイル"kernel"にまとめる。
ld -nostdlib -T link.lds -o kernel kernel.o main.o trapa.o trap.o liba.o print.o debug.o memory.o   # 全.oをkernelへリンク

# objcopyで、ELF形式のヘッダ情報などを全部取り除き、実際にメモリへ
# 並べる中身(生のバイナリ)だけを取り出す。loader.asmはこのkernel.binを
# そのままディスクから読み込んで、メモリへコピーするだけなので、
# ELFヘッダを解釈する機能などは要らない。
objcopy -O binary kernel kernel.bin

# ============================================================================
# 2. kernel.binの実サイズからセクタ数を計算する。手で決め打ちしないので、
#    今後カーネルが育っても自動的に追従し、読み込み不足で壊れることがない。
# ============================================================================
KERNEL_SIZE=$(stat -c%s kernel.bin)               # kernel.binの実際のバイト数
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))    # 512バイト単位のセクタ数に切り上げ

# boot.img全体に必要なセクタ数を計算する。
# boot(1) + loader(5, boot.asm側で固定) + kernel(KERNEL_SECTORS) + 余白10
TOTAL_SECTORS=$((1 + 5 + KERNEL_SECTORS + 10))

echo "kernel.bin: ${KERNEL_SIZE} bytes -> KERNEL_SECTORS=${KERNEL_SECTORS}, boot.img=${TOTAL_SECTORS} sectors"

# ============================================================================
# 3. 計算したセクタ数をnasmの-Dでloader.asm/boot.asmに渡す。
#    (-Dを付けずに単体アセンブルした場合は、それぞれのファイル内の
#    %ifndefフォールバック値が使われる)
# ============================================================================
nasm -f bin -o loader.bin loader.asm -DKERNEL_SECTORS=$KERNEL_SECTORS
nasm -f bin -o boot.bin boot.asm -DTOTAL_SECTORS=$TOTAL_SECTORS
# ↑ こちらの2つは -f bin (生のバイナリ形式)でアセンブルする。
#   BIOSがそのままメモリへ読み込んで実行するだけのものなので、
#   ELF形式のようなヘッダ情報は不要で、むしろ邪魔になる。

# ============================================================================
# 4. boot.img(仮想的な1枚のディスク)を組み立てる
# ============================================================================
rm -f boot.img   # 前回のboot.imgが残っていれば削除する(-fは無くても止まらないように)
# /dev/zero(読むと無限に0バイトが出てくる特殊ファイル)から、
# TOTAL_SECTORS分(1セクタ=512バイト)だけ読み取って、全部0の
# boot.imgを新規作成する。
dd if=/dev/zero of=boot.img bs=512 count=$TOTAL_SECTORS

# それぞれの内容を、決まった位置(セクタ番号)に書き込んでいく。
# conv=notrunc を付けないと、dd はデフォルトで書き込み先ファイルを
# 一度空にしてから書き込もうとするので、直前の内容が消えてしまう。
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc          # セクタ0(先頭)
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc          # セクタ1から
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc           # セクタ6から

# ============================================================================
# 5. QEMU(PCエミュレータ)でboot.imgを実際に起動する
# ============================================================================
# 各オプションの意味:
#   -drive format=raw,file=boot.img  起動するディスクとして、組み立てた
#                                     boot.imgをそのまま(生のセクタ列と
#                                     して)使う
#   -cpu qemu64,+pdpe1gb              CPUの種類をqemu64(汎用モデル)に
#                                     しつつ、+pdpe1gbで「1GBページに
#                                     対応している」と明示的に申告させる
#                                     (loader.asmのCPUIDチェックが
#                                     これを前提にしている)
#   -m 512M                           仮想マシンに割り当てるメモリ量
#   -no-reboot                        トリプルフォルトなど致命的なエラーが
#                                     起きても、勝手に再起動せずその場で
#                                     止まってくれる(原因調査がしやすい)
#   -d int,cpu_reset                  割り込み・例外の発生や、CPUリセットの
#                                     発生をログに出力する(デバッグ用。
#                                     通常の動作には影響しない)
#
# 【注意】この下のバックスラッシュ(\)による行継続の途中に、コメント行を
# 挟んではいけない。bashは継続中の行に#が来ると、そこで論理行が終わった
# ものとして扱ってしまい、それより後ろのオプション(-mや-no-rebootなど)が
# 「別の、存在しないコマンド」として実行されようとして壊れる
# (実際に一度この形でミスをして壊しかけた)。
qemu-system-x86_64 \
  -drive format=raw,file=boot.img \
  -cpu qemu64,+pdpe1gb \
  -m 512M \
  -no-reboot \
  -d int,cpu_reset
