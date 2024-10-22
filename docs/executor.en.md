**[[简体中文]](executor.zh-cn.md)**

# Executor

## Principle

`std::async` introduced an asynchronous execution framework based on the future/promise design pattern. However, its default mechanism creates a new thread for each asynchronous execution, and the built-in policy design lacks user extensibility, making it impractical for production environments.

To provide a practical solution for asynchronous execution in a future/promise design pattern, and to simplify the interface of asynchronous frameworks, the Executor implements a user-extensible asynchronous execution framework. It includes common executors such as a serial executor and a thread pool executor, making asynchronous programming easier. The built-in thread pool executor uses a `ConcurrentBoundedQueue` to support stronger concurrency performance.

## Usage

### Basic Usage

```c++
#include <babylon/executor.h>

using ::babylon::Executor;
using ::babylon::InplaceExecutor;
using ::babylon::ThreadPoolExecutor;

// Submit a task for execution and get a future
Executor* executor = ...
auto future = executor->execute([] {
  return 1 + 1;
});
// The submitted function will be executed by the executor
// The execution may be asynchronous or synchronous, depending on the executor implementation
// But you can interact with and retrieve the result using the returned future
future.get(); // == 2

// Submit a task without tracking its future
Executor* executor = ...
int value = 0;
/* 0 == */ executor->submit([&] {
  value = 1 + 1;
});
// The submitted function will be executed by the executor, but no future is returned to track it
// The execution may be asynchronous or synchronous, depending on the executor implementation
// Mainly used to reduce the overhead of constructing a future when tracking is unnecessary
value; // == 0
usleep(100000); // Demonstrating asynchronous execution effect; in practice, callback chains or other patterns are used
value; // == 2

// Inplace executor that directly executes the function on the current thread and returns after completion
// Mainly used in unit testing or debugging scenarios
InplaceExecutor executor;

// A drawback of the inplace executor is that submitting tasks within a task may cause recursion, leading to stack overflow
// In production environments, if you use a serial executor, you need to enable breadth-first expansion to avoid recursion
InplaceExecutor executor {true};

// A practical thread pool executor
ThreadPoolExecutor executor;
// The thread pool must be initialized with the number of threads and queue capacity before use
// Submitting new tasks after the queue is full will block until space is freed
executor.initialize(thread_num, queue_capacity);
... // Use the executor
// Wait for all submitted tasks to complete and shut down the execution threads
executor.stop();
```

### Extend a New Executor

```c++
#include <babylon/executor.h>

using ::babylon::Executor;
using ::babylon::MoveOnlyFunction;

class MyExecutor : public Executor {
    int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override {
        function(); // Can execute the function in-place
        s.saved_function = std::move(function); // Or move it to an asynchronous environment
        ...
        return 0; // Return 0 to confirm successful submission
    }
};
```
