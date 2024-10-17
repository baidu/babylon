#pragma once

#include "babylon/concurrent/deposit_box.h"
#include "babylon/coroutine/task.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

// Common part of Cancellable<A>
class BasicCancellable {
 public:
  // std::optional dont support void type, replace to babylon::Void for
  // convenience
  template <typename T>
  struct Optional;
  template <typename T>
  using OptionalType = typename Optional<T>::type;
  class Cancellation;

  // Setup a coroutine_handle as a callback, will be resumed after finish or
  // cancel.
  template <typename P>
  inline VersionedValue<uint32_t> emplace(
      ::std::coroutine_handle<P> handle) noexcept;

  // Setup the proxy coroutine. Awaitee of Cancellable will be add to proxy
  // coroutine only when resume successfully. Otherwise the awaitee will
  // directly resume on cancel, leave proxy coroutine standalone.
  inline void set_proxy_promise(BasicPromise* proxy_promise) noexcept;
  inline BasicPromise* proxy_promise() noexcept;

  // Use id returned by emplace to resume/cancel without **this** object.
  // An id can only be resume/cancel only once, first one success and is safe to
  // delete **this**, later one will fail without touching **this**.
  inline static bool cancel(VersionedValue<uint32_t> id) noexcept;
  inline static bool resume(VersionedValue<uint32_t> id) noexcept;

  // Awaitee can check whether triggered by resume or cancel.
  inline bool canceled() const noexcept;

 private:
  inline void do_cancel() noexcept;
  inline void do_resume() noexcept;

  BasicPromise* _proxy_promise {nullptr};
  BasicPromise* _promise {nullptr};
  ::std::coroutine_handle<> _handle;
  bool _canceled {false};
};

template <typename T>
struct BasicCancellable::Optional {
  using type = ::absl::optional<T>;
};
template <>
struct BasicCancellable::Optional<void> {
  struct type : public ::absl::optional<Void> {
    using ::absl::optional<Void>::optional;
    inline constexpr void operator*() const noexcept {}
  };
};

// Handle get back from Cancellable::on_suspend, Use operator() to trigger
// cancellation.
class BasicCancellable::Cancellation {
 public:
  inline Cancellation() noexcept = default;
  inline Cancellation(Cancellation&&) noexcept = default;
  inline Cancellation(const Cancellation&) noexcept = default;
  inline Cancellation& operator=(Cancellation&&) noexcept = default;
  inline Cancellation& operator=(const Cancellation&) noexcept = default;
  inline ~Cancellation() noexcept = default;

  inline bool operator()() const noexcept;

 private:
  inline Cancellation(VersionedValue<uint32_t> id) noexcept;

  VersionedValue<uint32_t> _id;

  template <typename A>
  friend class Cancellable; // Cancellation(VersionedValue<uint32_t>);
};

// Wrap an awaitable A to make it cancelable.
//
// Cancellable<A> can be co_await-ed just like A it self, but get an optional<T>
// instead. Additionally, a callback can be registered with on_suspend. When
// suspend happen in co_await, this callback will be called with a cancellation
// token.
//
// This cancellation token can be saved and used later to resume that
// suspension, before the inner awaitable A finished. If resumed by
// cancellation, the result optional<T> is empty.
//
// Also, it is safe to do cancellation after inner awaitable A finished. The
// resumption will happen only once.
//
// The typical usage of Cancellable is to add timeout support for co_await, by
// sending the cancellation token to a timer. E.g.
// replace:
// T result = co_await awaitable;
// to:
// optional<T> result = co_await
// Cancellable<A>(awaitable).on_suspend([](Cancellation token) {
//   on_timer(token, 100ms);
// });
template <typename A>
class Cancellable : public BasicCancellable {
 public:
  using ResultType = AwaitResultType<A, BasicPromise>;
  using OptionalResultType = OptionalType<ResultType>;

  inline explicit Cancellable(A awaitable) noexcept;

  // Called as callable(Cancellation) when co_await suspend current coroutine.
  // Received Cancellation can be invoked to cancel this co_await, at **any**
  // time, even inside the callback it self or long after awaitable finished.
  //
  // Usually it can be send to a timer and be called unconditionally after
  // arbitrary period. The co_await will canceled if not finished after that
  // period, or the cancellation will just no-op.
  template <typename C>
  inline Cancellable<A>& on_suspend(C&& callable) & noexcept;
  template <typename C>
  inline Cancellable<A>&& on_suspend(C&& callable) && noexcept;

  // Cancellable it self is awaitable by proxy to awaitable A inside.
  inline constexpr bool await_ready() noexcept;
  template <typename P>
    requires(::std::is_base_of<BasicPromise, P>::value)
  inline ::std::coroutine_handle<> await_suspend(
      ::std::coroutine_handle<P> handle) noexcept;
  inline OptionalResultType await_resume() noexcept;

 private:
  A _awaitable;
  Task<ResultType> _task;
  MoveOnlyFunction<void(Cancellation&&)> _on_suspend;
};

template <typename A>
class BasicPromise::Transformer<Cancellable<A>> {
 public:
  inline static Cancellable<A>&& await_transform(BasicPromise&,
                                                 Cancellable<A>&& awaitable) {
    return ::std::move(awaitable);
  }
};

////////////////////////////////////////////////////////////////////////////////
// BasicCancellable begin
template <typename P>
inline VersionedValue<uint32_t> BasicCancellable::emplace(
    ::std::coroutine_handle<P> handle) noexcept {
  _promise = &handle.promise();
  _handle = handle;
  return DepositBox<BasicCancellable*>::instance().emplace(this);
}

inline void BasicCancellable::set_proxy_promise(
    BasicPromise* proxy_promise) noexcept {
  _proxy_promise = proxy_promise;
}

inline BasicPromise* BasicCancellable::proxy_promise() noexcept {
  return _proxy_promise;
}

inline bool BasicCancellable::cancel(VersionedValue<uint32_t> id) noexcept {
  auto accessor = DepositBox<BasicCancellable*>::instance().take(id);
  if (accessor) {
    (*accessor)->do_cancel();
    return true;
  }
  return false;
}

inline bool BasicCancellable::resume(VersionedValue<uint32_t> id) noexcept {
  auto accessor = DepositBox<BasicCancellable*>::instance().take(id);
  if (accessor) {
    (*accessor)->do_resume();
    return true;
  }
  return false;
}

inline bool BasicCancellable::canceled() const noexcept {
  return _canceled;
}

inline void BasicCancellable::do_cancel() noexcept {
  _canceled = true;
  _promise->resume(_handle);
}

inline void BasicCancellable::do_resume() noexcept {
  _proxy_promise->set_awaiter(_handle, _promise->executor());
}
// BasicCancellable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Cancellation begin
inline bool BasicCancellable::Cancellation::operator()() const noexcept {
  return BasicCancellable::cancel(_id);
}

inline BasicCancellable::Cancellation::Cancellation(
    VersionedValue<uint32_t> id) noexcept
    : _id {id} {}
// Cancellation end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Cancellable begin
template <typename A>
inline Cancellable<A>::Cancellable(A awaitable) noexcept
    : _awaitable {::std::move(awaitable)} {}

template <typename A>
template <typename C>
inline Cancellable<A>& Cancellable<A>::on_suspend(C&& callable) & noexcept {
  _on_suspend = ::std::forward<C>(callable);
  return *this;
}

template <typename A>
template <typename C>
inline Cancellable<A>&& Cancellable<A>::on_suspend(C&& callable) && noexcept {
  return ::std::move(on_suspend(::std::forward<C>(callable)));
}

template <typename A>
inline constexpr bool Cancellable<A>::await_ready() noexcept {
  return false;
}

template <typename A>
template <typename P>
  requires(::std::is_base_of<BasicPromise, P>::value)
inline ::std::coroutine_handle<> Cancellable<A>::await_suspend(
    ::std::coroutine_handle<P> handle) noexcept {
  auto id = emplace(handle);
  _task = [](A awaitable, VersionedValue<uint32_t> id) -> Task<ResultType> {
    struct S {
      inline ~S() noexcept {
        resume(id);
      }
      VersionedValue<uint32_t> id;
    } s {.id {id}};
    co_return co_await ::std::forward<A>(awaitable);
  }(::std::forward<A>(_awaitable), id);
  auto proxy_handle = _task.handle();
  set_proxy_promise(&proxy_handle.promise());
  if (_on_suspend) {
    _on_suspend(Cancellation {id});
  }
  return proxy_handle;
}

template <typename A>
inline typename Cancellable<A>::OptionalResultType
Cancellable<A>::await_resume() noexcept {
  if (!canceled()) {
    return ::std::move(_task.handle().promise().value());
  }
  _task.release();
  return {};
}
// Cancellable end
////////////////////////////////////////////////////////////////////////////////

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
