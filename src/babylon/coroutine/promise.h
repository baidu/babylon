#pragma once

#include "babylon/basic_executor.h"
#include "babylon/coroutine/traits.h"

#include "absl/base/optimization.h"
#include "absl/types/optional.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_NAMESPACE_BEGIN

// Common part of CoroutineTask<T>::promise_type
class BasicCoroutinePromise {
 public:
  // Resume or direct switch to awaiter if there is one.
  class FinalAwaitable;

  // Extend point for BasicCoroutinePromise::await_transform.
  //
  // To make a type A awaitable, we can specialization like this:
  // template <>
  // class BasicCoroutinePromise::Transformer<A> {
  //   B await_transform(BasicCoroutinePromise&, A&&) {
  //     ...
  //   }
  // };
  template <typename A>
  class Transformer;

  // When building a co_await chain like `this co_await proxy co_await
  // awaitable`, we need to know whether the current awaiter is already proxy,
  // to avoid chaining infinitely. Use this wrapper to identify this.
  template <typename A>
  class NoTransformation;

  //////////////////////////////////////////////////////////////////////////////
  // Protocol to be a coroutine promise type, except result type-related
  // functions.
  // T get_return_object();
  inline constexpr ::std::suspend_always initial_suspend() const noexcept;
  inline FinalAwaitable final_suspend() noexcept;
  [[noreturn]] inline void unhandled_exception() noexcept;
  // void return_void();
  // void return_value(T);
  //////////////////////////////////////////////////////////////////////////////

  // Bind to specific executor. Both role of co_await, awaiter or awaitee, will
  // be send back to this executor in resumption if they are not there already.
  inline void set_executor(BasicExecutor& executor) noexcept;
  inline BasicExecutor* executor() const noexcept;

  // Register an awaiter, coroutine which issue a co_await to this coroutine,
  // along with it's binding executor if it's a babylon coroutine.
  //
  // Awaiter registered will be resumed after this coroutine finished. The
  // resumption will move awaiter coroutine back to it's binding executor. If we
  // already there or the awaiter has no binding executor, the resumption will
  // happen in-place.
  inline void set_awaiter(::std::coroutine_handle<> awaiter,
                          BasicExecutor* awaiter_executor) noexcept;
  inline bool awaiter_inplace_resumable() const noexcept;
  inline ::std::coroutine_handle<> awaiter() const noexcept;
  inline void resume_awaiter() noexcept;

  // After awaiter suspend and finish registration, this newly created coroutine
  // will send back to it's binding executor and resume. If we already there or
  // has no binding executor, the resumption will happen in-place.
  inline bool inplace_resumable() const noexcept;
  inline void resume(::std::coroutine_handle<> handle) noexcept;

  // Use Transformer do real transform
  template <typename A>
    requires requires {
               BasicCoroutinePromise::Transformer<::std::remove_cvref_t<A>>::
                   await_transform(::std::declval<BasicCoroutinePromise&>(),
                                   ::std::declval<A&&>());
             }
  inline auto await_transform(A&& awaitable) noexcept;

 private:
  inline static void resume_in_executor(
      BasicExecutor* executor, ::std::coroutine_handle<> handle) noexcept;

  BasicExecutor* _executor {nullptr};
  ::std::coroutine_handle<> _awaiter;
  BasicExecutor* _awaiter_executor {nullptr};
};

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

template <typename A>
class BasicCoroutinePromise::NoTransformation {
 public:
  inline NoTransformation(A awaitable) noexcept;
  inline operator A() noexcept;

 private:
  A _awaitable;
};
template <typename A>
class BasicCoroutinePromise::Transformer<
    BasicCoroutinePromise::NoTransformation<A>> {
 public:
  inline static A await_transform(
      BasicCoroutinePromise&,
      BasicCoroutinePromise::NoTransformation<A>&& awaitable) {
    return awaitable;
  }
};

template <typename T>
class CoroutinePromise : public BasicCoroutinePromise {
 private:
  using RemoveReferenceType = typename ::std::remove_reference<T>::type;
  using PromiseValueType =
      typename ::std::conditional<::std::is_lvalue_reference<T>::value,
                                  ::std::reference_wrapper<RemoveReferenceType>,
                                  RemoveReferenceType>::type;

 public:
  template <typename U>
  inline void return_value(U&& value) noexcept;
  inline void return_value(T& value) noexcept;
  inline T& value() noexcept;

 private:
  ::absl::optional<PromiseValueType> _value;
};

template <>
class CoroutinePromise<void> : public BasicCoroutinePromise {
 public:
  inline static constexpr void return_void() noexcept;
  inline static constexpr void value() noexcept;
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
// BasicCoroutinePromise::NoTransformation begin
template <typename A>
inline BasicCoroutinePromise::NoTransformation<A>::NoTransformation(
    A awaitable) noexcept
    : _awaitable {::std::forward<A>(awaitable)} {}

template <typename A>
inline BasicCoroutinePromise::NoTransformation<A>::operator A() noexcept {
  return ::std::forward<A>(_awaitable);
}
// BasicCoroutinePromise::NoTransformation end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicCoroutinePromise begin
inline constexpr ::std::suspend_always BasicCoroutinePromise::initial_suspend()
    const noexcept {
  return {};
}

inline BasicCoroutinePromise::FinalAwaitable
BasicCoroutinePromise::final_suspend() noexcept {
  return {this};
}

inline void BasicCoroutinePromise::unhandled_exception() noexcept {
  ::abort();
}

inline void BasicCoroutinePromise::set_executor(
    BasicExecutor& executor) noexcept {
  _executor = &executor;
}

inline BasicExecutor* BasicCoroutinePromise::executor() const noexcept {
  return _executor;
}

inline void BasicCoroutinePromise::set_awaiter(
    ::std::coroutine_handle<> awaiter,
    BasicExecutor* awaiter_executor) noexcept {
  _awaiter = awaiter;
  _awaiter_executor = awaiter_executor;
}

inline bool BasicCoroutinePromise::awaiter_inplace_resumable() const noexcept {
  return _awaiter_executor == nullptr || _awaiter_executor->is_running_in();
}

inline ::std::coroutine_handle<> BasicCoroutinePromise::awaiter()
    const noexcept {
  return _awaiter;
}

inline void BasicCoroutinePromise::resume_awaiter() noexcept {
  resume_in_executor(_awaiter_executor, _awaiter);
}

inline bool BasicCoroutinePromise::inplace_resumable() const noexcept {
  return _executor == nullptr || _executor->is_running_in();
}

inline void BasicCoroutinePromise::resume(
    ::std::coroutine_handle<> handle) noexcept {
  resume_in_executor(_executor, handle);
}

template <typename A>
  requires requires {
             BasicCoroutinePromise::Transformer<::std::remove_cvref_t<A>>::
                 await_transform(::std::declval<BasicCoroutinePromise&>(),
                                 ::std::declval<A&&>());
           }
inline auto BasicCoroutinePromise::await_transform(A&& awaitable) noexcept {
  return Transformer<::std::remove_cvref_t<A>>::await_transform(
      *this, ::std::forward<A>(awaitable));
}

inline void BasicCoroutinePromise::resume_in_executor(
    BasicExecutor* executor, ::std::coroutine_handle<> handle) noexcept {
  auto ret = executor->invoke([handle] {
    handle.resume();
  });
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    handle.resume();
  }
}
// BasicCoroutinePromise end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// CoroutinePromise begin
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

BABYLON_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine

#include "babylon/coroutine/future_awaitable.h"
