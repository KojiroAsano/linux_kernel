#ifndef _DEBUG_H_
#define _DEBUG_H_

#include "stdint.h"

// ASSERTマクロの定義
// ASSERTマクロは、条件が偽の場合にエラーを報告するためのマクロです。
// 例えば、ASSERT(x > 0)は、xが0以下の場合にエラーを報告します。
//  ASSERTマクロは、条件が偽の場合にerror_check関数を呼び出します。
// error_check関数は、エラーが発生したファイル名と行番号を表示し、無限ループで停止します。
//  ASSERTマクロは、デバッグ時にコードの正当性を確認するために使用されます。条件が偽の場合、エラーが報告されるため、問題の原因を特定しやすくなります。
// 例えば、ASSERT(x > 0)は、xが0以下の場合にエラーを報告します。
//  ASSERTマクロは、条件が偽の場合にerror_check関数を呼び出します。error_check関数は、エラーが発生したファイル名と行番号を表示し、無限ループで停止します。

#define ASSERT(e) do {                      \
        if (!(e))                           \
            error_check(__FILE__,__LINE__); \
} while (0) 

void error_check(char *file, uint64_t line);

#endif