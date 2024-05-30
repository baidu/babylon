# future

## 原理

对标std::future，额外提供了

- 使用模板参数支持自定义SchedInterface，支持在bthread等协程环境使用
  - 在[example/use-with-bthread](https://github.com/baidu/babylon/tree/main/example/use-with-bthread)可以找到一个结合在bthread使用的例子
- 增加on_finish/then功能，实现异步串联

## 使用方法

### Future

```c++
#include <babylon/future.h>

using ::babylon::Future;
using ::babylon::Promise;

{
	Promise<int> promise;
    auto future = promise.get_future();
    ::std::thread thread([&] () {
        // 异步做一些事情
        ...
		// 最终赋值
        promise.set_value(10086);
    });
    future.get(); // 等待set_value，结果 == 10086
}
 
{
    // 例如有一种XSchedInterface的协程机制，使用对应的机制来同步
	Promise<int, XSchedInterface> promise;
    auto future = promise.get_future();
    XThread thread([&] () {
        // 异步做一些事情
        ...
		// 最终赋值
        promise.set_value(10086);
    });
    future.get(); // 等待set_value（使用XSchedInterface协程同步，不占用pthread worker），结果 == 10086
}
 
{
	Promise<int> promise;
    auto future = promise.get_future();
	// 移动捕获promise，避免出作用域销毁
    ::std::thread thread([promise = ::std::move(promise)] () mutable {
        // 异步做一些事情
        ...
		// 最终赋值
        promise.set_value(10086);
    });
    Promise<int> promise2;
	auto future2 = promise2.get_future();
    future.on_finish([promise2 = ::std::move(promise2)] (int&& value) {
 		// 由set_value调用，传入value == 10086
		promise2.set_value(value + 10010);
	});
	// on_finish之后future不再可用，无法ready或者get
    future2.get(); // 等待set_value，结果 == 20096
}

// 更说明见注释
// 单测test/test_future.cpp
```

### CountDownLatch

```c++
#include <babylon/future.h>

using ::babylon::CountDownLatch;

{
    // 预计join 10个异步结果
    CountDownLatch<> latch(10);
    auto future = latch.get_future();
    ::std::vector<::std::thread> threads;
    for (size_t i = 0; i < 10; ++i) {
        threads.emplace_back([&] () {
            // 每个异步结果结束时采用count_down汇报
            latch.count_down();
        });
    }
    future.get(); // 等待计数递减到0再返回

    future.on_finish([] (int) {
         // 也是个future，所以也可以进行异步串联
    });
}

// 更说明见注释
// 单测test/test_future.cpp
```
