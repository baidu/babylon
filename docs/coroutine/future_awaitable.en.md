**[[简体中文]](future_awaitable.zh-cn.md)**

# FutureAwaitable

## Principle

The callback mechanism of [Future](../future.en.md) makes it well-suited to be an awaitable in a coroutine, largely because the internal structure of coroutines also follows the future/promise design pattern. Two versions of future awaitables are provided: `FutureAwaitable` in exclusive mode, where one future corresponds to a single awaiter (which suffices for most use cases), and `SharedFutureAwaitable` in shared mode, where multiple awaiters can await the same future and resume execution once the future is fulfilled.

## Usage Example

```c++
#include "babylon/coroutine/task.h"
#include "babylon/future.h"

using ::babylon::coroutine::Task;
using ::babylon::Future;

::babylon::Future<T> future = ...

// Exclusive mode: The value is moved from the future.
Task<...> some_coroutine(...) {
  ...
  // This suspends the coroutine, allowing other queued tasks to execute.
  T value = co_await ::std::move(future);
  // When promise.set_value(...) is called, the coroutine resumes execution.
  // The coroutine will return to the executor it was originally bound to.
  // Using co_await with Future&& results in a T&&, so T must be used to receive the result.
  ...
}

// Shared mode: The value is exposed by reference from the future and must be used with caution regarding modification safety.
Task<...> some_coroutine(...) {
  ...
  // This suspends the coroutine, allowing other queued tasks to execute.
  T& value = co_await future;
  // When promise.set_value(...) is called, the coroutine resumes execution.
  // The coroutine will return to the executor it was originally bound to.
  // Using co_await with Future& results in a T& reference, and T's lifecycle is controlled by the future.
  ...
}
```
