BABYLON_GCC_COPTS = ['-Wall', '-Wextra']
BABYLON_CLANG_COPTS = ['-faligned-new', '-Weverything', '-Wno-unknown-warning-option',
                       # 不保持老版本c++语法兼容
                       '-Wno-c++98-compat-pedantic', '-Wno-c99-designator',
                       # Boost Preprocessor中大量使用
                       '-Wno-disabled-macro-expansion',
                       # 未定义宏默认为0作为一个惯例特性保留使用
                       '-Wno-undef',
                       # 类内部的对齐空隙大体是无害的，而且比较难完全消除
                       '-Wno-padded',
                       # 常规的隐式转换一般是可控的
                       '-Wno-implicit-int-conversion', '-Wno-implicit-float-conversion', '-Wno-double-promotion', '-Wno-shorten-64-to-32',
                       # 静态初始化是注册器模式和Google Test使用的典型方案
                       '-Wno-global-constructors',
                       # 采用前置下划线方案表达内部宏是一个典型实践方案
                       '-Wno-reserved-id-macro', '-Wno-reserved-identifier',
                       # __VA_ARGS__扩展作为c++20之前的通用方案继续保留使用
                       '-Wno-gnu-zero-variadic-macro-arguments',
                       # 零长度数组用来做可变长结构是一种常用手段
                       '-Wno-zero-length-array',
                       # TODO(lijiang01): 逐步梳理清除
                       '-Wno-old-style-cast', '-Wno-shadow-field',
                       '-Wno-exit-time-destructors', '-Wno-sign-conversion',
                       '-Wno-shadow-field-in-constructor', '-Wno-gnu-anonymous-struct', '-Wno-nested-anon-types',
                       '-Wno-shadow-uncaptured-local', '-Wno-weak-vtables', '-Wno-float-conversion', '-Wno-switch-enum',
                       '-Wno-shadow', '-Wno-array-bounds-pointer-arithmetic', '-Wno-cast-align', '-Wno-vla-extension',
                       '-Wno-unneeded-member-function', '-Wno-deprecated-declarations']

BABYLON_COPTS = select({
    '//:compiler_gcc': BABYLON_GCC_COPTS,
    '//:compiler_clang': BABYLON_CLANG_COPTS,
    '//conditions:default': BABYLON_GCC_COPTS,
}) + select({
    '//:enable_werror': ['-Werror', '-DBABYLON_INCLUDE_EXTERNAL_AS_SYSTEM=1'],
    '//conditions:default': [],
})
