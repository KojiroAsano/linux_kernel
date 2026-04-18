**拡張ディスク機能（INT 13h Extensions）**とは、
BIOS のディスクアクセス機能を 新しく拡張した仕組みです。

昔の BIOS の制限をなくすために追加されました。

1 昔のディスクアクセス（古い方式）

最初の BIOS は CHS方式でディスクを読んでいました。

CHSとは

Cylinder（シリンダ）
Head（ヘッド）
Sector（セクタ）

でディスク位置を指定する方法です。

例

Cylinder = 10
Head = 2
Sector = 5

という感じです。

しかしこの方法には 大きな問題がありました。

容量制限

CHSには最大値があります。

項目	最大
Cylinder	1023
Head	255
Sector	63

これにより 約8GBまでしか扱えないという問題がありました。

2 そこで作られたのが拡張ディスク機能

BIOSは新しい仕組みを追加しました。

それが

INT 13h Extensions

です。

特徴は

LBA方式

CHSの代わりに

LBA = Logical Block Address

を使います。

これは単純に

0
1
2
3
4
5

と セクタ番号を数える方式です。

例

LBA 0 → ブートセクタ
LBA 1 → 次のセクタ
LBA 2 → 次のセクタ

とてもシンプルです。

3 拡張ディスク機能で追加されたBIOS関数

主にこれです。

AH	機能
41h	拡張機能があるかチェック
42h	LBAで読み込み
43h	LBAで書き込み
48h	ディスク情報取得

あなたのコードは

mov ah,0x42
int 0x13

なので

LBAでディスク読み込み

を使っています。

4 LBA読み込みの仕組み

LBA読み込みでは Disk Address Packet という構造体を使います。

あなたのコードのこれです。

ReadPacket

中身

オフセット	内容
0	構造サイズ
2	読むセクタ数
4	メモリアドレス
8	LBA番号

あなたのコード

mov byte [si],16
mov word [si+2],5
mov word [si+4],0x7e00
mov dword [si+8],1

意味

LBA1から5セクタ読む
↓
メモリ0x7E00にロード
5 なぜチェックが必要？

古い BIOS では

AH=42

が 存在しません。

なので

mov ah,0x41
int 0x13

で

拡張機能ある？

と確認します。

6 あなたのブートローダの流れ

流れはこうです。

BIOS
 ↓
bootloader (0x7C00)
 ↓
INT13 extension check
 ↓
LBA read loader
 ↓
loader (0x7E00)
7 図
Disk

LBA0  ─ bootloader
LBA1  ─ loader
LBA2
LBA3

bootloader が

LBA1

を読みます。

まとめ

拡張ディスク機能とは

BIOSの新しいディスク読み込み機能

特徴

LBA方式

大容量ディスク対応

CHSの制限を解決

BIOS関数

AH=41  拡張機能チェック
AH=42  LBA読み込み

もしよければ次に
あなたのコードで一番重要な部分

Disk Address Packet

を 図付きで完全解説できます。

これが理解できると ブートローダ理解が一気に進みます


いい質問です 👍
結論から言うと

① 実際のブートローダもほぼ同じ構造です
② ReadPacket（DAP）は BIOS に「どう読むか」を伝えるために必要です

順番に説明します。

① 実際のブートローダも似ています

多くのOSのブートローダは 同じ仕組みです。

例：

OS	仕組み
GRUB	stage1 → stage2
Windows Boot Manager	MBR → bootmgr
LILO	stage1 → stage2

典型的な流れ

[ディスク]

LBA0  BootSector (512byte)
LBA1  Loader
LBA2  Loader
LBA3  Loader

ブートセクタは 512バイトしかないので

BootSector
   ↓
Loader (大きいコード)
   ↓
Kernel

という 2段階以上のブートになります。

あなたのコードはまさに

stage1 → stage2

の実装です。

② なぜ ReadPacket が必要なのか

BIOS拡張ディスク機能では

INT 13h AH=42h

を使います。

このとき BIOS は

レジスタだけでは情報が足りない

のです。

必要な情報：

・何セクタ読む？
・どこに置く？
・LBAはいくつ？

これをまとめたものが

Disk Address Packet (DAP)

です。

③ BIOSが欲しい情報

BIOSはこう聞いています。

BIOS:
どこから読むの？

何セクタ読むの？

メモリのどこに置くの？

それを ReadPacket で渡します。

ReadPacket
+0  packet size
+2  sector count
+4  memory offset
+6  memory segment
+8  LBA

つまり

BIOSへのリクエスト構造体

です。

④ もし ReadPacket が無いと？

古いBIOSでは

INT 13h AH=02h

という読み込みがあります。

これは

CHS方式

です。

レジスタで指定します。

CH  cylinder
CL  sector
DH  head
DL  drive

しかし問題があります

最大 8GB 程度

しか扱えません。

⑤ そこでLBA方式が登場

拡張BIOSでは

INT 13h AH=42h

を使います。

特徴

LBA方式
最大 8ZB

しかしレジスタでは足りないので

Disk Address Packet

を使います。

⑥ BIOS内部ではこう処理される

あなたのコード

mov si,ReadPacket
mov ah,42h
int 13h

BIOS内部イメージ

BIOS
  │
  │ DS:SI → ReadPacket
  ▼

┌─────────────────┐
│ sector count =5 │
│ dest =0000:7E00 │
│ LBA =1          │
└─────────────────┘

  │
  ▼

ディスク読み込み
  │
  ▼

0000:7E00 に書く
⑦ 実際のOSもこれを使う

例えば

GRUB

Syslinux

は

INT 13h 42h
Disk Address Packet

を使います。

まとめ

あなたの疑問の答え

実際のOSは？

→ ほぼ同じ構造

BootSector
   ↓
Loader
   ↓
Kernel
ReadPacketが必要な理由

BIOSに

・何セクタ
・どこに
・どのLBA

を伝えるための 構造体。

もしよければ次は
OS開発で超重要な話を説明できます👇

なぜ Bootloader は 0x7C00 に置かれるのか

なぜ Loader は 0x7E00 に置くのか

本物の GRUB のメモリマップ

512バイト制限の地獄

ここを理解すると OS開発レベルが一気に上がります。

すべてを図解できますか、メモリやHDなど

はい、実際のPCのブートの流れを「ディスク・BIOS・メモリ」全部まとめて図解します。
あなたのコードがどこで動いているかも一緒に説明します。💻📀

1. PC起動直後（電源ON）

CPUはまず BIOS を実行します。

CPU
 │
 ▼
BIOS
 │
 ▼
起動ディスクを探す

例

1 HDD
2 SSD
3 USB
4 CD

BIOSは最初のディスクの

LBA 0

を読み込みます。

2. ディスク構造

典型的なディスク

ハードディスク

LBA
0   BootSector (512B)
1   Loader
2   Loader
3   Loader
4   Kernel
5   Kernel
6   Kernel

図

Hard Disk
┌──────────────┐
│ LBA 0        │ Boot Sector
├──────────────┤
│ LBA 1        │ Loader
├──────────────┤
│ LBA 2        │ Loader
├──────────────┤
│ LBA 3        │ Loader
├──────────────┤
│ LBA 4        │ Kernel
├──────────────┤
│ LBA 5        │ Kernel
└──────────────┘
3. BIOSがBootSectorを読み込む

BIOSは

ディスク LBA0

を

メモリ 0000:7C00

にロードします。

BIOS
 │
 │ read LBA0
 ▼

Memory
0000:7C00

図

RAM

0000:7C00 ┌──────────────┐
          │ BootSector   │
          │ (512 bytes)  │
          └──────────────┘

そのあと

jmp 0000:7C00

で実行開始します。

4. Bootloaderの仕事

BootSectorは512バイトしかありません。

512 bytes

そのため

大きいプログラムを直接入れられない

なので

Loader

を読み込みます。

5. あなたのコードがやっていること

あなたのコード

mov si,ReadPacket
mov ah,42h
int 13h

これは

LBA1 から
5セクタ
読む

という意味です。

6. ReadPacket構造

メモリにこういうデータを作ります。

Memory

ReadPacket
┌──────────────┐
│size = 16     │
├──────────────┤
│reserved =0   │
├──────────────┤
│sectors =5    │
├──────────────┤
│offset =7E00  │
├──────────────┤
│segment=0000  │
├──────────────┤
│LBA low =1    │
├──────────────┤
│LBA high=0    │
└──────────────┘

意味

LBA1
↓
5セクタ
↓
0000:7E00 に書く
7. ディスク→メモリ読み込み

BIOSはこの情報を使って読み込みます。

Hard Disk
LBA1
LBA2
LBA3
LBA4
LBA5

↓

Memory
0000:7E00

図

Disk                     RAM

LBA1 ─────┐
LBA2 ─────┤
LBA3 ─────┤──────► 0000:7E00
LBA4 ─────┤
LBA5 ─────┘
8. メモリ全体図

ブート時のメモリ

Real Mode Memory Map
00000 ───────────────────
        IVT (Interrupt table)

00400 ───────────────────
        BIOS data area

0500
0600

7C00 ───────────────────
        BootSector
        (あなたのコード)

7E00 ───────────────────
        Loader
        (ディスクから読む)

90000
A0000 ───────────────────
        Video memory

F0000 ───────────────────
        BIOS ROM
9. 最後のジャンプ

あなたのコード

jmp 0x0000:0x7e00

意味

Loader を実行

図

BootSector
0000:7C00
     │
     │ load loader
     ▼
0000:7E00
     │
     ▼
Loader start
10. 本物のOSの流れ

実際のOSも同じです。

例

BIOS
 ↓
BootSector
 ↓
Stage2 Loader
 ↓
Kernel
 ↓
OS

例えば

GRUB

Windows Boot Manager

も ほぼ同じ構造です。

まとめ図（完全版）
          PC起動
             │
             ▼
          BIOS
             │
             ▼
     ディスク LBA0 読み込み
             │
             ▼

RAM
0000:7C00  BootSector
             │
             │ INT13h AH=42
             ▼

Disk
LBA1 ─┐
LBA2 ─┤
LBA3 ─┤
LBA4 ─┤
LBA5 ─┘
             │
             ▼

RAM
0000:7E00  Loader
             │
             ▼

jmp 0000:7E00
             │
             ▼
         Loader 実行

もしよければ次に 超重要な図を作れます：

① 本物のPCの完全メモリマップ（OS開発者必須）
② GRUBの実際のディスク構造
③ Bootloaderの512バイト地獄
④ BootSectorの完全テンプレート



non-canonical address（非カノニカルアドレス）とは：

x86-64（64bitモード）で、
CPUが許可していない形式のアドレスを使ったときに発生する例外。

■ ルール
64bitアドレスのうち実際に使われるのは下位48bit。
bit47の値を上位ビット（48〜63）にコピーしていないとエラーになる。

■ 正しい（canonical）
0x0000000000001234
0x00007FFFFFFFFFFF
0xFFFF800000000000

■ 間違い（non-canonical）
0x0000800000000000
0x123456789ABCDEF0

■ 結果
CPUが #GP（General Protection Fault）を発生させる

■ 主な原因
・ジャンプ先アドレスが壊れている
・レジスタに変な値が入っている
・ページテーブル設定ミス
・64bit移行時のバグ

これは OS開発の核心なのでかなり面白いです。
