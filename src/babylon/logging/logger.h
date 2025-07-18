#pragma once

#include "babylon/concurrent/thread_local.h" // babylon::EnumerableThreadLocal
#include "babylon/concurrent/transient_hash_table.h" // babylon::ConcurrentTransientHashMap
#include "babylon/logging/interface.h"               // babylon::LogInterface
#include "babylon/logging/log_severity.h"            // babylon::LogSeverity
#include "babylon/logging/log_stream.h"              // babylon::LogStream

#include <array> // std::array
#include <functional>

BABYLON_NAMESPACE_BEGIN

class LoggerBuilder;
class LoggerManager;
class Logger final {
 public:
  Logger() noexcept;
  ~Logger() noexcept = default;

  inline bool initialized() const noexcept;
  inline LogSeverity min_severity() const noexcept;
  LogStream& stream(LogSeverity severity, StringView file, int line,
                    StringView function) noexcept;

 private:
  using ThreadLocalLogStream =
      EnumerableThreadLocal<::std::unique_ptr<LogStream>>;
  using Storage = ::std::array<Logger::ThreadLocalLogStream, LogSeverity::NUM>;
  using PointerStorage =
      ::std::array<::std::atomic<ThreadLocalLogStream*>, LogSeverity::NUM>;

  Logger(const Logger& other) noexcept;
  Logger& operator=(const Logger& other) noexcept;

  void set_initialized(bool initialized) noexcept;
  void set_min_severity(LogSeverity min_severity) noexcept;
  void set_log_stream(LogSeverity severity,
                      ThreadLocalLogStream& log_stream) noexcept;

  PointerStorage _log_streams;
  LogSeverity _min_severity;
  bool _initialized;

  friend class LogInterface;
  friend class LoggerBuilder;
  friend class LoggerManager;
};

class LoggerBuilder final {
 public:
  using Creator = ::std::function<::std::unique_ptr<LogStream>()>;

  LoggerBuilder() noexcept;

  Logger build() noexcept;

  void set_log_stream_creator(Creator creator) noexcept;
  void set_log_stream_creator(LogSeverity severity, Creator creator) noexcept;

  void set_min_severity(LogSeverity min_severity) noexcept;

 private:
  using Storage =
      ::std::array<::std::pair<LogSeverity, Logger::ThreadLocalLogStream>,
                   LogSeverity::NUM>;
  Storage _log_streams;
  LogSeverity _min_severity;
};

class LoggerManager final {
 public:
  LoggerManager(LoggerManager&&) = delete;
  LoggerManager(const LoggerManager&) = delete;
  LoggerManager& operator=(LoggerManager&&) = delete;
  LoggerManager& operator=(const LoggerManager&) = delete;

  static LoggerManager& instance() noexcept;

  void set_root_builder(LoggerBuilder&& logger) noexcept;
  void set_builder(StringView name, LoggerBuilder&& logger) noexcept;
  void apply() noexcept;
  void clear() noexcept;

  inline Logger& get_root_logger() noexcept;
  Logger& get_logger(StringView name) noexcept;

 private:
  LoggerManager() noexcept;
  ~LoggerManager() noexcept = default;

  void apply_to(StringView name, Logger& logger) noexcept;
  LoggerBuilder* find_nearest_builder(StringView name) noexcept;

  Logger _root_logger;
  ConcurrentTransientHashMap<::std::string, Logger, ::std::hash<StringView>>
      _loggers;

  ::std::mutex _builder_mutex;
  ::std::unique_ptr<LoggerBuilder> _root_builder;
  ConcurrentTransientHashMap<::std::string, LoggerBuilder,
                             ::std::hash<StringView>>
      _builders;
};

class DefaultLoggerManagerInitializer {
 public:
  static void initialize(LoggerManager& manager) noexcept;
};

// 通过&操作符将返回值转为void的工具类
// 辅助BABYLON_LOG宏实现条件日志
class Voidify {
 public:
  template <typename T>
  inline void operator&(T&&) noexcept {}
};

inline LogSeverity Logger::min_severity() const noexcept {
  return _min_severity;
}

inline bool Logger::initialized() const noexcept {
  return _initialized;
}

////////////////////////////////////////////////////////////////////////////////
// LoggerManager begin
inline Logger& LoggerManager::get_root_logger() noexcept {
  return _root_logger;
}
// LoggerManager end
////////////////////////////////////////////////////////////////////////////////
BABYLON_NAMESPACE_END

#define BABYLON_LOG_STREAM(logger, severity, ...)                           \
  (logger).min_severity() > ::babylon::LogSeverity::severity                \
      ? (void)0                                                             \
      : ::babylon::Voidify() &                                              \
            ::babylon::ScopedLogStream(                                     \
                (logger).stream(::babylon::LogSeverity::severity, __FILE__, \
                                __LINE__, __func__),                        \
                ##__VA_ARGS__)                                              \
                .stream()

#define BABYLON_LOG(severity)                                                \
  BABYLON_LOG_STREAM(::babylon::LoggerManager::instance().get_root_logger(), \
                     severity)
