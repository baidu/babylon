#pragma once

#include "babylon/basic_executor.h"
#include "babylon/coroutine/traits.h"

#include "absl/base/optimization.h"
#include "absl/types/optional.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

// Common part of Task<T>::promise_type
class BasicPromise {
 public:
  // Resume or direct switch to awaiter if there is one.
  class FinalAwaitable;

  // Extend point for BasicPromise::await_transform.
  //
  // To make a type A awaitable, we can specialization like this:
  // template <>
  // class BasicPromise::Transformer<A> {
  //   B await_transform(BasicPromise&, A&&) {
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

  class Resumption;

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
               BasicPromise::Transformer<::std::remove_cvref_t<A>>::
                   await_transform(::std::declval<BasicPromise&>(),
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

class BasicPromise::FinalAwaitable {
 public:
  inline FinalAwaitable(BasicPromise* promise) noexcept;

  inline constexpr bool await_ready() const noexcept;
  inline ::std::coroutine_handle<> await_suspend(
      ::std::coroutine_handle<>) const noexcept;
  inline void await_resume() const noexcept;

 private:
  BasicPromise* _promise {nullptr};
};

template <typename A>
class BasicPromise::NoTransformation {
 public:
  inline NoTransformation(A awaitable) noexcept;
  inline operator A() noexcept;

 private:
  A _awaitable;
};
template <typename A>
class BasicPromise::Transformer<BasicPromise::NoTransformation<A>> {
 public:
  inline static A await_transform(
      BasicPromise&, BasicPromise::NoTransformation<A>&& awaitable) {
    return awaitable;
  }
};

class BasicPromise::Resumption {
 public:
  inline Resumption(BasicPromise* promise,
                    ::std::coroutine_handle<> handle) noexcept
      : _promise {promise}, _handle {handle} {}

  inline void run() noexcept {
    _promise->resume(_handle);
  }

  inline operator bool() const noexcept {
    return _promise != nullptr;
  }

 private:
  BasicPromise* _promise;
  ::std::coroutine_handle<> _handle;
};

template <typename T>
class Promise : public BasicPromise {
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
  inline T& value() & noexcept;
  inline T&& value() && noexcept;

 private:
  ::absl::optional<PromiseValueType> _value;
};

template <>
class Promise<void> : public BasicPromise {
 public:
  inline static constexpr void return_void() noexcept;
  inline static constexpr Void value() noexcept;
};

////////////////////////////////////////////////////////////////////////////////
// BasicPromise::FinalAwaitable begin
inline BasicPromise::FinalAwaitable::FinalAwaitable(
    BasicPromise* promise) noexcept
    : _promise {promise} {}

inline constexpr bool BasicPromise::FinalAwaitable::await_ready()
    const noexcept {
  return false;
}

inline ::std::coroutine_handle<> BasicPromise::FinalAwaitable::await_suspend(
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

inline void BasicPromise::FinalAwaitable::await_resume() const noexcept {}
// BasicPromise::FinalAwaitable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicPromise::NoTransformation begin
template <typename A>
inline BasicPromise::NoTransformation<A>::NoTransformation(A awaitable) noexcept
    : _awaitable {::std::forward<A>(awaitable)} {}

template <typename A>
inline BasicPromise::NoTransformation<A>::operator A() noexcept {
  return ::std::forward<A>(_awaitable);
}
// BasicPromise::NoTransformation end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BasicPromise begin
inline constexpr ::std::suspend_always BasicPromise::initial_suspend()
    const noexcept {
  return {};
}

inline BasicPromise::FinalAwaitable BasicPromise::final_suspend() noexcept {
  return {this};
}

inline void BasicPromise::unhandled_exception() noexcept {
  ::abort();
}

inline void BasicPromise::set_executor(BasicExecutor& executor) noexcept {
  _executor = &executor;
}

inline BasicExecutor* BasicPromise::executor() const noexcept {
  return _executor;
}

inline void BasicPromise::set_awaiter(
    ::std::coroutine_handle<> awaiter,
    BasicExecutor* awaiter_executor) noexcept {
  _awaiter = awaiter;
  _awaiter_executor = awaiter_executor;
}

inline bool BasicPromise::awaiter_inplace_resumable() const noexcept {
  return _awaiter_executor == nullptr || _awaiter_executor->is_running_in();
}

inline ::std::coroutine_handle<> BasicPromise::awaiter() const noexcept {
  return _awaiter;
}

inline void BasicPromise::resume_awaiter() noexcept {
  resume_in_executor(_awaiter_executor, _awaiter);
}

inline bool BasicPromise::inplace_resumable() const noexcept {
  return _executor == nullptr || _executor->is_running_in();
}

inline void BasicPromise::resume(::std::coroutine_handle<> handle) noexcept {
  resume_in_executor(_executor, handle);
}

template <typename A>
  requires requires {
             BasicPromise::Transformer<::std::remove_cvref_t<A>>::
                 await_transform(::std::declval<BasicPromise&>(),
                                 ::std::declval<A&&>());
           }
inline auto BasicPromise::await_transform(A&& awaitable) noexcept {
  return Transformer<::std::remove_cvref_t<A>>::await_transform(
      *this, ::std::forward<A>(awaitable));
}

inline void BasicPromise::resume_in_executor(
    BasicExecutor* executor, ::std::coroutine_handle<> handle) noexcept {
  auto ret = executor->invoke([handle] {
    handle.resume();
  });
  if (ABSL_PREDICT_FALSE(ret != 0)) {
    handle.resume();
  }
}
// BasicPromise end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Promise begin
template <typename T>
template <typename U>
inline void Promise<T>::return_value(U&& value) noexcept {
  _value.emplace(::std::forward<U>(value));
}

template <typename T>
inline void Promise<T>::return_value(T& value) noexcept {
  _value.emplace(value);
}

inline constexpr void Promise<void>::return_void() noexcept {}

template <typename T>
inline T& Promise<T>::value() & noexcept {
  return *_value;
}

template <typename T>
inline T&& Promise<T>::value() && noexcept {
  return ::std::move(*_value);
}

inline constexpr Void Promise<void>::value() noexcept {
  return {};
}
// BasicPromise end
////////////////////////////////////////////////////////////////////////////////

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine

#include "babylon/coroutine/future_awaitable.h"
