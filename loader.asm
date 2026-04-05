[BITS 16]
[ORG 0x7E00]

start:
    mov ax,cs
    mov ds,ax
    mov es,ax

    mov [DriveID],dl

; --- kernel load ---
;   Set up the packet for reading the kernel (the third sector, LBA=2) into memory at 0x1000. The packet structure is as follows:
; Offset  Size  Description
; 0       1     Size of the packet (16 bytes)
; 2       2     Number of sectors to read (20 sectors)
; 4       2     Segment of the buffer (0x1000 >> 4
; 6       2     Offset of the buffer (0x1000 & 0xFFFF)
; 8       4     LBA of the first sector to read (2)
    mov si,ReadPacket
    mov byte [si],16
    mov word [si+2],20
    mov word [si+4],0
    mov word [si+6],0x1000
    mov dword [si+8],6

    
    mov ah,0x42 ;
    mov dl,[DriveID]
    int 0x13
    jc $

; --- protected mode ---
    cli ; disable interrupts
    lgdt [Gdt32Ptr] ; load GDT for protected mode

    mov eax,cr0
    or eax,1
    mov cr0,eax
    jmp 0x08:pmode

[BITS 32]
pmode:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,0x80000

; --- paging ---
mov edi,0x70000
xor eax,eax
mov ecx,0x3000/4
rep stosd

mov dword [0x70000],0x71003
mov dword [0x70000+4],0

mov dword [0x71000],0x72003
mov dword [0x71000+4],0

; =========================
; ★ここを変更（最重要）
; =========================
mov ecx,64              ; ← 範囲も増やす
mov edi,0x72000
xor eax,eax

.map:
    mov edx,eax
    or edx,0x87         ; ★ USER用なのでこれでOK

    mov [edi],edx
    mov dword [edi+4],0 ; ★ これ絶対必要

    add eax,0x200000
    add edi,8
    loop .map

; --- long mode ---
    lgdt [Gdt64Ptr]

    mov eax,cr4
    or eax,(1<<5)
    mov cr4,eax

    mov eax,0x70000
    mov cr3,eax

    mov ecx,0xC0000080
    rdmsr
    or eax,(1<<8)
    wrmsr

    mov eax,cr0
    or eax,(1<<31)
    mov cr0,eax

    jmp 0x08:lmode

[BITS 64]
lmode:
    mov esp,0x80000
    mov rsp,0x80000

    mov rsi,0x10000
    mov rdi,0x200000
    mov rcx,(20*512)/8
    rep movsq

    jmp 0x200000

DriveID db 0
ReadPacket times 16 db 0

Gdt32:
    ; | Base | Limit | Access | Flags |
    ; 部分	意味
    ; FFFF	Limit（下位）
    ; 0000	Base（下位）
    ; 00	Base（中位）
    ; 9A	Access
    ; CF	Flags + Limit（上位）
    ; 00	Base（上位）
    dq 0
    dq 0x00CF9A000000FFFF   ; code segment  executable, readable, accessed
    dq 0x00CF92000000FFFF   ; data segment executable, readable, accessed

Gdt32Ptr:
    ; GDTのサイズとアドレスを指定する構造体
    
    dw $-Gdt32-1 ; GDTのサイズを指定する。GDTは3エントリで、各エントリは8バイトなので、3*8-1=23=0x17
    dd Gdt32    ;   GDTのアドレスを指定する。Gdt32はこのコードのどこかに配置されているので、そのアドレスを指定する

Gdt64:
    dq 0
    dq 0x00AF9A000000FFFF   ; code (64bit)
    dq 0x00AF92000000FFFF   ; data

Gdt64Ptr:
    dw $-Gdt64-1
    dd Gdt64