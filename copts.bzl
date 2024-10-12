BABYLON_GCC_COPTS = ['-Wall', '-Wextra']
BABYLON_CLANG_COPTS = ['-Weverything', '-Wno-unknown-warning-option',
                       # 不保持老版本c++语法兼容
                       '-Wno-c++98-compat-pedantic',
                       # Boost Preprocessor中大量使用
                       '-Wno-disabled-macro-expansion',
                       # 未定义宏默认为0作为一个惯例特性保留使用
                       '-Wno-undef',
                       # 类内部的对齐空隙大体是无害的，而且比较难完全消除
                       '-Wno-padded',
                       # 常规的隐式转换一般是可控的
                       '-Wno-implicit-int-conversion', '-Wno-implicit-float-conversion', '-Wno-double-promotion', '-Wno-shorten-64-to-32',
                       # 采用前置下划线方案表达内部宏是一个典型实践方案
                       '-Wno-reserved-id-macro', '-Wno-reserved-identifier',
                       # __VA_ARGS__扩展作为c++20之前的通用方案继续保留使用
                       '-Wno-gnu-zero-variadic-macro-arguments',
                       # 零长度数组用来做可变长结构是一种常用手段
                       '-Wno-zero-length-array',
                       # BABYLON_SERIALIZABLE宏内部定义的一些辅助成员在继承场景下会发生同名隐藏
                       # 但是这个场景下的隐藏本身是无害的，目前也还没有很好的方法做命名区分
                       '-Wno-shadow-field',
                       # 采用和gcc更一致的-Wswitch-default风格
                       '-Wno-covered-switch-default',
                       # TODO(lijiang01): 逐步梳理清除
                       '-Wno-weak-vtables', '-Wno-float-conversion', '-Wno-switch-enum', '-Wno-c++17-extensions',
                       '-Wno-gnu-anonymous-struct', '-Wno-nested-anon-types',
                       '-Wno-array-bounds-pointer-arithmetic', '-Wno-cast-align', '-Wno-vla-extension',
                       '-Wno-unneeded-member-function', '-Wno-deprecated-declarations', '-Wno-unsafe-buffer-usage']

BABYLON_COPTS = select({
    '//:compiler_gcc': BABYLON_GCC_COPTS,
    '//:compiler_clang': BABYLON_CLANG_COPTS,
    '//conditions:default': BABYLON_GCC_COPTS,
}) + select({
    '//:enable_werror': ['-Werror', '-DBABYLON_INCLUDE_EXTERNAL_AS_SYSTEM=1'],
    '//conditions:default': [],
})

BABYLON_TEST_COPTS = BABYLON_COPTS + select({
    '//:compiler_gcc': [],
    '//:compiler_clang': [
      # 静态初始化是注册器模式和Google Test使用的典型方案
      '-Wno-global-constructors',
      # 静态变量的析构顺序可能引起的问题对于单测本身并无大碍，且可以简化单测实现
      '-Wno-exit-time-destructors',
      # 跨符号转换对单测自身并无大碍，且可以简化单测实现
      '-Wno-sign-conversion',
      # googletest-1.15开始使用了c++17属性
      '-Wno-c++17-attribute-extensions',
      # 单测中使用了一些变量简写有意进行了无害隐藏
      '-Wno-shadow',
    ],
    '//conditions:default': [],
})
