#include "lib.h"

int main(void)
{
    while (1) {
        printf("process 1 running\n");
        sleepu(50);   // 約0.5秒(PITは100Hzなので50ティック)待ってから繰り返す
    }
    return 0;
}
