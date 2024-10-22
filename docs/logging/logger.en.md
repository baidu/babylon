**[[简体中文]](logger.zh-cn.md)**

# logger

## LogStream

`LogStream` is designed as a baseline interface for implementing streaming log macros, providing two extension points through inheritance:
- It uses `std::streambuf` as the underlying buffer provider, facilitating integration with other streaming log ecosystems.
- It introduces `begin/end` plugin points, allowing non-streaming log ecosystems to format data in a thread-local buffer before exporting it completely. Native implementations can also leverage the `begin/end` plugin points to create custom layouts.

### Usage Example

```c++
#include "babylon/logging/log_stream.h"

using babylon::LogStream;

// Implement a meaningful LogStream through inheritance
class SomeLogStream : public LogStream {
  // The base class constructor must receive a usable subclass of std::streambuf
  // All write operations ultimately affect this buffer
  SomeLogStream() : LogStream(stringbuf) {}

  // Additional actions taken at the beginning and end of a log transaction
  virtual void do_begin() noexcept override {
    *this << ... // Typical usage is to implement the prefix output for the log header
  }
  virtual void do_end() noexcept override {
    write('\n'); // Generally, a newline is needed for text logs
                 // No default implementation is provided to also express non-text logs
    ...          // Typically, the final submission to the actual backend of the logging system is required
  }

  ...

  // Here, std::stringbuf is used as an example; it can actually be any custom stream buffer
  std::stringbuf stringbuf;
};

// Typically, LogStream is not used directly, but rather through logging macros provided by Logger
// The macro will automatically manage the calls to begin/end
LogStream& ls = ...
ls.begin();
ls << ...
ls.end();

// In addition to stream operators, printf-like formatting actions are also supported
// The actual formatting functionality is provided by absl::Format
ls.format("some spec %d", value);
```

## Logger & LoggerBuilder

The actual visible logging actions are performed by `Logger`, not directly using `LogStream`. The `Logger` mainly provides two capabilities:
- The log stream `LogStream` is generally non-thread-safe; `Logger` uses `ThreadLocal` for competition protection.
- It allows setting log levels and configuring separate `LogStream`s for each level, primarily supporting scenarios where two different level streams may need to be written simultaneously in no-flush mode (e.g., logging a warning during the assembly of an info log).

`Logger` is solely for use; a `Logger` is created through `LoggerBuilder`.

### Usage Example

```c++
#include "babylon/logging/logger.h"

using babylon::Logger;
using babylon::LoggerBuilder;
using babylon::LogSeverity;

LoggerBuilder builder;
// Set a unified LogStream for all log levels
// Since an instance will be created once for each thread,
// a function to create the instance is passed in instead of a pre-created instance
builder.set_log_stream_creator([] {
  auto ls = ...
  return std::unique_ptr<LogStream>(ls);
});
// A separate LogStream can also be set for a specific level
builder.set_log_stream_creator(LogSeverity::INFO, [] {
  auto ls = ...
  return std::unique_ptr<LogStream>(ls);
});
// Set the minimum log level; any logs below this level will be treated with an empty LogStream, regardless of settings
// The log macro will also recognize the minimum level and skip the entire stream operation for lower levels
// LogSeverity includes four levels: {DEBUG, INFO, WARNING, FATAL}
builder.set_min_severity(LogSeverity min_severity);

// Construct a usable Logger according to the settings
Logger logger = builder.build();

// Use the logger to print logs at the specified level
// *** can append other business common headers, adhering to the no-flush rule to output only once
// ... is the normal log stream input
BABYLON_LOG_STREAM(logger, INFO, ***) << ...
```

## LoggerManager

`LoggerManager` maintains a hierarchical Logger tree. It is used for configuration during initialization and for obtaining Logger instances from various levels at runtime. The transition between configuration and runtime is completed through explicit initialization actions.
- An unconfigured Logger tree will cause any obtained Logger to exhibit default behavior, outputting to standard error.
- Once the Logger is initialized, all previously obtained Loggers that exhibited default behavior will switch to the actual initialized Logger in a thread-safe manner.
- New Logger nodes can be dynamically added to the Logger tree or their log levels changed, also in a thread-safe manner.

### Usage Example

```c++
#include "babylon/logging/logger.h"

using babylon::LoggerManager;

// LoggerManager is used via a global singleton
auto& manager = LoggerManager::instance();

// Obtain a logger at a certain level based on its name
// The name is used to find the configuration hierarchically; for example, for name = "a.b.c",
// it will attempt "a.b.c" -> "a.b" -> "a" -> root in sequence.
// The hierarchy also supports "a::b::c" -> "a::b" -> "a" -> root.
auto& logger = manager.get_logger("...");
// Directly obtain the root logger
auto& root_logger = manager.get_root_logger();

// Any Logger obtained before any setup actions are in default state, all outputting to standard error

// Construct the builder that will take effect
LoggerBuilder&& builder = ...
// Set the root logger
manager.set_root_builder(builder);
// Set a logger for a specific level
manager.set_builder("a.b.c", builder);
// All settings will only take effect after apply
manager.apply();

// After the setup is complete, any Logger obtained "before" will change from the default state to the correct state
// The transition is thread-safe, and Loggers in use will be handled correctly
// Any Logger obtained "after" will be in the correctly configured state

// The default macros will also use the root logger
BABYLON_LOG(INFO) << ...
// Use the specified logger
BABYLON_LOG_STREAM(logger, INFO) << ...

// Generally, the end of a statement is treated as the end of the log submission
// Supports using noflush for gradual assembly
BABYLON_LOG(INFO) << "some " << ::babylon::noflush;
BABYLON_LOG(INFO) << "thing";   // Outputs [header] some thing

// Supports printf-like formatting functionality, with support provided by the abseil-cpp library
BABYLON_LOG(INFO).format("hello %s", world).format(" +%d", 10086);
```
