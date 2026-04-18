[BITS 16]
[ORG 0x7c00]

start:
    mov ah, 0x0e
    mov al, 'B'
    int 0x10

    
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [DriveId], dl

; --- 拡張INT13チェック ---
    mov ah, 0x41
    mov bx, 0x55aa
    int 0x13
    jc disk_error
    cmp bx, 0xaa55
    jne disk_error

; --- loader 読み込み ---
    mov si, dap
    mov word [si], 0x10        ; size
    mov word [si+2], 5         ; 1 sector
    mov word [si+4], 0x7e00    ; offset
    mov word [si+6], 0x0000    ; segment
    mov dword [si+8], 1        ; LBA = 1
    mov dword [si+12], 0

    mov dl, [DriveId]
    mov ah, 0x42
    int 0x13
    jc disk_error

; --- loaderへ ---
    mov dl, [DriveId]
    jmp 0x0000:0x7e00

disk_error:
    mov ah,0x13
    mov al,1
    mov bx,0xa
    xor dx,dx
    mov bp,Message
    mov cx,MessageLen 
    int 0x10

hang:
    hlt
    jmp hang

DriveId db 0
dap times 16 db 0
Message:    db "We have an error in boot process"
MessageLen: equ $-Message

times (0x1be-($-$$)) db 0

    db 80h
    db 0,2,0
    db 0f0h
    db 0ffh,0ffh,0ffh
    dd 1
    dd (20*16*63-1)
	
    times (16*3) db 0

    db 0x55
    db 0xaa

; times 510-($-$$) db 0
; dw 0xAA55