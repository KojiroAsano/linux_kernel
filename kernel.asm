[BITS 64]
[ORG 0x10000]

start:
    mov byte [0xb8000],'K'
    mov byte [0xb8001],0x0A

loop:
    hlt
    jmp loop