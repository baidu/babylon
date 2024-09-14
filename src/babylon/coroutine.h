#pragma once

#include "babylon/current_executor.h"
#include "babylon/future.h"
#include "babylon/logging/logger.h"

#include "absl/types/optional.h"

#if __cpp_concepts && __cpp_lib_coroutine

#include <coroutine>

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// Get awaitable type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the first step in co_await: convert expression to awaitable
template <typename P, typename A>
struct CoroutineAwaitableFrom {
  using type = A;
};
template <typename P, typename A>
  requires requires {
             ::std::declval<P>().await_transform(::std::declval<A>());
           }
struct CoroutineAwaitableFrom<P, A> {
  using type =
      decltype(::std::declval<P>().await_transform(::std::declval<A>()));
};
template <typename P, typename A>
using CoroutineAwaitableType = typename CoroutineAwaitableFrom<P, A>::type;
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get awaiter type
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// follow the second step in co_await: convert awaitable to awaiter
template <typename P, typename A>
struct CoroutineAwaiterFrom {
  using type = CoroutineAwaitableType<P, A>;
};
template <typename P, typename A>
  requires requires {
             ::std::declval<CoroutineAwaitableType<P, A>>().operator co_await();
           }
struct CoroutineAwaiterFrom<P, A> {
  using type = decltype(::std::declval<CoroutineAwaitableType<P, A>>().
                        operator co_await());
};
template <typename P, typename A>
  requires requires {
             operator co_await(::std::declval<CoroutineAwaitableType<P, A>>());
           }
struct CoroutineAwaiterFrom<P, A> {
  using type = decltype(operator co_await(
      ::std::declval<CoroutineAwaitableType<P, A>>()));
};
template <typename P, typename A>
using CoroutineAwaiterType = typename CoroutineAwaiterFrom<P, A>::type;
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Get result type of a co_await expression
// P stand for promise type of awaiter coroutine
// A stand for raw awaitee object
//
// function as if: decltype(co_await expression)
//
// Given:
// SomeTaskType awaiter_coroutine() {
//   auto x = co_await awaitee_object;
// }
//
// Then decltype(x) is:
// CoroutineAwaitableType<SomeTaskType::promise_type, decltype(awaitee_object)>
template <typename P, typename A>
using CoroutineAwaitResultType =
    decltype(::std::declval<CoroutineAwaiterType<P, A>>().await_resume());
////////////////////////////////////////////////////////////////////////////////

class Executor;
template <typename T = void>
class CoroutineTask;
template <typename T, typename F>
class FutureAwaitable;
// Common part of CoroutineTask<T>::promise_type
class BasicCoroutinePromise {
 public:
  class FinalAwaitable;

  //////////////////////////////////////////////////////////////////////////////
  // Protocol to be a coroutine promise type, except result type-related
  // functions.
  template <typename T>
  inline CoroutineTask<T> get_return_object() noexcept;
  inline constexpr ::std::suspend_always initial_suspend() const noexcept;
  inline FinalAwaitable final_suspend() noexcept;
  inline void unhandled_exception() noexcept;
  // void return_void() noexcept;
  // void return_value(T) noexcept;
  //////////////////////////////////////////////////////////////////////////////

  inline void set_executor(Executor& executor) noexcept;
  inline Executor* executor() const noexcept;

  inline void set_awaiter(::std::coroutine_handle<> awaiter,
                          Executor* awaiter_executor) noexcept;
  inline bool awaiter_inplace_resumable() const noexcept;
  inline ::std::coroutine_handle<> awaiter() const noexcept;
  void resume_awaiter() noexcept;

  inline bool inplace_resumable() const noexcept;
  void resume(::std::coroutine_handle<> handle) noexcept;

  inline void set_auto_destroy(bool auto_destroy) noexcept;
  inline bool auto_destroy() const noexcept;

  static void* operator new(size_t size) {
    void* ptr = ::operator new(size);
    BABYLON_LOG(INFO) << "BasicCoroutinePromise::new " << ptr << " size "
                      << size;
    return ptr;
  }

  static void operator delete(void* ptr, size_t size) {
    BABYLON_LOG(INFO) << "BasicCoroutinePromise::delete " << ptr << " size "
                      << size;
    ::operator delete(ptr, size);
  }

  template <typename A>
  struct Untransformable {
    A awaitable;
  };

  template <typename T>
  CoroutineTask<T>&& await_transform(CoroutineTask<T>&& task) noexcept {
    BABYLON_LOG(INFO)
        << "BasicCoroutinePromise::await_transform<CoroutineTask<T>> " << this;
    if (!task.executor()) {
      task.set_executor(*_executor);
    }
    return ::std::move(task);
  }

  template <typename T, typename F>
  FutureAwaitable<T, F>&& await_transform(
      FutureAwaitable<T, F>&& future) noexcept {
    return ::std::move(future);
  }

  template <typename T, typename F>
  FutureAwaitable<T, F> await_transform(Future<T, F>&& future) noexcept {
    return ::std::move(future);
  }

  template <typename A>
    requires(!IsSpecialization<typename ::std::remove_reference<A>::type,
                               CoroutineTask>::value)
  CoroutineTask<CoroutineAwaitResultType<void, A>> await_transform(
      A&& awaitable) noexcept {
    BABYLON_LOG(INFO) << "BasicCoroutinePromise::await_transform<A> " << this;
    return [](A awaitable) -> CoroutineTask<CoroutineAwaitResultType<void, A>> {
      co_return co_await Untransformable<A> {.awaitable =
                                                 ::std::move(awaitable)};
    }(::std::move(awaitable));
  }

  template <typename A>
  A await_transform(Untransformable<A>&& awaitable) noexcept {
    return ::std::move(awaitable.awaitable);
  }

 private:
  Executor* _executor {nullptr};
  ::std::coroutine_handle<> _awaiter;
  Executor* _awaiter_executor {nullptr};
  bool _auto_destroy {false};
};

class BasicCoroutinePromise::FinalAwaitable {
 public:
  inline FinalAwaitable(BasicCoroutinePromise* promise) noexcept;
  inline ~FinalAwaitable() noexcept {
    BABYLON_LOG(INFO) << "FinalAwaitable::~FinalAwaitable " << this;
  }

  inline constexpr bool await_ready() const noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      ::std::coroutine_handle<>) const noexcept;
  inline void await_resume() const noexcept;

 private:
  BasicCoroutinePromise* _promise;
};

template <typename T>
class CoroutinePromise : public BasicCoroutinePromise {
 public:
  inline CoroutineTask<T> get_return_object() noexcept;
  template <typename U>
  inline void return_value(U&& value) noexcept {
    BABYLON_LOG(INFO) << "CoroutinePromise::return_value " << this;
    _value.emplace(::std::forward<U>(value));
  }
  inline T& value() noexcept {
    return *_value;
  }

 private:
  ::absl::optional<T> _value;
};

template <>
class CoroutinePromise<void> : public BasicCoroutinePromise {
 public:
  inline CoroutinePromise() noexcept = default;
  inline ~CoroutinePromise() noexcept {
    BABYLON_LOG(INFO) << "promise_type::~promise_type " << this;
  }

  inline CoroutineTask<> get_return_object() noexcept;
  void return_void() {
    BABYLON_LOG(INFO) << "promise_type::return_void " << this;
  }
  inline static constexpr void value() noexcept {}
};

// template <typename T>
// class CoroutinePromise;
template <typename T>
class CoroutineTask {
 public:
  using promise_type = CoroutinePromise<T>;
  using HandleType = ::std::coroutine_handle<promise_type>;

  inline CoroutineTask() noexcept {
    BABYLON_LOG(INFO) << "CoroutineTask::CoroutineTask " << this;
  }
  inline CoroutineTask(CoroutineTask&& other) noexcept;
  CoroutineTask(const CoroutineTask&) = delete;
  inline CoroutineTask& operator=(CoroutineTask&& other) noexcept;
  CoroutineTask& operator=(const CoroutineTask&) = delete;
  inline ~CoroutineTask() noexcept;

  inline operator bool() const noexcept;

  inline CoroutineTask<T>& set_executor(Executor& executor) noexcept;
  inline Executor* executor() const noexcept;

  inline constexpr bool await_ready() const noexcept;

  template <typename U>
  ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<CoroutinePromise<U>> awaiter) noexcept;

  T await_resume() noexcept;

 private:
  inline HandleType release() noexcept {
    auto handle = _handle;
    _handle = nullptr;
    handle.promise().set_auto_destroy(true);
    return handle;
  }

  ::std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiter,
                                          Executor* executor) noexcept;

  CoroutineTask(HandleType handle) : _handle(handle) {
    BABYLON_LOG(INFO) << "CoroutineTask::CoroutineTask " << this << " handle "
                      << _handle.address();
  }

 private:
  ::std::coroutine_handle<promise_type> _handle;

  friend BasicCoroutinePromise;
  friend Executor;
};

template <typename T, typename F>
class FutureAwaitable {
 public:
  FutureAwaitable(Future<T, F>&& future) noexcept
      : _future {::std::move(future)} {}

  inline bool await_ready() noexcept {
    auto ready = _future.ready();
    if (ready) {
      _value.emplace(::std::move(_future.get()));
    }
    return ready;
  }

  inline void await_suspend(
      ::std::coroutine_handle<typename CoroutineTask<T>::promise_type>
          handle) noexcept {
    BABYLON_LOG(INFO) << "FutureAwaitable::await_suspend";
    _future.on_finish([this, handle](T&& value) {
      BABYLON_LOG(INFO) << "FutureAwaitable::await_suspend on_finish";
      _value.emplace(::std::move(value));
      handle.promise().resume(handle);
    });
  }

  inline T await_resume() noexcept {
    return ::std::move(*_value);
  }

 private:
  Future<T, F> _future;
  ::absl::optional<T> _value;
};

template <typename C, typename... Args>
concept CoroutineInvocable =
    requires {
      typename ::std::coroutine_traits<::std::invoke_result_t<C, Args...>,
                                       Args...>::promise_type;
    };

template <typename C, typename... Args>
concept CoroutineTaskInvocable =
    CoroutineInvocable<C, Args...> &&
    IsSpecialization<::std::invoke_result_t<C, Args...>, CoroutineTask>::value;

////////////////////////////////////////////////////////////////////////////////
// BasicCoroutinePromise::FinalAwaitable begin
inline BasicCoroutinePromise::FinalAwaitable::FinalAwaitable(
    BasicCoroutinePromise* promise) noexcept
    : _promise {promise} {
  BABYLON_LOG(INFO) << "FinalAwaitable::FinalAwaitable " << this << " promise "
                    << promise;
}

inline constexpr bool BasicCoroutinePromise::FinalAwaitable::await_ready()
    const noexcept {
  return false;
}

inline ::std::coroutine_handle<>
BasicCoroutinePromise::FinalAwaitable::await_suspend(
    ::std::coroutine_handle<> handle) const noexcept {
  struct S {
    inline ~S() noexcept {
      if (promise->auto_destroy()) {
        handle.destroy();
      }
    }
    BasicCoroutinePromise* promise;
    ::std::coroutine_handle<> handle;
  } s {.promise {_promise}, .handle {handle}};

  BABYLON_LOG(INFO) << "FinalAwaitable::await_suspend " << this << " promise "
                    << _promise << " awaiter " << _promise->awaiter().address()
                    << " inplace " << _promise->awaiter_inplace_resumable();
  auto awaiter = _promise->awaiter();
  if (awaiter) {
    if (_promise->awaiter_inplace_resumable()) {
      return awaiter;
    }
    _promise->resume_awaiter();
  }
  return ::std::noop_coroutine();
}

inline void BasicCoroutinePromise::FinalAwaitable::await_resume()
    const noexcept {
  BABYLON_LOG(INFO) << "FinalAwaitable::await_resume " << this;
}
// BasicCoroutinePromise::FinalAwaitable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicCoroutinePromise begin
template <typename T>
inline CoroutineTask<T> BasicCoroutinePromise::get_return_object() noexcept {
  return {::std::coroutine_handle<CoroutinePromise<T>>::from_promise(
      static_cast<CoroutinePromise<T>&>(*this))};
}

inline constexpr ::std::suspend_always BasicCoroutinePromise::initial_suspend()
    const noexcept {
  return {};
}

inline BasicCoroutinePromise::FinalAwaitable
BasicCoroutinePromise::final_suspend() noexcept {
  BABYLON_LOG(INFO) << "BasicCoroutinePromise::final_suspend " << this
                    << " awaiter " << _awaiter.address();
  return {this};
}

inline void BasicCoroutinePromise::unhandled_exception() noexcept {
  BABYLON_LOG(INFO) << "BasicCoroutinePromise::unhandled_exception " << this;
  abort();
}

inline void BasicCoroutinePromise::set_executor(Executor& executor) noexcept {
  _executor = &executor;
}

inline Executor* BasicCoroutinePromise::executor() const noexcept {
  return _executor;
}

inline void BasicCoroutinePromise::set_awaiter(
    ::std::coroutine_handle<> awaiter, Executor* awaiter_executor) noexcept {
  _awaiter = awaiter;
  _awaiter_executor = awaiter_executor;
}

inline bool BasicCoroutinePromise::awaiter_inplace_resumable() const noexcept {
  return _awaiter_executor == nullptr ||
         _awaiter_executor == CurrentExecutor::get();
}

inline ::std::coroutine_handle<> BasicCoroutinePromise::awaiter()
    const noexcept {
  return _awaiter;
}

inline bool BasicCoroutinePromise::inplace_resumable() const noexcept {
  return _executor == nullptr || _executor == CurrentExecutor::get();
}

inline void BasicCoroutinePromise::set_auto_destroy(
    bool auto_destroy) noexcept {
  _auto_destroy = auto_destroy;
}

inline bool BasicCoroutinePromise::auto_destroy() const noexcept {
  return _auto_destroy;
}
// BasicCoroutinePromise end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CoroutinePromise begin
template <typename T>
inline CoroutineTask<T> CoroutinePromise<T>::get_return_object() noexcept {
  return BasicCoroutinePromise::get_return_object<T>();
}

inline CoroutineTask<> CoroutinePromise<void>::get_return_object() noexcept {
  return BasicCoroutinePromise::get_return_object<void>();
}
// BasicCoroutinePromise end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CoroutineTask begin
template <typename T>
inline CoroutineTask<T>::CoroutineTask(CoroutineTask&& other) noexcept {
  ::std::swap(_handle, other._handle);
}

template <typename T>
inline CoroutineTask<T>& CoroutineTask<T>::operator=(
    CoroutineTask&& other) noexcept {
  ::std::swap(_handle, other._handle);
  return *this;
}

template <typename T>
inline CoroutineTask<T>::~CoroutineTask() noexcept {
  BABYLON_LOG(INFO) << "CoroutineTask::~CoroutineTask " << this << " handle "
                    << _handle.address();
  if (_handle) {
    _handle.destroy();
  }
}

template <typename T>
inline CoroutineTask<T>::operator bool() const noexcept {
  return static_cast<bool>(_handle);
}

template <typename T>
inline CoroutineTask<T>& CoroutineTask<T>::set_executor(
    Executor& executor) noexcept {
  _handle.promise().set_executor(executor);
  return *this;
}

template <typename T>
inline Executor* CoroutineTask<T>::executor() const noexcept {
  return _handle.promise().executor();
}

template <typename T>
inline constexpr bool CoroutineTask<T>::await_ready() const noexcept {
  return false;
}

template <typename T>
template <typename U>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<CoroutinePromise<U>> awaiter) noexcept {
  BABYLON_LOG(INFO) << "CoroutineTask::await_suspend CoroutinePromise " << this;
  auto handle = _handle;
  auto& promise = handle.promise();
  promise.set_awaiter(awaiter, awaiter.promise().executor());
  if (promise.inplace_resumable()) {
    return handle;
  }
  handle.promise().resume(handle);
  return ::std::noop_coroutine();
}

template <typename T>
inline T CoroutineTask<T>::await_resume() noexcept {
  BABYLON_LOG(INFO) << "CoroutineTask::await_resume " << this;
  return ::std::move(_handle.promise().value());
}

template <>
inline void CoroutineTask<void>::await_resume() noexcept {
  BABYLON_LOG(INFO) << "CoroutineTask::await_resume<void> " << this;
}
// CoroutineTask end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
