[BITS 16]
[ORG 0x7E00]

start:

    mov ax,cs
    mov ds,ax
    mov es,ax

    mov [DriveID],dl

    mov eax,0x80000000
    cpuid
    cmp eax,0x80000001
    jb NotSupport

    mov eax,0x80000001
    cpuid

    test edx,(1<<29)
    jz NotSupport

    ; test edx,(1<<26)
    ; jz NotSupport

LoadKernel:
    mov si,ReadPacket

    mov byte  [si],16       ; packet size
    mov byte  [si+1],0      ; reserved
    mov word  [si+2],90    ; sectors to read // use 90 instead of 100
    mov word  [si+4],0x0    ;offset
    mov word  [si+6],0x1000; segment
    mov dword [si+8],6      ; LBA low
    mov dword [si+12],0     ; LBA high

    mov ah,0x42
    mov dl,[DriveID]
    int 0x13
    jc ReadError

GetMemInfoStart:
    mov eax,0xE820
    mov edx,0x534D4150 ; 'SMAP' 
    mov ecx,20
    mov edi,0x9000
    xor ebx,ebx
    int 0x15
    jc NotSupport

GetMemInfo:
    add edi,20
    mov eax,0xE820
    mov edx,0x534D4150 ; 'SMAP' 
    mov ecx,20
    int 0x15
    jc GetMemDone

    test ebx,ebx
    jnz  GetMemInfo

GetMemDone:
TestA20:
    mov ax, 0xffff
    mov es, ax
    mov word [ds:0x7c00], 0xa200
    cmp word [es:0x7c10], 0xa200
    jne SetA20LineDone
    mov word [0x7c00], 0xb200
    cmp word [es:0x7c10], 0xb200
    je End


SetA20LineDone:
    xor ax, ax
    mov es, ax

SetVideoMode:
    mov ax,0x0003
    int 0x10

    cli 
    lgdt [Gdt32Ptr]
    lidt [Idt32Ptr]

    mov eax, cr0
    or eax, 0x1 ; set PE bit
    mov cr0, eax
    jmp 0x08:PMEntry

NotSupport:
ReadError:
End:

    hlt
    jmp End

[BITS 32]
PMEntry:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,0x7c00

    ; mov dword [0x80000], 0x817007   ; entry point of the kernel
    ; mov dword [0x81000], 10000111b     ; flags: present, ring 0, code segment, accessed

    ; --- ページテーブル作成（超簡易） ---
    mov dword [0x80000], 0x81003   ; PML4 → PDP
    mov dword [0x81000], 0x82003   ; PDP → PD
    mov dword [0x82000], 0x000083  ; PD → 2MBページ



    lgdt [Gdt64Ptr]
    mov eax, cr4
    or eax, (1 << 5) ; enable PAE
    mov cr4, eax

    mov eax, 0x80000
    mov cr3, eax

    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8 ; set LME bit
    wrmsr  

    mov eax, cr0
    or eax,1<<31 ; set LMA bit
    mov cr0, eax

    jmp 8:LMEntry



PEnd:
    hlt
    jmp PEnd

[BITS 64]
LMEntry:
    mov rsp, 0x7c00

    mov byte[0xb8000], 'L'
    mov byte[0xb8001], 0xa

LEnd:
    hlt
    jmp LEnd

DriveID db 0
Message db "Text mode is set",0
FailMsg db "Long mode is not supported",0
ReadErrorMsg db "Failed to read the kernel",0
ReadPacket:
    times 16 db 0

Gdt32:
    dq 0x0000 ; null descriptor
Code32:
    dw 0xffff ; code segment descriptor
    dw 0x0000
    db 0
    db 0x9a; Type 1010, S 1, DPL 00, P 1
    db 0xcf
    db 0
Data32:
    dw 0xffff ; data segment descriptor
    dw 0x0000
    db 0
    db 0x92; Type 0010, S 1, DPL
    db 0xcf; P 1, AVL 0, L 0, D/B 1, G 1
    db 0

Gdt32Len equ $ - Gdt32

Gdt32Ptr: dw Gdt32Len - 1
              dd Gdt32

Idt32Ptr: dw 0
          dd 0

Gdt64:
    dq 0
    dq 0x00af9a000000ffff ; code
    dq 0x00af92000000ffff ; data ; code segment descriptor

Gdt64Len: equ $ - Gdt64

Gdt64Ptr: dw Gdt64Len - 1
       dq Gdt64
