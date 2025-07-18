#pragma once

#include "babylon/environment.h"

#include <string>
#include <tuple>

BABYLON_NAMESPACE_BEGIN

// 增强了absl::strings_internal::STLStringResizeUninitialized的能力
// 除了libc++以外，增加了对libstdc++的支持
// 增强通过STLStringResizeUninitialized模板的特化实现
// 包含此头文件即可生效
//
// 由于在absl中属于内部功能，未包含函数功能说明，这里提供一些功能说明
// 执行类似于std::string::resize的操作
// 特别之处在于【尽可能】不对新扩充部分进行默认值填充
// 在作为缓冲区使用的情况下，默认值填充并非必要，省去可以提升性能
//
// 对于std::string类型，提供了内置的实现
// 对于其他『像string』的类型，通过探测T::__resize_default_init
// 成员存在性来选择性调用，这个成员也是libc++提供的特殊隐藏函数的功能名
//
// 典型用法如下
// std::string s;
// absl::strings_internal::STLStringResizeUninitialized(s, 128);
// memcpy(&s[0], some_src_data, 128);

// 保留之前的使用方法，内部实际调用了
// absl::strings_internal::STLStringResizeUninitialized
template <typename T>
inline typename T::pointer resize_uninitialized(
    T& string, typename T::size_type size) noexcept;

// 执行类似于std::string::reserve的操作，主要用于支持『保留容量重建』场景
// 大多情况下直接使用std::string::reserve，但对于有些实现，持续进行
// capacity -> reserve -> capacity -> ...
// 循环，会因为容量预测导致容量持续膨胀，专门包装一个版本确保循环安全
template <typename T>
inline void stable_reserve(T& string,
                           typename T::size_type min_capacity) noexcept;

#if __GLIBCXX__ && _GLIBCXX_USE_CXX11_ABI
inline ::std::tuple<char*, size_t> exchange_string_buffer(
    ::std::string& string, char* buffer, size_t buffer_size) noexcept;
#endif // __GLIBCXX__ && _GLIBCXX_USE_CXX11_ABI

BABYLON_NAMESPACE_END

#include "babylon/string.hpp"
