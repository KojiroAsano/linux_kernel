// ============================================================================
// process.c — プロセスの作成・スケジューリング・終了処理
// ============================================================================
// このファイルは大きく3つの役目を持つ。
//   1. init_process(): loader.asmがディスクから読み込んでおいた3本の
//      ユーザープログラムを、それぞれ独立したプロセスとして作る。
//   2. launch()/schedule()/yield(): どのプロセスを次にCPUで走らせるかを
//      決め、実際に切り替える(コンテキストスイッチ)。
//   3. sleep()/wake_up()/exit()/wait(): プロセスの待ち合わせ・終了・
//      後片付け。
// ============================================================================

#include "process.h"
#include "trap.h"
#include "memory.h"
#include "print.h"
#include "lib.h"
#include "debug.h"

// kernel.asmが定義している実際のTSS(global Tss)を、struct TSSとして
// 参照する。set_tss()がここを直接書き換える。
extern struct TSS Tss;

static struct Process process_table[NUM_PROC];   // 全プロセスの実体(固定サイズの配列)
static int pid_num = 1;                            // 次に割り当てるPID
static struct ProcessControl pc;                     // スケジューラの状態(readyList等)

// アイドルプロセス。process_table(実際のプロセス用)とは別に1つだけ
// 持っておく特別な存在で、ready_listに実行可能なプロセスが1つも
// 無くなった(=全プロセスがスリープ中)時に、schedule()がここへ
// 切り替える。詳しくはinit_idle()とschedule()のコメント参照。
static struct Process idle_process;

// ============================================================================
// set_tss — 次にCPUで動かすプロセス用に、TSSのRSP0を書き換える
// ============================================================================
// RSP0は「リング3実行中に割り込み/例外が起きた時、CPUが自動的に
// 切り替える先のカーネルスタック」。プロセスを切り替えるたびに、
// 新しく動かすプロセス自身のカーネルスタックへ向け直す必要がある
// (これをしないと、別プロセスのスタックを割り込みハンドラが
// 使ってしまい、簡単に壊れる)。
static void set_tss(struct Process *proc)
{
    Tss.rsp0 = proc->stack + STACK_SIZE;   // スタックは下方向に伸びるので、
                                             // 確保した領域の末尾を渡す
}

// ============================================================================
// find_unused_process — process_tableから空いているスロットを1つ探す
// ============================================================================
static struct Process* find_unused_process(void)
{
    struct Process *process = NULL;

    for (int i = 0; i < NUM_PROC; i++) {
        if (process_table[i].state == PROC_UNUSED) {
            process = &process_table[i];
            break;
        }
    }

    return process;
}

// ============================================================================
// set_process_entry — 1つのプロセスを実際に組み立てる
// ============================================================================
// addrは、loader.asmがそのユーザープログラムを読み込んでおいた物理
// アドレス(higher-half側から見るのでP2Vして渡される)。ここでやって
// いることは大きく3つ:
//   1. このプロセス専用のカーネルスタックを確保する
//   2. そのスタックの中に「リング3へ入るための偽の割り込みフレーム」
//      (tf)と、「最初にswap()されたらTrapReturnへ飛ぶための偽の
//      コンテキスト」(context)を仕込んでおく
//   3. このプロセス専用のページテーブルを組み立て、ユーザープログラムの
//      中身をそこへコピーする(setup_kvm/setup_uvm、memory.c)
static void set_process_entry(struct Process *proc, uint64_t addr)
{
    uint64_t stack_top;

    proc->state = PROC_INIT;
    proc->pid = pid_num++;

    // このプロセス専用のカーネルスタック(2MBページ1枚)を確保する。
    proc->stack = (uint64_t)kalloc();
    ASSERT(proc->stack != 0);

    memset((void*)proc->stack, 0, PAGE_SIZE);
    stack_top = proc->stack + STACK_SIZE;

    // スタックの一番上から、TrapFrame(tf)を1つ分、さらにその下に
    // レジスタ6個分+戻り先1個分(7*8バイト)の「偽のswap()コンテキスト」を
    // 積んでおく。実際のメモリ配置は下から上へ:
    //   [context] rbx,rbp,r12,r13,r14,r15,TrapReturnのアドレス(7個)
    //   [tf]      TrapFrame一式
    // swap()が呼ばれると6個のレジスタをpopしてret(=7個目のTrapReturnの
    // アドレスへジャンプ)する。これにより、一度もCPUで動いたことの
    // ないプロセスでも、初めてswap()で選ばれた瞬間にTrapReturn→iretqを
    // 経由してリング3のエントリポイントへ着地する(xv6などでも使われる
    // 定番のテクニック)。
    proc->context = stack_top - sizeof(struct TrapFrame) - 7*8;
    *(uint64_t*)(proc->context + 6*8) = (uint64_t)TrapReturn;

    proc->tf = (struct TrapFrame*)(stack_top - sizeof(struct TrapFrame));
    proc->tf->cs = 0x10|3;               // ユーザーコードセグメント|RPL3
    proc->tf->rip = 0x400000;             // ユーザープログラムのエントリポイント
                                            // (setup_uvmが必ずここへマップする)
    proc->tf->ss = 0x18|3;                  // ユーザーデータセグメント|RPL3
    proc->tf->rsp = 0x400000 + PAGE_SIZE;    // ユーザースタック(同じ1ページの末尾を使う)
    proc->tf->rflags = 0x202;                 // IF(割り込み許可)ビットを立てておく。
                                                // これにより、このプロセスへ最初に
                                                // 切り替わった瞬間から割り込みが有効になる
                                                // (KMain側でenable_interrupts()を
                                                // 呼ぶ必要が無い)。

    // このプロセス専用のページテーブルを新しく組み立て(setup_kvm、
    // memory.c、カーネル領域だけをマップした状態)、そこへユーザー
    // プログラム本体を0x400000番地にマップ+コピーする(setup_uvm)。
    proc->page_map = setup_kvm();
    ASSERT(proc->page_map != 0);
    ASSERT(setup_uvm(proc->page_map, P2V(addr), 5120));
    proc->state = PROC_READY;
}

// ============================================================================
// get_pc — スケジューラの状態を取り出す(ただのアクセサ)
// ============================================================================
static struct ProcessControl* get_pc(void)
{
    return &pc;
}

// ============================================================================
// init_idle — アイドルプロセスを組み立てる
// ============================================================================
// 【なぜ必要か】このデモのuser1〜3は、画面へ1行出力したら
// sleepu(50)でスリープする、という動作を繰り返す。3プロセスとも
// ほぼ同時にスリープしてしまうと、ready_listが空になる瞬間が
// 実際に起こりうる。その状態でsleep()がschedule()を呼ぶと、
// 「次に切り替える先が無い」ことになってしまう。実行可能な
// プロセスがどれだけ減っても、少なくとも1つは必ずCPUで「動いて」
// いなければならない(swap()は必ずどこかのRSPへ切り替える前提の
// 仕組みなので)ため、何もしていない時専用の、この特別な
// プロセスを用意しておく。
//
// 通常のプロセス(set_process_entry)と違い、リング3のユーザー
// コードは持たず、context の戻り先はtrap.asmのidle_loop(TrapReturn
// ではない)。ページテーブルはsetup_kvm()でカーネル領域だけ
// マップしたものを使う(idle_loopはずっとリング0で動き続けるので、
// ユーザー領域のマッピングは不要)。
static void init_idle(void)
{
    idle_process.stack = (uint64_t)kalloc();
    ASSERT(idle_process.stack != 0);
    memset((void*)idle_process.stack, 0, PAGE_SIZE);

    // 通常のプロセスと同じ「偽のswap()コンテキスト」を仕込むが、
    // 戻り先(7個目のスロット)にはTrapReturnではなくidle_loopの
    // アドレスを入れる。TrapFrameは使わない(idle_loopはリング3へ
    // 行かないので、iretq用のフレームが要らない)ので、スタックの
    // 一番上からそのままこのコンテキストを置く。
    uint64_t stack_top = idle_process.stack + STACK_SIZE;
    idle_process.context = stack_top - 7*8;
    *(uint64_t*)(idle_process.context + 6*8) = (uint64_t)idle_loop;

    idle_process.page_map = setup_kvm();
    ASSERT(idle_process.page_map != 0);
}

// ============================================================================
// init_process — 最初の3プロセスを作る
// ============================================================================
// loader.asmが物理0x20000/0x30000/0x40000にそれぞれ読み込んでおいた
// user1.bin/user2.bin/user3.binを元に、3つのプロセスを作ってready_list
// へ登録する。
void init_process(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;
    uint64_t addr[3] = {0x20000, 0x30000, 0x40000};

    process_control = get_pc();
    list = &process_control->ready_list;

    for (int i = 0; i < 3; i++) {
        process = find_unused_process();
        set_process_entry(process, addr[i]);
        append_list_tail(list, (struct List*)process);
    }

    init_idle();   // schedule()がフォールバック先として使うアイドルプロセスを用意する
}

// ============================================================================
// launch — 最初のプロセスへ制御を渡す
// ============================================================================
// KMainの最後から一度だけ呼ばれる。ready_listの先頭を取り出し、
// TSS.RSP0とCR3をそのプロセス用に設定してから、pstart()(trap.asm)で
// tf一式をiretqしてリング3へ着地する。ここから先、C言語の制御はもう
// KMainには戻らない。
void launch(void)
{
    struct ProcessControl *process_control;
    struct Process *process;

    process_control = get_pc();
    process = (struct Process*)remove_list_head(&process_control->ready_list);
    process->state = PROC_RUNNING;
    process_control->current_process = process;

    set_tss(process);
    switch_vm(process->page_map);
    pstart(process->tf);   // ここから戻ってこない
}

// ============================================================================
// switch_process — 実際にプロセスを切り替える
// ============================================================================
static void switch_process(struct Process *prev, struct Process *current)
{
    set_tss(current);          // 次のプロセス用のRSP0に切り替える
    switch_vm(current->page_map);   // 次のプロセス用のページテーブルに切り替える
    swap(&prev->context, current->context);   // カーネルスタックのコンテキストを
                                                 // 切り替える(trap.asm)。ここで
                                                 // 一旦「今のプロセス」の実行は
                                                 // 止まり、いつか誰かが同じように
                                                 // このプロセスをswap()し直すまで
                                                 // 再開しない。
}

// ============================================================================
// schedule — ready_listの先頭を取り出し、今のプロセスと入れ替える
// ============================================================================
// ready_listが空(=実行可能なプロセスが1つも無い)場合は、
// idle_process(init_idle参照)へ切り替える。全プロセスがsleepu()で
// 眠るタイミングが重なると実際にこの状態になりうるため、
// 「起こりえない」と決めつけてASSERTするのではなく、正しく
// 処理する必要がある。
static void schedule(void)
{
    struct Process *prev_proc;
    struct Process *current_proc;
    struct ProcessControl *process_control;
    struct HeadList *list;

    process_control = get_pc();
    prev_proc = process_control->current_process;
    list = &process_control->ready_list;

    if (is_list_empty(list)) {
        current_proc = &idle_process;   // 他に誰も実行可能でないので、アイドルへ
    }
    else {
        current_proc = (struct Process*)remove_list_head(list);
    }

    current_proc->state = PROC_RUNNING;
    process_control->current_process = current_proc;

    switch_process(prev_proc, current_proc);
}

// ============================================================================
// yield — 今のプロセスを実行可能な状態のままready_listの末尾に戻し、
// 次のプロセスへ切り替える
// ============================================================================
// trap.cのhandler()が、タイマー割り込み(ベクタ32)のたびに呼ぶ
// (プリエンプティブなラウンドロビンスケジューリング)。
void yield(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;

    process_control = get_pc();
    list = &process_control->ready_list;

    if (is_list_empty(list)) {
        // 他に実行可能なプロセスが無いなら、切り替えても意味が無いので
        // そのまま今のプロセスを続ける。
        return;
    }

    process = process_control->current_process;
    process->state = PROC_READY;
    append_list_tail(list, (struct List*)process);
    schedule();
}

// ============================================================================
// sleep — 今のプロセスを、waitという理由でスリープさせる
// ============================================================================
// wake_up(wait)が同じ値を指定して呼ばれるまで、二度とready_listへは
// 戻らない。
void sleep(int wait)
{
    struct ProcessControl *process_control;
    struct Process *process;

    process_control = get_pc();
    process = process_control->current_process;
    process->state = PROC_SLEEP;
    process->wait = wait;

    append_list_tail(&process_control->wait_list, (struct List*)process);
    schedule();
}

// ============================================================================
// wake_up — waitという理由でスリープしているプロセスを、全部起こす
// ============================================================================
void wake_up(int wait)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *ready_list;
    struct HeadList *wait_list;

    process_control = get_pc();
    ready_list = &process_control->ready_list;
    wait_list = &process_control->wait_list;
    process = (struct Process*)remove_list(wait_list, wait);

    // 同じ理由でスリープしているプロセスが複数いる可能性があるので、
    // 見つからなくなるまで繰り返す。
    while (process != NULL) {
        process->state = PROC_READY;
        append_list_tail(ready_list, (struct List*)process);
        process = (struct Process*)remove_list(wait_list, wait);
    }
}

// ============================================================================
// exit — 今のプロセスを終了させる
// ============================================================================
// この場でメモリを解放してしまうと、まだ自分自身が使っているスタックや
// ページテーブルを壊すことになるので、実際の後片付けはしない。
// kill_listに載せておき、wait()(いわゆるinitプロセス役)が後で
// 回収するのを待つ。
void exit(void)
{
    struct ProcessControl *process_control;
    struct Process* process;
    struct HeadList *list;

    process_control = get_pc();
    process = process_control->current_process;
    process->state = PROC_KILLED;

    list = &process_control->kill_list;
    append_list_tail(list, (struct List*)process);

    wake_up(1);   // wait()がwait(1)でスリープしているかもしれないので起こす
    schedule();
}

// ============================================================================
// kill_process — 指定したPIDのプロセスを、外部から強制終了させる
// ============================================================================
// exit()が「自分自身」しか終了させられないのに対し、こちらは
// 「他のプロセスをPIDで指定して」終了させるためのもの
// (sys_kill/killu経由でユーザープログラムから呼べる)。
//
// 狙ったプロセスが今どこにいるかで、やることが変わる:
//   ・自分自身のPIDを指定した場合   → exit()にそのまま任せる
//     (スケジューラの切り替えが必要なので、この関数の中では
//      完結できない)
//   ・ready_listかwait_listにいる場合 → そのリストから外して
//     kill_listへ移すだけでよい(今CPUを使っているのは呼び出し元
//     なので、コンテキストスイッチは不要)
//   ・どちらにも見つからない場合     → 存在しないPID、すでに
//     終了済み、あるいはアイドルプロセス(PID0は現在誰にも
//     割り当てない番号なので、間違って一致することもない)
//
// 戻り値は「実際に終了させられたか」。呼び出し元のプロセス自身を
// 終了させた場合(exit()経由)は、この関数自体はもう戻ってこない。
bool kill_process(int pid)
{
    struct ProcessControl *process_control;
    struct Process *process;

    process_control = get_pc();

    if (process_control->current_process->pid == pid) {
        exit();   // 自分自身を指定された場合。ここから戻ってこない。
        return true;
    }

    // まずready_list(実行可能待ち)を探し、無ければwait_list
    // (スリープ中)を探す。
    process = (struct Process*)remove_list_by_pid(&process_control->ready_list, pid);
    if (process == NULL) {
        process = (struct Process*)remove_list_by_pid(&process_control->wait_list, pid);
    }

    if (process == NULL) {
        return false;   // 該当するPIDが見つからなかった
    }

    process->state = PROC_KILLED;
    append_list_tail(&process_control->kill_list, (struct List*)process);
    wake_up(1);   // wait()を起こす(exit()と同じ後片付けの流れに合流させる)

    return true;
}

// ============================================================================
// wait — kill_listにたまった終了済みプロセスを回収し続ける
// ============================================================================
// このデモではsys_wait経由でユーザープログラムから直接呼べるように
// なっているが、本来はOS全体で1つだけ動く「initプロセス」のような
// 役割を想定している。
void wait(void)
{
    struct ProcessControl *process_control;
    struct Process *process;
    struct HeadList *list;

    process_control = get_pc();
    list = &process_control->kill_list;

    while (1) {
        if (!is_list_empty(list)) {
            process = (struct Process*)remove_list_head(list);
            ASSERT(process->state == PROC_KILLED);

            // スタック・ページテーブルを解放してから、
            // process_table上のスロットを丸ごと0クリアして
            // PROC_UNUSEDへ戻す(構造体の全フィールドが0になるので、
            // state(=PROC_UNUSEDの値0)も自動的にそうなる)。
            kfree(process->stack);
            free_vm(process->page_map);
            memset(process, 0, sizeof(struct Process));
        }
        else {
            sleep(1);   // 回収するものが無ければ、理由1でスリープして待つ
        }
    }
}
