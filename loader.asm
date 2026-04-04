[BITS 16]
[ORG 0x7E00]

start:
    mov ax,cs
    mov ds,ax
    mov es,ax

    mov [DriveID],dl

; --- load kernel (LBA=6 → 0x1000:0x0000 = 0x10000) ---
    mov si,ReadPacket
    mov byte [si],16
    mov byte [si+1],0
    mov word [si+2],20
    mov word [si+4],0x0000
    mov word [si+6],0x1000
    mov dword [si+8],6
    mov dword [si+12],0

    mov ah,0x42
    mov dl,[DriveID]
    int 0x13
    jc $

; --- protected mode ---
    cli
    lgdt [Gdt32Ptr]

    mov eax,cr0
    or eax,1
    mov cr0,eax

    jmp 0x08:pmode

; =====================
[BITS 32]
pmode:
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,0x90000

; --- clear page tables ---
    mov edi,0x70000
    xor eax,eax
    mov ecx,0x3000/4
    rep stosd

; PML4
    mov dword [0x70000],0x71003
; PDP
    mov dword [0x71000],0x72003
; PD (identity 0-2MB)
    mov dword [0x72000],0x000083
; PD (map 2MB → kernel)
    mov dword [0x72008],0x200083

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

; =====================
[BITS 64]
lmode:
    mov rsp,0x90000

; copy kernel (0x10000 → 0x200000)
    mov rsi,0x10000
    mov rdi,0x200000
    mov rcx,(20*512)/8
    rep movsq

    jmp 0x200000

; =====================
DriveID db 0
ReadPacket times 16 db 0

; --- GDT32 ---
Gdt32:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
Gdt32Ptr:
    dw $-Gdt32-1
    dd Gdt32

; --- GDT64 ---
Gdt64:
    dq 0
    dq 0x0020980000000000
Gdt64Ptr:
    dw $-Gdt64-1
    dd Gdt64