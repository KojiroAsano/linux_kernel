[BITS 16]
[ORG 0x1000]

start:
    mov ah,0x13
    mov al,1
    mov bx,0x0007
    xor dx,dx
    mov bp,msg
    mov cx,msg_len
    int 0x10

hang:
    hlt
    jmp hang

msg db "KERNEL OK"
msg_len equ $-msg