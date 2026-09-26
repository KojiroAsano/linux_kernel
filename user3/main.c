#include "lib.h"

int main(void)
{
    int count = 0;

    while (1) {
        printf("process 3 running\n");
        sleepu(50);   // 約0.5秒(PITは100Hzなので50ティック)待ってから繰り返す

        count++;
        if (count == 5) {
            // killu()の動作確認用デモ。PID=2は、process.cのinit_process()が
            // user1→user2→user3の順にプロセスを作る(pid_numが1から順に
            // 割り振られる)ことが分かっているので、ここでは決め打ちで
            // user2を狙っている。これ以降、画面から
            // "process 2 running"が出なくなれば、killu()が正しく効いた証拠。
            printf("process 3: killing pid 2\n");
            killu(2);
        }
    }
    return 0;
}
