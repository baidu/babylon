#include "babylon/logging/interface.h"

#include "babylon/logging/logger.h"

#include <iostream> // std::cerr
#include <mutex>    // std::mutex

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// LogStreamProvider begin
LogStreamProvider::~LogStreamProvider() noexcept {}
// LogStreamProvider end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// DefaultLogStreamProvider begin
class DefaultLogStreamProvider : public LogStreamProvider {
 private:
  static constexpr StringView SEVERITY_NAME[] = {
      "DEBUG ",
      "INFO ",
      "WARNING ",
      "FATAL ",
  };

 public:
  virtual LogStream& stream(int severity, StringView file,
                            int line) noexcept override {
    static thread_local DefaultLogStream stream;
    stream.set_severity(static_cast<LogSeverity>(severity));
    stream.set_file(file);
    stream.set_line(line);
    return stream;
  }
};
#if __cplusplus < 201703L
constexpr StringView DefaultLogStreamProvider::SEVERITY_NAME[];
#endif // __cplusplus < 201703L
static DefaultLogStreamProvider s_default_provider;
// DefaultLogStreamProvider end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// LogInterface begin
void LogInterface::set_min_severity(int severity) noexcept {
  _s_min_severity = severity;
  LoggerManager::instance().get_root_logger().set_min_severity(
      static_cast<LogSeverity>(severity));
}

void LogInterface::set_provider(
    ::std::unique_ptr<LogStreamProvider>&& provider) noexcept {
  static ::std::unique_ptr<LogStreamProvider> s_provider;
  s_provider = ::std::move(provider);
  if (s_provider) {
    _s_provider = s_provider.get();
  } else {
    _s_provider = &s_default_provider;
  }
}

// TODO(lijiang01): 理论上应该写成
// int LogInterface::_s_min_severity {LogInterface::SEVERITY_INFO};
// 但是在多编译单元-std=不统一时，会因为inline与否造成各种链接问题
// 暂时采用非符号的常量手段尽可能规避
int LogInterface::_s_min_severity {1};

// 设置为弱符号，用来支持应用层定制其默认日志后端
// 尽管一般而言通过LogInterface::set_provider已经可以实现自定义功能
// 但对于一些希望在静态初始化就打印日志的场景，其自定义动作希望更早发生
// 此时通过链接时符号替换可以提供更早的时机
ABSL_ATTRIBUTE_WEAK LogStreamProvider* LogInterface::_s_provider {
    &s_default_provider};
// LogInterface end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
