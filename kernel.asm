[BITS 64]
[ORG 0x200000]

start:
    cli

; セグメント
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x80000

; 画面クリア
    mov rdi, 0xb8000
    mov rcx, 80*25
    mov ax, 0x0720
.clear:
    stosw
    loop .clear

; GDT / IDT
    lgdt [GdtPtr]
    lidt [IdtPtr]

; 確認
    mov byte [0xb8000], 'K'
    mov byte [0xb8002], '0'

; =========================
; ★ user.binへジャンプ
; =========================
    mov rax, 0x90000

    push 0x23        ; SS (user)
    push rax         ; RSP
    push 0x202       ; RFLAGS
    push 0x1B        ; CS (user)
    push 0x400000    ; ★ user.bin entry

    iretq

.hang:
    hlt
    jmp .hang

; =========================
; 例外ハンドラ
; =========================
isr_stub:
    cli
    mov byte [0xb8008], 'E'
    mov byte [0xb800A], 'R'
.loop:
    hlt
    jmp .loop

; =========================
; IDT
; =========================
%macro IDT_ENTRY 0
    dw isr_stub & 0xFFFF
    dw 0x08
    db 0
    db 0x8E
    dw (isr_stub >> 16) & 0xFFFF
    dd (isr_stub >> 32)
    dd 0
%endmacro

Idt:
%rep 256
    IDT_ENTRY
%endrep

IdtPtr:
    dw IdtEnd - Idt - 1
    dq Idt
IdtEnd:

; =========================
; GDT
; =========================
Gdt:
    dq 0
    dq 0x0020980000000000 ; kernel code (0x08)
    dq 0x0000920000000000 ; kernel data (0x10)
    dq 0x0020F80000000000 ; user code (0x18)
    dq 0x0000F20000000000 ; user data (0x20)

GdtPtr:
    dw $ - Gdt - 1
    dq Gdt