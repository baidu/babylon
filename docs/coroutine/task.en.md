**[[简体中文]](task.zh-cn.md)**

# Task

## Principle

The execution of coroutine tasks is designed to rely on an [Executor](../executor.en.md). A root coroutine must be submitted to an [Executor](../executor.en.md) for execution, after which the coroutine will be bound to that specific executor. During coroutine execution, sub-coroutines can be started by `co_await` another task, which will, by default, also be bound to the same executor. However, it is possible to bind a sub-coroutine to a different executor. When a task is suspended and later resumed, it will always return to the executor to which it was originally bound.

A coroutine task can `co_await` another task or other built-in Babylon objects, such as [Future](../future.en.md). Additionally, template specialization enables custom types to be integrated into the framework as awaitables.

## Usage Example

### Run Task

```c++
#include "babylon/coroutine/task.h"
#include "babylon/executor.h"

using ::babylon::coroutine::Task;
using ::babylon::Executor;

// Any Executor implementation can be used to submit coroutines
Executor& executor = ...;

// Supports basic functions
struct S {
  static Task<...> coroutine_plain_function(...) {
    ...
  }
};
// When submitting a coroutine to an Executor, it returns a Future<...> wrapping the co_return result, instead of Task<...>.
// The returned future can be retrieved with get or wait_for, similar to submitting regular functions.
auto future = executor.execute(S::coroutine_plain_function, ...);
// You can also submit without waiting for completion.
auto success = executor.submit(S::coroutine_plain_function, ...);

// Supports member functions
struct S {
  Task<...> coroutine_member_function(...) {
    ...
  }
} s;
auto future = executor.execute(&S::coroutine_member_function, &s, ...);

// Supports operator()
struct S {
  Task<...> operator()(...) {
    ...
  }
} s;
auto future = executor.execute(s, ...);

// Supports lambda functions
auto future = executor.execute([&](...) -> Task<...> {
  ...
});

// Launch sub-coroutines
Task<...> some_coroutine(...) {
  ...
  // Suspends some_coroutine and switches to coroutine_member_function
  ... = co_await s.coroutine_member_function(...);
  // After coroutine_member_function completes, it switches back
  ...
}

// Launch sub-coroutines on a different Executor
Task<...> some_coroutine(...) {
  ...
  // Suspends some_coroutine and coroutine_member_function will be executed on other_executor
  ... = co_await s.coroutine_member_function(...).set_executor(other_executor);
  // After coroutine_member_function completes, it resumes some_coroutine
  // some_coroutine still returns to its originally bound executor
  ...
}

//////////////////////////////// Advanced Usage //////////////////////////////////

// By default, coroutine execution state is destroyed along with the task, but it can be explicitly released and a handle can be obtained.
std::coroutine_handle<...> handle = some_coroutine(...).release();
// The handle can be transferred to another thread and resumed, which is what the Executor internally does.
handle.resume();
// No need to call destroy; when not co_awaited as a sub-coroutine, it will automatically be destroyed after completion.
// handle.destroy();
```

### Task co_await Custom Type

```c++
#include "babylon/coroutine/task.h"

using ::babylon::coroutine::Task;

// For types that already meet the awaitable standards, no specialization is required.
// For example, tasks from other coroutine mechanisms can be directly co_awaited.
Task<...> some_coroutine(...) {
  ...
  ... = co_await some_awaitable;
  ...
}

// For types that do not meet the awaitable standard, a custom wrapper can be used to provide support.
template <>
class BasicCoroutinePromise::Transformer<SomeCustomType> {
 public:
  // The first parameter is the promise of the coroutine initiating the co_await. It can be ignored if not needed.
  // In Task's own await_transform, the promise is used to inherit the Executor.
  static SomeCustomAwaitable await_transform(BasicCoroutinePromise&,
                                             SomeCustomType&& some_custom_type) {
    return to_custom_awaitable(::std::move(some_custom_type));
  }
};
Task<...> some_coroutine(...) {
  ...
  ... = co_await some_custom_type;
  ...
}
```
