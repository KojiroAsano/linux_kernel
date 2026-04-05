[BITS 64]
[ORG 0x400000]

start:
    mov rax, 0xB8000
    mov byte [rax], 'U'
    mov byte [rax+2], '3'

.loop:
    ; hlt
    jmp .loop