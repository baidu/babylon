#ifndef BABYLON_PROTECT_H
#define BABYLON_PROTECT_H
#undef BABYLON_UNPROTECT_H

// 有些库使用了名称过于通用的全局宏定义，会干扰代码编译
// 采用成对的push_macro/pop_marco进行环境清理，保证中间段落编译正常
// 需要成对使用，例如
// #include "babylon/protect.h"
// ... 中间代码不受相应宏定义影响
// ... 中间如果存在其他#include确保不要重入使用protect.h
// #include "babylon/unprotect.h"

// 例如brpc，使用了-Dprivate=public的方式来实现访问私有成员的trick
// 会影响<sstream>等其他库的编译
#pragma push_macro("private")
#undef private

// 例如ps/se/vhtmlparser中定义了BLOCK_SIZE，容易产生干扰
#pragma push_macro("BLOCK_SIZE")
#undef BLOCK_SIZE

// 例如baidu/feed-mlarch/feature-extract-framework中定义了F
#pragma push_macro("F")
#undef F

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored \
    "-Wc99-designator" // Things like T[] {[0] = value0, [1] = value1}
#pragma GCC diagnostic ignored \
    "-Wc++20-compat" // Things like T {.field1 = value1, .field2 = value2}

#else // BAIDU_PROTECT_H
#error("protect.h without unprotect.h")
#endif // BAIDU_PROTECT_H
