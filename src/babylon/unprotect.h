#ifndef BABYLON_UNPROTECT_H
#define BABYLON_UNPROTECT_H

#ifndef BABYLON_PROTECT_H
#error("unprotect.h without protect.h")
#else
#undef BABYLON_PROTECT_H
#endif // BABYLON_PROTECT_H

#undef BABYLON_UNPROTECT_H

// 有些库使用了名称过于通用的全局宏定义，会干扰代码编译
// 采用成对的push_macro/pop_marco进行环境清理，保证中间段落编译正常
// 需要成对使用，例如
// #include <babylon/protect.h>
// ... 中间代码不受相应宏定义影响
// #include <babylon/unprotect.h>

// 例如brpc，使用了-Dprivate=public的方式来实现访问私有成员的trick
// 会影响<sstream>等其他库的编译
#pragma pop_macro("private")

// 例如ps/se/vhtmlparser中定义了BLOCK_SIZE，容易产生干扰
#pragma pop_macro("BLOCK_SIZE")

// 例如baidu/feed-mlarch/feature-extract-framework中定义了F
#pragma pop_macro("F")

#pragma GCC diagnostic pop

#endif // BABYLON_UNPROTECT_H
