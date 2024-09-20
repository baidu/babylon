#pragma once

#include "babylon/concurrent/sched_interface.h"

#include <atomic> // std::atomic
#include <memory> // std::shared_ptr
#include <mutex>  // std::mutex

BABYLON_NAMESPACE_BEGIN

namespace internal {
namespace future {
template <typename T>
struct NonVoid;
template <typename C, typename T>
struct ResultOfCallback;
template <typename C, typename T>
struct IsCompatibleCallback;
} // namespace future
} // namespace internal
template <typename T, typename M>
class FutureContext;
template <typename T, typename M>
class Promise;
template <typename T, typename M = SchedInterface>
class Future {
 public:
  using ResultType = typename internal::future::NonVoid<T>::type;
  template <typename C>
  using IsCompatibleCallback =
      typename internal::future::IsCompatibleCallback<C, T>;
  template <typename C>
  using ThenType = typename internal::future::ResultOfCallback<C, T>::type;
  template <typename C, typename N>
  using ThenFuture = Future<ThenType<C>, N>;
  template <typename C, typename N>
  using ThenPromise = Promise<ThenType<C>, N>;

  // 默认构造的Future不关联Promise
  inline Future() noexcept = default;
  // 可以移动&拷贝
  inline Future(Future&&) noexcept = default;
  inline Future& operator=(Future&&) noexcept = default;
  inline Future(const Future& other) noexcept = default;
  inline Future& operator=(const Future&) noexcept = default;
  inline ~Future() noexcept = default;

  // 是否关联了Promise，大部分操作只有在关联了Promise时可用
  inline bool valid() const noexcept;
  inline operator bool() const noexcept;

  // 检测关联的Promise是否就绪，即关联的Promise::set_value是否完成
  inline bool ready() const noexcept;

  // 使用M阻塞等待关联的Promise直到就绪，之后返回相应的T&
  // T == void的情况下返回值无意义
  inline ResultType& get() noexcept;

  // 使用M阻塞等待关联的Promise直到就绪，或者等待超过timeout
  // 等待成功返回true，等待超时返回false
  template <typename R, typename P>
  inline bool wait_for(
      const ::std::chrono::duration<R, P>& timeout) const noexcept;

  // 注册回调函数，之后取消关联Promise
  // 如果调用on_finish时未就绪，会在就绪后，使用调用Promise::set_value的线程执行回调
  // 如果调用时已经就绪，则使用当前线程原地执行回调
  // C可以是R(T&&)，R(T&)，R(const T&)或者是R()，前三个形式会接收到相应的引用
  // 其中形式R(T&&)和R(T&)使用时应注意底层的T可能被多个Future共享，非独占时需要考虑并发竞争问题
  // 回调收到的T的生命周期只保证到回调结束，如果需要进一步做异步计算，需要合理拷贝或移动
  template <typename C, typename = typename ::std::enable_if<
                            IsCompatibleCallback<C>::value>::type>
  inline void on_finish(C&& callback) noexcept;
  // 和on_finish基本一致，区别是会进一步包装回调函数的执行返回为新的Future
  // 可以方便进行级联调用的编写，和boost::future::then语义类似
  template <typename C, typename = typename ::std::enable_if<
                            IsCompatibleCallback<C>::value>::type>
  inline ThenFuture<C, M> then(C&& callback) noexcept;

 private:
  // 通过FutureContext构造，由Promise使用，将context共享给Future
  inline Future(::std::shared_ptr<FutureContext<T, M>>& context) noexcept;

  ::std::shared_ptr<FutureContext<T, M>> _context;

  friend class Promise<T, M>;
};

template <typename T, typename M = SchedInterface>
class Promise {
 private:
  template <typename C>
  using IsCompatibleCallback =
      typename internal::future::IsCompatibleCallback<C, T>;

 public:
  // 默认构造的Promise是可用的
  inline Promise() noexcept;
  // 可以移动
  inline Promise(Promise&&) noexcept;
  inline Promise& operator=(Promise&&) noexcept;
  // 不能拷贝
  inline Promise(const Promise&) noexcept = delete;
  inline Promise& operator=(const Promise&) noexcept = delete;
  // 析构前必须进行set_value，否则导致等待者无法唤醒
  // 此时会记录FATAL日志
  inline ~Promise() noexcept;

  // 获取关联到自身的Future实例
  inline Future<T, M> get_future() noexcept;

  // 构造并发布数据，进入就绪状态
  // 可以通过Future的get和on_finish获取结果
  template <typename... Args>
  inline void set_value(Args&&... args);

  // 检查当前是否已经完成数据发布，等效于
  // promise.get_future().ready()
  // 不过节省了一次无谓的引用计数操作对
  inline bool ready() const noexcept;

  // 注册发布监听函数，等效于
  // promise.get_future().on_finsih(...)
  // 不过节省了一次无谓的引用计数操作对
  template <typename C, typename = typename ::std::enable_if<
                            IsCompatibleCallback<C>::value>::type>
  inline void on_finish(C&& callback) noexcept;

  // 重置promise状态，执行后自身以及对应的future可以重新使用
  inline void clear() noexcept;

 private:
  ::std::shared_ptr<FutureContext<T, M>> _context;
};

template <typename M = SchedInterface>
class CountDownLatch {
 public:
  // 初始化进行count次计数，count == 0的情况下直接进入就绪态
  inline CountDownLatch(size_t count) noexcept;
  // 可以移动
  inline CountDownLatch(CountDownLatch&&) noexcept;
  inline CountDownLatch& operator=(CountDownLatch&&) noexcept;
  // 不能拷贝
  inline CountDownLatch(const CountDownLatch&) noexcept = delete;
  inline CountDownLatch& operator=(const CountDownLatch&) noexcept = delete;
  inline ~CountDownLatch() noexcept = default;

  // 获取到感知count down到0的Future实例
  inline Future<size_t, M> get_future() noexcept;
  // 递减计数count，当计数到0时通知future
  inline void count_down(size_t down = 1) noexcept;

 private:
  Promise<size_t, M> _promise;
  ::std::atomic<size_t> _count;
};

BABYLON_NAMESPACE_END

#include "babylon/future.hpp"
