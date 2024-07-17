#pragma once

#include "babylon/logging/async_file_appender.h" // AsyncFileAppender
#include "babylon/logging/file_object.h"         // FileObject
#include "babylon/logging/log_stream.h"          // LogStream
#include "babylon/logging/logger.h"              // LoggerBuilder

BABYLON_NAMESPACE_BEGIN

// 使用AsyncFileAppender作为后端，实现异步刷写的LogStream
// 提供插件点实现对通用日志头格式的定制
// 同时提供对接到LoggerBuilder的构建方法封装
//
// 典型使用方法是
// ... // 准备好AsyncFileAppender写入器
// ... // 以及FileObject文件目标
// ... // 准备好HeaderFormatter定制日志头格式
// LoggerBuilder builder;
// builder.set_log_stream_creator(AsyncLogStream::creator(appender, file_object,
// formatter)); LoggerManager::instance().set_builder(::std::move(builder));
class AsyncLogStream : public LogStream {
 public:
  using HeaderFormatter = ::std::function<void(AsyncLogStream&)>;

  // 封装对接LoggerBuilder的构造方法
  // 本身不持有appender和file_object的生命周期，需要在外部单独管理
  // formatter被拷贝构造后持有在构造方法内部
  static LoggerBuilder::Creator creator(AsyncFileAppender& appender,
                                        FileObject& file_object) noexcept;
  static LoggerBuilder::Creator creator(AsyncFileAppender& appender,
                                        FileObject& file_object,
                                        HeaderFormatter formatter) noexcept;

  AsyncLogStream() = delete;
  AsyncLogStream(AsyncLogStream&&) = delete;
  AsyncLogStream(const AsyncLogStream&) = delete;
  AsyncLogStream& operator=(AsyncLogStream&&) = delete;
  AsyncLogStream& operator=(const AsyncLogStream&) = delete;
  virtual ~AsyncLogStream() noexcept override = default;

 private:
  static void default_header_formatter(AsyncLogStream& ls) noexcept;

  AsyncLogStream(AsyncFileAppender* appender, FileObject* file_object,
                 HeaderFormatter formatter) noexcept;

  virtual void do_begin() noexcept override;
  virtual void do_end() noexcept override;

  AsyncFileAppender* _appender {nullptr};
  FileObject* _file_object {nullptr};
  LogStreamBuffer _buffer;
  HeaderFormatter _formatter;
};

BABYLON_NAMESPACE_END
