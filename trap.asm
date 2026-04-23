
section .text
extern handler ; handlerはC言語で定義されている割り込みハンドラ関数
global vector0 ; vector0は割り込みベクタ0のエントリポイント
global vector1 ; vector1は割り込みベクタ1のエントリポイント
global vector2 ; vector2は割り込みベクタ2のエントリポイント
global vector3 ; vector3は割り込みベクタ3のエントリポイント
global vector4 ; vector4は割り込みベクタ4のエントリポイント
global vector5 ; vector5は割り込みベクタ5のエントリポイント
global vector6 ; vector6は割り込みベクタ6のエントリポイント
global vector7 ; vector7は割り込みベクタ7のエントリポイント
global vector8  ; vector8は割り込みベクタ8のエントリポイント
global vector10 ; vector10は割り込みベクタ10のエントリポイント
global vector11 ; vector11は割り込みベクタ11のエントリポイント
global vector12 ; vector12は割り込みベクタ12のエントリポイント
global vector13 ; vector13は割り込みベクタ13のエントリポイント     
global vector14 ; vector14は割り込みベクタ14のエントリポイント
global vector16 ; vector16は割り込みベクタ16のエントリポイント
global vector17 ; vector17は割り込みベクタ17のエントリポイント
global vector18 ; vector18は割り込みベクタ18のエントリポイント  
global vector19
global vector32
global vector39
global eoi
global read_isr
global load_idt

Trap: ; 割り込みが発生したときに呼び出される共通の割り込みハンドラ
    push rax
    push rbx  
    push rcx
    push rdx  	  
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    inc byte[0xb8010]
    mov byte[0xb8011],0xe

    mov rdi,rsp
    call handler ; handlerはC言語で定義されている割り込みハンドラ関数 in c file

TrapReturn: ; restore registers and return from interrupt
    pop	r15
    pop	r14
    pop	r13
    pop	r12
    pop	r11
    pop	r10
    pop	r9
    pop	r8
    pop	rbp
    pop	rdi
    pop	rsi  
    pop	rdx
    pop	rcx
    pop	rbx
    pop	rax       

    add rsp,16
    iretq



vector0:
    push 0 ; 0は割り込みベクタ0を示す値
    push 0
    jmp Trap

vector1:
    push 0
    push 1
    jmp Trap

vector2:
    push 0
    push 2
    jmp Trap

vector3:
    push 0
    push 3	
    jmp Trap 

vector4:
    push 0
    push 4	
    jmp Trap   

vector5:
    push 0
    push 5
    jmp Trap    

vector6:
    push 0
    push 6	
    jmp Trap      

vector7:
    push 0
    push 7	
    jmp Trap  

vector8: ; just push index number only error code is pushed by CPU automatically
    push 8
    jmp Trap  

;vector9 is reserved by Intel, so we skip it
vector10:
    push 10	
    jmp Trap 
                   
vector11:
    push 11	
    jmp Trap
    
vector12:
    push 12	
    jmp Trap          
          
vector13:
    push 13	
    jmp Trap
    
vector14:
    push 14	
    jmp Trap 

; vector15 is reserved by Intel, so we skip it
vector16:
    push 0
    push 16	
    jmp Trap          
          
vector17:
    push 17	
    jmp Trap                         
                                                          
vector18:
    push 0
    push 18	
    jmp Trap 
                   
vector19:
    push 0
    push 19	
    jmp Trap

vector32:
    push 0
    push 32
    jmp Trap

; vector33-38は割り込みベクタ33-38のエントリポイントで、必要に応じて定義することができます
vector39:
    push 0
    push 39
    jmp Trap

eoi:
    mov al,0x20
    out 0x20,al
    ret

read_isr:
    mov al,11
    out 0x20,al
    in al,0x20
    ret

load_idt:
    lidt [rdi]
    ret


