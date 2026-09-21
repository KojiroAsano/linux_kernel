#include "trap.h"

// idt_pointerとvectorsはstatic(このファイルの外からは見えない)変数。
// vectors[256]が実際のIDT本体で、256個ある割り込み/例外番号それぞれに
// 対応する1エントリずつが入る配列。
static struct IdtPtr idt_pointer;
static struct IdtEntry vectors[256];

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
    entry->low = (uint16_t)addr;         // アドレスの下位16bit
    entry->selector = 8;                  // ハンドラはいつもカーネルコードセグメント(8)で実行
    entry->res0 = ist;  // IST(Interrupt Stack Table)インデックス。0=切り替えなし
    entry->attr = attribute;              // アクセス権フラグをそのまま書き込む
    entry->mid = (uint16_t)(addr>>16);    // アドレスの次の16bit
    entry->high = (uint32_t)(addr>>32);   // アドレスの残り32bit
}

// ============================================================================
// init_idt — IDT全体を組み立てて、CPUに登録する
// ============================================================================
// main.cのKMain()から一番最初に呼ばれる。ここが終わるまでは、割り込み
// ハンドラの行き先が定まっていないので、万が一何か割り込みが起きると
// 予測できない場所へ飛んでしまう(実際にはこの時点ではまだ
// enable_interrupts()を呼んでいないので割り込みは発生しない)。
void init_idt(void)
{
    // 使うベクタ(0-8,10-14,16-19,32,39)だけを登録する。使わない番号
    // (9, 15, 20-31, 33-38, 40-255など)は、vectors配列がstatic変数で
    // 最初から全部0クリアされていることを利用し、あえて何もしていない
    // = Presentビットが立っていない = そのベクタが発生したら
    // CPUがさらに別の例外(#GP)を起こす、という状態のままにしてある。
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
    init_idt_entry(&vectors[39],(uint64_t)vector39,0x8e,0);  // IRQ7(スプリアス割り込み)

    // 組み立てたIDT(vectors配列)の場所とサイズをidt_pointerにまとめ、
    // load_idt()(trap.asm、中身はlidt命令)でCPUに登録する。
    idt_pointer.limit = sizeof(vectors)-1;  // IDT全体のバイト数-1
    idt_pointer.addr = (uint64_t)vectors;   // IDTの実アドレス
    load_idt(&idt_pointer);                  // CPUに登録する(trap.asm、lidt命令)
}

// ============================================================================
// handler — 割り込み/例外が起きるたびに、trap.asmのTrap:から呼ばれる
// ============================================================================
// tf(TrapFrame)を見れば、「どの番号の割り込み/例外が起きたか(trapno)」
// 「その時CPUがどんな状態だったか(rip/cs/rflagsなど)」が全部分かる。
void handler(struct TrapFrame *tf)
{
    unsigned char isr_value;

    switch (tf->trapno) {
        case 32: // IRQ0 = タイマー割り込み(kernel.asmのInitPITで
                 // 約100Hzになるよう設定済み)。
                 // 今のところは「割り込みが来た」という事実を
                 // PICへ伝える(eoi)だけで、他には何もしていない。
                 // 将来スケジューラを作るときは、ここで「次に動かす
                 // プロセスへ切り替える」処理を追加することになる。
            eoi();   // PICへ処理完了を伝える
            break;   // このcaseを抜ける

        case 39: // IRQ7 = マスタPICのスプリアス(まぎれ込み)割り込み。
                 // 本物のIRQ7デバイス割り込みと区別するため、ISR
                 // レジスタのbit7を確認する。bit7が立っていれば本物
                 // なのでEOIを送る。立っていなければ「スプリアス」
                 // (実際にはどの機器からの要求でもない、偽の割り込み)
                 // なので、あえてEOIを送らずに無視する
                 // (スプリアス割り込みにEOIを送ると、他の正常な
                 // 割り込みの処理と辻褄が合わなくなることがあるため)。
            isr_value = read_isr();       // マスタPICのISRレジスタを読む
            if ((isr_value&(1<<7)) != 0) {  // bit7(IRQ7)が立っているか確認
                eoi();                        // 立っていれば本物なのでEOIを送る
            }
            break;   // このcaseを抜ける

        default:
            // 上記以外の番号(想定していない例外など)が来た場合。
            // 今はまだ、原因を画面に表示する仕組みが無いので、
            // とりあえず無限ループで止めて、それ以上何も壊さない
            // ようにしている。
            while (1) { }
    }
}
