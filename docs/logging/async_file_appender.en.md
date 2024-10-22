**[[简体中文]](async_file_appender.zh-cn.md)**

# async_file_appender

## LogEntry & LogStreamBuffer

`LogStreamBuffer` is an implementation of `std::stringbuf`, with actual memory management handled in fixed-size paged memory. `LogEntry` serves as the maintenance structure for this fixed-size paged memory.

### Usage Example

```c++
#include "babylon/logging/log_entry.h"

using babylon::LogStreamBuffer;
using babylon::LogEntry;

// Before using LogStreamBuffer, a PageAllocator must be set
PageAllocator& page_allocator = ...
LogStreamBuffer buffer;
buffer.set_page_allocator(page_allocator);

// The buffer can be reused multiple times
loop:
  buffer.begin();                   // Each use requires begin to trigger preparation actions
  buffer.sputn(...);                // Writing can proceed afterward; typically, this is not called directly, but acts as the underlying mechanism of LogStream
  LogEntry& entry = buffer.end();   // Writing is complete, returning the final assembled result
  ...                               // LogEntry itself is only the size of one cache line, allowing for lightweight copying and transferring

consumer:
  ::std::vector<struct ::iovec> iov;
  // Generally, LogEntry is transferred to the consumer via an asynchronous queue
  LogEntry& entry = ...
  // Append into an iovec structure for easy integration with writev
  entry.append_to_iovec(page_allocator.page_size(), iov);
```

## FileObject

`FileObject` is an abstraction for log writing targets, providing a usable file descriptor (fd) externally. For scenarios requiring rotation, it manages the rotation and old file handling internally.

```c++
#include "babylon/logging/file_object.h"

using babylon::FileObject;

class CustomFileObject : public FileObject {
  // Core functionality function, must be called by the upper layer before each write to obtain the file descriptor
  // This function performs file rotation checks and other operations internally, returning the final prepared descriptor
  // Since file rotation may occur, the return value is a tuple of new and old descriptors (fd, old_fd)
  // fd:
  //   >=0: Current file descriptor, subsequent writes by the caller are initiated using this descriptor
  //   < 0: An exception occurred, and the file cannot be opened
  // old_fd:
  //   >=0: File switching has occurred, returning the previous file descriptor
  //        Usually caused by file rotation; the caller needs to perform a close action
  //        Before closing, the caller can perform final write operations, etc.
  //   < 0: No file switching has occurred
  virtual ::std::tuple<int, int> check_and_get_file_descriptor() noexcept override {
    ...
  }
};
```

## RollingFileObject

Implements a `FileObject` for rolling files, supporting rotation based on time intervals and providing capabilities for quantitative retention and cleanup.

```c++
#include "babylon/logging/rolling_file_object.h"

using babylon::RollingFileObject;

RollingFileObject object;
object.set_directory("dir");                // Directory for log files
object.set_file_pattern("name.%Y-%m-%d");   // Log file name template, supports strftime syntax
                                            // When the time-driven file name changes, file rotation occurs
object.set_max_file_number(7);              // Maximum number of files to retain

// Actual files will be written with names like this
// dir/name.2024-07-18
// dir/name.2024-07-19

// Calling this interface during startup scans the directory and records existing files matching the pattern
// It adds them to the tracking list to support continued proper file retention during restart scenarios
object.scan_and_tracking_existing_files();

loop:
  // Check if the tracked list has exceeded the retention limit; if so, perform cleanup
  object.delete_expire_files();
  // In some scenarios, processes may simultaneously output many log files
  // Actively calling this allows for background thread implementation of all log expiration deletions
  ...
  sleep(1);
```

## AsyncFileAppender & AsyncLogStream

`AsyncFileAppender` implements queued transmission of `LogEntry` and performs asynchronous writing to `FileObject`. `AsyncLogStream` wraps `AsyncFileAppender`, `FileObject`, and `LogStreamBuffer`, connecting to the Logger mechanism.

```c++
#include "babylon/logging/async_log_stream.h"

using babylon::AsyncFileAppender;
using babylon::AsyncLogStream;
using babylon::FileObject;
using babylon::LoggerBuilder;
using babylon::PageAllocator;

// Prepare a PageAllocator and a FileObject&
PageAllocator& page_allocator = ...
FileObject& file_object = ...

AsyncFileAppender appender;
appender.set_page_allocator(page_allocator);
// Set the queue length
appender.set_queue_capacity(65536);
appender.initialize();

// Combine AsyncFileAppender and FileObject to create an AsyncLogStream capable of generating a Logger
LoggerBuilder builder;
builder.set_log_stream_creator(AsyncLogStream::creator(appender, object));
LoggerManager::instance().set_root_builder(::std::move(builder));
LoggerManager::instance().apply();

// Logging macros will start taking effect afterward
BABYLON_LOG(INFO) << ...
```
