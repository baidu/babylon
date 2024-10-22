**[[简体中文]](README.zh-cn.md)**

# Coroutine

## Principle

A coroutine mechanism implemented based on the [C++20](https://en.cppreference.com/w/cpp/20) [coroutine](https://en.cppreference.com/w/cpp/language/coroutines) standard. According to the standard, a coroutine function, in addition to containing the `co_xxx` keyword expressions, also has special requirements for its return type. These requirements can generally be divided into two main categories: one supports a single return value coroutine, commonly referred to as a `task`, while the other supports multiple return values, typically called a `generator`.

![promise](images/promise.png)
![awaitable](images/awaitable.png)

The [coroutine](https://en.cppreference.com/w/cpp/language/coroutines) standard does not directly define the coroutine mechanism but rather abstracts and standardizes the API part of the coroutine mechanism while separating the fine-grained SPI (Service Provider Interface) part, which remains invisible to the user. The coroutine mechanism operates through the following flow: User -> API -> Compiler -> SPI -> Coroutine mechanism. The overall operation mode is:

- The end user expresses coroutine semantics using unified keyword operators, such as `co_await` and `co_return`.
- The end user defines the coroutine mechanism being utilized through the coroutine function’s return type.
- The compiler handles the coroutine suspension and resumption in stages, according to the operator semantics defined in the standard.
- The compiler invokes the corresponding functions of the coroutine framework at specific standard points before suspension and after resumption.

## Submodule Documentation

- [task](task.en.md)
- [future_awaitable](future_awaitable.en.md)
- [cancellable](cancellable.en.md)
- [futex](futex.en.md)
