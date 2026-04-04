[BITS 16]
[ORG 0x7C00] ;telling the assembler that the code will be loaded at 0x7C00

start:
    cli
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7C00
    sti

    mov [DriveId],dl

; --- INT13拡張チェック ---
    mov ah,0x41
    mov bx,0x55AA
    int 0x13
    jc fail
    cmp bx,0xAA55
    jne fail

; --- loader読み込み（LBA=1）---
    mov si,Packet
    mov byte [si],16
    mov word [si+2],5
    mov word [si+4],0x7E00
    mov word [si+6],0x0000
    mov dword [si+8],1

    mov ah,0x42
    mov dl,[DriveId]
    int 0x13
    jc fail

    jmp 0x0000:0x7E00

fail:
    hlt
    jmp fail

DriveId db 0
Packet times 16 db 0

times 510-($-$$) db 0
dw 0xAA55