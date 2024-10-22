**[[English]](executor.en.md)**

# executor

## 原理

std::async提出了一种基于future/promise设计模式的异步执行框架，但是其默认的异步执行机制采用了每次新建一个线程的方法，而且自带的policy设计并不具备用户扩展能力，导致其并不具备生产环境实用性

为了让future/promise设计模式下的异步执行能够实用，同时也方便收敛异步执行框架的接口；Executor实现了一套可用户扩展的异步执行框架，并自带实现了最常用的串行执行器和线程池执行器，提供了比较方便的异步执行编程方法；自带的线程池执行器的执行队列采用了ConcurrentBoundedQueue支持，以便提供更强的并发性能

## 使用方法

### 基础用法

```c++
#include <babylon/executor.h>

using ::babylon::Executor;
using ::babylon::InplaceExecutor;
using ::babylon::ThreadPoolExecutor;

// 提交执行任务并获取future
Executor* executor = ...
auto future = executor->execute([] {
  return 1 + 1;
});
// 提交后传入函数会交给executor执行
// 执行可能是异步的，可能是同步的，具体由executor实现决定
// 但是都可以用返回的future进行等待交互和结果获取
future.get(); // == 2

// 提交执行任务
Executor* executor = ...
int value = 0;
// 返回值标识是否成功提交，只有成功提交才会最终运行函数
/* 0 == */ executor->submit([&] {
  value = 1 + 1;
});
// 提交后传入函数会交给executor执行，但不会返回用于跟踪的future
// 执行可能是异步的，可能是同步的，具体由executor实现决定
// 主要用于在不需要跟踪的场景减少一次future的构造成本
value; // == 0
usleep(100000); // 演示可能在异步执行的效果，实际一般程序会用回调链等模式进行更实际的无future串联
value; // == 2

// 原地执行器，会直接在当前线程执行函数并在完成后才返回
// 一般主要用于单测或调试等特殊场景
InplaceExecutor executor;

// 原地执行器有个问题是如果在函数中继续提交任务会形成递归，可能有爆栈的风险
// 确实在生产环境使用串行执行器时，需要打开展开功能，此时会通过辅助结构将递归的深度遍历改为广度遍历
InplaceExecutor executor {true};

// 具备典型实用意义的线程池执行器
ThreadPoolExecutor executor;
// 线程池需要初始化后才能使用，初始化时设置线程数和队列容量
// 超过队列容量后，再提交新任务会发生阻塞，直到队列任务消化腾出空间为止
executor.initialize(thread_num, queue_capacity);
... // 投入使用
// 等待已提交任务排空，并关闭执行线程
executor.stop();
```

### 扩展新的执行器

```c++
#include <babylon/executor.h>

using ::babylon::Executor;
using ::babylon::MoveOnlyFunction;

class MyExecutor : public Executor {
    int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override {
        function(); // 可以原地执行
        s.saved_function = std::move(function); // 也可以移动到异步环境去执行
        ...
        return 0; // 确认提交成功返回0
    }
};
```
