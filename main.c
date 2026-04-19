#include "stdint.h"
#include "stddef.h"

void KMain(void) {
    // const char *p = "Hello, World!";
    char *p = (char *)0xb8000;
    p[0] = 'c';

        while (1) {
        // 無限ループで停止
    }
}