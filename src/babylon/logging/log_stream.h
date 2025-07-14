#pragma once

#include "babylon/logging/log_severity.h" // LogSeverity
#include "babylon/type_traits.h"          // BABYLON_DECLARE_MEMBER_INVOCABLE

// clang-format off
#include BABYLON_EXTERNAL(absl/strings/str_format.h) // absl::Format
// clang-format on

#include <inttypes.h> // ::*int*_t

#include <mutex> // std::mutex

BABYLON_NAMESPACE_BEGIN

// 典型的日志前端接口，用于流式格式化一行日志，并发送给日志框架
// 通过如下方式使用
// ls.begin(args...); // 开启一个日志行，args作为日志头依次预先写入
// ls << ...;         // 增量写入其他内容
// ls.end();          // 整体发送给日志框架
//
// 中途可以使用noflush挂起即
// ls.begin(args...);
// ls << ... << ::babylon::noflush; // 挂起
// ls.end();                        // 此时不发送给日志框架
// ls.begin(args...);               // 恢复挂起，且不再次打印日志头
// ls << ...;                       // 继续增量写入其他内容
// ls.end();                        // 作为一条日志发送给日志框架
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

  inline void set_severity(LogSeverity severity) noexcept;
  inline LogSeverity severity() const noexcept;

  inline void set_file(StringView file) noexcept;
  inline StringView file() const noexcept;

  inline void set_line(int line) noexcept;
  inline int line() const noexcept;

  inline void set_function(StringView function) noexcept;
  inline StringView function() const noexcept;

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
  template <typename T,
            typename ::std::enable_if<IsDirectWritable<LogStream, T>::value,
                                      int>::type = 0>
  inline LogStream& operator<<(const T& object) noexcept;

  // 支持基于std::ostream的自定义类型扩展
  template <typename T,
            typename ::std::enable_if<!IsDirectWritable<LogStream, T>::value,
                                      int>::type = 0>
  inline LogStream& operator<<(const T& object) noexcept;

  // 支持基于std::ostream的流操作符
  inline LogStream& operator<<(
      ::std::ostream& (&function)(::std::ostream&)) noexcept;
  inline LogStream& operator<<(LogStream& (&function)(LogStream&)) noexcept;

 private:
  // 支持absl::Format
  friend inline void AbslFormatFlush(LogStream* ls,
                                     ::absl::string_view sv) noexcept {
    ls->write(sv.data(), sv.size());
  }

  // 早期abseil不支持ADL函数查找约定，降级到std::ostream
  template <typename T, typename... Args,
            typename ::std::enable_if<
                ::std::is_convertible<T*, ::absl::FormatRawSink>::value,
                int>::type = 0>
  inline static LogStream& do_absl_format(
      T* ls, const ::absl::FormatSpec<Args...>& format,
      const Args&... args) noexcept;
  template <typename T, typename... Args,
            typename ::std::enable_if<
                !::std::is_convertible<T*, ::absl::FormatRawSink>::value,
                int>::type = 0>
  inline static LogStream& do_absl_format(
      T* ls, const ::absl::FormatSpec<Args...>& format,
      const Args&... args) noexcept;

  template <typename T, typename... Args>
  inline void write_objects(const T& object, const Args&... args) noexcept;
  inline void write_objects() noexcept;

  inline LogStream& write_object(StringView sv) noexcept;
  inline LogStream& write_object(char c) noexcept;

#define BABYLON_TMP_GEN_WRITE(ctype, fmt)            \
  inline LogStream& write_object(ctype v) noexcept { \
    return format("%" fmt, v);                       \
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
  LogSeverity _severity {LogSeverity::DEBUG};
  int _line {-1};
  StringView _file;
  StringView _function;
};

// 便于实现LOG宏的RAII控制器
class ScopedLogStream {
 public:
  ScopedLogStream() = delete;
  ScopedLogStream(ScopedLogStream&&) = delete;
  ScopedLogStream(const ScopedLogStream&) = delete;
  ScopedLogStream& operator=(ScopedLogStream&&) = delete;
  ScopedLogStream& operator=(const ScopedLogStream&) = delete;
  inline ~ScopedLogStream() noexcept;

  template <typename... Args>
  inline ScopedLogStream(LogStream& stream, Args&&... args) noexcept;

  inline LogStream& stream() noexcept;

 private:
  LogStream& _stream;
};

class DefaultLogStream : public LogStream {
 public:
  DefaultLogStream() noexcept;

 private:
  static ::std::mutex& mutex() noexcept;

  virtual void do_begin() noexcept override;
  virtual void do_end() noexcept override;
};

class NullLogStream : public LogStream {
 public:
  NullLogStream() noexcept;

 private:
  class Buffer;

  static Buffer& buffer() noexcept;
};

////////////////////////////////////////////////////////////////////////////////
// LogStream begin
inline void LogStream::set_severity(LogSeverity severity) noexcept {
  _severity = severity;
}

inline LogSeverity LogStream::severity() const noexcept {
  return _severity;
}

inline void LogStream::set_file(StringView file) noexcept {
  _file = file;
}

inline StringView LogStream::file() const noexcept {
  return _file;
}

inline void LogStream::set_line(int line) noexcept {
  _line = line;
}

inline int LogStream::line() const noexcept {
  return _line;
}

inline void LogStream::set_function(StringView function) noexcept {
  _function = function;
}

inline StringView LogStream::function() const noexcept {
  return _function;
}

template <typename... Args>
inline LogStream& LogStream::begin(const Args&... args) noexcept {
  if (ABSL_PREDICT_FALSE(++_depth != 1)) {
    return *this;
  }
  if (!_noflush) {
    do_begin();
    write_objects(args...);
  } else {
    _noflush = false;
  }
  return *this;
}

inline LogStream& LogStream::noflush() noexcept {
  if (ABSL_PREDICT_FALSE(_depth != 1)) {
    return *this;
  }
  _noflush = true;
  return *this;
}

inline LogStream& LogStream::end() noexcept {
  if (ABSL_PREDICT_FALSE(--_depth > 0)) {
    return *this;
  }
  if (!_noflush) {
    do_end();
    clear();
  }
  return *this;
}

inline LogStream& LogStream::write(const char* data, size_t size) noexcept {
  rdbuf()->sputn(data, static_cast<ssize_t>(size));
  return *this;
}

inline LogStream& LogStream::write(char c) noexcept {
  rdbuf()->sputc(c);
  return *this;
}

template <typename... Args>
inline LogStream& LogStream::format(const ::absl::FormatSpec<Args...>& format,
                                    const Args&... args) noexcept {
  return do_absl_format(this, format, args...);
}

template <
    typename T, typename... Args,
    typename ::std::enable_if<
        ::std::is_convertible<T*, ::absl::FormatRawSink>::value, int>::type>
inline LogStream& LogStream::do_absl_format(
    T* ls, const ::absl::FormatSpec<Args...>& format,
    const Args&... args) noexcept {
  ::absl::Format(ls, format, args...);
  return *ls;
}

template <
    typename T, typename... Args,
    typename ::std::enable_if<
        !::std::is_convertible<T*, ::absl::FormatRawSink>::value, int>::type>
inline LogStream& LogStream::do_absl_format(
    T* ls, const ::absl::FormatSpec<Args...>& format,
    const Args&... args) noexcept {
  ::absl::Format(static_cast<::std::ostream*>(ls), format, args...);
  return *ls;
}

template <typename T,
          typename ::std::enable_if<
              LogStream::IsDirectWritable<LogStream, T>::value, int>::type>
inline LogStream& LogStream::operator<<(const T& object) noexcept {
  return write_object(object);
}

template <typename T,
          typename ::std::enable_if<
              !LogStream::IsDirectWritable<LogStream, T>::value, int>::type>
inline LogStream& LogStream::operator<<(const T& object) noexcept {
  *static_cast<::std::ostream*>(this) << object;
  return *this;
}

inline LogStream& LogStream::operator<<(
    ::std::ostream& (&function)(::std::ostream&)) noexcept {
  function(*this);
  return *this;
}

inline LogStream& LogStream::operator<<(
    LogStream& (&function)(LogStream&)) noexcept {
  return function(*this);
}

inline LogStream::LogStream(::std::streambuf& streambuf) noexcept
    : ::std::ostream(&streambuf) {}

template <typename T, typename... Args>
inline void LogStream::write_objects(const T& object,
                                     const Args&... args) noexcept {
  *this << object;
  write_objects(args...);
}

inline void LogStream::write_objects() noexcept {}

inline LogStream& LogStream::write_object(StringView sv) noexcept {
  return write(sv.data(), sv.size());
}

inline LogStream& LogStream::write_object(char c) noexcept {
  return write(c);
}
// LogStream end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ScopedLogStream begin
inline ScopedLogStream::~ScopedLogStream() noexcept {
  _stream.end();
}

template <typename... Args>
inline ScopedLogStream::ScopedLogStream(LogStream& stream,
                                        Args&&... args) noexcept
    : _stream {stream} {
  _stream.begin(::std::forward<Args>(args)...);
}

inline LogStream& ScopedLogStream::stream() noexcept {
  return _stream;
}
// ScopedLogStream end
////////////////////////////////////////////////////////////////////////////////

inline LogStream& noflush(LogStream& stream) {
  return stream.noflush();
}

BABYLON_NAMESPACE_END
