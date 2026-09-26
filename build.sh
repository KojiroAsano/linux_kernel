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
nasm -f elf64 -o kernel.o kernel.asm   # kernel.asm → kernel.o
nasm -f elf64 -o trapa.o trap.asm       # trap.asm → trapa.o
nasm -f elf64 -o liba.o lib.asm          # lib.asm → liba.o

# --- C言語ファイルのコンパイル ---
# -std=c99:            C99規格でコンパイルする
# -mcmodel=large:       higher-half(非常に大きい)アドレスにあるデータを
#                       正しく参照できるようにする。
# -ffreestanding:        「OSが無い環境向け」のコンパイルであることをgccに伝える。
# -fno-stack-protector:  gccが自動で挿入する「スタック破壊検出」を無効化する
#                       (__stack_chk_failが存在しないためリンクエラーになる)。
# -mno-red-zone:         割り込みハンドラがいつでもこのスタックの直下を
#                       使う可能性があるため、レッドゾーン最適化を無効化する。
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c main.c      # main.c → main.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c trap.c       # trap.c → trap.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c print.c       # print.c → print.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c debug.c        # debug.c → debug.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c memory.c        # memory.c → memory.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c process.c        # process.c → process.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c syscall.c         # syscall.c → syscall.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c lib.c              # lib.c → lib.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c keyboard.c           # keyboard.c → keyboard.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c ata.c                  # ata.c → ata.o
gcc -std=c99 -mcmodel=large -ffreestanding -fno-stack-protector -mno-red-zone -c fat.c                   # fat.c → fat.o

# --- リンク ---
# -nostdlib: 標準ライブラリをリンクしない(存在しないので)
# -T link.lds: メモリ配置のルールとしてlink.ldsを使う
ld -nostdlib -T link.lds -o kernel kernel.o main.o trapa.o trap.o liba.o print.o debug.o memory.o process.o syscall.o lib.o keyboard.o ata.o fat.o

# objcopyで、ELF形式のヘッダ情報などを全部取り除き、実際にメモリへ
# 並べる中身(生のバイナリ)だけを取り出す。
objcopy -O binary kernel kernel.bin

# ============================================================================
# 2. kernel.binの実サイズからセクタ数を計算する。手で決め打ちしないので、
#    今後カーネルが育っても自動的に追従し、読み込み不足で壊れることがない。
# ============================================================================
KERNEL_SIZE=$(stat -c%s kernel.bin)               # kernel.binの実際のバイト数
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))    # 512バイト単位のセクタ数に切り上げ

echo "kernel.bin: ${KERNEL_SIZE} bytes -> KERNEL_SECTORS=${KERNEL_SECTORS}"

# ============================================================================
# 3. ユーザーランド(lib/ → user1/ → user2/ → user3/)をビルドする。
# ============================================================================
# USER_SECTORS: 1本のユーザープログラムに割り当てるディスク領域(セクタ数)。
# loader.asm側の%define USER_SECTORS(固定10)と必ず一致させること
# (ここを変えるならloader.asmも合わせて変える必要がある)。
USER_SECTORS=10
USER_MAX_BYTES=$((USER_SECTORS * 512))

# boot(1) + loader(5、boot.asm/loader.asm側で固定) + kernel(実測)の直後から
# user1/user2/user3を順番に10セクタずつ配置する。
USER1_LBA=$((1 + 5 + KERNEL_SECTORS))
USER2_LBA=$((USER1_LBA + USER_SECTORS))
USER3_LBA=$((USER2_LBA + USER_SECTORS))

echo "USER1_LBA=${USER1_LBA} USER2_LBA=${USER2_LBA} USER3_LBA=${USER3_LBA}"

# lib/(ユーザーランド共通ライブラリ)を先にビルドし、lib.aを
# 各ユーザープログラムのフォルダへコピーしてから、それぞれbuild.shを呼ぶ。
(cd lib && ./build.sh)

for dir in user1 user2 user3; do
    cp lib/lib.a "${dir}/"
    (cd "${dir}" && ./build.sh)

    # 実際のuserN.binが、loader.asmが読み込む固定10セクタ(5120バイト)を
    # 超えていないか確認する。超えていたら、process.cのsetup_uvm()が
    # 呼ばれる時点でコピー元(loader.asmが読み込んだ範囲)が足りず、
    # プログラムの後半が化けた状態で実行されてしまうので、ここで
    # はっきり止める。
    USER_SIZE=$(stat -c%s "${dir}/${dir}.bin")
    echo "${dir}.bin: ${USER_SIZE} bytes (limit ${USER_MAX_BYTES})"
    if [ "$USER_SIZE" -gt "$USER_MAX_BYTES" ]; then
        echo "ERROR: ${dir}.bin (${USER_SIZE} bytes) exceeds the ${USER_SECTORS}-sector budget (${USER_MAX_BYTES} bytes)." >&2
        echo "        loader.asm/build.shのUSER_SECTORSを増やすか、プログラムを小さくしてください。" >&2
        exit 1
    fi
done

# ============================================================================
# 4. boot.img全体のセクタ数を計算し、-Dでloader.asm/boot.asmに渡す。
# ============================================================================
# boot(1) + loader(5) + kernel(実測) + user1/2/3(10セクタ×3) + 余白10
TOTAL_SECTORS=$((1 + 5 + KERNEL_SECTORS + USER_SECTORS*3 + 10))

echo "boot.img=${TOTAL_SECTORS} sectors"

nasm -f bin -o loader.bin loader.asm \
    -DKERNEL_SECTORS=$KERNEL_SECTORS \
    -DUSER1_LBA=$USER1_LBA \
    -DUSER2_LBA=$USER2_LBA \
    -DUSER3_LBA=$USER3_LBA
nasm -f bin -o boot.bin boot.asm -DTOTAL_SECTORS=$TOTAL_SECTORS
# ↑ こちらの2つは -f bin (生のバイナリ形式)でアセンブルする。
#   BIOSがそのままメモリへ読み込んで実行するだけのものなので、
#   ELF形式のようなヘッダ情報は不要で、むしろ邪魔になる。

# ============================================================================
# 5. boot.img(仮想的な1枚のディスク)を組み立てる
# ============================================================================
rm -f boot.img   # 前回のboot.imgが残っていれば削除する(-fは無くても止まらないように)
# /dev/zero(読むと無限に0バイトが出てくる特殊ファイル)から、
# TOTAL_SECTORS分(1セクタ=512バイト)だけ読み取って、全部0の
# boot.imgを新規作成する。
dd if=/dev/zero of=boot.img bs=512 count=$TOTAL_SECTORS status=none

# それぞれの内容を、決まった位置(セクタ番号)に書き込んでいく。
# conv=notrunc を付けないと、dd はデフォルトで書き込み先ファイルを
# 一度空にしてから書き込もうとするので、直前の内容が消えてしまう。
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc status=none          # セクタ0(先頭)
dd if=loader.bin of=boot.img bs=512 seek=1 conv=notrunc status=none          # セクタ1から
dd if=kernel.bin of=boot.img bs=512 seek=6 conv=notrunc status=none           # セクタ6から
dd if=user1/user1.bin of=boot.img bs=512 seek=$USER1_LBA conv=notrunc status=none
dd if=user2/user2.bin of=boot.img bs=512 seek=$USER2_LBA conv=notrunc status=none
dd if=user3/user3.bin of=boot.img bs=512 seek=$USER3_LBA conv=notrunc status=none

# ============================================================================
# 6. QEMU(PCエミュレータ)でboot.imgを実際に起動する
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
# 2台目の-drive(fat.img)について:
# 1台目(boot.img)がプライマリATAバスのマスタドライブ(ata.cが今まで
# 読み書きしていたのと同じ、0x1F0-0x1F7・ドライブ選択bit=0)になるのに
# 対し、-driveをもう1つ追加すると、QEMUのデフォルトの割り当て順で
# 2台目は同じプライマリバスのスレーブドライブ(同じ0x1F0-0x1F7、
# ドライブ選択bit=1)になる。ポート番号は共有するので、ata.c側は
# セカンダリバス用の別ポート対応を増やさずに、ドライブ選択ビットを
# 切り替えるだけでこちらも読めるようになる予定(次のステップで対応)。
# fat.imgはホスト側で`mkfs.vfat -F 12 fat.img`してテスト用ファイルを
# 入れたもの(このリポジトリに同梱)。boot.img/起動の仕組みには
# 一切手を入れていない。
#
# 【注意】この下のバックスラッシュ(\)による行継続の途中に、コメント行を
# 挟んではいけない。bashは継続中の行に#が来ると、そこで論理行が終わった
# ものとして扱ってしまい、それより後ろのオプション(-mや-no-rebootなど)が
# 「別の、存在しないコマンド」として実行されようとして壊れる
# (実際に一度この形でミスをして壊しかけた)。
qemu-system-x86_64 \
  -drive format=raw,file=boot.img \
  -drive format=raw,file=fat.img \
  -cpu qemu64,+pdpe1gb \
  -m 512M \
  -no-reboot \
  -d int,cpu_reset
