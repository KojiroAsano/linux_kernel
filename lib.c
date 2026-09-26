// ============================================================================
// lib.c — プロセスの連結リスト操作(process.cのready/wait/kill listで使う)
// ============================================================================
// process.cは「実行可能待ちのプロセス一覧」「スリープ中のプロセス一覧」
// 「終了済みのプロセス一覧」を、それぞれstruct HeadListで管理している。
// struct Process(process.h)は先頭にstruct List *nextを持つので、
// struct Process*をstruct List*にキャストして、ここの汎用リスト関数へ
// そのまま渡せる。
// ============================================================================

#include "lib.h"
#include "process.h"
#include "stddef.h"
#include "debug.h"

// ============================================================================
// append_list_tail — itemをlistの末尾に追加する
// ============================================================================
void append_list_tail(struct HeadList *list, struct List *item)
{
    item->next = NULL;   // 新しく末尾になるので、次は無い

    if (is_list_empty(list)) {
        // リストが空だった場合は、先頭も末尾もitem自身になる
        list->next = item;
        list->tail = item;
    }
    else {
        // 今までの末尾(tail)の次にitemをつなぎ、tailを更新する
        list->tail->next = item;
        list->tail = item;
    }
}

// ============================================================================
// remove_list_head — listの先頭を取り除いて返す
// ============================================================================
struct List* remove_list_head(struct HeadList *list)
{
    struct List *item;

    if (is_list_empty(list)) {
        return NULL;   // 空なら何も取り出せない
    }

    item = list->next;        // 今の先頭を取り出す
    list->next = item->next;  // 先頭を次の要素へ進める

    if (list->next == NULL) {
        // 取り出した結果リストが空になったなら、tailもクリアしておく
        list->tail = NULL;
    }

    return item;
}

// ============================================================================
// remove_list — listの中から、waitフィールドがwaitと一致する最初の要素を
// 取り除いて返す
// ============================================================================
// sleep()で「waitという値の理由でスリープ中」のプロセスを、wake_up()が
// waitの値を指定して探し出し、リストから取り除くために使う。先頭とは
// 限らない位置の要素を取り除く必要があるので、remove_list_headとは別に
// 一つ前の要素(prev)を覚えながら辿っていく実装になっている。
struct List* remove_list(struct HeadList *list, int wait)
{
    struct List *current = list->next;
    struct List *prev = (struct List*)list;   // HeadListの「next」フィールドが
                                                // struct Listの「next」と同じ
                                                // オフセットにあることを利用し、
                                                // 番人役(dummy head)として扱う
    struct List *item = NULL;

    while (current != NULL) {
        if (((struct Process*)current)->wait == wait) {
            prev->next = current->next;   // currentを飛ばしてつなぎ直す
            item = current;

            if (list->next == NULL) {
                list->tail = NULL;               // 取り除いた結果空になった
            }
            else if (current->next == NULL) {
                list->tail = (struct List*)prev;  // 取り除いたのが末尾だった
            }

            break;
        }

        prev = current;
        current = current->next;
    }

    return item;
}

// ============================================================================
// remove_list_by_pid — listの中から、pidフィールドがpidと一致する要素を
// 取り除いて返す
// ============================================================================
// process.cのkill_process()が、「PIDを指定して、ready_listかwait_list
// のどちらにいるか分からないプロセスを探して取り除く」ために使う。
// 考え方・実装ともremove_list()(wait理由で探す版)と全く同じで、
// 比較する対象がwaitからpidに変わっただけ。
struct List* remove_list_by_pid(struct HeadList *list, int pid)
{
    struct List *current = list->next;
    struct List *prev = (struct List*)list;   // remove_list()と同じく、番人役として扱う
    struct List *item = NULL;

    while (current != NULL) {
        if (((struct Process*)current)->pid == pid) {
            prev->next = current->next;   // currentを飛ばしてつなぎ直す
            item = current;

            if (list->next == NULL) {
                list->tail = NULL;               // 取り除いた結果空になった
            }
            else if (current->next == NULL) {
                list->tail = (struct List*)prev;  // 取り除いたのが末尾だった
            }

            break;
        }

        prev = current;
        current = current->next;
    }

    return item;
}

// ============================================================================
// is_list_empty — listが空かどうかを返す
// ============================================================================
bool is_list_empty(struct HeadList *list)
{
    return (list->next == NULL);
}
