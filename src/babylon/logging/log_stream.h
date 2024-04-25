#pragma once

#include "babylon/type_traits.h"    // BABYLON_DECLARE_MEMBER_INVOCABLE

#include BABYLON_EXTERNAL(absl/strings/str_format.h)                    // absl::Format

#include <inttypes.h>                                   // ::*int*_t

BABYLON_NAMESPACE_BEGIN

// 典型的日志前端接口，用于流式格式化一行日志，并发送给日志框架
// 通过如下方式使用
// ls.begin(args...);       // 开启一个日志行，args作为日志头依次预先写入
// ls << ...;               // 增量写入其他内容
// ls.end();                // 整体发送给日志框架
//
// 中途可以使用noflush挂起即
// ls.begin(args...);
// ls << ... << noflush;    // 挂起
// ls.end();                // 此时不发送给日志框架
// ls.begin(args...);       // 恢复挂起，且不再次打印日志头
// ls << ...;               // 继续增量写入其他内容
// ls.end();                // 作为一条日志发送给日志框架
//
// 除流式输出，也通过集成absl::Format，支持printf语法
// ls.begin(args...);
// ls.format("...", ...).format("...", ...);
// ls.end();
class LogStream : protected ::std::ostream {
private:
    using Base = ::std::ostream;

    BABYLON_DECLARE_MEMBER_INVOCABLE(write_object, IsDirectWritable)

public:
    using Base::rdbuf;

    inline LogStream(::std::streambuf& streambuf) noexcept;

    template <typename... Args>
    inline LogStream& begin(const Args&... args) noexcept;
    inline LogStream& noflush() noexcept;
    inline LogStream& end() noexcept;

    // 写入一段无格式数据
    inline LogStream& write(const char* data, size_t size) noexcept;
    inline LogStream& write(char c) noexcept;

    // 使用类printf语法格式化输出，例如
    // format("%s %s", "hello", "world");
    template <typename... Args>
    inline LogStream& format(const ::absl::FormatSpec<Args...>& format,
                             const Args&... args) noexcept;

    // 内置类型绕过std::ostream层直接输出
    template <typename T, typename ::std::enable_if<
        IsDirectWritable<LogStream, T>::value, int>::type = 0>
    inline LogStream& operator<<(const T& object) noexcept;

    // 支持基于std::ostream的自定义类型扩展
    template <typename T, typename ::std::enable_if<
        !IsDirectWritable<LogStream, T>::value, int>::type = 0>
    inline LogStream& operator<<(const T& object) noexcept;

    // 支持基于std::ostream的流操作符
    inline LogStream& operator<<(
            ::std::ostream& (*function)(::std::ostream&)) noexcept;

private:
    // 支持absl::Format
    friend inline void AbslFormatFlush(LogStream* ls,
                                       ::absl::string_view sv) noexcept {
        ls->write(sv.data(), sv.size());
    }

    // 早期abseil不支持ADL函数查找约定，降级到std::ostream
    template <typename T, typename... Args, typename ::std::enable_if<
        ::std::is_convertible<
            T*, ::absl::FormatRawSink>::value, int>::type = 0>
    inline static LogStream& do_absl_format(
            T* ls, const ::absl::FormatSpec<Args...>& format,
            const Args&... args) noexcept;
    template <typename T, typename... Args, typename ::std::enable_if<
        !::std::is_convertible<
            T*, ::absl::FormatRawSink>::value, int>::type = 0>
    inline static LogStream& do_absl_format(
            T* ls, const ::absl::FormatSpec<Args...>& format,
            const Args&... args) noexcept;

    template <typename T, typename... Args>
    inline void write_objects(const T& object, const Args&... args) noexcept;
    inline void write_objects() noexcept;

    inline LogStream& write_object(StringView sv) noexcept;
    inline LogStream& write_object(char c) noexcept;

#define BABYLON_TMP_GEN_WRITE(ctype, fmt) \
    inline LogStream& write_object(ctype v) noexcept { \
        return format("%" fmt, v); \
    }
    BABYLON_TMP_GEN_WRITE(int16_t, PRId16)
    BABYLON_TMP_GEN_WRITE(int32_t, PRId32)
    BABYLON_TMP_GEN_WRITE(int64_t, PRId64)
    BABYLON_TMP_GEN_WRITE(uint8_t, PRIu8)
    BABYLON_TMP_GEN_WRITE(uint16_t, PRIu16)
    BABYLON_TMP_GEN_WRITE(uint32_t, PRIu32)
    BABYLON_TMP_GEN_WRITE(uint64_t, PRIu64)
    BABYLON_TMP_GEN_WRITE(float, "g")
    BABYLON_TMP_GEN_WRITE(double, "g")
    BABYLON_TMP_GEN_WRITE(long double, "Lg")
#undef BABYLON_TMP_GEN_WRITE

    virtual void do_begin() noexcept;
    virtual void do_end() noexcept;

    size_t _depth {0};
    bool _noflush {false};
};

BABYLON_NAMESPACE_END

#include "babylon/logging/log_stream.hpp"
