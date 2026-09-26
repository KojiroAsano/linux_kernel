; ============================================================================
; lib.asm(ユーザーランド版) — memset/memcpy/memmove/memcmpの実体
; ============================================================================
; カーネル側のlib.asmと全く同じ実装。ユーザープログラムも標準ライブラリを
; 使えないので、同じものをこちら側にも独立して持たせている(カーネルの
; メモリ空間とユーザープロセスのメモリ空間は完全に別物なので、
; カーネル側のコードを直接呼ぶことはできない)。

section .text
global memset
global memcpy
global memmove
global memcmp

memset:
    cld
    mov ecx,edx
    mov al,sil
    rep stosb
    ret

memcmp:
    cld
    xor eax,eax
    mov ecx,edx
    repe cmpsb
    setnz al
    ret

memcpy:
memmove:
    cld
    cmp rsi,rdi
    jae .copy
    mov r8,rsi
    add r8,rdx
    cmp r8,rdi
    jbe .copy

.overlap:
    std
    add rdi,rdx
    add rsi,rdx
    sub rdi,1
    sub rsi,1

.copy:
    mov ecx,edx
    rep movsb
    cld
    ret
