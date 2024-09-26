#pragma once

#include "babylon/concurrent/vector.h"
#include "babylon/current_executor.h"
#include "babylon/future.h"
#include "babylon/logging/logger.h"

#include "absl/types/optional.h"

#if __cpp_concepts && __cpp_lib_coroutine

#include <coroutine>

BABYLON_NAMESPACE_BEGIN

class Executor;
template <typename T = void>
class CoroutineTask;
template <typename T, typename F = ::babylon::SchedInterface>
class FutureAwaitable;
template <typename T, typename F = ::babylon::SchedInterface>
class SharedFutureAwaitable;

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

// Check if a callable invocation C(Args...) is a coroutine
template <typename C, typename... Args>
concept CoroutineInvocable =
    requires {
      typename ::std::coroutine_traits<::std::invoke_result_t<C, Args...>,
                                       Args...>::promise_type;
    };

// Check if a callable invocation C(Args...) is a babylon coroutine, thus,
// return CoroutineTask<T>
template <typename C, typename... Args>
concept CoroutineTaskInvocable =
    CoroutineInvocable<C, Args...> &&
    IsSpecialization<::std::invoke_result_t<C, Args...>, CoroutineTask>::value;

// Common part of CoroutineTask<T>::promise_type
class BasicCoroutinePromise {
 public:
  class Resumption;
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

  // Bind to specific executor. Both role of co_await, awaiter or awaitee, will
  // be send back to this executor in resumption if they are not there already.
  inline void set_executor(Executor& executor) noexcept;
  inline Executor* executor() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  // Register an awaiter, coroutine which issue a co_await to this coroutine,
  // along with it's binding executor if it's a babylon coroutine.
  //
  // Awaiter registered will be resumed after this coroutine finished. The
  // resumption will move awaiter coroutine back to it's binding executor. If we
  // already there or the awaiter has no binding executor, the resumption will
  // happen in-place.
  inline void set_awaiter(::std::coroutine_handle<> awaiter,
                          Executor* awaiter_executor) noexcept;
  inline bool awaiter_inplace_resumable() const noexcept;
  inline ::std::coroutine_handle<> awaiter() const noexcept;
  void resume_awaiter() noexcept;
  //////////////////////////////////////////////////////////////////////////////

  // After awaiter suspend and finish registration, this newly created coroutine
  // will send back to it's binding executor and resume. If we already there or
  // has no binding executor, the resumption will happen in-place.
  inline bool inplace_resumable() const noexcept;
  void resume(::std::coroutine_handle<> handle) noexcept;

  // Propagate executor to awaitee automatically if not specified
  template <typename T>
    requires(IsSpecialization<typename ::std::remove_reference<T>::type,
                              CoroutineTask>::value)
  inline T&& await_transform(T&& task) noexcept;

  // Wrap future to a awaitable to support co_await future. Reference type of
  // future will propagate to result type of co_await expression, which means:
  //
  // // r1 and r2 will both copy from value that keep inside future
  // T r1 = co_await future;
  // T r2 = co_await future;
  //
  // // r1 will moved out from value that keep inside future
  // // r2 will get an empty value
  // T r1 = co_await ::std::move(future);
  // T r2 = co_await ::std::move(future);
  template <typename T, typename F>
  inline FutureAwaitable<T, F> await_transform(Future<T, F>&& future) noexcept;
  template <typename T, typename F>
  inline SharedFutureAwaitable<T, F> await_transform(
      const Future<T, F>& future) noexcept;

  //////////////////////////////////////////////////////////////////////////////
  // When co_await other awaitable, it will be their responsibility to resume
  // current coroutine in a callback manner. As they may also have some async
  // schedule mechanism, the resumption can happen outside the binding executor.
  //
  // To get back, a proxy task is inserted to construct a co_await chain like:
  // this co_await proxy co_await awaitable
  //
  // After awaitable finish they directly resume the empty proxy task. The proxy
  // task will properly check and send origin awaiter back to it's executor.
 private:
  template <typename A>
    requires requires { typename CoroutineAwaitResultType<void, A>; }
  struct WrapperTask;
  template <typename A>
  using WrapperTaskType = typename WrapperTask<A>::type;
  template <typename A>
  class ReferenceWrapper;

 public:
  template <typename A>
    requires(!IsSpecialization<typename ::std::remove_reference<A>::type,
                               CoroutineTask>::value)
  inline WrapperTaskType<A&&> await_transform(A&& awaitable) noexcept;
  template <typename A>
  inline A await_transform(ReferenceWrapper<A> awaitable) noexcept;
  //////////////////////////////////////////////////////////////////////////////

 private:
  Executor* _executor {nullptr};
  ::std::coroutine_handle<> _awaiter;
  Executor* _awaiter_executor {nullptr};
};

class BasicCoroutinePromise::Resumption {
 public:
  inline Resumption(BasicCoroutinePromise* promise,
                    ::std::coroutine_handle<> handle) noexcept
      : _promise {promise}, _handle {handle} {}

  inline void run() noexcept {
    _promise->resume(_handle);
  }

  inline operator bool() const noexcept {
    return _promise != nullptr;
  }

 private:
  BasicCoroutinePromise* _promise;
  ::std::coroutine_handle<> _handle;
};

// Switch to awaiter if there is one.
class BasicCoroutinePromise::FinalAwaitable {
 public:
  inline FinalAwaitable(BasicCoroutinePromise* promise) noexcept;

  inline constexpr bool await_ready() const noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      ::std::coroutine_handle<>) const noexcept;
  inline void await_resume() const noexcept;

 private:
  BasicCoroutinePromise* _promise {nullptr};
};

// template <typename A>
// struct BasicCoroutinePromise::WrapperTask {
// };
//  Awaitable may return rvalue reference. Remove that reference to make it fit
//  T in a CoroutineTask<T>.
template <typename A>
  requires requires { typename CoroutineAwaitResultType<void, A>; }
struct BasicCoroutinePromise::WrapperTask {
  using ResultType = CoroutineAwaitResultType<void, A>;
  using ForwardType = typename ::std::conditional<
      ::std::is_rvalue_reference<ResultType>::value,
      typename ::std::remove_reference<ResultType>::type, ResultType>::type;
  using type = CoroutineTask<ForwardType>;
};

// When building a co_await chain like `this co_await proxy co_await awaitable`,
// we need to know whether the current awaiter is already proxy, to avoid
// chaining infinitely. Use this wrapper to identify this.
template <typename A>
class BasicCoroutinePromise::ReferenceWrapper {
 public:
  inline ReferenceWrapper(A awaitable) noexcept;
  inline operator A() noexcept;

 private:
  A _awaitable;
};

// CoroutineTask<T>::promise_type for non-void T
template <typename T>
class CoroutinePromise : public BasicCoroutinePromise {
 private:
  using RemoveReferenceType = typename ::std::remove_reference<T>::type;
  using PromiseValueType =
      typename ::std::conditional<::std::is_lvalue_reference<T>::value,
                                  ::std::reference_wrapper<RemoveReferenceType>,
                                  RemoveReferenceType>::type;

 public:
  inline CoroutineTask<T> get_return_object() noexcept;
  template <typename U>
  inline void return_value(U&& value) noexcept;
  inline void return_value(T& value) noexcept;
  inline T& value() noexcept;

 private:
  ::absl::optional<PromiseValueType> _value;
};

// CoroutineTask<T>::promise_type for void T
template <>
class CoroutinePromise<void> : public BasicCoroutinePromise {
 public:
  inline CoroutineTask<> get_return_object() noexcept;
  inline static constexpr void return_void() noexcept;
  inline static constexpr void value() noexcept;
};

template <typename T>
class CoroutineTask {
 public:
  using promise_type = CoroutinePromise<T>;

  inline CoroutineTask() noexcept = default;
  inline CoroutineTask(CoroutineTask&& other) noexcept;
  CoroutineTask(const CoroutineTask&) = delete;
  inline CoroutineTask& operator=(CoroutineTask&& other) noexcept;
  CoroutineTask& operator=(const CoroutineTask&) = delete;
  inline ~CoroutineTask() noexcept;

  inline CoroutineTask<T>& set_executor(Executor& executor) noexcept;
  inline Executor* executor() const noexcept;

  //////////////////////////////////////////////////////////////////////////////
  // Protocol to be an awaitable
  inline static constexpr bool await_ready() noexcept;
  template <typename U>
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<CoroutinePromise<U>> awaiter) noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter) noexcept;
  inline T await_resume() noexcept;
  //////////////////////////////////////////////////////////////////////////////

 private:
  inline CoroutineTask(::std::coroutine_handle<promise_type> handle) noexcept;

  inline ::std::coroutine_handle<> release() noexcept;

  inline ::std::coroutine_handle<> await_suspend(
      std::coroutine_handle<> awaiter, Executor* awaiter_executor) noexcept;

  ::std::coroutine_handle<promise_type> _handle;

  friend BasicCoroutinePromise;
  friend Executor;
};

template <typename T, typename F>
class BasicFutureAwaitable {
 public:
  inline BasicFutureAwaitable(Future<T, F>&& future) noexcept
      : _future {::std::move(future)} {}
  inline BasicFutureAwaitable(const Future<T, F>& future) noexcept
      : _future {future} {}

  inline bool await_ready() const noexcept {
    return _future.ready();
  }

  template <typename U>
  inline void await_suspend(
      ::std::coroutine_handle<CoroutinePromise<U>> handle) noexcept {
    _future.on_finish([handle] {
      handle.promise().resume(handle);
    });
  }

  inline void await_suspend(::std::coroutine_handle<> handle) noexcept {
    _future.on_finish([handle] {
      handle.resume();
    });
  }

  inline T& await_resume() noexcept {
    return _future.get();
  }

 private:
  Future<T, F> _future;
};

template <typename T, typename F>
class FutureAwaitable : public BasicFutureAwaitable<T, F> {
 private:
  using Base = BasicFutureAwaitable<T, F>;

 public:
  using Base::Base;
  inline T&& await_resume() noexcept {
    return ::std::move(Base::await_resume());
  }
};

template <typename T, typename F>
class SharedFutureAwaitable : public BasicFutureAwaitable<T, F> {
 public:
  using BasicFutureAwaitable<T, F>::BasicFutureAwaitable;
};

template <typename T>
class CoroutineCancelable {
 public:
  inline CoroutineCancelable(CoroutineTask<T>&& task) noexcept {}

  inline static bool cancel(VersionedValue<uint32_t> id) noexcept {
    auto& box = DepositBox<int>::instance();
    auto result = box.take(id);
    if (!result) {
      return false;
    }
    auto cancelable = *result;
    cancelable->
  }

  inline bool await_ready() const noexcept {
    return _task.await_ready();
  }

  inline void await_suspend(
      ::std::coroutine_handle<typename CoroutineTask<T>::promise_type>
          handle) noexcept {
    (void)handle;
  }

  inline ::std::optional<T> await_resume() noexcept {
    if (_canceled) {
      return {};
    }
    return _task.await_resume();
  }

 private:
  VersionedValue<uint32_t> _resumption_id;
  CoroutineTask<T> _task;
  bool _canceled {false};
};

////////////////////////////////////////////////////////////////////////////////
// BasicCoroutinePromise::FinalAwaitable begin
inline BasicCoroutinePromise::FinalAwaitable::FinalAwaitable(
    BasicCoroutinePromise* promise) noexcept
    : _promise {promise} {}

inline constexpr bool BasicCoroutinePromise::FinalAwaitable::await_ready()
    const noexcept {
  return false;
}

inline ::std::coroutine_handle<>
BasicCoroutinePromise::FinalAwaitable::await_suspend(
    ::std::coroutine_handle<> handle) const noexcept {
  auto awaiter = _promise->awaiter();
  if (awaiter) {
    if (_promise->awaiter_inplace_resumable()) {
      return awaiter;
    }
    _promise->resume_awaiter();
  } else {
    handle.destroy();
  }
  return ::std::noop_coroutine();
}

inline void BasicCoroutinePromise::FinalAwaitable::await_resume()
    const noexcept {}
// BasicCoroutinePromise::FinalAwaitable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicCoroutinePromise::ReferenceWrapper begin
template <typename A>
inline BasicCoroutinePromise::ReferenceWrapper<A>::ReferenceWrapper(
    A awaitable) noexcept
    : _awaitable {::std::forward<A>(awaitable)} {}

template <typename A>
inline BasicCoroutinePromise::ReferenceWrapper<A>::operator A() noexcept {
  return ::std::forward<A>(_awaitable);
}
// BasicCoroutinePromise::ReferenceWrapper end
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
  return {this};
}

ABSL_ATTRIBUTE_NORETURN
inline void BasicCoroutinePromise::unhandled_exception() noexcept {
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

template <typename T>
  requires(IsSpecialization<typename ::std::remove_reference<T>::type,
                            CoroutineTask>::value)
inline T&& BasicCoroutinePromise::await_transform(T&& task) noexcept {
  if (!task.executor()) {
    task.set_executor(*_executor);
  }
  return ::std::forward<T>(task);
}

template <typename T, typename F>
inline FutureAwaitable<T, F> BasicCoroutinePromise::await_transform(
    Future<T, F>&& future) noexcept {
  return ::std::move(future);
}

template <typename T, typename F>
inline SharedFutureAwaitable<T, F> BasicCoroutinePromise::await_transform(
    const Future<T, F>& future) noexcept {
  return future;
}

template <typename A>
  requires(!IsSpecialization<typename ::std::remove_reference<A>::type,
                             CoroutineTask>::value)
inline BasicCoroutinePromise::WrapperTaskType<
    A&&> BasicCoroutinePromise::await_transform(A&& awaitable) noexcept {
  return [](A&& inner_awaitable) -> WrapperTaskType<A&&> {
    co_return co_await ReferenceWrapper<A> {::std::forward<A>(inner_awaitable)};
  }(::std::forward<A>(awaitable));
}

template <typename A>
inline A BasicCoroutinePromise::await_transform(
    ReferenceWrapper<A> awaitable) noexcept {
  return awaitable;
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

template <typename T>
template <typename U>
inline void CoroutinePromise<T>::return_value(U&& value) noexcept {
  _value.emplace(::std::forward<U>(value));
}

template <typename T>
inline void CoroutinePromise<T>::return_value(T& value) noexcept {
  _value.emplace(value);
}

inline constexpr void CoroutinePromise<void>::return_void() noexcept {}

template <typename T>
inline T& CoroutinePromise<T>::value() noexcept {
  return *_value;
}

inline constexpr void CoroutinePromise<void>::value() noexcept {}
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
  if (_handle) {
    _handle.destroy();
  }
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
inline constexpr bool CoroutineTask<T>::await_ready() noexcept {
  return false;
}

template <typename T>
template <typename U>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<CoroutinePromise<U>> awaiter) noexcept {
  return await_suspend(awaiter, awaiter.promise().executor());
}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<> awaiter) noexcept {
  return await_suspend(awaiter, nullptr);
}

template <typename T>
inline T CoroutineTask<T>::await_resume() noexcept {
  return _handle.promise().value();
}

template <>
inline void CoroutineTask<void>::await_resume() noexcept {}

template <typename T>
inline CoroutineTask<T>::CoroutineTask(
    ::std::coroutine_handle<promise_type> handle) noexcept
    : _handle(handle) {}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::release() noexcept {
  return ::std::exchange(_handle, nullptr);
}

template <typename T>
inline ::std::coroutine_handle<> CoroutineTask<T>::await_suspend(
    ::std::coroutine_handle<> awaiter, Executor* awaiter_executor) noexcept {
  auto& promise = _handle.promise();
  promise.set_awaiter(awaiter, awaiter_executor);
  if (promise.inplace_resumable()) {
    return _handle;
  }
  promise.resume(_handle);
  return ::std::noop_coroutine();
}
// CoroutineTask end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
