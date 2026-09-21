// ============================================================================
// memory.c — 物理メモリ管理(e820解析 / ページ単位アロケータ / ページテーブル)
// ============================================================================
// このファイルは大きく3つの役目を持つ。
//   1. init_memory(): BIOSが教えてくれたメモリマップ(e820)を解析して、
//      「実際に使える空きメモリはどこか」を調べる。
//   2. kalloc()/kfree(): 調べた空きメモリを、2MB単位の「ページ」として
//      1個ずつ貸し出したり返してもらったりする、簡単なアロケータ。
//   3. map_pages()/init_kvm(): kalloc()で取ったページを使って、
//      カーネル専用の新しいページテーブルを組み立て、実際に切り替える。
// ============================================================================

#include "memory.h"
#include "print.h"
#include "debug.h"
#include "lib.h"
#include "stddef.h"
#include "stdbool.h"

static void free_region(uint64_t v, uint64_t e);

static struct FreeMemRegion free_mem_region[50];  // e820から拾った、使える領域の一覧
static struct Page free_memory;      // 空きページの連結リストの「先頭」を指す番人役
                                     // (free_memory自身はページではなく、
                                     //  free_memory.nextが本当の先頭ページ)
static uint64_t memory_end;           // 見つかった空きメモリの一番高いアドレス
uint64_t page_map;                     // init_kvm()が新しく作るページテーブルの
                                       // (仮想)アドレス

// linker script(link.lds)が用意してくれる特殊なシンボル。「カーネルの
// 実行ファイルが、メモリ上でどこまで使っているか(の直後)」を表す。
// endという名前の実体は存在せず、あくまで「そのアドレス」を得るために
// &end という形で使う(値そのものではなく、アドレスが欲しいのでこの書き方)。
extern char end;

// ============================================================================
// init_memory — e820のメモリマップを解析し、空きページをアロケータへ登録する
// ============================================================================
void init_memory(void)
{
    // loader.asmが物理0x9000番地に書き込んでおいた「取得件数」と、
    // 0x9008番地から並んでいる実際のE820構造体の配列を読み出す。
    int32_t count = *(int32_t*)0x9000;
    uint64_t total_mem = 0;
    struct E820 *mem_map = (struct E820*)0x9008;
    int free_region_count = 0;

    // free_mem_region配列は50個分しか用意していないので、万が一それを
    // 超える件数が返ってきたら、ここで検出して止める(ASSERTについては
    // debug.h参照)。
    ASSERT(count <= 50);

    // 取得できた領域を1つずつ見ていく。
	for(int32_t i = 0; i < count; i++) {
        if(mem_map[i].type == 1) {
            // type==1は「Usable」、つまりOSが自由に使ってよい空きメモリ。
            // それ以外(予約領域など)は無視する。
            free_mem_region[free_region_count].address = mem_map[i].address;
            free_mem_region[free_region_count].length = mem_map[i].length;
            total_mem += mem_map[i].length;
            free_region_count++;
        }
        // 見つかった領域を1件ずつ画面に表示する(デバッグ・確認用)。
        printk("%x  %uKB  %u\n",mem_map[i].address,mem_map[i].length/1024,(uint64_t)mem_map[i].type);
	}

    // 使える領域が見つかるたびに、実際にkalloc用の連結リストへ
    // 登録していく(free_region()を呼ぶ)。ただし、カーネル自身が
    // 今まさに使っているメモリ(&endより手前)を誤って「空き」として
    // 配ってしまわないよう、&end以降の部分だけを渡すようにしている。
    for (int i = 0; i < free_region_count; i++) {
        uint64_t vstart = P2V(free_mem_region[i].address);  // 物理→仮想アドレスへ変換
        uint64_t vend = vstart + free_mem_region[i].length;

        if (vstart > (uint64_t)&end) {
            // 領域全体がカーネルより後ろにあるので、そのまま丸ごと登録できる
            free_region(vstart, vend);
        }
        else if (vend > (uint64_t)&end) {
            // 領域の前半はカーネルと重なっているが、後半は空いているので、
            // &end以降の部分だけを登録する
            free_region((uint64_t)&end, vend);
        }
        // (どちらでもない、つまり領域全体がカーネルより手前にある場合は、
        //  登録できる空き部分が無いので何もしない)
    }

    // 最後に登録できたページのアドレス+1ページ分を、扱える空きメモリの
    // 上限として覚えておく(setup_kvm()がページテーブルを組み立てる
    // 範囲の指定に使う)。
    memory_end = (uint64_t)free_memory.next+PAGE_SIZE;
    printk("%x\n",memory_end);
}

// ============================================================================
// free_region — 指定した仮想アドレス範囲を、2MBページ単位でkfree()していく
// ============================================================================
static void free_region(uint64_t v, uint64_t e)
{
    // PA_UP(v)で「vを含む最初のページの先頭」から始め、PAGE_SIZEずつ
    // 進めながら、eに収まりきる分だけkfree()していく。
    for (uint64_t start = PA_UP(v); start+PAGE_SIZE <= e; start += PAGE_SIZE) {
        // 0xffff800040000000 = KERNEL_BASE + 1GB。今のページテーブルの
        // 組み方(loader.asm)では、higher-half側は先頭1GBしか
        // マッピングされていないので、それを超える範囲は配らないように
        // 上限を設けている。
        if (start+PAGE_SIZE <= 0xffff800040000000) {
           kfree(start);
        }
    }
}

// ============================================================================
// kfree — 使い終わったページを、空きページの連結リストに戻す
// ============================================================================
void kfree(uint64_t v)
{
    // 3つの前提条件を確認する:
    //   ・vはちゃんと2MB境界に揃っているか
    //   ・vはカーネル自身が使っている範囲より後ろか
    //   ・vは今マッピングされている範囲(先頭1GB)に収まっているか
    // どれか崩れていたら、静かに変なメモリを壊す前に、はっきり止まる。
    ASSERT(v % PAGE_SIZE == 0);
    ASSERT(v >= (uint64_t) & end);
    ASSERT(v+PAGE_SIZE <= 0xffff800040000000);

    // 「空きページの連結リスト」の先頭にvを追加する。
    // vのアドレスが指す先(今は誰にも使われていないページの中身)に、
    // struct Pageとして書き込む。つまり、この空きページの先頭8バイトを
    // 「次の空きページへのポインタ」として間借りしている。
    struct Page *page_address = (struct Page*)v;
    page_address->next = free_memory.next;  // 今までの先頭を、自分のnextにする
    free_memory.next = page_address;         // 自分を新しい先頭にする
}

// ============================================================================
// kalloc — 空きページを1つ取り出す
// ============================================================================
void* kalloc(void)
{
    // 連結リストの先頭を取り出す(空だったらNULLのまま)。
    struct Page *page_address = free_memory.next;

    if (page_address != NULL) {
        // kfree()と同じ3つの前提条件を、念のため取り出す時にも確認する
        // (リストが壊れていないかの二重チェック)。
        ASSERT((uint64_t)page_address % PAGE_SIZE == 0);
        ASSERT((uint64_t)page_address >= (uint64_t)&end);
        ASSERT((uint64_t)page_address+PAGE_SIZE <= 0xffff800040000000);

        // リストの先頭を、取り出したページのnext(=その次のページ)に進める
        free_memory.next = page_address->next;
    }

    return page_address;
}

// ============================================================================
// find_pml4t_entry / find_pdpt_entry — ページテーブルを1階層ずつたどる
// ============================================================================
// x86-64のページング(1GBページを使う場合)は、
//   PML4テーブル(512エントリ) → PDPTテーブル(512エントリ、各エントリが
//   1GBページそのものを表す)
// という2階層構造になっている。仮想アドレスのどのビットが、どの階層の
// 「何番目のエントリ」に対応するかは仕様で決まっていて、
//   PML4のインデックス = アドレスのbit47-39(9bit)
//   PDPTのインデックス = アドレスのbit38-30(9bit)
// という計算になる(下のindexの計算がそれに対応している)。

// mapが指すPML4テーブルの中から、仮想アドレスvに対応するエントリを探し、
// その先にあるPDPTテーブル(の仮想アドレス)を返す。
// allocが1で、かつまだそのエントリが存在しない(Presentでない)場合は、
// kalloc()で新しいページを取ってきて、PDPTテーブルとして初期化してから
// PML4に登録する(この動作を「必要になったら作る」という意味で
// 「遅延割り当て」と呼ぶことがある)。
static PDPTR find_pml4t_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute)
{
    PDPTR *map_entry = (PDPTR*)map;
    PDPTR pdptr = NULL;
    unsigned int index = (v >> 39) & 0x1FF;   // bit47-39を取り出す(9bit=0-511)

    if ((uint64_t)map_entry[index] & PTE_P) {
        // すでにこのエントリは存在する(Presentビットが立っている)ので、
        // そこに書かれている物理アドレスを仮想アドレスに変換して返す。
        pdptr = (PDPTR)P2V(PDE_ADDR(map_entry[index]));
    }
    else if (alloc == 1) {
        // まだ存在しないので、新しいページを1つ確保してPDPTテーブルとして使う。
        pdptr = (PDPTR)kalloc();
        if (pdptr != NULL) {
            memset(pdptr, 0, PAGE_SIZE);   // 中身を全部0で初期化(全エントリ「未使用」状態に)
            // PML4のこのエントリに、新しく作ったPDPTテーブルの物理アドレスと
            // フラグ(attribute、Present/Writable/Userなど)を書き込む。
            map_entry[index] = (PDPTR)(V2P(pdptr) | attribute);
        }
    }

    return pdptr;
}

// find_pml4t_entry()を使ってPDPTテーブルまでたどり着いた後、その中から
// 仮想アドレスvに対応する「PD」(実際にはこのOSでは1GBページのエントリ
// そのもの)を探す。考え方はfind_pml4t_entry()と全く同じで、1階層下がっただけ。
static PD find_pdpt_entry(uint64_t map, uint64_t v, int alloc, uint32_t attribute)
{
    PDPTR pdptr = NULL;
    PD pd = NULL;
    unsigned int index = (v >> 30) & 0x1FF;   // bit38-30を取り出す(9bit=0-511)

    pdptr = find_pml4t_entry(map, v, alloc, attribute);
    if (pdptr == NULL)
        return NULL;

    if ((uint64_t)pdptr[index] & PTE_P) {
        pd = (PD)P2V(PDE_ADDR(pdptr[index]));
    }
    else if (alloc == 1) {
        pd = (PD)kalloc();
        if (pd != NULL) {
            memset(pd, 0, PAGE_SIZE);
            pdptr[index] = (PD)(V2P(pd) | attribute);
        }
    }

    return pd;
}

// ============================================================================
// map_pages — 仮想アドレス範囲[v,e)を、物理アドレスpaから順にマッピングする
// ============================================================================
// 「ページテーブルを実際に組み立てる」という、このファイルの中心となる
// 関数。呼び出し側は「この仮想アドレス範囲を、この物理アドレスに、
// こういう権限(attribute)でマッピングしたい」とだけ指定すればよく、
// 途中のPML4/PDPTテーブルを自分で用意する必要はない(必要なテーブルは
// find_pml4t_entry/find_pdpt_entryが自動で作ってくれる)。
bool map_pages(uint64_t map, uint64_t v, uint64_t e, uint64_t pa, uint32_t attribute)
{
    uint64_t vstart = PA_DOWN(v);   // 開始アドレスを2MB境界に切り下げる
    uint64_t vend = PA_UP(e);       // 終了アドレスを2MB境界に切り上げる
    PD pd = NULL;
    unsigned int index;

    ASSERT(v < e);                              // 範囲がちゃんと正しい向きか
    ASSERT(pa % PAGE_SIZE == 0);                 // 物理アドレスが2MB境界に揃っているか
    ASSERT(pa+vend-vstart <= 1024*1024*1024);    // 1GBの範囲に収まっているか
                                                  // (このOSは1GBページしか
                                                  //  使わないため、1回の
                                                  //  map_pages呼び出しで
                                                  //  1GBを超える範囲は扱えない)

    // vstartから2MBずつ進みながら、1ページ分ずつマッピングしていく。
    do {
        // このページが属するPDPT(1GB単位のテーブル)を、無ければ
        // 作りながら取得する。
        pd = find_pdpt_entry(map, vstart, 1, attribute);
        if (pd == NULL) {
            return false;   // kalloc()が失敗した(メモリが尽きた)場合など
        }

        index = (vstart >> 21) & 0x1FF;   // bit29-21を取り出す(2MBページの番号)
        ASSERT(((uint64_t)pd[index] & PTE_P) == 0);  // 二重にマッピングしようと
                                                     // していないかの確認

        // このエントリに、物理アドレスpa・権限attribute・PS(2MBページ)
        // フラグをまとめて書き込む。これで晴れて仮想→物理の対応が1つ
        // 出来上がる。
        pd[index] = (PDE)(pa | attribute | PTE_ENTRY);

        vstart += PAGE_SIZE;
        pa += PAGE_SIZE;
    } while (vstart + PAGE_SIZE <= vend);

    return true;
}

// ============================================================================
// switch_vm — 実際に使うページテーブルを切り替える
// ============================================================================
void switch_vm(uint64_t map)
{
    // CR3レジスタに、使いたいページテーブル(PML4)の物理アドレスを
    // 設定する。この瞬間から、CPUのアドレス変換がこの新しいテーブルを
    // 基準に行われるようになる。
    load_cr3(V2P(map));
}

// ============================================================================
// setup_kvm — カーネル専用の新しいページテーブルを組み立てる
// ============================================================================
static void setup_kvm(void)
{
    // まず、ページテーブル自身(PML4)を置くための1ページを確保する。
    page_map = (uint64_t)kalloc();
    ASSERT(page_map != 0);

    memset((void*)page_map, 0, PAGE_SIZE);
    // KERNEL_BASEからmemory_end(init_memory()が見つけた空きメモリの
    // 上限)までの仮想アドレス範囲を、対応する物理アドレス(V2P(KERNEL_BASE)、
    // つまり物理0番地)から、Present・Writableの権限でマッピングする。
    // これで、loader.asmが暫定で作ったページテーブルと同じ内容を、
    // 自前できちんと組み立て直したことになる。
    bool status = map_pages(page_map, KERNEL_BASE, memory_end, V2P(KERNEL_BASE), PTE_P|PTE_W);
    ASSERT(status == true);
}

// ============================================================================
// init_kvm — カーネル専用のページテーブルを作り、実際に切り替える
// ============================================================================
void init_kvm(void)
{
    setup_kvm();          // 新しいページテーブルを組み立てる
    switch_vm(page_map);   // CR3を書き換えて、実際にそれを使い始める
    printk("memory manager is working now");
    // ここまで無事に実行できれば、loader.asm由来の暫定ページテーブル
    // から、memory.cが自前で組み立てたページテーブルへの切り替えが
    // 成功したことになる(切り替えに失敗していれば、この直後の
    // 命令フェッチやprintkのメモリアクセスの時点で即座にページ
    // フォルトが起きるはず)。
}
