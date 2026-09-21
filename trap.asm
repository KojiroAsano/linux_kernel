; ============================================================================
; trap.asm — 割り込み・例外が発生した時に最初に実行される、共通の入口
; ============================================================================
; CPUは割り込みや例外が起きると、IDT(trap.cが用意する)を見て、
; 対応する番号(ベクタ)のハンドラへ自動的にジャンプする。このファイルは
; 256個ありうるベクタそれぞれについて「ここに飛んできたらこう処理する」
; という最初の受け口(vector0, vector1, ...)を用意し、最終的に全部
; 共通のTrap:へ合流させて、C言語のhandler()関数(trap.c)を呼び出す。
;
; なぜ全部C言語のhandler一つに集約するかというと、レジスタの保存・復元や
; スタックの後片付けといった「機械的で間違えやすい」部分をアセンブリで
; 一箇所にまとめ、実際の「このベクタが来たら何をするか」という判断は
; C言語側で読みやすく書けるようにするため。
; ============================================================================

section .text
extern handler   ; trap.cで定義されているC言語のhandler()関数
global vector0
global vector1
global vector2
global vector3
global vector4
global vector5
global vector6
global vector7
global vector8
global vector10
global vector11
global vector12
global vector13
global vector14
global vector16
global vector17
global vector18
global vector19
global vector32
global vector39
global eoi
global read_isr
global load_idt
global load_cr3 ; This is used by the scheduler to switch page tables
global enable_interrupts
global disable_interrupts
global enter_usermode

; ============================================================================
; Trap — 全ベクタ共通の処理: レジスタ保存 → C関数呼び出し → レジスタ復元
; ============================================================================
; 割り込み/例外が起きると、CPUは自動的にRIP・CS・RFLAGS(・RSP・SS)を
; スタックに積んでからハンドラへ飛んでくる。それに加えて、各vectorNが
; さらに「エラーコード(無ければダミーの0)」と「ベクタ番号」を積んでから
; ここに合流してくる。なので、Trap:に入った時点でのスタックの中身は、
; trap.hのstruct TrapFrameの後半(trapno, errorcode, rip, cs, rflags,
; rsp, ss)と完全に対応している。
Trap:
    ; 汎用レジスタを全部スタックに退避する。C言語のhandler()を呼ぶと、
    ; その中で自由にレジスタを使われてしまうので、割り込まれる直前の
    ; 値を後で正確に復元できるよう、先に全部保存しておく。
    ; この15個のpushで、trap.hのTrapFrame構造体の前半(r15～rax)と
    ; ぴったり同じ並び・同じバイト数になるように作ってある。
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

    ; 今のRSPは、ちょうどTrapFrame構造体の先頭(r15の位置)を指している。
    ; これをそのままC言語の関数の第1引数として渡す
    ; (x86-64のSystem V ABIでは、第1引数はRDIレジスタで渡す決まり)。
    mov rdi,rsp
    call handler   ; trap.cのhandler(struct TrapFrame *tf) を呼ぶ

TrapReturn:
    ; C言語側の処理が終わったら、さっき保存した順番と逆順にレジスタを
    ; 復元していく(スタックは後入れ先出しなので、pushと逆順にpopする)。
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

    ; 各vectorNが積んだ「エラーコード」と「ベクタ番号」の2つ(16バイト)は
    ; レジスタに戻す必要が無い値なので、popせずにスタックポインタを
    ; そのまま16進めることで読み捨てる。
    add rsp,16

    ; iretq(Interrupt Return, 64bit版)で、残っているRIP・CS・RFLAGS・
    ; RSP・SSをスタックから取り出し、割り込まれる直前の状態へ正確に
    ; 復帰する。もし権限レベルが変わる復帰(リング0→リング3など)なら、
    ; ここでCPUがSS・RSPも正しく切り替えてくれる。
    iretq



; ============================================================================
; vectorN — 各割り込み/例外番号ごとの受け口
; ============================================================================
; CPUが例外を起こす時、種類によっては「エラーコード」という追加情報を
; 自動でスタックに積んでくれるものと、積んでくれないものがある。
; 積んでくれない場合は、後でTrapFrameの並びが崩れないよう、こちら側で
; 代わりにダミーの0を積んでおく必要がある。
; エラーコード「あり」の例外: 8(ダブルフォルト), 10, 11, 12, 13(一般保護
; 例外), 14(ページフォルト), 17。これらは push 0 を書いていない。
; それ以外は自分で push 0 してから番号をpushしている。

vector0:            ; #DE 0除算エラー(エラーコード無し)
    push 0
    push 0
    jmp Trap

vector1:            ; #DB デバッグ例外(エラーコード無し)
    push 0
    push 1
    jmp Trap

vector2:            ; NMI 割り込み不可能割り込み(エラーコード無し)
    push 0
    push 2
    jmp Trap

vector3:            ; #BP ブレークポイント(エラーコード無し)
    push 0
    push 3
    jmp Trap

vector4:            ; #OF オーバーフロー(エラーコード無し)
    push 0
    push 4
    jmp Trap

vector5:            ; #BR 範囲外アクセス(エラーコード無し)
    push 0
    push 5
    jmp Trap

vector6:            ; #UD 未定義命令(エラーコード無し)
    push 0
    push 6
    jmp Trap

vector7:            ; #NM デバイス使用不可(エラーコード無し)
    push 0
    push 7
    jmp Trap

vector8:            ; #DF ダブルフォルト(エラーコードあり、CPUが自動で
                    ; 積んでくれるのでpush 0は不要。IDT側でIST1
                    ; 〈trap.c/kernel.asmのDFStack〉を使うよう設定済み)
    push 8
    jmp Trap

vector10:           ; #TS 不正なTSS(エラーコードあり)
    push 10
    jmp Trap

vector11:           ; #NP セグメント不在(エラーコードあり)
    push 11
    jmp Trap

vector12:           ; #SS スタックセグメント例外(エラーコードあり)
    push 12
    jmp Trap

vector13:           ; #GP 一般保護例外(エラーコードあり)
                    ; 権限違反やセグメント不正など、色々な原因で発生する。
    push 13
    jmp Trap

vector14:           ; #PF ページフォルト(エラーコードあり)
                    ; マッピングされていない、または権限不足なメモリへの
                    ; アクセス時に発生する(例: リング3から未マッピングの
                    ; アドレスに触った時)。
    push 14
    jmp Trap

vector16:           ; #MF x87浮動小数点例外(エラーコード無し)
    push 0
    push 16
    jmp Trap

vector17:           ; #AC アライメントチェック(エラーコードあり)
    push 17
    jmp Trap

vector18:           ; #MC マシンチェック(エラーコード無し)
    push 0
    push 18
    jmp Trap

vector19:           ; #XM SIMD浮動小数点例外(エラーコード無し)
    push 0
    push 19
    jmp Trap

vector32:           ; IRQ0 = タイマー割り込み(PICでベクタ32番に
                    ; 再マップ済み。kernel.asmのInitPIC参照)
    push 0
    push 32
    jmp Trap

vector39:           ; IRQ7 = マスタPICのスプリアス(まぎれ込み)割り込み。
                    ; ハードウェアの仕様上、本物の割り込みでなくても
                    ; たまに発生することがあるベクタ。trap.cのhandler()
                    ; 側でISRレジスタを確認し、本物かどうかを見分けている。
    push 0
    push 39
    jmp Trap

; ============================================================================
; ヘルパー関数群 — C言語側から呼ばれる小さな部品
; ============================================================================

; PICに「この割り込みの処理は終わったよ」と伝える(End Of Interrupt)。
; これを送らないと、PICは次の割り込みを受け付けてくれなくなる。
eoi:
    mov al,0x20    ; 0x20 = EOIコマンド
    out 0x20,al    ; マスタPICのコマンドポートへ送る
    ret

; マスタPICの「ISR(In-Service Register、現在処理中の割り込みが
; どれか)」を読み出す。IRQ7(スプリアス割り込み)が本物かどうかの
; 判定に使う。
read_isr:
    mov al,11        ; OCW3: 0x0b = 「次の読み出しでISRを見せて」という指定
    out 0x20,al
    in al,0x20       ; ISRの値を読み出す
    ret

; IDT(割り込み記述子テーブル)をCPUに登録する。trap.cのinit_idt()から
; 呼ばれる。
load_idt:
    lidt [rdi]   ; rdi = struct IdtPtr へのポインタ(C言語の第1引数)
    ret

; CR3レジスタ(現在使うページテーブルの物理アドレス)を書き換える。
; プロセスごとに別々の仮想メモリ空間を持たせるスケジューラを作る時に
; 使う想定(memory.cのswitch_vm()から呼ばれる)。
load_cr3:
    mov rax,rdi
    mov cr3,rax
    ret

; CPUの割り込み許可フラグ(RFLAGSのIFビット)を立てる。これを呼ぶまでは、
; PIC/PIT/IDTをどれだけ設定していても、実際には一度も割り込みが
; CPUに届かない。
enable_interrupts:
    sti
    ret

; 逆に割り込みを禁止する。何か「途中で邪魔されたくない」処理を
; 実行する前に使う想定。
disable_interrupts:
    cli
    ret

; ============================================================================
; enter_usermode — リング0からリング3へ切り替える
; ============================================================================
; void enter_usermode(uint64_t entry, uint64_t stack)
; rdi=エントリポイント, rsi=スタックトップ。呼び出し元には戻ってこない
; (iretqでリング3へジャンプするので、以降はC言語側の制御を離れる)。
;
; 通常iretqは「割り込みから戻る」ために使うが、ここでは逆に、
; 自分でそれらしいスタックの中身を手作りしてからiretqを実行することで、
; 「今から割り込みがあって、そこから復帰するところ」を装っている。
; iretqは復帰先のCSのDPL(ここでは3)を見て、必要ならCPUの権限レベルを
; 自動的に切り替えてくれるので、これだけでリング0→リング3の移行が
; できる。積む順番は、CPUがiretqで取り出す順(逆順)に合わせる必要がある。
enter_usermode:
    push qword 0x18|3   ; SS = ユーザーデータセグメント(0x18) | RPL3
                        ; 「|3」はセレクタの下位2bit(要求特権レベル)を
                        ; 3にする、という意味。CPUはこの値を見て、
                        ; ジャンプ先がリング3であることを認識する。
    push rsi             ; RSP = ユーザースタックの先頭アドレス(引数で受け取った値)
    push qword 0x202       ; RFLAGS = 0x202。
                            ; bit9(IF)が立っているので、リング3へ移った
                            ; 瞬間から割り込みが有効になる。bit1は
                            ; 常に1でなければならない予約ビット。
    push qword 0x10|3         ; CS = ユーザーコードセグメント(0x10) | RPL3
    push rdi                    ; RIP = エントリポイント(引数で受け取った値)
    iretq                        ; 積んだ値を使って、リング3のentryへジャンプ
