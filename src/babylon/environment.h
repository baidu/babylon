#pragma once

// 限定仅支持兼容GNU C的编译器和clang
// 单独列出clang是因为较新的clang可以关闭对GNU C编译器的宏模拟
#if !defined __GNUC__ && !defined __clang__
#error("support clang or gnu-like compiler only")
#endif

// 限定支持c++且至少启用了-std=c++11
#if __cplusplus < 201103L
#error("support at least -std=c++11")
#endif

// 限定支持x86和aarch64体系架构
#if !__x86_64__ && !__aarch64__
#error("support __x86_64__ and __aarch64__ only")
#endif

// 缓存行大小，指导一些并发结构的对齐原则
// 理论上，『c++17』之后的
// std::hardware_destructive_interference_size
// std::hardware_constructive_interference_size
// 能够更好的刻画这一行为
//
// 但是由于
// - 目前编译器对c++17的支持都并不完成，这一特性当前版本基本均未实现
// - 较新的gcc中实现了这一特性，但是并未考虑Spatial Prefetcher的特殊影响
// - 主流服务端cpu的缓存行大小相对固定，暂时按体系结构枚举基本足够
//
// Spatial Prefetcher相关特性记载在intel
// 64-ia-32-architectures-optimization-manual.pdf 中的E.2.5.4 Data
// Prefetching部分
//
// 综上，目前应用中还是采用单独的宏来提供
#ifdef __x86_64__
#define BABYLON_CACHELINE_SIZE 128
#else // !__x86_64__
#define BABYLON_CACHELINE_SIZE 64
#endif // !__x86_64__

// GNU C编译器版本，或GNU C兼容编译器的兼容版本
// 例如gcc-12.1.0 => 120100
// 注意，由于GNU C兼容的编译器都会定义此版本信息
// 因此并不能标识使用的编译器就是gcc的某个版本
// 例如，即使高版本clang，也只一般会将自己的版本定义为gcc-4.2.1
//
// 因此根据某些编译器特性进行兼容编码时往往需要按照
// #if BABYLON_GCC_VERSION > X || CLANG_VERSION > Y
// 的方式来指定，而且尤其需要注意避免使用
// #if BABYLON_GCC_VERSION < X
// 的方式来试图判定『不具备』某种能力
// 因为其他GNU C兼容的编译器往往不能通过此判定
//
// 另外这只是判断『编译器』版本的方式
// 如果希望针对libstdc++/libc++等标准库版本进行兼容适配
// 需要采用GLIBCXX_VERSION/LIBCPP_VERSION来完成
#ifdef __GNUC__
#define BABYLON_GCC_VERSION \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif // __GNUC__
#ifndef GCC_VERSION
#define GCC_VERSION BABYLON_GCC_VERSION
#endif // GCC_VERSION

// clang编译器声明的GNU C兼容版本基本是固定不变的gcc-4.2.1
// 因此其编译器特性一般需要结合自己独立的版本号体系来判定
#ifdef __clang__
#define CLANG_VERSION \
  (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#endif // __clang__

// 头文件的存在性是用来判断是否具备某些特性的典型手段
// 尤其对于一些不具有标准版本号体系的代码，例如abseil-cpp
// 但古老的编译器不支持__has_include预处理宏，为了兼容需要双重嵌套实现
// #ifdef __has_include
// #if __has_include(...)
// ...
// #endif
// #endif
// 注意，由于宏函数的特性，下面的单行表达式是无法正确工作的
// #if defined __has_include && __has_include(...)
// ...
// #endif
// 在特性检测场景下，往往『无法检测』和『检测不具备条件』是可以等同处理的
// 因此专门提供宏来满足这类场景的简化书写
#ifdef __has_include
#define BABYLON_HAS_INCLUDE(header) __has_include(header)
#else // !__has_include
#define BABYLON_HAS_INCLUDE(header) false
#endif // !__has_include
#ifdef __has_feature
#define BABYLON_HAS_FEATURE(feature) __has_feature(feature)
#else // !__has_feature
#define BABYLON_HAS_FEATURE(feature) false
#endif // !__has_feature

// c++14/c++17对constexpr进行了升级，有些场景只有在c++14/c++17后才能够constexpr
// 制作对应的辅助宏来帮助
#if __cplusplus >= 201703L
#define CONSTEXPR_SINCE_CXX17 constexpr
#define CONSTEXPR_SINCE_CXX14 constexpr
#elif __cplusplus >= 201402L // __cplusplus < 201703L
#define CONSTEXPR_SINCE_CXX17
#define CONSTEXPR_SINCE_CXX14 constexpr
#else // __cplusplus < 201402L
#define CONSTEXPR_SINCE_CXX17
#define CONSTEXPR_SINCE_CXX14
#endif // __cplusplus < 201402L

// 通过『任意』引入一个c++头文件来引入一些基础环境信息，例如
// - GLIBCXX/LIBCPP标准库版本信息
// - _GLIBCXX_USE_CXX11_ABI开关信息
// 选择cstddef是因为其本体只包含少量宏定义，相对比较轻量
#include <cstddef>

// 用于确定是否使用libstdc++，以及对应的版本
// GLIBCXX_VERSION采用大版本号 +
// 发布日期来做版本确定，参考https://gcc.gnu.org/develop.html#timeline
// >=420140522L == >=gcc4.8.3
// >=720191114L == >=gcc7.5.0
// >=820180726L == >=gcc8.2.0
// >=920210601L == >=gcc9.4.0
// >=1020200507L == >=gcc10.1.0
// >=1220220506L == >=gcc12.1.0
#ifdef __GLIBCXX__
// 从GLIBCXX 7起定义了_GLIBCXX_RELEASE，来表示大版本号，直接拼接即可
#ifdef _GLIBCXX_RELEASE
#define GLIBCXX_VERSION (_GLIBCXX_RELEASE * 100000000UL + __GLIBCXX__)
// 对于未定义_GLIBCXX_RELEASE的老版本GNU C，统一认定为4，作为历史兼容已经足够
#else // !_GLIBCXX_RELEASE
#define GLIBCXX_VERSION (4 * 100000000UL + __GLIBCXX__)
#endif // !_GLIBCXX_RELEASE
#endif // __GLIBCXX__

// 抽离出来，方便快速调整整个库的名空间
#define BABYLON_NAMESPACE_BEGIN namespace babylon {
#define BABYLON_NAMESPACE_END }

#define BABYLON_COROUTINE_NAMESPACE_BEGIN \
  BABYLON_NAMESPACE_BEGIN                 \
  namespace coroutine {
#define BABYLON_COROUTINE_NAMESPACE_END \
  }                                     \
  BABYLON_NAMESPACE_END

// 早期arena patch版本
// - protobuf_3-2-1-8_PD_BL
// - protobuf_3-2-1-9_PD_BL
// - protobuf_3-10-1-20_PD_BL
// 不支持c++17，禁用依赖protobuf的功能
#if BABYLON_HAS_INCLUDE("google/protobuf/patch_std_string_for_arena.h")
#include "google/protobuf/patch_std_string_for_arena.h"
#if __cplusplus >= 201703L && \
    (!defined(PROTOBUF_PATCH_STD_STRING) || PROTOBUF_PATCH_STD_STRING)
#define BABYLON_USE_PROTOBUF 0
#endif
#endif
#ifndef BABYLON_USE_PROTOBUF
#define BABYLON_USE_PROTOBUF 1
#endif

#if BABYLON_HAS_INCLUDE(<version>)
#include <version> // __cpp_xxxx
#endif             // BABYLON_HAS_INCLUDE(<version>)

// clang-format off
// 在bazel环境启用treat_warnings_as_errors
// bazel的external_include_paths模式会对依赖库
#if BABYLON_INCLUDE_EXTERNAL_AS_SYSTEM
#define BABYLON_EXTERNAL(file) <file>
#else // !BABYLON_HAS_INCLUDE(<absl/base/config.h>)
#define BABYLON_EXTERNAL(file) #file
#endif // !BABYLON_HAS_INCLUDE(<absl/base/config.h>)
// clang-format on

#if __has_attribute(init_priority) && \
    (!defined(__clang__) || CLANG_VERSION >= 140000L)
#define BABYLON_HAS_INIT_PRIORITY 1
#define BABYLON_INIT_PRIORITY(priority) __attribute__((init_priority(priority)))
#else
#define BABYLON_HAS_INIT_PRIORITY 0
#define BABYLON_INIT_PRIORITY(priority)
#endif

#if defined(__SANITIZE_ADDRESS__)
#define BABYLON_HAS_ADDRESS_SANITIZER 1
#elif BABYLON_HAS_FEATURE(address_sanitizer)
#define BABYLON_HAS_ADDRESS_SANITIZER 1
#else
#define BABYLON_HAS_ADDRESS_SANITIZER 0
#endif
