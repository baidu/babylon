# async_file_appender

## LogEntry&LogStreamBuffer

LogStreamBuffer是一个std::stringbuf的实现，实际内存管理在分页定长内存上，LogEntry就是这个分页定长内存本身的维护结构。

### 用法示例

```c++
#include "babylon/logging/log_entry.h"

using babylon::LogStreamBuffer;
using babylon::LogEntry;

// 使用LogStreamBuffer需要先设置一个PageAllocator
PageAllocator& page_allocator = ...
LogStreamBuffer buffer;
buffer.set_page_allocator(page_allocator);

// 后续buffer可以反复使用
loop:
  buffer.begin();                   // 每次使用需要用begin触发准备动作
  buffer.sputn(...);                // 之后可以进行写入动作，一般不直接调用，而是成为LogStream的底层
  LogEntry& entry = buffer.end();   // 一轮写入完成，返回最终的组装结果
  ...                               // LogEntry本身只有一个cache line大小，可以轻量拷贝转移

consumer:
  ::std::vector<struct ::iovec> iov;
  // 一般LogEntry经过异步队列转移到消费者执行
  LogEntry& entry = ...
  // 追加倒出成iovec结构，主要方便对接writev
  entry.append_to_iovec(page_allocator.page_size(), iov);
```

## FileObject

FileObject是对于日志写入对向的抽象，功能为对外提供可用的fd，对于需要滚动的场景，内部完成滚动和老文件管理

```c++
#include "babylon/logging/file_object.h"

using babylon::FileObject;

class CustomFileObject : public FileObject {
  // 核心功能函数，上层在每次写出前需要调用此函数来获得文件操作符
  // 函数内部完成文件滚动检测等操作，返回最终准备好的描述符
  // 由于可能发生文件的滚动，返回值为新旧描述符(fd, old_fd)二元组
  // fd:
  //   >=0: 当前文件描述符，调用者后续写入通过此描述符发起
  //   < 0: 发生异常无法打开文件
  // old_fd:
  //   >=0: 发生文件切换，返回之前的文件描述符
  //        一般由文件滚动引起，需要调用者执行关闭动作
  //        关闭前调用者可以进行最后的收尾写入等操作
  //   < 0: 未发生文件切换
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept override {
    ...
  }
};
```

## RollingFileObject

实现滚动文件的FileObject，支持按照时间间隔滚动切换，并提供定量保留清理能力

```c++
#include "babylon/logging/rolling_file_object.h"

using babylon::RollingFileObject;

RollingFileObject object;
object.set_directory("dir");                // 日志所在目录
object.set_file_pattern("name.%Y-%m-%d");   // 日志文件名模板，支持strftime语法
                                            // 当时间驱动文件名发生变化时，执行文件滚动
object.set_max_file_number(7);              // 最多保留个数

// 实际会写入类似这样名称的文件当中
// dir/name.2024-07-18
// dir/name.2024-07-19

// 启动期间调用此接口可以扫描目录并记录其中符合pattern的已有文件
// 并加入到跟踪列表，来支持重启场景下继续跟进正确的文件定量保留
object.scan_and_tracking_existing_files();

loop:
  // 检查目前跟踪列表中是否超出了保留数目，超出则进行清理
  object.delete_expire_files();
  // 一些场景进程会同时输出很多路日志文件
  // 主动调用便于在一个后台线程实现所有日志的过期删除
  ...
  sleep(1);
```

## AsyncFileAppender&AsyncLogStream

AsyncFileAppender实现了LogEntry的队列传输，以及最终异步向FileObject执行写入动作
AsyncLogStream包装了AsyncFileAppender、FileObject和LogStreamBuffer，对接到Logger机制

```c++
#include "babylon/logging/async_log_stream.h"

using babylon::AsyncFileAppender;
using babylon::AsyncLogStream;
using babylon::FileObject;
using babylon::LoggerBuilder;
using babylon::PageAllocator;

// 需要先准备一个PageAllocator和一个FileObject&
PageAllocator& page_allocator = ...
FileObject& file_object = ...

AsyncFileAppender appender;
appender.set_page_allocator(page_allocator);
// 设置队列长度
appender.set_queue_capacity(65536);
appender.initialize();

// 组合AsyncFileAppender和FileObject行程一个能够生成Logger的AsyncLogStream
LoggerBuilder builder;
builder.set_log_stream_creator(AsyncLogStream::creator(appender, object));
LoggerManager::instance().set_root_builder(::std::move(builder));
LoggerManager::instance().apply();

// 之后就会在日志宏背后开始生效
BABYLON_LOG(INFO) << ...
```
