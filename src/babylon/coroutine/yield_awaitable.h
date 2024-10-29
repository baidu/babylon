#pragma once

#include "babylon/coroutine/promise.h" // BasicPromise

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

class YieldAwaitable {
 public:
  inline bool await_ready() const noexcept;
  template <typename P>
    requires(::std::is_base_of<BasicPromise, P>::value)
  inline void await_suspend(::std::coroutine_handle<P> handle) noexcept;
  inline static constexpr void await_resume() noexcept;

  inline YieldAwaitable&& set_non_inplace() && noexcept;

 private:
  inline void set_ready(bool ready) noexcept;

  bool _ready {false};
  bool _non_inplace {false};

  friend BasicPromise::Transformer<YieldAwaitable>;
};

template <>
class BasicPromise::Transformer<YieldAwaitable> {
 public:
  static YieldAwaitable await_transform(BasicPromise& promise,
                                        YieldAwaitable&& awaitable) {
    if (ABSL_PREDICT_FALSE(promise.executor() == nullptr)) {
      awaitable.set_ready(true);
    }
    return ::std::move(awaitable);
  }
};

////////////////////////////////////////////////////////////////////////////////
// YieldAwaitable begin
inline bool YieldAwaitable::await_ready() const noexcept {
  return _ready;
}

template <typename P>
  requires(::std::is_base_of<BasicPromise, P>::value)
inline void YieldAwaitable::await_suspend(
    ::std::coroutine_handle<P> handle) noexcept {
  if (_non_inplace) {
    BasicExecutor::RunnerScope runner_scope(
        *reinterpret_cast<BasicExecutor*>(0));
    handle.promise().resume(handle);
  } else {
    handle.promise().resume(handle);
  }
}

inline constexpr void YieldAwaitable::await_resume() noexcept {}

inline YieldAwaitable&& YieldAwaitable::set_non_inplace() && noexcept {
  _non_inplace = true;
  return ::std::move(*this);
}

inline void YieldAwaitable::set_ready(bool ready) noexcept {
  _ready = ready;
}
// YieldAwaitable end
////////////////////////////////////////////////////////////////////////////////

inline YieldAwaitable yield() noexcept {
  return {};
}

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
