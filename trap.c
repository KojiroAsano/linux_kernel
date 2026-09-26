#include "trap.h"
#include "print.h"
#include "syscall.h"
#include "process.h"
#include "keyboard.h"
#include "debug.h"

// idt_pointerとvectorsはstatic(このファイルの外からは見えない)変数。
// vectors[256]が実際のIDT本体で、256個ある割り込み/例外番号それぞれに
// 対応する1エントリずつが入る配列。
static struct IdtPtr idt_pointer;
static struct IdtEntry vectors[256];
static uint64_t ticks;   // タイマー割り込み(ベクタ32)が発生した回数の累計。
                          // get_ticks()で参照でき、sys_sleep(syscall.c)が
                          // 「指定ティック数だけ待つ」ために使う。

// ============================================================================
// init_idt_entry — IDTの1エントリ分を組み立てるヘルパー関数
// ============================================================================
// entry:     組み立てる先のIDTエントリへのポインタ
// addr:      このベクタが発生した時に実行してほしいハンドラ関数の
//            アドレス(trap.asmのvectorNのアドレス)
// attribute: アクセス権フラグ(このプロジェクトでは全部0x8eを使う。
//            0x8e = Present・リング0専用・64bit割り込みゲート)
// ist:       IST(緊急スタック)番号。0なら通常通り、割り込まれた時点の
//            スタックをそのまま使う。1以上ならTSSのIST1-7で指定した
//            専用スタックへ強制的に切り替える。
static void init_idt_entry(struct IdtEntry *entry, uint64_t addr, uint8_t attribute, uint8_t ist)
{
    entry->low = (uint16_t)addr;        // アドレスの下位16bit
    entry->selector = 8;                // ハンドラはいつもカーネルコードセグメント(8)で実行
    entry->res0 = ist;                  // IST(Interrupt Stack Table)インデックス。0=切り替えなし
    entry->attr = attribute;            // アクセス権フラグをそのまま書き込む
    entry->mid = (uint16_t)(addr>>16);  // アドレスの次の16bit
    entry->high = (uint32_t)(addr>>32); // アドレスの残り32bit
}

// ============================================================================
// init_idt — IDT全体を組み立てて、CPUに登録する
// ============================================================================
// main.cのKMain()から一番最初に呼ばれる。ここが終わるまでは、割り込み
// ハンドラの行き先が定まっていないので、万が一何か割り込みが起きると
// 予測できない場所へ飛んでしまう(実際にはこの時点ではまだ割り込みが
// 有効になっていない。process.cのlaunch()が最初のプロセスへ切り替わる
// 瞬間〈iretqがRFLAGSのIFビットを立てた状態を復元する〉まで発生しない)。
void init_idt(void)
{
    // 使うベクタ(0-8,10-14,16-19,32,33,39,0x80)だけを登録する。使わない
    // 番号(9, 15, 20-31, 34-38, 40-127, 129-255など)は、vectors配列が
    // static変数で最初から全部0クリアされていることを利用し、あえて
    // 何もしていない = Presentビットが立っていない = そのベクタが
    // 発生したらCPUがさらに別の例外(#GP)を起こす、という状態のまま
    // にしてある。
    init_idt_entry(&vectors[0],(uint64_t)vector0,0x8e,0);    // #DE 0除算
    init_idt_entry(&vectors[1],(uint64_t)vector1,0x8e,0);    // #DB デバッグ
    init_idt_entry(&vectors[2],(uint64_t)vector2,0x8e,0);    // NMI
    init_idt_entry(&vectors[3],(uint64_t)vector3,0x8e,0);    // #BP ブレークポイント
    init_idt_entry(&vectors[4],(uint64_t)vector4,0x8e,0);    // #OF オーバーフロー
    init_idt_entry(&vectors[5],(uint64_t)vector5,0x8e,0);    // #BR 範囲外アクセス
    init_idt_entry(&vectors[6],(uint64_t)vector6,0x8e,0);    // #UD 未定義命令
    init_idt_entry(&vectors[7],(uint64_t)vector7,0x8e,0);    // #NM デバイス使用不可
    // ベクタ8(ダブルフォルト)だけはIST1(kernel.asmのTSSで設定した専用スタック)
    // を使う。スタック自体が壊れて起きるダブルフォルトを、同じ壊れたスタックの
    // まま処理しようとして連鎖的にトリプルフォルトするのを防ぐため。
    init_idt_entry(&vectors[8],(uint64_t)vector8,0x8e,1);    // #DF ダブルフォルト(IST1)
    init_idt_entry(&vectors[10],(uint64_t)vector10,0x8e,0);  // #TS 不正なTSS
    init_idt_entry(&vectors[11],(uint64_t)vector11,0x8e,0);  // #NP セグメント不在
    init_idt_entry(&vectors[12],(uint64_t)vector12,0x8e,0);  // #SS スタックセグメント例外
    init_idt_entry(&vectors[13],(uint64_t)vector13,0x8e,0);  // #GP 一般保護例外
    init_idt_entry(&vectors[14],(uint64_t)vector14,0x8e,0);  // #PF ページフォルト
    init_idt_entry(&vectors[16],(uint64_t)vector16,0x8e,0);  // #MF x87浮動小数点例外
    init_idt_entry(&vectors[17],(uint64_t)vector17,0x8e,0);  // #AC アライメントチェック
    init_idt_entry(&vectors[18],(uint64_t)vector18,0x8e,0);  // #MC マシンチェック
    init_idt_entry(&vectors[19],(uint64_t)vector19,0x8e,0);  // #XM SIMD浮動小数点例外
    init_idt_entry(&vectors[32],(uint64_t)vector32,0x8e,0);  // IRQ0(タイマー)
    init_idt_entry(&vectors[33],(uint64_t)vector33,0x8e,0);  // IRQ1(キーボード)
    init_idt_entry(&vectors[39],(uint64_t)vector39,0x8e,0);  // IRQ7(スプリアス割り込み)
    // ベクタ0x80(システムコール)だけは属性0xee(0x8eのDPLを3にしたもの)を
    // 使う。DPL=3を指定しないと、リング3から`int 0x80`を実行した瞬間に
    // 「呼び出し元の権限(CPL=3)がこのゲートの権限(DPL=0)より低い」と
    // 見なされ、#GP(一般保護例外)になってしまう。
    init_idt_entry(&vectors[0x80],(uint64_t)sysint,0xee,0);  // システムコール(DPL3)

    // 組み立てたIDT(vectors配列)の場所とサイズをidt_pointerにまとめ、
    // load_idt()(trap.asm、中身はlidt命令)でCPUに登録する。
    idt_pointer.limit = sizeof(vectors)-1;  // IDT全体のバイト数-1
    idt_pointer.addr = (uint64_t)vectors;   // IDTの実アドレス
    load_idt(&idt_pointer);                  // CPUに登録する(trap.asm、lidt命令)
}

// ============================================================================
// get_ticks — タイマー割り込みが発生した累計回数を返す
// ============================================================================
uint64_t get_ticks(void)
{
    return ticks;
}

// ============================================================================
// timer_handler — タイマー割り込み(ベクタ32)のたびに呼ばれる
// ============================================================================
static void timer_handler(void)
{
    ticks++;
    wake_up(-1);   // sys_sleep(syscall.c)がsleep(-1)で待っているプロセスを起こす
}

// ============================================================================
// handler — 割り込み/例外が起きるたびに、trap.asmのTrap:から呼ばれる
// ============================================================================
// tf(TrapFrame)を見れば、「どの番号の割り込み/例外が起きたか(trapno)」
// 「その時CPUがどんな状態だったか(rip/cs/rflagsなど)」が全部分かる。
//
// 【VGAメモリアクセスについての注意】以前このセッションで、リング3から
// VGAメモリへ直接アクセスする、あるいはこの割り込みハンドラの中から
// 直接VGAメモリへ書き込む、という2パターンで、QEMU環境固有と思われる
// 原因不明のフォールトを確認したことがある(ページテーブルの権限設定は
// gdbで直接確認して正しかったにも関わらず発生した)。今回のsys_write
// (syscall.c)はこのhandler経由でwrite_screen()を呼ぶため、同じ経路の
// 中でVGAへ書き込むことになる。もし同様のフォールトが再発するようなら
// この注意書きを手がかりに調査すること(起動直後の最初の1回だけで
// 起きていたので、タイミング依存の可能性がある)。
void handler(struct TrapFrame *tf)
{
    unsigned char isr_value;

    switch (tf->trapno) {
        case 32:  // IRQ0 = タイマー割り込み(kernel.asmのInitPITで
                  // 約100Hzになるよう設定済み)。
            timer_handler();
            eoi();   // PICへ処理完了を伝える
            break;   // このcaseを抜ける

        case 33:  // IRQ1 = キーボード割り込み
            keyboard_handler();
            eoi();
            break;

        case 39: // IRQ7 = マスタPICのスプリアス(まぎれ込み)割り込み。
                 // 本物のIRQ7デバイス割り込みと区別するため、ISR
                 // レジスタのbit7を確認する。bit7が立っていれば本物
                 // なのでEOIを送る。立っていなければ「スプリアス」
                 // (実際にはどの機器からの要求でもない、偽の割り込み)
                 // なので、あえてEOIを送らずに無視する
                 // (スプリアス割り込みにEOIを送ると、他の正常な
                 // 割り込みの処理と辻褄が合わなくなることがあるため)。
            isr_value = read_isr();          // マスタPICのISRレジスタを読む
            if ((isr_value&(1<<7)) != 0) {   // bit7(IRQ7)が立っているか確認
                eoi();                       // 立っていれば本物なのでEOIを送る
            }
            break;   // このcaseを抜ける

        case 0x80:  // システムコール(ユーザープログラムがint 0x80で呼び出す)
            system_call(tf);   // syscall.c。戻り値はtf->raxに書き戻される
            break;

        default:
            // 上記以外の番号(想定していない例外など)が来た場合。
            if ((tf->cs & 3) == 3) {
                // CS の下位2bit(RPL)が3 = リング3(ユーザープログラム)
                // で起きた例外。カーネル全体を巻き込まず、そのプロセス
                // だけを終了させて他のプロセスは動き続けられるようにする。
                printk("Exception is %d\n", tf->trapno);
                exit();   // ここから戻ってこない(schedule()が別プロセスへ切り替える)
            }
            else {
                // リング0(カーネル自身)で起きた予期しない例外。
                // 原因を画面に表示する仕組みがまだ無いので、
                // とりあえず無限ループで止めて、それ以上何も壊さない
                // ようにしている。
                while (1) { }
            }
    }

    if (tf->trapno == 32) {
        // タイマー割り込みの処理が終わるたびに、必ずyield()を呼んで
        // 次のプロセスへ切り替える。これが「プリエンプティブな
        // ラウンドロビンスケジューリング」の実体で、各プロセスは
        // 自分から処理を手放さなくても、一定時間(PITの周期)ごとに
        // 強制的に次のプロセスへ切り替わる。
        yield();
    }
}
