#include "babylon/logging/logger.h"

#include "babylon/logging/interface.h"

BABYLON_NAMESPACE_BEGIN

Logger::Logger() noexcept
    : _min_severity {LogSeverity::DEBUG}, _initialized {false} {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static ThreadLocalLogStream default_log_stream {
      [](::std::unique_ptr<LogStream>* ptr) {
        new (ptr)::std::unique_ptr<LogStream> {new DefaultLogStream};
      }};
#pragma GCC diagnostic pop
  for (size_t i = 0; i < LogSeverity::NUM; ++i) {
    _log_streams[i].store(&default_log_stream, ::std::memory_order_relaxed);
  }
}

Logger::Logger(const Logger& other) noexcept
    : _min_severity {other._min_severity}, _initialized {other._initialized} {
  for (size_t i = 0; i < LogSeverity::NUM; ++i) {
    _log_streams[i].store(
        other._log_streams[i].load(::std::memory_order_relaxed),
        ::std::memory_order_relaxed);
  }
}

Logger& Logger::operator=(const Logger& other) noexcept {
  for (size_t i = 0; i < LogSeverity::NUM; ++i) {
    _log_streams[i].store(
        other._log_streams[i].load(::std::memory_order_relaxed),
        ::std::memory_order_relaxed);
  }
  _min_severity = other._min_severity;
  _initialized = other._initialized;
  return *this;
}

LogStream& Logger::stream(LogSeverity severity, StringView file,
                          int line) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static NullLogStream nls;
#pragma GCC diagnostic pop

  // TODO(oathdruid): remove this after remove LogStreamProvider in interface.h
  if (ABSL_PREDICT_FALSE(!_initialized)) {
    if (severity >= LogInterface::min_severity()) {
      return LogInterface::provider().stream(severity, file, line);
    }
    return nls;
  }

  if (ABSL_PREDICT_FALSE(severity < min_severity())) {
    return nls;
  }

  auto& stream =
      *_log_streams[severity].load(::std::memory_order_acquire)->local();
  stream.set_severity(severity);
  stream.set_file(file);
  stream.set_line(line);
  return stream;
}

void Logger::set_initialized(bool initialized) noexcept {
  _initialized = initialized;
}

void Logger::set_min_severity(LogSeverity min_severity) noexcept {
  _min_severity = min_severity;
}

void Logger::set_log_stream(LogSeverity severity,
                            ThreadLocalLogStream& log_stream) noexcept {
  _log_streams[severity].store(&log_stream, ::std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
// LoggerBuilder begin
LoggerBuilder::LoggerBuilder() noexcept : _min_severity {LogSeverity::INFO} {
  for (size_t i = 0; i < LogSeverity::NUM; ++i) {
    auto severity = static_cast<LogSeverity>(i);
    _log_streams[i].first = severity;
    _log_streams[i].second.set_constructor(
        [severity](::std::unique_ptr<LogStream>* ptr) {
          new (ptr)::std::unique_ptr<LogStream> {new DefaultLogStream};
          (*ptr)->set_severity(severity);
        });
  }
}

Logger LoggerBuilder::build() noexcept {
  Logger logger;
  for (auto& pair : _log_streams) {
    logger.set_log_stream(pair.first, pair.second);
  }
  logger.set_min_severity(_min_severity);
  logger.set_initialized(true);
  return logger;
}

void LoggerBuilder::set_log_stream_creator(
    LoggerBuilder::Creator creator) noexcept {
  for (size_t i = 0; i < LogSeverity::NUM; ++i) {
    auto severity = static_cast<LogSeverity>(i);
    auto& stream = _log_streams[i].second;
    stream.set_constructor(
        [severity, creator](::std::unique_ptr<LogStream>* ptr) {
          new (ptr)::std::unique_ptr<LogStream> {creator()};
          (*ptr)->set_severity(severity);
        });
  }
}

void LoggerBuilder::set_log_stream_creator(
    LogSeverity severity, LoggerBuilder::Creator creator) noexcept {
  auto& stream = _log_streams[severity].second;
  stream.set_constructor(
      [severity, creator](::std::unique_ptr<LogStream>* ptr) {
        new (ptr)::std::unique_ptr<LogStream> {creator()};
        (*ptr)->set_severity(severity);
      });
}

void LoggerBuilder::set_min_severity(LogSeverity min_severity) noexcept {
  _min_severity = min_severity;
}
// LoggerBuilder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// LoggerManager begin
LoggerManager& LoggerManager::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static LoggerManager object;
#pragma GCC diagnostic pop
  return object;
}

void LoggerManager::set_root_builder(LoggerBuilder&& builder) noexcept {
  ::std::lock_guard<::std::mutex> lock {_builder_mutex};
  _root_builder.reset(new LoggerBuilder {::std::move(builder)});
}

void LoggerManager::set_builder(StringView name,
                                LoggerBuilder&& builder) noexcept {
  ::std::lock_guard<::std::mutex> lock {_builder_mutex};
  _builders.emplace(name, ::std::move(builder));
}

void LoggerManager::apply() noexcept {
  ::std::lock_guard<::std::mutex> lock {_builder_mutex};
  if (_root_builder == nullptr) {
    _root_builder.reset(new LoggerBuilder);
  }
  _root_logger = _root_builder->build();
  for (auto& pair : _loggers) {
    apply_to(pair.first, pair.second);
  }
}

Logger& LoggerManager::get_logger(StringView name) noexcept {
  if (name.empty()) {
    return _root_logger;
  }

  auto result = _loggers.emplace(name);
  if (!result.second) {
    return result.first->second;
  }

  ::std::lock_guard<::std::mutex> lock {_builder_mutex};
  apply_to(name, result.first->second);
  return result.first->second;
}

LoggerManager::LoggerManager() noexcept {
  DefaultLoggerManagerInitializer::initialize(*this);
}

void LoggerManager::apply_to(StringView name, Logger& logger) noexcept {
  auto builder = find_nearest_builder(name);
  if (builder != nullptr) {
    logger = builder->build();
  }
}

LoggerBuilder* LoggerManager::find_nearest_builder(StringView name) noexcept {
  ::std::string needle {name.data(), name.size()};
  while (!needle.empty()) {
    auto iter = _builders.find(needle);
    if (iter != _builders.end()) {
      return &iter->second;
    }
    auto colon_pos = needle.rfind("::");
    auto dot_pos = needle.rfind('.');
    auto pos = colon_pos != needle.npos ? colon_pos : 0;
    if (dot_pos != needle.npos && dot_pos > pos) {
      pos = dot_pos;
    }
    needle.resize(pos);
  }
  return _root_builder.get();
}
// LoggerManager end
////////////////////////////////////////////////////////////////////////////////

ABSL_ATTRIBUTE_WEAK void DefaultLoggerManagerInitializer::initialize(
    LoggerManager&) noexcept {}

BABYLON_NAMESPACE_END
