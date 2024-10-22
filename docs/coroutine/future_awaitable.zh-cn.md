**[[English]](future_awaitable.en.md)**

# future_awaitable

## 原理

[Future](../future.zh-cn.md)本身的回调机制使其非常适合成为一个协程的awaitable，一定程度上也是因为协程内部本身也采用future/promise设计模式的原因；对于[Future](../future.zh-cn.md)进行awaitable的包装提供了两个不同版本，FutureAwaitable为独占模式，即一个Future对应单一Awaiter，一般情况下独占模式可以满足大多数需求；另一个版本是SharedFutureAwaitable共享模式，同一个future可以对应多个Awaiter，完成时同时恢复多个协程；

## 用法示例

```c++
#include "babylon/coroutine/task.h"
#include "babylon/future.h"

using ::babylon::coroutine::Task;
using ::babylon::Future;

::babylon::Future<T> future = ...

// 独占模式，value会从future中被move出来
Task<...> some_coroutine(...) {
  ...
  // 执行后当前线程会挂起some_coroutine，可能会开始执行其他排队中的任务
  T value = co_await ::std::move(future);
  // 当promise.set_value(...)被执行后会恢复执行some_coroutine
  // some_coroutine依然会回到原先其绑定的executor执行
  // 采用co_await Future&&最终返回值会得到T&&，需要采用T本体接收
  ...
}

// 共享模式，value会以引用行驶从future中被暴露出来，需要共享使用方考虑修改安全性
Task<...> some_coroutine(...) {
  ...
  // 执行后当前线程会挂起some_coroutine，可能会开始执行其他排队中的任务
  T& value = co_await future;
  // 当promise.set_value(...)被执行后会恢复执行some_coroutine
  // some_coroutine依然会回到原先其绑定的executor执行
  // 采用co_await Future&最终返回值会得到T&，此时T指向了内部的本体，生命周期随future控制
  ...
}
```
