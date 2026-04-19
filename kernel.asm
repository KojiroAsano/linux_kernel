[BITS 64]
[ORG 0x200000]

start:
    mov rdi, Idt
    mov rax, handler0
    mov [rdi], ax ; offset low
    shr rax, 16
    mov [rdi+6], ax ; offset high
    shr rax, 16
    mov [rdi+8], eax ; offset upper

    mov rax, Timer
    add rdi, 32*16
    mov [rdi], ax ; offset low
    shr rax, 16
    mov [rdi+6], ax ; offset high
    shr rax, 16
    mov [rdi+8], eax ; offset upper


    lgdt [Gdt64Ptr]
    lidt [IdtPtr]


    push 0x8
    push KernelEntry
    db 0x48 
    retf 

KernelEntry:
    ; mov eax,cr4
    ; or eax,(1<<5)
    ; mov cr4,eax

    ; mov eax,0x200000
    ; mov cr3,eax

    ; mov ecx,0xc0000080
    ; rdmsr
    ; or eax,(1<<8)
    ; wrmsr

    ; mov eax,cr0
    ; or eax,(1<<31)
    ; mov cr0,eax

    mov byte [0xb8000], 'K'
    mov byte [0xb8001], 0xa

InitPIT:
    mov al, (1<<2) | (3<<4) ; square wave generator, lobyte/hibyte
    out 0x43, al
    mov ax, 1193182 / 100 ; 100Hz
    out 0x40, al
    mov al, ah
    out 0x40, al

InitPIC:
    mov al, 0x11
    out 0x20, al
    out 0xa0, al
    
    mov al, 32
    out 0x21, al
    mov al, 40
    out 0xa1, al

    mov al, 0x04
    out 0x21, al
    mov al, 0x02
    out 0xa1, al

    mov al, 0x01
    out 0x21, al
    out 0xa1, al

    mov al, 11111110b; mask all interrupts except timer
    out 0x21, al
    mov al, 11111111b; mask all interrupts
    out 0xa1, al

    sti


 

End:
    hlt
    jmp End

handler0:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov byte [0xb8000], 'I'
    mov byte [0xb8001], 0xc

    jmp End

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    iretq;

Timer:
    ; out 0x20, 0x20 ; EOI

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15


    mov byte [0xb8010], 'T'
    mov byte [0xb8011], 0xe

    jmp End
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    iretq
    
Gdt64:
    dq 0
    dq 0x0020980000000000
Gdt64Len equ $-Gdt64

Gdt64Ptr: dw Gdt64Len -1
             dq Gdt64

Idt:
    %rep 256
        dw  0
        dw  0x8 ; code segment selector
        db  0
        db  0x8e; present, ring 0, interrupt gate
        dw  0
        dd  0
        dd  0
    %endrep
IdtLen equ $-Idt

IdtPtr: dw IdtLen -1
             dq Idt