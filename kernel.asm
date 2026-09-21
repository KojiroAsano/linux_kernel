section .data

; セレクタ番号(0x08/0x10/0x18)はこのファイル内でのみ有効。loader.asmが
; ロングモード移行に使う一時的なGdt64とは無関係で、ここに来た時点で
; lgdtにより完全に置き換わっている(loader.asm側の0x10はデータセグメント
; だが、こちらの0x10はリング3のコードセグメントであり意味が異なる)。
Gdt64:
    dq 0
    dq 0x0020980000000000  ; 0x08: カーネル(リング0)コードセグメント
    dq 0x0020f80000000000  ; 0x10: ユーザー(リング3)コードセグメント
    dq 0x0000f20000000000  ; 0x18: ユーザー(リング3)データセグメント
    dq 0x0000920000000000  ; 0x20: カーネル(リング0)データセグメント
TssDesc:
    dw TssLen-1
    dw 0
    db 0
    db 0x89
    db 0
    db 0
    dq 0

Gdt64Len: equ $-Gdt64

Gdt64Ptr: dw Gdt64Len-1
          dq Gdt64

DFStack: times 4096 db 0   ; ダブルフォルト(ベクタ8)専用のIST1スタック。
DFStackTop:                ; スタック自体が壊れて起きるダブルフォルトを、同じ壊れた
                            ; スタックのまま処理しようとして連鎖的にトリプルフォルト
                            ; するのを防ぐため、専用の領域を別に用意しておく。

Tss:
    dd 0
    dq 0xffff800000190000  ; RSP0: リング3からの割り込み/例外時にロードするカーネル
                            ; スタック。物理0x190000はkalloc()が&end(カーネル本体の
                            ; 終端、2MB以降)より前を配らないため今のところ衝突しない。
    times 24 db 0           ; RSP1, RSP2, 予約(未使用)
    dq DFStackTop           ; IST1: ダブルフォルト用スタック(上のDFStackTop)
    times 48 db 0           ; IST2-7(未使用)
    times 8 db 0            ; 予約(未使用)
    dw 0                    ; 予約領域(仕様上ゼロでなければならない)
    dw TssLen                ; I/OマップベースアドレスをTSSリミット(TssLen-1)より
                             ; 大きい値にすることで、I/O許可ビットマップ無し
                             ; (リング3からの生ポート入出力は全部例外)を明示する

TssLen: equ $-Tss

section .text
extern KMain
global start

start:
    mov rax, Gdt64Ptr
    lgdt [rax]

    ; SS/DS/ESはloader.asmの一時的なGdt64から読み込まれた値のまま残っている。
    ; loader.asm側の0x10はデータセグメントだったが、上でロードしたこちらの
    ; Gdt64では0x10はリング3コードセグメントなので、SSにコードセグメントが
    ; 乗ったまま割り込みが発生するとiretqのSS再ロードで#GPになる
    ; (実際に割り込みを有効化した際に踏んだ)。ここで明示的に読み直す。
    mov ax, 0x20
    mov ss, ax
    mov ds, ax
    mov es, ax

SetTss:
    mov rax,Tss
    mov rdi,TssDesc
    mov [rdi+2],ax
    shr rax,16
    mov [rdi+4],al
    shr rax,8
    mov [rdi+7],al
    shr rax,8
    mov [rdi+8],eax
    mov ax,0x28
    ltr ax

InitPIT:
    mov al,(1<<2)|(3<<4)  ; channel0, lobyte/hibyte, mode2(rate generator), binary
    out 0x43,al

    mov ax,11931
    out 0x40,al
    mov al,ah
    out 0x40,al

InitPIC:
    mov al,0x11
    out 0x20,al
    out 0xa0,al

    mov al,32
    out 0x21,al
    mov al,40
    out 0xa1,al

    mov al,4
    out 0x21,al
    mov al,2
    out 0xa1,al

    mov al,1
    out 0x21,al
    out 0xa1,al

    mov al,11111110b
    out 0x21,al
    mov al,11111111b
    out 0xa1,al

    mov rax, KernelEntry
    push 8
    push rax
    db 0x48
    retf

KernelEntry:
    mov rsp,0xffff800000200000
    call KMain
    
End:
    hlt
    jmp End


