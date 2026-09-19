[BITS 16]
[ORG 0x7e00]

%define KERNEL_SECTORS 90   ; kernel.binの実サイズが変わったらここだけ調整する

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti; enable interrupt for keyboard
    mov [DriveId],dl

    mov ah, 0x0e
    mov al, 'L'
    int 0x10

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb NoLongMode

    mov eax, 0x80000001
    cpuid
    test edx, (1<<29)      ; LMビット（ロングモード対応）
    jz NoLongMode
    ; 注: Page1GBビット(bit26)はあえてチェックしない。
    ; QEMUの -cpu qemu64 はロングモード対応と申告する一方でPage1GB非対応と
    ; 申告するが、実際にはPS=1のPDPTEを問題なく処理できる。ここでbit26を
    ; 弾くと、この環境では常にNoLongMode側に落ちてしまう(実際に踏んだハング)。

    jmp skip_ext

NoLongMode:
    mov ah, 0x0e
    mov al, 'X'
    int 0x10
    jmp NotSupport

skip_ext:

LoadKernel:

    mov si,ReadPacket
    mov word[si],0x10
    mov word[si+2],KERNEL_SECTORS
    mov word[si+4],0
    mov word[si+6],0x1000
    mov dword[si+8],6
    mov dword[si+0xc],0

    mov dl,[DriveId]
    mov ah,0x42
    int 0x13
    jc  ReadError

    mov ah, 0x0e
    mov al, 'S'
    int 0x10

GetMemInfoStart:
    mov eax, 0xe820
    mov edx, 0x534d4150 ; 'SMAP'
    mov ecx, 20
    mov dword[0x9000],0
    mov edi, 0x9008
    xor ebx, ebx
    int 0x15
    jc NotSupport

GeMemInfo:
    add edi, 20
    inc dword[0x9000]
    test ebx, ebx
    jz GetMemDone
    mov eax, 0xe820
    mov edx, 0x534d4150 ; 'SMAP'
    mov ecx, 20
    int 0x15
    jnc GeMemInfo

    test ebx, ebx
    jnz GeMemInfo

GetMemDone:
    mov ah, 0x0e
    mov al, 'D'
    int 0x10

TestA20:
    mov ax,0xffff
    mov es,ax
    mov word[ds:0x7c00],0xa200
    cmp word[es:0x7c10],0xa200
    jne SetA20LineDone
    mov word[0x7c00],0xb200
    cmp word[es:0x7c10],0xb200
    jne SetA20LineDone

    ; A20が無効 → fast A20ゲート(ポート0x92)で有効化
    in al, 0x92
    or al, 2
    and al, 0xfe        ; bit0(高速リセット)は誤って立てない
    out 0x92, al

    ; 有効化後に再検証
    mov word[0x7c00],0xd200
    cmp word[es:0x7c10],0xd200
    je A20Error

SetA20LineDone:
    xor ax,ax
    mov es,ax
    mov ah, 0x0e
    mov al, 'A'
    int 0x10

SetVideoMode:
    mov ax, 3
    int 0x10

    cli

    ;global register
    lgdt [Gdt32Ptr]
    lidt [Idt32Ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:ProtectedModeStart

ReadError:
NotSupport:
A20Error:
End:
    hlt
    jmp End

[BITS 32]
ProtectedModeStart:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7c00

    mov byte [0xb8000], 'P'
    mov byte [0xb8001], 0xa

    cld
    mov edi,0x70000
    xor eax,eax
    mov ecx,0x10000/4
    rep stosd

    mov dword[0x70000],0x71003   ; PML4[0] → 0x71000番地のPDPTを指す (Present, Writable)
    mov dword[0x71000],10000111b ; PDPT[0]: Present, Writable, User, PS=1 → 物理0番地から1GBを恒等マッピング

    mov eax,(0xffff800000000000 >> 39)
    and eax,0x1ff
    mov dword[0x70000 + eax*8],0x72003   ; PML4[256] → 0x72000番地のPDPTを指す
    mov dword[0x72000],10000011b         ; PDPT[0]: Present, Writable, PS=1 → 物理0番地から1GBをKERNEL_BASEへマッピング

    lgdt [Gdt64Ptr]

    mov eax,cr4
    or eax,(1<<5)
    mov cr4,eax

    mov eax,0x70000
    mov cr3,eax

    mov ecx,0xc0000080
    rdmsr
    or eax,(1<<8)
    wrmsr

    mov eax,cr0
    or eax,(1<<31)
    mov cr0,eax

    jmp 0x8:LongModeStart




PEnd:
    hlt
    jmp PEnd

[BITS 64]
LongModeStart:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x7c00

    mov byte [0xb8000], 'L'
    mov byte [0xb8001], 0xa

    cld
    mov rdi,0x200000
    mov rsi,0x10000
    mov rcx,(KERNEL_SECTORS*512)/8
    rep movsq

    mov rax,0xffff800000200000
    jmp rax

LEnd:
    hlt
    jmp LEnd


DriveId:    db 0
ReadPacket: times 16 db 0

Gdt32:
    dq 0
Code32:
    dw 0xffff
    dw 0
    db 0
    db 0x9a
    db 0xcf
    db 0
Data32:
    dw 0xffff
    dw 0
    db 0
    db 0x92
    db 0xcf
    db 0

Gdt32Len equ $-Gdt32

Gdt32Ptr: dw Gdt32Len -1
          dd Gdt32

Idt32Ptr: dw 0
          dd 0

Gdt64:
    dq 0
    dq 0x0020980000000000   ; コードセグメント (L=1, 64bit)
    dq 0x0000920000000000   ; データセグメント
Gdt64Len equ $-Gdt64

Gdt64Ptr: dw Gdt64Len -1
             dq Gdt64
