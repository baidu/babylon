# Use with glog

## glog to babylon

程序整体日志系统使用babylon::LoggerManager，但部分不方便修改的子模块采用glog作为日志接口，可以采用定制google::LogSink的方法将日志重定向到babylon::LoggerManager日志框架

```c++
class BabylonLogSink : public ::google::LogSink {
 public:
  virtual void send(::google::LogSeverity glog_severity,
                    const char* full_filename, const char*,
                    int line, const ::google::LogMessageTime&,
                    const char* message, size_t message_len) noexcept override {
    // severity mapping
    static ::babylon::LogSeverity mapping[] = {
        [::google::INFO] = ::babylon::LogSeverity::INFO,
        [::google::WARNING] = ::babylon::LogSeverity::WARNING,
        [::google::ERROR] = ::babylon::LogSeverity::FATAL,
        [::google::FATAL] = ::babylon::LogSeverity::FATAL,
    };

    // root logger severity pre-check
    auto severity = mapping[glog_severity];
    auto& logger = ::babylon::LoggerManager::instance().get_root_logger();
    if (logger.min_severity() > severity) {
      return;
    }

    // write to root logger
    auto& stream = logger.stream(severity, full_filename, line);
    stream.begin();
    stream.write(message, message_len);
    stream.end();
  }
};

// glog -> babylon sink -> babylon
BabylonLogSink sink;
::google::AddLogSink(&sink);
```

## babylon to glog

程序整体日志系统使用glog，可以采用定制babylon::LogStream的方法将babylon内部日志重定向到glog

```c++
class GLogStream : public ::babylon::LogStream {
 public:
  GLogStream() noexcept : LogStream(*reinterpret_cast<std::streambuf*>(0)) {}

  virtual void do_begin() noexcept override {
    // severity mapping
    static ::google::LogSeverity mapping[] = {
        [::babylon::LogSeverity::DEBUG] = ::google::INFO,
        [::babylon::LogSeverity::INFO] = ::google::INFO,
        [::babylon::LogSeverity::WARNING] = ::google::WARNING,
        [::babylon::LogSeverity::FATAL] = ::google::FATAL,
    };

    // get underlying std::streambuf from glog. set to babylon::LogStream
    auto& message = reinterpret_cast<::google::LogMessage&>(_message_storage);
    new (&message) ::google::LogMessage(file().data(), line(), mapping[severity()]);
    rdbuf(message.stream().rdbuf());
  }

  virtual void do_end() noexcept override {
    // destruct google::LogMessage to flush
    auto& message = reinterpret_cast<::google::LogMessage&>(_message_storage);
    message.~LogMessage();
  }

  ::std::aligned_storage<sizeof(::google::LogMessage), alignof(::google::LogMessage)>::type _message_storage;
};

// babylon -> logger -> glog stream -> glog
::babylon::LoggerBuilder builder;
builder.set_log_stream_creator([] {
  return ::std::make_unique<GLogStream>();
});
::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
::babylon::LoggerManager::instance().apply();
```
