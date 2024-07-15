#pragma once

#include "babylon/logging/log_severity.h" // LogSeverity
#include "babylon/logging/log_stream.h"   // LogStream
#include "babylon/string_view.h"          // StringView

BABYLON_NAMESPACE_BEGIN

// 将日志系统接口抽象描述为一个日志流提供者
// 用于将BABYLON_LOG对接到不同的日志系统后端
//
// 接口参数包含最基础的日志等级，文件名和行号
// 日志系统可以通过这些参数进行输出流选择，以及日志头格式化
// 获得日志流的生命周期由日志系统内部管理
//
// 典型的使用场景示意如下
// auto& stream = provider.stream(severity, __FILE__, __LINE__);
// stream.begin(...); // 日志头
// stream << ...;     // 日志体
// stream.end();      // 日志输出
// 一般通过日志宏来包装这个过程
class LogStreamProvider {
 public:
  virtual ~LogStreamProvider() noexcept;

  // 根据日志等级，文件名和行号获得日志流
  virtual LogStream& stream(int severity, StringView file,
                            int line) noexcept = 0;
};

// 用于设置和访问日志系统的接口层
class LogInterface {
 public:
  static constexpr int SEVERITY_DEBUG = static_cast<int>(LogSeverity::DEBUG);
  static constexpr int SEVERITY_INFO = static_cast<int>(LogSeverity::INFO);
  static constexpr int SEVERITY_WARNING =
      static_cast<int>(LogSeverity::WARNING);
  static constexpr int SEVERITY_FATAL = static_cast<int>(LogSeverity::FATAL);
  static constexpr int SEVERITY_NUM = static_cast<int>(LogSeverity::NUM);

  // 设置最低日志等级，默认为>=INFO级别
  static void set_min_severity(int severity) noexcept;
  inline static int min_severity() noexcept;

  // 设置底层日志系统的日志流提供者，默认为输出到stderr
  // 也可以通过自定义LogInterface::_s_provider弱符号修改默认值
  static void set_provider(
      ::std::unique_ptr<LogStreamProvider>&& provider) noexcept;
  inline static LogStreamProvider& provider() noexcept;

 private:
  static int _s_min_severity;
  static LogStreamProvider* _s_provider;
};

////////////////////////////////////////////////////////////////////////////////
// LogInterface begin
inline int LogInterface::min_severity() noexcept {
  return _s_min_severity;
}

inline LogStreamProvider& LogInterface::provider() noexcept {
  return *_s_provider;
}
// LogInterface end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
