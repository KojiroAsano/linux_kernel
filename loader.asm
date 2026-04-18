[BITS 16]
[ORG 0x7e00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [DriveId],dl

    mov ah, 0x0e
    mov al, 'L'
    int 0x10
    
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb skip_ext

    mov eax, 0x80000001
    cpuid
    test edx, (1<<29)
    jz skip_ext
    test edx, (1<<26)
    jz skip_ext

; ここでOK

skip_ext:

LoadKernel:

    mov si,ReadPacket
    mov word[si],0x10
    mov word[si+2],90; kernel size  = 90 sectors
    mov word[si+4],0
    mov word[si+6],0x1000
    mov dword[si+8],6
    mov dword[si+0xc],0
    
    mov dl,[DriveId]
    mov ah,0x42
    int 0x13
    jc  ReadError

    mov ah, 0x0e
    mov al, 'S'
    int 0x10

GetMemInfoStart:
    mov eax, 0xe820
    mov edx, 0x534d4150 ; 'SMAP'
    mov ecx, 20
    mov edi, 0x9000
    xor ebx, ebx
    int 0x15
    jc NotSupport

GeMemInfo:
    add edi, 20
    mov eax, 0xe820
    mov edx, 0x534d4150 ; 'SMAP'
    mov ecx, 20
    int 0x15
    jc GetMemDone

    test ebx, ebx
    jnz GeMemInfo

GetMemDone:
    mov ah, 0x0e
    mov al, 'D'
    int 0x10


ReadError:
NotSupport:
End:
    hlt
    jmp End

DriveId:    db 0
Message:    db "kernel is loaded"
MessageLen: equ $-Message
ReadPacket: times 16 db 0