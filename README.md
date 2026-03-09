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
