[BITS 16]
[ORG 0x7C00] ;telling the assembler that the code will be loaded at 0x7C00

start:
    cli ; disable interrupts
    xor ax,ax ; clear all registers
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7C00
    sti

    mov [DriveId],dl; save the drive number passed by BIOS in dl to DriveId

; --- INT13拡張チェック ---
; Check if the drive supports INT13 extensions by calling function 0x41 with bx=0x55AA. If the call fails or returns a different value in bx, jump to fail.
    mov ah,0x41 ; INT13 extensions check
    mov bx,0x55AA; the value to check for in bx after the call
    int 0x13
    jc fail ; if the carry flag is set, the call failed, so jump to fail
    cmp bx,0xAA55
    jne fail

; --- loader読み込み（LBA=1）---
; Set up the packet for reading the loader (the second sector, LBA=1) into memory at 0x7E00. The packet structure is as follows:
; Offset  Size  Description 
; 0       1     Size of the packet (16 bytes)
; 2       2     Number of sectors to read (1 sector)
; 4       2     Segment of the buffer (0x7E00 >>
; 6       2     Offset of the buffer (0x7E00 & 0xFFFF)
; 8       4     LBA of the first sector to read (1)
; si is source index register, we will use it to point to the packet structure in memory. We will fill the packet structure with the appropriate values for reading 1 sector from LBA 1 into memory at 0x7E00.
    mov si,Packet
    mov byte [si],16
    mov word [si+2],5
    mov word [si+4],0x7E00
    mov word [si+6],0x0000
    mov dword [si+8],1

    ;mov ah, <機能番号>
    ;int 0x13
    mov ah,0x42 ; INT13 extensions read this is the one we checked for earlier
    mov dl,[DriveId]
    int 0x13
    jc fail

    jmp 0x0000:0x7E00; jump to the loaded code at 0x7E00
    ; writing 0x0000:0x7E00 is the same as writing 0x7E00:0x0000, so we can just jump to 0x7E00
    ;   CS;IP will be set to 0x7E00, and DS, ES, SS will still be 0x0000, which is fine for the loader code
    ; CS * 16 + IP = 0x0000 * 16 + 0x7E00 = 0x7E00, so the CPU will fetch instructions from 0x7E00 after the jump

fail:
    hlt
    jmp fail

DriveId db 0
Packet times 16 db 0

times 510-($-$$) db 0
dw 0xAA55