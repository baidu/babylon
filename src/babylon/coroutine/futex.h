#pragma once

#include "babylon/concurrent/deposit_box.h"
#include "babylon/coroutine/promise.h"

#include <mutex>

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

// Work like FUTEX(2), but suspend corouine instead of thread.
class Futex {
 public:
  class Awaitable;
  class Cancellation;

  inline Futex() noexcept = default;
  Futex(Futex&&) = delete;
  Futex(const Futex&&) = delete;
  Futex& operator=(Futex&&) = delete;
  Futex& operator=(const Futex&&) = delete;
  inline ~Futex() noexcept = default;

  // Get the futex word to manipulate
  inline uint64_t& value() noexcept;
  inline ::std::atomic<uint64_t>& atomic_value() noexcept;

  // Wait and wakeup on futex. Result Awaitable is expected used for co_await on
  // it. When co_await on this result awaitable, the coroutine will only be
  // suspended when futex word is equal to the expected_value.
  //
  // The value check and suspend happen atomically just like FUTEX(2).
  inline Awaitable wait(uint64_t expected_value) noexcept;
  int wake_one() noexcept;
  int wake_all() noexcept;

 private:
  struct Node;
  struct BasicNode {
    BasicNode* prev {nullptr};
    Node* next {nullptr};
  };

  bool add_awaiter(Node* node, uint64_t expected_value) noexcept;
  void remove_awaiter(Node* node) noexcept;

  ::std::mutex _mutex;
  uint64_t _value {0};

  BasicNode _awaiter_head;
};

// Result of Futex::wait. Like Cancellable<T>, this result also support register
// a callable with on_suspend to get a cancellation token back. The cancellation
// token is used to stop co_await before any wake operation happen. typical
// usage is to add timeout support for co_await, by sending the cancellation
// token to a timer. E.g.
//
// futex.wait(25).on_suspend([](Cancellation token) {
//   on_timer(token, 100ms);
// });
class Futex::Awaitable {
 public:
  template <typename C>
  inline Awaitable& on_suspend(C&& callable) & noexcept;
  template <typename C>
  inline Awaitable&& on_suspend(C&& callable) && noexcept;

  inline bool await_ready() noexcept;
  template <typename P>
    requires(::std::is_base_of<BasicPromise, P>::value)
  inline bool await_suspend(::std::coroutine_handle<P> handle) noexcept;
  inline static constexpr void await_resume() noexcept;

 private:
  inline Awaitable(Futex* futex, uint64_t expected_value) noexcept;
  inline static bool cancel(VersionedValue<uint32_t> id) noexcept;

  Futex* _futex {nullptr};
  uint64_t _expected_value {0};
  MoveOnlyFunction<void(Cancellation&&)> _on_suspend;

  friend Futex;
};

class Futex::Cancellation {
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

  friend Awaitable;
};

struct Futex::Node : public Futex::BasicNode {
  Futex* futex {nullptr};
  VersionedValue<uint32_t> id {0};
  BasicPromise* promise {nullptr};
  ::std::coroutine_handle<> handle;
};

template <>
class BasicPromise::Transformer<Futex::Awaitable> {
 public:
  inline static Futex::Awaitable&& await_transform(
      BasicPromise&, Futex::Awaitable&& awaitable) {
    return ::std::move(awaitable);
  }
};

////////////////////////////////////////////////////////////////////////////////
// Futex::Awaitable begin
template <typename C>
inline Futex::Awaitable& Futex::Awaitable::on_suspend(C&& callable) & noexcept {
  _on_suspend = ::std::forward<C>(callable);
  return *this;
}

template <typename C>
inline Futex::Awaitable&& Futex::Awaitable::on_suspend(
    C&& callable) && noexcept {
  return ::std::move(on_suspend(::std::forward<C>(callable)));
}

inline bool Futex::Awaitable::await_ready() noexcept {
  return false;
}

template <typename P>
  requires(::std::is_base_of<BasicPromise, P>::value)
inline bool Futex::Awaitable::await_suspend(
    ::std::coroutine_handle<P> handle) noexcept {
  auto& box = DepositBox<Node>::instance();
  auto id = box.emplace();
  auto node = &box.unsafe_get(id);
  node->futex = _futex;
  node->id = id;
  node->promise = &handle.promise();
  node->handle = handle;
  auto success = _futex->add_awaiter(node, _expected_value);
  if (success && _on_suspend) {
    _on_suspend({id});
  }
  return success;
}

inline constexpr void Futex::Awaitable::await_resume() noexcept {}

inline Futex::Awaitable::Awaitable(Futex* futex,
                                   uint64_t expected_value) noexcept
    : _futex {futex}, _expected_value {expected_value} {}

inline bool Futex::Awaitable::cancel(VersionedValue<uint32_t> id) noexcept {
  auto& box = DepositBox<Node>::instance();
  auto node = box.take_released(id);
  if (!node) {
    return false;
  }
  node->futex->remove_awaiter(node);
  node->promise->resume(node->handle);
  box.finish_released(id);
  return true;
}
// Futex::Awaitable end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Futex::Cancellation begin
inline bool Futex::Cancellation::operator()() const noexcept {
  return Futex::Awaitable::cancel(_id);
}

inline Futex::Cancellation::Cancellation(VersionedValue<uint32_t> id) noexcept
    : _id {id} {}
// Futex::Cancellation end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Futex begin
inline uint64_t& Futex::value() noexcept {
  return _value;
}

inline ::std::atomic<uint64_t>& Futex::atomic_value() noexcept {
  return reinterpret_cast<::std::atomic<uint64_t>&>(_value);
}

inline Futex::Awaitable Futex::wait(uint64_t expected_value) noexcept {
  return {this, expected_value};
}
// Futex end
////////////////////////////////////////////////////////////////////////////////

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
