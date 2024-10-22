**[[English]](task.en.md)**

# task

## 原理

协程Task的执行方式设计为依托[Executor](../executor.zh-cn.md)完成；一个根协程需要通过提交到一个[Executor](../executor.zh-cn.md)来得到执行，提交后协程将被设定为绑定到对应[Executor](../executor.zh-cn.md)上；协程执行中，可以通过`co_await`另一个task的方式启动子协程，子协程默认也绑定到同一个[Executor](../executor.zh-cn.md)上，但也支持绑定到其他[Executor](../executor.zh-cn.md)；当一个Task中断并恢复后，会确保回到绑定的[Executor](../executor.zh-cn.md)内；

协程Task默认支持`co_await`另一个Task以及一些其他babylon内置对象，例如[Future](../future.zh-cn.md)；此外，也提供了基于模板特化的定制能力，用来支持用户将其他自定义类型接入框架变成awaitable；

## 用法示例

### Run Task

```c++
#include "babylon/coroutine/task.h"
#include "babylon/executor.h"

using ::babylon::coroutine::Task;
using ::babylon::Executor;

// 任意Executor实现都可以用来提交协程
Executor& executor = ...;

// 支持基础函数
struct S {
  static Task<...> coroutine_plain_function(...) {
    ...
  }
};
// Executor提交协程时不会返回协程标称的Task<...>而是内部实际co_return的结果的Future包装Future<...>
// 返回的future也可以和提交普通函数一样被get或者wait_for
auto future = executor.execute(S::coroutine_plain_function, ...);
// 同样支持不关心完成情况的单纯submit
auto success = executor.submit(S::coroutine_plain_function, ...);

// 支持成员函数
struct S {
  Task<...> coroutine_member_function(...) {
    ...
  }
} s;
auto future = executor.execute(&S::coroutine_member_function, &s, ...);

// 支持operator()
struct S {
  Task<...> operator()(...) {
    ...
  }
} s;
auto future = executor.execute(s, ...);

// 支持lambda
auto future = executor.execute([&](...) -> Task<...> {
  ...
});

// 启动子协程
Task<...> some_coroutine(...) {
  ...
  // 执行后当前线程会挂起some_coroutine并切换到coroutine_member_function
  ... = co_await s.coroutine_member_function(...);
  // 当coroutine_member_function执行完成后会切换回来
  ...
}

// 将子协程启动到其他Executor
Task<...> some_coroutine(...) {
  ...
  // 执行后当前线程会挂起some_coroutine，可能会开始执行其他排队中的任务
  // coroutine_member_function会被发送到other_executor执行
  ... = co_await s.coroutine_member_function(...).set_executor(other_executor);
  // 当coroutine_member_function执行完成后会恢复执行some_coroutine
  // some_coroutine依然会回到原先其绑定的executor执行
  ...
}

//////////////////////////////// 高级用法 //////////////////////////////////

// 默认coroutine的执行状态本体随task一同销毁，可以主动release释放并得到句柄
std::coroutine_handle<...> handle = some_coroutine(...).release();
// 可以将句柄传输给其他线程并执行，这也是Executor内部的做法
handle.resume();
// 注意无需调用destroy，对于非co_await作为子协程拉起的情况，运行结束后会自动销毁
// handle.destroy();
```

### Task co_await custom type

```c++
#include "babylon/coroutine/task.h"

using ::babylon::coroutine::Task;

// 对于本身已经满足awaitable标准的类型，不需要任何特化
// 例如其他协程机制的task，可以直接进行混合co_await
Task<...> some_coroutine(...) {
  ...
  ... = co_await some_awaitable;
  ...
}

// 对于本身不满足awaitable的情况，可以通过转换包装支持
template <>
class BasicCoroutinePromise::Transformer<SomeCustomType> {
 public:
  // 第一个传入参数为发起co_await的协程的promise，不关心的话可以忽略
  // 在Task自身的await_transform中promise用来继承Executor
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
