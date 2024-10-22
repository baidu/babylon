**[[English]](futex.en.md)**

# futex

## 原理

标准[coroutine](https://zh.cppreference.com/w/cpp/language/coroutines)机制中的`co_await`基本对标了[std::future](https://zh.cppreference.com/w/cpp/thread/future)并行同步模式；但更多复杂的同步模式例如[std::mutex](https://zh.cppreference.com/w/cpp/thread/mutex)或者[std::condition_variable](https://zh.cppreference.com/w/cpp/thread/condition_variable)的支持可以统一通过类似[futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html)的机制统一支持；

![](images/futex.png)

实现上通过每个futex实例伴随一个`std::mutex`来实现值检测和等待回调链串联原子性；通过[DepositBox](../concurrent/deposit_box.zh-cn.md)实现取消和唤醒的唯一性；

## 用法示例

```c++
#include "babylon/coroutine/task.h"
#include "babylon/coroutine/futex.h"

using ::babylon::coroutine::Task;
using ::babylon::coroutine::Futex;

using Cancellation = Futex::Cancellation;

// Futex构造时内部值为0
Futex futex;

// 读写futex内部值
futex.value() = ...
// 原子式读写futex内部值
futex.atomic_value().xxxx(...);

Task<...> some_coroutine(...) {
  ...
  // 原子检测内部值是否为expected_value，是则挂起some_coroutine，否则直接继续执行
  co_await futex.wait(expected_value).on_suspend(
    // 通过回调函数在协程挂起后接收相应的取消句柄
    // 注意，如果协程未经历挂起，回调不会被调用
    [&](Cancellation cancel) {
      // 典型操作是把cancel句柄注册到某种timer机制，并在指定时间后调用cancel()发起取消
      // 从回调被执行开始，cancel就可用了，甚至在回调内部也可以直接发起cancel()，虽然一般这并没有什么意义
      on_timer(cancel, 100ms);
    }
  );
  // 有种可能执行到这里
  // 1. 未满足expected_value
  // 2. 挂起后futex.wake_one或者futex.wake_all被调用
  // 3. 挂起后cancel()被调用
  ...
}
```
