**[[简体中文]](future.zh-cn.md)**

# Future

## Principle

This `Future` implementation is modeled after `std::future` with additional features, including:

- Support for custom `SchedInterface` through template parameters, enabling usage in coroutine environments such as `bthread`. A practical example of combining this with `bthread` can be found in [example/use-with-bthread](https://github.com/baidu/babylon/tree/main/example/use-with-bthread).
- Added `on_finish`/`then` functionalities to enable asynchronous chaining of tasks.

## Usage

### Future

```c++
#include <babylon/future.h>

using ::babylon::Future;
using ::babylon::Promise;

{
    Promise<int> promise;
    auto future = promise.get_future();
    ::std::thread thread([&]() {
        // Perform some asynchronous operations
        ...
        // Set the final value
        promise.set_value(10086);
    });
    future.get();  // Waits for set_value, result == 10086
}

{
    // Example using an XSchedInterface coroutine mechanism for synchronization
    Promise<int, XSchedInterface> promise;
    auto future = promise.get_future();
    XThread thread([&]() {
        // Perform some asynchronous operations
        ...
        // Set the final value
        promise.set_value(10086);
    });
    future.get();  // Waits for set_value (using XSchedInterface for coroutine
                   // synchronization without occupying pthread workers), result
                   // == 10086
}

{
    Promise<int> promise;
    auto future = promise.get_future();
    // Move-capture promise to avoid destruction out of scope
    ::std::thread thread([promise = ::std::move(promise)]() mutable {
        // Perform some asynchronous operations
        ...
        // Set the final value
        promise.set_value(10086);
    });
    Promise<int> promise2;
    auto future2 = promise2.get_future();
    future.on_finish([promise2 = ::std::move(promise2)](int&& value) {
        // Called by set_value, with value == 10086
        promise2.set_value(value + 10010);
    });
    // After on_finish, future is no longer available, cannot be ready or get
    future2.get();  // Waits for set_value, result == 20096
}

// For further details, see comments and test cases
// Unit tests: test/test_future.cpp
```

### CountDownLatch

```c++
#include <babylon/future.h>

using ::babylon::CountDownLatch;

{
    // Expecting to join 10 asynchronous results
    CountDownLatch<> latch(10);
    auto future = latch.get_future();
    ::std::vector<::std::thread> threads;
    for (size_t i = 0; i < 10; ++i) {
        threads.emplace_back([&] () {
            // Each asynchronous result reports completion with count_down
            latch.count_down();
        });
    }
    future.get(); // Waits until the count decrements to 0

    future.on_finish([] (int) {
         // It's also a future, so you can chain further asynchronous tasks
    });
}
```
