#pragma once

#include "babylon/move_only_function.h" // MoveOnlyFunction

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h) // ABSL_PREDICT_FALSE
// clang-format on

#include <cassert> // assert

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace future {

// 当T == void时，利用char[0]改写为零大小类型，方便统一编码处理
template <typename T>
struct NonVoid {
  struct S {
    char s[0];
  };
  typedef
      typename ::std::conditional<::std::is_void<T>::value, S, T>::type type;
};

// 当T& value就绪后，统一执行回调的函数，便于不同的函数签名
// 支持C = R(T&&)，R(const T&)，R(T&)，R()的情况
template <
    typename C, typename T,
    typename ::std::enable_if<IsInvocable<C, T&&>::value, int32_t>::type = 0>
inline auto run_callback(C& callback, T& value) noexcept
    -> decltype(callback(::std::move(value))) {
  return callback(::std::move(value));
}
template <typename C, typename T,
          typename ::std::enable_if<!IsInvocable<C, T&&>::value &&
                                        IsInvocable<C, T&>::value,
                                    int32_t>::type = 0>
inline auto run_callback(C& callback, T& value) noexcept
    -> decltype(callback(value)) {
  return callback(value);
}
template <typename C, typename T,
          typename ::std::enable_if<IsInvocable<C>::value, int32_t>::type = 0>
inline auto run_callback(C& callback, T&) noexcept -> decltype(callback()) {
  return callback();
}

// 执行回调函数的返回值类型
template <typename C, typename T>
struct ResultOfCallback {
  using U = typename NonVoid<T>::type;
  using F = decltype(&run_callback<C, U>);
  using type = ::std::invoke_result_t<F, C&, U&>;
};

// 可以支持使用run_callback(const C&, T&&)来执行的回调函数
// 实际会根据C的情况确定如何传递T，以便实际支持多个模式
// R(T&)，R(T&&)，R(const T&)，R()
template <typename C, typename T>
struct IsCompatibleCallback {
  static constexpr bool value =
      IsInvocable<C, typename NonVoid<T>::type&&>::value ||
      IsInvocable<C, typename NonVoid<T>::type&>::value ||
      IsInvocable<C>::value;
};

// 执行回调并设置到另一个Promise中，适配void类型
template <typename T, typename M, typename C, typename U,
          typename ::std::enable_if<
              !::std::is_void<::std::invoke_result_t<
                  decltype(&run_callback<C, U>), C&, U&>>::value,
              int>::type = 0>
inline void run_callback(Promise<T, M>& promise, C& callback,
                         U& value) noexcept {
  promise.set_value(run_callback<>(callback, value));
}
template <typename T, typename M, typename C, typename U,
          typename ::std::enable_if<
              ::std::is_void<::std::invoke_result_t<
                  decltype(&run_callback<C, U>), C&, U&>>::value,
              int>::type = 0>
inline void run_callback(Promise<T, M>& promise, C& callback,
                         U& value) noexcept {
  run_callback(callback, value);
  promise.set_value();
}

} // namespace future
} // namespace internal

// future和promise底层的共享状态
template <typename T, typename M>
class FutureContext {
 public:
  using ResultType = typename Future<T, M>::ResultType;
  using RemoveReferenceType =
      typename ::std::remove_reference<ResultType>::type;
  using ValueType =
      typename ::std::conditional<::std::is_lvalue_reference<ResultType>::value,
                                  ::std::reference_wrapper<RemoveReferenceType>,
                                  RemoveReferenceType>::type;

  // Promise的构造和赋值是分离的，为了支持无法默认构造的类型
  // 使用未初始化块替代类型本身作为成员，后续显式调用构造和析构
  // 回调链节点
  struct CallbackNode {
    template <typename C>
    inline CallbackNode(C&& function) noexcept;

    MoveOnlyFunction<void(void)> function;
    CallbackNode* next;
  };

  template <typename C>
  using IsCompatibleCallback = internal::future::IsCompatibleCallback<C, T>;

  // 共享状态生命周期由unique_ptr管理，不提供拷贝和移动能力
  inline FutureContext() noexcept;
  inline FutureContext(const FutureContext&) noexcept = delete;
  inline FutureContext(FutureContext&&) noexcept = delete;
  inline FutureContext& operator=(const FutureContext&) noexcept = delete;
  inline FutureContext& operator=(FutureContext&&) noexcept = delete;
  inline ~FutureContext() noexcept;

  // 检查是否已经就绪
  inline bool ready(::std::memory_order memory_order) const noexcept;
  // 构造赋值并发布数据，生命周期内只能调用一次
  template <typename... Args,
            typename = typename ::std::enable_if<
                ::std::is_constructible<ValueType, Args...>::value>::type>
  inline void set_value(Args&&... args);
  // 等待数据发布完成，之后返回引用
  inline ValueType& get() noexcept;
  template <typename R, typename P>
  inline bool wait_for(const ::std::chrono::duration<R, P>& timeout) noexcept;
  // 注册回调，就绪后会激活回调函数
  template <typename C, typename = typename ::std::enable_if<
                            IsCompatibleCallback<C>::value>::type>
  inline void on_finish(C&& callback) noexcept;
  // 已经注册了回调，主要用于检测遗漏数据发布的情况
  inline bool has_callback(::std::memory_order memory_order) const noexcept;
  // 清理执行环境，清理后可以再次使用
  inline void clear() noexcept;

 private:
  static constexpr uint64_t SEALED_HEAD_VALUE = 0xFFFFFFFFFFFFFFFFL;
  // 表示已经就绪，不再接受等待者注册
  static constexpr uint32_t READY_MASK = 0x80000000U;

  inline static constexpr bool is_sealed(CallbackNode* head) noexcept;

  inline CallbackNode* seal() noexcept;

  inline ValueType& value() noexcept;
  inline ValueType* pointer() noexcept;

  void wait_slow() noexcept;
  bool wait_for_slow(int64_t timeout_ns) noexcept;

  Futex<M> _futex {0};
  ::std::atomic<CallbackNode*> _head {nullptr};
  alignas(ValueType) uint8_t _storage[sizeof(ValueType)];
};

///////////////////////////////////////////////////////////////////////////////
// FutureContext begin
template <typename T, typename M>
template <typename C>
inline FutureContext<T, M>::CallbackNode::CallbackNode(C&& callable) noexcept
    : function(::std::forward<C>(callable)) {}

template <typename T, typename M>
inline FutureContext<T, M>::FutureContext() noexcept {}

template <typename T, typename M>
inline FutureContext<T, M>::~FutureContext() noexcept {
  auto* head = _head.load(::std::memory_order_relaxed);
  if (is_sealed(head)) {
    value().~ValueType();
  } else {
    while (head != nullptr) {
      auto* next_head = head->next;
      delete head;
      head = next_head;
    }
  }
}

template <typename T, typename M>
inline bool FutureContext<T, M>::ready(
    ::std::memory_order memory_order) const noexcept {
  return is_sealed(_head.load(memory_order));
}

template <typename T, typename M>
template <typename... Args, typename>
inline void FutureContext<T, M>::set_value(Args&&... args) {
  // 构造并设置value
  new (pointer()) ValueType(::std::forward<Args>(args)...);
  // 原子发布数据：
  // 1、获取当前注册的回调链
  // 2、标记后续不再接受回调注册
  auto head = seal();
  // 唤醒阻塞等待者
  auto waiter_num =
      _futex.value().exchange(READY_MASK, ::std::memory_order_release);
  if (waiter_num > 0) {
    _futex.wake_all();
  }
  // 运行回调链
  while (head != nullptr) {
    head->function();
    auto* next_head = head->next;
    delete head;
    head = next_head;
  }
}

template <typename T, typename M>
inline typename FutureContext<T, M>::ValueType&
FutureContext<T, M>::get() noexcept {
  auto futex_value = _futex.value().load(::std::memory_order_acquire);
  if (!(futex_value & READY_MASK)) {
    wait_slow();
  }
  return value();
}

template <typename T, typename M>
template <typename R, typename P>
inline bool FutureContext<T, M>::wait_for(
    const ::std::chrono::duration<R, P>& timeout) noexcept {
  auto value = _futex.value().load(::std::memory_order_acquire);
  if (value & READY_MASK) {
    return true;
  }
  int64_t timeout_ns = ::std::chrono::nanoseconds(timeout).count();
  return wait_for_slow(::std::max<int64_t>(0, timeout_ns));
}

template <typename T, typename M>
template <typename C, typename>
inline void FutureContext<T, M>::on_finish(C&& callback) noexcept {
  auto head = _head.load(::std::memory_order_acquire);
  if (is_sealed(head)) {
// In set_value Seal head is done in release order after value assign. This
// ensure value definitely initialized here.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    internal::future::run_callback(callback, value());
#pragma GCC diagnostic pop
    return;
  }

#if __cplusplus >= 201402L
  auto node = new CallbackNode(
      [this, captured_callback = ::std::forward<C>(callback)]() mutable {
        internal::future::run_callback(captured_callback, value());
      });
#else  // __cplusplus < 201402L
  auto node = new CallbackNode(
      ::std::bind(internal::future::run_callback<C, ValueType>,
                  uncomposable_bind_argument(::std::forward<C>(callback)),
                  ::std::ref(*pointer())));
#endif // __cplusplus

  while (true) {
    node->next = head;
    if (_head.compare_exchange_weak(head, node, ::std::memory_order_acq_rel)) {
      break;
    }
    if (is_sealed(head)) {
      node->function();
      delete node;
      break;
    }
  }
}

template <typename T, typename M>
inline bool FutureContext<T, M>::has_callback(
    ::std::memory_order memory_order) const noexcept {
  auto head = _head.load(memory_order);
  return !is_sealed(head) && head != nullptr;
}

template <typename T, typename M>
inline void FutureContext<T, M>::clear() noexcept {
  _futex.value().store(0, ::std::memory_order_relaxed);
  auto* head = _head.exchange(nullptr, ::std::memory_order_relaxed);
  if (is_sealed(head)) {
    pointer()->~ValueType();
  } else {
    while (head != nullptr) {
      auto* next_head = head->next;
      delete head;
      head = next_head;
    }
  }
}

template <typename T, typename M>
inline constexpr bool FutureContext<T, M>::is_sealed(
    CallbackNode* head) noexcept {
  return SEALED_HEAD_VALUE == reinterpret_cast<uint64_t>(head);
}

template <typename T, typename M>
inline typename FutureContext<T, M>::CallbackNode*
FutureContext<T, M>::seal() noexcept {
  return _head.exchange(reinterpret_cast<CallbackNode*>(SEALED_HEAD_VALUE),
                        ::std::memory_order_acq_rel);
}

template <typename T, typename M>
inline typename FutureContext<T, M>::ValueType&
FutureContext<T, M>::value() noexcept {
  assert(ready(::std::memory_order_acquire) &&
         "cannot read value before ready");
  return *pointer();
}

template <typename T, typename M>
inline typename FutureContext<T, M>::ValueType*
FutureContext<T, M>::pointer() noexcept {
  return reinterpret_cast<ValueType*>(_storage);
}

template <typename T, typename M>
ABSL_ATTRIBUTE_NOINLINE void FutureContext<T, M>::wait_slow() noexcept {
  auto value = _futex.value().fetch_add(1, ::std::memory_order_acquire) + 1;
  while (!(value & READY_MASK)) {
    _futex.wait(value, nullptr);
    value = _futex.value().load(::std::memory_order_acquire);
  }
}

template <typename T, typename M>
ABSL_ATTRIBUTE_NOINLINE bool FutureContext<T, M>::wait_for_slow(
    int64_t timeout_ns) noexcept {
  ::timespec spec;
  if (ABSL_PREDICT_FALSE(0 != ::clock_gettime(CLOCK_MONOTONIC, &spec))) {
    return false;
  }
  int64_t until_ns = static_cast<int64_t>(spec.tv_sec) * (1000 * 1000 * 1000);
  until_ns += spec.tv_nsec + timeout_ns;

  auto value = _futex.value().fetch_add(1, ::std::memory_order_acquire) + 1;
  while (!(value & READY_MASK)) {
    spec.tv_sec = timeout_ns / (1000 * 1000 * 1000);
    spec.tv_nsec = timeout_ns % (1000 * 1000 * 1000);
    _futex.wait(value, &spec);
    value = _futex.value().load(::std::memory_order_acquire);

    if (ABSL_PREDICT_FALSE(0 != ::clock_gettime(CLOCK_MONOTONIC, &spec))) {
      return false;
    }
    int64_t now_ns = static_cast<int64_t>(spec.tv_sec) * (1000 * 1000 * 1000);
    now_ns += spec.tv_nsec;
    timeout_ns = until_ns - now_ns;
    if (timeout_ns <= 0) {
      return false;
    }
  }
  return true;
}
// FutureContext end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Future begin
template <typename T, typename M>
inline bool Future<T, M>::valid() const noexcept {
  return static_cast<bool>(_context);
}

template <typename T, typename M>
inline Future<T, M>::operator bool() const noexcept {
  return valid();
}

template <typename T, typename M>
inline bool Future<T, M>::ready() const noexcept {
  return valid() && _context->ready(::std::memory_order_acquire);
}

template <typename T, typename M>
inline typename Future<T, M>::ResultType& Future<T, M>::get() noexcept {
  if (ABSL_PREDICT_TRUE(valid())) {
    return _context->get();
  }

  assert(false && "get value from invalid future");

  static typename ::std::aligned_storage<
      sizeof(ResultType), alignof(ResultType)>::type uninitialized_value;
  return reinterpret_cast<ResultType&>(uninitialized_value);
}

template <typename T, typename M>
template <typename R, typename P>
inline bool Future<T, M>::wait_for(
    const ::std::chrono::duration<R, P>& timeout) const noexcept {
  if (ABSL_PREDICT_TRUE(valid())) {
    return _context->wait_for(timeout);
  }
  assert(false && "wait on invalid future");
  return false;
}

template <typename T, typename M>
template <typename C, typename>
inline void Future<T, M>::on_finish(C&& callback) noexcept {
  assert(valid() && "try watch invalid future");
  _context->on_finish(::std::forward<C>(callback));
}

template <typename T, typename M>
template <typename C, typename>
inline typename Future<T, M>::template ThenFuture<C, M> Future<T, M>::then(
    C&& callback) noexcept {
  ThenPromise<C, M> promise;
  auto future = promise.get_future();
#if __cplusplus >= 201402L
  on_finish([captured_promise = ::std::move(promise),
             captured_callback =
                 ::std::forward<C>(callback)](ResultType& value) mutable {
    internal::future::run_callback(captured_promise, captured_callback, value);
  });
#else  // __cplusplus < 201402L
  on_finish(
      ::std::bind(internal::future::run_callback<ThenType<C>, M, C, ResultType>,
                  ::std::move(promise),
                  uncomposable_bind_argument(::std::forward<C>(callback)),
                  ::std::placeholders::_1));
#endif // __cplusplus
  return future;
}

template <typename T, typename M>
inline Future<T, M>::Future(
    ::std::shared_ptr<FutureContext<T, M>>& context) noexcept
    : _context(context) {}
// Future end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Promise begin
template <typename T, typename M>
inline Promise<T, M>::Promise() noexcept
    : _context(::std::make_shared<FutureContext<T, M>>()) {}

template <typename T, typename M>
inline Promise<T, M>::Promise(Promise&& other) noexcept
    : _context(::std::move(other._context)) {}

template <typename T, typename M>
inline Promise<T, M>& Promise<T, M>::operator=(Promise&& other) noexcept {
  _context = ::std::move(other._context);
  return *this;
}

template <typename T, typename M>
inline Promise<T, M>::~Promise() noexcept {
  // 已经完成了set_value
  assert(!_context || _context->ready(::std::memory_order_relaxed) ||
         // 否则不能有其他Future在等待
         (_context.use_count() == 1 &&
          // 也不能有回调注册
          !_context->has_callback(::std::memory_order_relaxed)));
}

template <typename T, typename M>
inline Future<T, M> Promise<T, M>::get_future() noexcept {
  return Future<T, M>(_context);
}

template <typename T, typename M>
template <typename... Args>
inline void Promise<T, M>::set_value(Args&&... args) {
  if (ABSL_PREDICT_TRUE(_context &&
                        !_context->ready(::std::memory_order_relaxed))) {
    auto context = _context; // 确保context在set_value执行完后再析构
    context->set_value(::std::forward<Args>(args)...);
    return;
  }

  assert(false && "set value to empty or ready promise");
}

template <typename T, typename M>
inline bool Promise<T, M>::ready() const noexcept {
  return _context->ready(::std::memory_order_acquire);
}

template <typename T, typename M>
template <typename C, typename>
inline void Promise<T, M>::on_finish(C&& callback) noexcept {
  _context->on_finish(::std::forward<C>(callback));
}

template <typename T, typename M>
inline void Promise<T, M>::clear() noexcept {
  _context->clear();
}
// Promise end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CountDownLatch begin
template <typename M>
inline CountDownLatch<M>::CountDownLatch(size_t count) noexcept
    : _count(count) {
  if (ABSL_PREDICT_FALSE(_count == 0)) {
    _promise.set_value(0);
  }
}

template <typename M>
inline CountDownLatch<M>::CountDownLatch(CountDownLatch&& other) noexcept
    : _count(other._count.load(::std::memory_order_relaxed)),
      _promise(::std::move(other._promise)) {}

template <typename M>
inline CountDownLatch<M>& CountDownLatch<M>::operator=(
    CountDownLatch&& other) noexcept {
  auto count = _count.load(::std::memory_order_relaxed);
  _count.store(other._count.load(::std::memory_order_relaxed),
               ::std::memory_order_relaxed);
  other._count.store(count, ::std::memory_order_relaxed);
  Promise<size_t, M> promise = ::std::move(_promise);
  _promise = ::std::move(other._promise);
  other._promise = ::std::move(promise);
  return *this;
}

template <typename M>
inline Future<size_t, M> CountDownLatch<M>::get_future() noexcept {
  return _promise.get_future();
}

template <typename M>
inline void CountDownLatch<M>::count_down(size_t down) noexcept {
  auto count = _count.fetch_sub(down, ::std::memory_order_acq_rel) - down;
  if (count == 0) {
    _promise.set_value(0);
  }
}
// CountDownLatch end
///////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
