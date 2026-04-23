section .text
global memset
global memmove
global memcpy
global memcmp

memset:
    cld ; Clear direction flag
    mov ecx, edx
    mov al, sil
    rep stosb
    ;rdi buffer, rsi value, rdx size
    ret

memcmp:
    cld
    xor eax, eax
    mov ecx, edx
    repe cmpsb
    setz al
    ret

memcpy:
memmove:
    cld
    cmp rsi, rdi
    jae .copy
    mov r8, rsi
    add r8, rdx
    cmp r8, rdi
    jbe .copy

.overlap:
    std
    add rdi, rdx
    add rsi, rdx
    sub rdi, 1
    sub rsi, 1

.copy:
    mov ecx, edx
    rep movsb
    cld
    ret