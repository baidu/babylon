#pragma once

#include "babylon/coroutine/promise.h" // BasicPromise
#include "babylon/future.h"            // Future

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

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

  template <typename P>
    requires(::std::is_base_of<BasicPromise, P>::value)
  inline void await_suspend(::std::coroutine_handle<P> handle) noexcept {
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

template <typename T, typename F = SchedInterface>
class FutureAwaitable : public BasicFutureAwaitable<T, F> {
 private:
  using Base = BasicFutureAwaitable<T, F>;

 public:
  using Base::Base;
  inline T&& await_resume() noexcept {
    return ::std::move(Base::await_resume());
  }
};

template <typename T, typename F = SchedInterface>
class SharedFutureAwaitable : public BasicFutureAwaitable<T, F> {
 public:
  using BasicFutureAwaitable<T, F>::BasicFutureAwaitable;
};

template <typename T, typename F>
class BasicPromise::Transformer<Future<T, F>> {
 public:
  static FutureAwaitable<T, F> await_transform(BasicPromise&,
                                               Future<T, F>&& future) {
    return ::std::move(future);
  }
  static SharedFutureAwaitable<T, F> await_transform(BasicPromise&,
                                                     Future<T, F>& future) {
    return future;
  }
};

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
