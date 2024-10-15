#pragma once

#include "babylon/coroutine/promise.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

template <typename T = void>
class Task {
 public:
  class promise_type;

  inline Task() noexcept = default;
  inline Task(Task&& other) noexcept;
  Task(const Task&) = delete;
  inline Task& operator=(Task&& other) noexcept;
  Task& operator=(const Task&) = delete;
  inline ~Task() noexcept;

  inline Task<T>& set_executor(BasicExecutor& executor) & noexcept;
  inline Task<T>&& set_executor(BasicExecutor& executor) && noexcept;
  inline BasicExecutor* executor() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  // Protocol to be an awaitable
  inline static constexpr bool await_ready() noexcept;
  template <typename P>
    requires(::std::is_base_of<BasicPromise, P>::value)
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<P> awaiter) noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter) noexcept;
  inline T await_resume() noexcept;
  //////////////////////////////////////////////////////////////////////////////

  inline ::std::coroutine_handle<promise_type> handle() noexcept {
    return _handle;
  }

  inline ::std::coroutine_handle<promise_type> release() noexcept;

 private:
  inline Task(::std::coroutine_handle<promise_type> handle) noexcept;

  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter,
      BasicExecutor* awaiter_executor) noexcept;

  ::std::coroutine_handle<promise_type> _handle;

  friend BasicPromise;
  friend class Executor;
};

template <typename T>
class Task<T>::promise_type : public Promise<T> {
 public:
  inline Task<T> get_return_object() noexcept {
    return {::std::coroutine_handle<promise_type>::from_promise(*this)};
  }
};

template <typename A>
struct WrapperTask {};
template <typename A>
  requires requires { typename AwaitResultType<A, void>; }
struct WrapperTask<A> {
  using ResultType = AwaitResultType<A, void>;
  using ForwardType = typename ::std::conditional<
      ::std::is_rvalue_reference<ResultType>::value,
      typename ::std::remove_reference<ResultType>::type, ResultType>::type;
  using type = Task<ForwardType>;
};
template <typename A>
using WrapperTaskType = typename WrapperTask<A>::type;

// Check if C(Args...) is a babylon coroutine, thus,
// return Task<T>
template <typename C, typename... Args>
concept TaskInvocable =
    CoroutineInvocable<C, Args...> &&
    IsSpecialization<::std::invoke_result_t<C, Args...>, Task>::value;

template <typename T>
class BasicPromise::Transformer<Task<T>> {
 public:
  inline static Task<T>&& await_transform(BasicPromise& promise,
                                          Task<T>&& task) {
    if (!task.executor()) {
      task.set_executor(*promise.executor());
    }
    return ::std::move(task);
  }
};

template <typename A>
  requires requires { typename WrapperTaskType<A&&>; }
class BasicPromise::Transformer<A> {
 public:
  inline static WrapperTaskType<A&&> await_transform(BasicPromise&,
                                                     A&& awaitable) noexcept {
    return [](A&& inner_awaitable) -> WrapperTaskType<A&&> {
      co_return co_await BasicPromise::NoTransformation<A> {
          ::std::forward<A>(inner_awaitable)};
    }(::std::forward<A>(awaitable));
  }
};

////////////////////////////////////////////////////////////////////////////////
// Task begin
template <typename T>
inline Task<T>::Task(Task&& other) noexcept {
  ::std::swap(_handle, other._handle);
}

template <typename T>
inline Task<T>& Task<T>::operator=(Task&& other) noexcept {
  ::std::swap(_handle, other._handle);
  return *this;
}

template <typename T>
inline Task<T>::~Task() noexcept {
  if (_handle) {
    _handle.destroy();
  }
}

template <typename T>
inline Task<T>& Task<T>::set_executor(BasicExecutor& executor) & noexcept {
  _handle.promise().set_executor(executor);
  return *this;
}

template <typename T>
inline Task<T>&& Task<T>::set_executor(BasicExecutor& executor) && noexcept {
  return ::std::move(set_executor(executor));
}

template <typename T>
inline BasicExecutor* Task<T>::executor() const noexcept {
  return _handle.promise().executor();
}

template <typename T>
inline constexpr bool Task<T>::await_ready() noexcept {
  return false;
}

template <typename T>
template <typename P>
  requires(::std::is_base_of<BasicPromise, P>::value)
inline ::std::coroutine_handle<> Task<T>::await_suspend(
    ::std::coroutine_handle<P> awaiter) noexcept {
  return await_suspend(awaiter, awaiter.promise().executor());
}

template <typename T>
inline ::std::coroutine_handle<> Task<T>::await_suspend(
    ::std::coroutine_handle<> awaiter) noexcept {
  return await_suspend(awaiter, nullptr);
}

template <typename T>
inline T Task<T>::await_resume() noexcept {
  return _handle.promise().value();
}

template <>
inline void Task<void>::await_resume() noexcept {}

template <typename T>
inline Task<T>::Task(::std::coroutine_handle<promise_type> handle) noexcept
    : _handle(handle) {}

template <typename T>
inline ::std::coroutine_handle<typename Task<T>::promise_type>
Task<T>::release() noexcept {
  return ::std::exchange(_handle, nullptr);
}

template <typename T>
inline ::std::coroutine_handle<> Task<T>::await_suspend(
    ::std::coroutine_handle<> awaiter,
    BasicExecutor* awaiter_executor) noexcept {
  auto& promise = _handle.promise();
  promise.set_awaiter(awaiter, awaiter_executor);
  if (promise.inplace_resumable()) {
    return _handle;
  }
  promise.resume(_handle);
  return ::std::noop_coroutine();
}
// Task end
////////////////////////////////////////////////////////////////////////////////

BABYLON_COROUTINE_NAMESPACE_END

BABYLON_NAMESPACE_BEGIN
template <typename T = void>
using CoroutineTask = coroutine::Task<T>;
BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
