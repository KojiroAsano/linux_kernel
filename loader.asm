[BITS 16]
[ORG 0x7E00]

start:

    mov ax,cs
    mov ds,ax
    mov es,ax

    mov [DriveID],dl

    mov eax,0x80000000
    cpuid
    cmp eax,0x80000001
    jb NotSupport

    mov eax,0x80000001
    cpuid

    test edx,(1<<29)
    jz NotSupport

    ; test edx,(1<<26)
    ; jz NotSupport

LoadKernel:
    mov si,ReadPacket

    mov byte  [si],16       ; packet size
    mov byte  [si+1],0      ; reserved
    mov word  [si+2],90    ; sectors to read // use 90 instead of 100
    mov word  [si+4],0x0    ;offset
    mov word  [si+6],0x1000; segment
    mov dword [si+8],6      ; LBA low
    mov dword [si+12],0     ; LBA high

    mov ah,0x42
    mov dl,[DriveID]
    int 0x13
    jc ReadError

GetMemInfoStart:
    mov eax,0xE820
    mov edx,0x534D4150 ; 'SMAP' 
    mov ecx,20
    mov edi,0x9000
    xor ebx,ebx
    int 0x15
    jc NotSupport

GetMemInfo:
    add edi,20
    mov eax,0xE820
    mov edx,0x534D4150 ; 'SMAP' 
    mov ecx,20
    int 0x15
    jc GetMemDone

    test ebx,ebx
    jnz  GetMemInfo

GetMemDone:
TestA20:
    mov ax, 0xffff
    mov es, ax
    mov word [ds:0x7c00], 0xa200
    cmp word [es:0x7c10], 0xa200
    jne SetA20LineDone
    mov word [0x7c00], 0xb200
    cmp word [es:0x7c10], 0xb200
    je End


SetA20LineDone:
    xor ax, ax
    mov es, ax

SetVideoMode:
    mov ax,0x0003
    int 0x10

    mov ax,0xB800
    mov es,ax
    xor di,di

    mov si,Message

PrintMessage:
    mov al,[si]
    cmp al,0
    je End

    mov [es:di],al
    mov byte [es:di+1],0x0A

    add di,2
    inc si
    jmp PrintMessage

NotSupport:
ReadError:
End:

    hlt
    jmp End


DriveID db 0
Message db "Text mode is set",0
FailMsg db "Long mode is not supported",0
ReadErrorMsg db "Failed to read the kernel",0
ReadPacket:
    times 16 db 0