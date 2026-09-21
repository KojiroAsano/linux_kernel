#ifndef _TRAP_H_
#define _TRAP_H_

#include "stdint.h"

// ============================================================================
// struct IdtEntry — IDT(割り込み記述子テーブル)の1エントリ分
// ============================================================================
// IDTは256個のエントリからなる配列で、何番目のエントリかが「割り込み/
// 例外の番号(ベクタ)」に対応する。1エントリは16バイトで、その中に
// 「その番号の割り込みが来たら、どこへジャンプするか(ハンドラの
// アドレス)」を格納する。
//
// ハンドラのアドレスは64bit(8バイト)あるのに、なぜ low/mid/high と
// 3つに分かれた変な配置になっているかというと、これは32bit時代の
// IDTエントリの形式(offset_low + selector + offset_high の8バイト)を
// そのまま64bit用に拡張した仕様だから。昔からの形式を壊さずに拡張した
// 結果、アドレスがバイト列の途中と最後に分割される、という配置に
// なっている。
struct IdtEntry {
    uint16_t low;      // ハンドラアドレスの下位16bit (offset low)
    uint16_t selector;  // ハンドラを呼ぶときのコードセグメントセレクタ。
                        // 常にカーネルコードセグメント(8)を指定する
                        // (割り込みが起きた時、必ずリング0のこの
                        // セグメントでハンドラが実行されるようにするため)。
    uint8_t res0;        // IST(Interrupt Stack Table)番号。0=切り替えなし、
                        // 1-7ならTSSのIST1-7で指定したスタックへ強制的に
                        // 切り替える(kernel.asmのDFStack/IST1を参照。
                        // ベクタ8=ダブルフォルトだけ1を指定している)。
    uint8_t attr;        // アクセス権フラグ。Present・DPL(権限レベル)・
                        // ゲート種別(割り込みゲートか、トラップゲートか)
                        // などをまとめて表す1バイト。このプロジェクトでは
                        // 全エントリ共通で0x8e(64bit割り込みゲート、
                        // リング0専用)を使っている。
    uint16_t mid;         // ハンドラアドレスの次の16bit (offset mid)
    uint32_t high;         // ハンドラアドレスの残り32bit (offset high)
    uint32_t res1;          // 予約(未使用、0でなければならない)
};

// ============================================================================
// struct IdtPtr — lidt命令に渡す「IDTの場所」の情報
// ============================================================================
struct IdtPtr { // load_idtの引数の構造体
    uint16_t limit;   // IDT全体のバイト数-1
    uint64_t addr;     // IDTが実際に置かれているメモリアドレス
} __attribute__((packed)); // packed属性は、構造体のメンバが連続して配置されるようにするための属性
                            // (これが無いと、コンパイラが64bitの境界に
                            // 合わせるためのパディング〈隙間〉を勝手に
                            // 挟んでしまい、lidt命令が期待する「2バイト+
                            // 8バイトで合計10バイト」という並びと
                            // ズレてしまう)

// ============================================================================
// struct TrapFrame — 割り込み発生時にスタック上にできる「状態の記録」
// ============================================================================
// この構造体のフィールドの並び順は、trap.asmのTrap:が実際にレジスタを
// pushする順番と完全に一致するように作られている。C言語では構造体は
// 「宣言した順に、低いアドレスから高いアドレスへ」並ぶので、下の並びは
// そのまま「スタックの一番上(一番最近pushされたもの)から順に」という
// 意味になる。もしこの並びとtrap.asmのpush順が食い違うと、handler()が
// 全然違う値を読んでしまう(例えばrflagsのつもりでrspを読んでしまう、等)
// ので、両方を変更するときは必ずセットで直す必要がある。
struct TrapFrame {
    // --- ここから15個、trap.asmのTrap:がpushした汎用レジスタ ---
    // (push順の逆＝スタックの一番上から。r15を最後にpushしたので先頭)
    int64_t r15;
    int64_t r14;
    int64_t r13;
    int64_t r12;
    int64_t r11;
    int64_t r10;
    int64_t r9;
    int64_t r8;
    int64_t rbp;
    int64_t rdi;
    int64_t rsi;
    int64_t rdx;
    int64_t rcx;
    int64_t rbx;
    int64_t rax;
    // --- ここから2個、各vectorNがpushしたもの ---
    int64_t trapno;     // 割り込み/例外の番号(vectorNの中でpushした値)
    int64_t errorcode;  // エラーコード。CPUが自動で積むもの、または
                        // 無い場合はvectorN側がダミーで積んだ0
    // --- ここから5個、割り込み発生時にCPUが自動で積んだもの ---
    int64_t rip;       // 割り込まれた瞬間の、次に実行するはずだった命令のアドレス
    int64_t cs;         // 割り込まれた瞬間のコードセグメントセレクタ
    int64_t rflags;      // 割り込まれた瞬間のフラグレジスタ
    int64_t rsp;           // 割り込まれた瞬間のスタックポインタ
    int64_t ss;              // 割り込まれた瞬間のスタックセグメントセレクタ
                            // (rsp/ssは、権限レベルが変わらない割り込み
                            // でも64bitモードでは常に積まれる)
};


// --- trap.asmで定義されている、各割り込み/例外番号ごとの受け口 ---
// (中身の詳しい説明はtrap.asm参照。ここではinit_idt()がIDTへ
//  アドレスを登録するために、関数として宣言しているだけ)
void vector0(void);
void vector1(void);
void vector2(void);
void vector3(void);
void vector4(void);
void vector5(void);
void vector6(void);
void vector7(void);
void vector8(void);
void vector10(void);
void vector11(void);
void vector12(void);
void vector13(void);
void vector14(void);
void vector16(void);
void vector17(void);
void vector18(void);
void vector19(void);
void vector32(void);
void vector39(void);

void init_idt(void);      // IDTを組み立ててCPUに登録する(trap.c)
void eoi(void);            // PICへ「割り込み処理完了」を伝える(trap.asm)
void load_idt(struct IdtPtr *ptr);   // IDTをCPUに登録する(trap.asm)
unsigned char read_isr(void);         // マスタPICのISRレジスタを読む(trap.asm)
void enable_interrupts(void);          // 割り込みを許可する。sti(trap.asm)
void disable_interrupts(void);          // 割り込みを禁止する。cli(trap.asm)
void enter_usermode(uint64_t entry, uint64_t stack);  // リング0からリング3へ
                                                       // 切り替える(trap.asm)

#endif
