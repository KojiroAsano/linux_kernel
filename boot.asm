[BITS 16]
[ORG 0x7C00]

start:
    cli ; Disable interrupts short for clear interrupts flag
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7C00
    sti ; Enable interrupts short for set interrupts flag

    mov [DriveId],dl   ; Store the drive number for later use
    ; Jump to the code that checks for INT13h extensions and loads the next stage
    ; 80 for hard disk, 00 for floppy disk



;-----------------------------
; Check INT13h Extensions
;-----------------------------
    ; Check if the BIOS supports INT13h extensions, which allow for LBA access and larger disk sizes.
    mov ah,0x41 ; Check Extensions
    mov bx,0x55AA;signature for BIOS  
    int 0x13
    jc NotSupport; Carry flag(CF) set means not support
    cmp bx,0xAA55
    jne NotSupport

LoadLoader:

    mov si,ReadPacket

    mov byte  [si],16       ; packet size
    mov byte  [si+1],0      ; reserved
    mov word  [si+2],5      ; sectors to read
    mov word  [si+4],0x7e00 ; offset
    mov word  [si+6],0x0000 ; segment
    mov dword [si+8],1      ; LBA low
    mov dword [si+12],0     ; LBA high

    mov ah,0x42
    mov dl,[DriveId]
    int 0x13
    jc ReadError

    jmp 0x0000:0x7e00

;-----------------------------
; Print success message
;-----------------------------

PrintMessage:

    mov ax,cs
    mov ds,ax
    mov es,ax

    mov si,Message

PrintLoop:
    lodsb
    cmp al,0
    je End

    mov ah,0x0E
    int 0x10
    jmp PrintLoop


ReadError:
NotSupport:

    mov ax,cs
    mov ds,ax
    mov es,ax

    mov si,FailMsg

FailLoop:
    lodsb
    cmp al,0
    je End

    mov ah,0x0E
    int 0x10
    jmp FailLoop

End:
    cli
    hlt
    jmp End

;-----------------------------
; Data
;-----------------------------

DriveId db 0

Message db "Disk extension is supported",0
FailMsg db "We have a read error",0
ReadPacket times 16 db 0

;-----------------------------
; Partition Table
;-----------------------------

times (0x1BE-($-$$)) db 0

    db 0x80
    db 0x00,0x02,0x00
    db 0xF0
    db 0xFF,0xFF,0xFF
    dd 1
    dd 2048

times (16*3) db 0

;-----------------------------
; Boot Signature
;-----------------------------

dw 0xAA55
