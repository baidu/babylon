# logger

## LogStream

LogStream设计了一个实现streaming log宏的基准接口表达，并通过继承实现对用户提供了两个扩展点
- 采用std::streambuf作为底层缓冲区提供者，便于对接到其他steaming log生态
- 增加了begin/end插件点，对于非streaming log生态可以在thread local buffer格式化后完整导出对接，原生实现也可以利用begin/end插件点实现自定义layout

### 用法示例

```c++
#include "babylon/logging/log_stream.h"

using babylon::LogStream;

// 通过继承方式实现一个有实际意义的LogStream
class SomeLogStream : public LogStream {
  // 基类构造必须传入一个可用的std::streambuf的子类
  // 所有写入动作最终都会生效到这个缓冲区中
  SomeLogStream() : LogStream(stringbuf) {}

  // 一次日志事务的开头结尾进行的额外动作
  virtual void do_begin() noexcept override {
    *this << ... // 典型用法是实现日志头的前置输出
  }
  virtual void do_end() noexcept override {
    write('\n'); // 一般对于文本日志都需要追加结尾换行
                 // 没有默认实现主要为了能够同样表达非文本日志
    ...          // 一般最后都需要提交到日志系统的实际后端实现
  }

  ...

  // 这里以std::stringbuf为例，实际上可以是任意自定义的流缓冲区
  std::stringbuf stringbuf;
};

// 一般无需直接使用LogStream，而是通过Logger的包装宏使用
// 包装宏会自动完成begin/end的调用管理
LogStream& ls = ...
ls.begin();
ls << ...
ls.end();

// 除了流操作符，也支持printf-like的格式化动作
// 实际格式化功能由absl::Format提供
ls.format("some spec %d", value);
```

## Logger&LoggerBuilder

实际最终日志打印动作可见的对向是Logger，而非直接使用LogStream。Logger主要提供了两个能力
- 日志流LogStream一般本身都是非线程安全的，Logger使用ThreadLocal进行了竞争保护
- 提供了日志等级设置，以及为每个等级设置独立LogStream的能力，主要支持在noflush模式下可能同时需要两个不同等级独立写出流的场景（循环组装一行info的过程中，打出一条warning）

Logger本身仅供使用，创建一个Logger通过LoggerBuilder完成

### 用法示例

```c++
#include "babylon/logging/logger.h"

using babylon::Logger;
using babylon::LoggerBuilder;
using babylon::LogSeverity;

LoggerBuilder builder;
// 对所有日志等级设置统一的LogStream
// 由于最终会对每个线程创建一次
// 所以传入的不是创建好的实例，而是创建实例的函数
builder.set_log_stream_creator([] {
  auto ls = ...
  return std::unique_ptr<LogStream>(ls);
});
// 也可以对某个级别设置单独的LogStream
builder.set_log_stream_creator(LogSeverity::INFO, [] {
  auto ls = ...
  return std::unique_ptr<LogStream>(ls);
});
// 设置最低日志等级，低于最低等级无论设置与否都会按照空LogStream处理
// 同时日志宏包装也会识别最低等级进行整段流操作符的省略
// LogSeverity包含{DEBUG, INFO, WARNING, FATAL}四个等级
builder.set_min_severity(LogSeverity min_severity);

// 按照设置实际构造一个可以使用的Logger
Logger logger = builder.build();

// 使用logger，打印置顶级别的日志
// ***部分可以追加其他业务通用头，也同样遵守noflush时只输出一次的规则
// ...部分是正常的日志流输入
BABYLON_LOG_STREAM(logger, INFO, ***) << ...
```

## LoggerManager

LoggerManager维护了层级化的Logger树，初始化阶段用来配置，运行时用来获取获取各个层级节点的Logger，配置和运行阶段通过明确的初始化动作完成转换
- 未配置的Logger树，获取到的Logger表现为默认行为，输出到标准错误
- Logger初始化后，所有之前获取的表现为默认行为的Logger会切换到实际被初始化的真实Logger，过程中是线程安全的
- 可以动态向Logger树增加节点Logger或者改变日志等级，过程中也是线程安全的

### 用法示例

```c++
#include "babylon/logging/logger.h"

using babylon::LoggerManager;

// LoggerManager通过全局单例使用
auto& manager = LoggerManager::instance();

// 根据name取得某个层级的logger
// name根据层级查找配置，例如对于name = "a.b.c"
// 会依次尝试"a.b.c" -> "a.b" -> "a" -> root
// 层次同时兼容"a::b::c" -> "a::b" -> "a" -> root
auto& logger = manager.get_logger("...");
// 直接取得root logger
auto& root_logger = manager.get_root_logger();

// 在任何设置动作发生之前，取得的Logger都是默认状态，全部输出到标准错误

// 构造计划生效的builder
LoggerBuilder&& builder = ...
// 设置root logger
manager.set_root_builder(builder);
// 设置某个层级的logger
manager.set_builder("a.b.c", builder);
// 所有设置只有在apply之后才会统一生效
manager.apply();

// 完成设置后，『之前』取得的Logger也会从默认状态，变更为正确的状态
// 变更过程是线程安全的，正在使用中的Logger也会被正确处理
// 『之后』取得的Logger直接就会是正确设置的状态

// 默认宏内部也会使用root logger
BABYLON_LOG(INFO) << ...
// 使用指定的logger
BABYLON_LOG_STREAM(logger, INFO) << ...

// 一般在语句结束后，就会视为结束进行日志提交
// 支持使用noflush来进行渐进组装
BABYLON_LOG(INFO) << "some " << ::babylon::noflush;
BABYLON_LOG(INFO) << "thing";   // 输出[header] some thing

// 支持类printf格式化功能，底层由abseil-cpp库提供支持
BABYLON_LOG(INFO).format("hello %s", world).format(" +%d", 10086);
```
