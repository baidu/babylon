#pragma once

#include "babylon/concurrent/bounded_queue.h"
#include "babylon/executor.h"

BABYLON_NAMESPACE_BEGIN

// 采用ConcurrentBoundedQueue实现的MPSC按需消费队列
// 按需消费机制类似bthread::ExecutionQueue，在队列持续为空时，无消费者线程
// 当开始加入待消费数据时，按需启动消费线程，并在消费完成后退出
// 典型在需要大量偶发活跃队列的场景（比如大量socket，大量raft
// log），可以显著节省线程数 在使用系统线程执行时尤其重要
// 即使是使用类似bthread的协程模式，节省栈数目也可以节省内存开销
template <typename T, typename S = SchedInterface>
class ConcurrentExecutionQueue {
 private:
  using Queue = ConcurrentBoundedQueue<T, S>;

 public:
  using Iterator = typename Queue::Iterator;

  ConcurrentExecutionQueue() = default;
  ConcurrentExecutionQueue(ConcurrentExecutionQueue&&) = delete;
  ConcurrentExecutionQueue(const ConcurrentExecutionQueue&) = delete;
  ConcurrentExecutionQueue& operator=(ConcurrentExecutionQueue&&) = delete;
  ConcurrentExecutionQueue& operator=(const ConcurrentExecutionQueue&) = delete;
  ~ConcurrentExecutionQueue() noexcept = default;

  // 初始化
  // capacity: 队列容量，积攒的未执行数据超过容量时提交被阻塞
  // executor: 消费者执行器，在需要激活消费者时，通过这个执行器提交消费者任务
  //           例如使用ThreadPoolExecutor可以采用线程池来异步执行消费
  //           也可以使用InplaceExecutor来原地执行，让第一个生产者转化为消费者
  // consume_function: 实际执行消费动作的线程，签名为void(T* begin, T*
  // end)，区间为[begin, end)语义
  template <typename C>
  int initialize(size_t capacity_hint, Executor& executor,
                 C&& consume_function) noexcept;

  // 队列实际容量
  inline size_t capacity() const noexcept;

  // 队列待消费数
  inline size_t size() const noexcept;

  // 提交接口，入队数据，并按需启动消费线程，队列满时会阻塞等待
  // return 0: 成功
  //        !0: 需要启动消费线程，但启动失败，数据正常入队，但是无法被消费
  //            如果Executor异常可恢复，恢复后下一个任务到来时可自动恢复消费
  //            如果任务本身不确保一定会出现，恢复后可以主动调用signal_push_event恢复消费
  //            signal_push_event多线程多次调用也是线程安全的
  inline int execute(T&& value) noexcept;
  inline int execute(const T& value) noexcept;

  // 一般无需主动调用，提交接口内部会自动触发
  // 仅在提交接口遇到Executor异常失败时，可以用于主动恢复
  // return 0: 成功
  //        !0: 需要启动消费线程，但启动失败
  inline int signal_push_event() noexcept;

  // 等待当前已经入队的数据全部消费完成
  void join() noexcept;

 private:
  using ConsumeFunction = MoveOnlyFunction<void(Iterator, Iterator)>;

  int start_consumer() noexcept;
  void consume_until_empty() noexcept;

  Queue _queue;
  ::std::atomic<size_t> _events {0};
  Executor* _executor {nullptr};
  ConsumeFunction _consume_function;
};

template <typename T, typename S>
template <typename C>
int ConcurrentExecutionQueue<T, S>::initialize(size_t capacity_hint,
                                               Executor& executor,
                                               C&& consume_function) noexcept {
  _queue.reserve_and_clear(capacity_hint);
  _executor = &executor;
  _consume_function = ::std::forward<C>(consume_function);
  return 0;
}

template <typename T, typename S>
inline size_t ConcurrentExecutionQueue<T, S>::capacity() const noexcept {
  return _queue.capacity();
}

template <typename T, typename S>
inline size_t ConcurrentExecutionQueue<T, S>::size() const noexcept {
  return _queue.size();
}

template <typename T, typename S>
inline int ConcurrentExecutionQueue<T, S>::execute(T&& value) noexcept {
  _queue.template push<true, false, false>(::std::move(value));
  return signal_push_event();
}

template <typename T, typename S>
inline int ConcurrentExecutionQueue<T, S>::execute(const T& value) noexcept {
  _queue.template push<true, false, false>(value);
  return signal_push_event();
}

template <typename T, typename S>
void ConcurrentExecutionQueue<T, S>::join() noexcept {
  while (_events.load(::std::memory_order_acquire)) {
    S::usleep(1000);
  }
}

template <typename T, typename S>
inline int ConcurrentExecutionQueue<T, S>::signal_push_event() noexcept {
  if (0 != _events.fetch_add(1, ::std::memory_order_acq_rel)) {
    return 0;
  }

  return start_consumer();
}

template <typename T, typename S>
int ConcurrentExecutionQueue<T, S>::start_consumer() noexcept {
  size_t events = 1;
  do {
    auto ret =
        _executor->submit(&ConcurrentExecutionQueue::consume_until_empty, this);
    if (ret == 0) {
      return 0;
    }
  } while (
      !_events.compare_exchange_strong(events, 0, ::std::memory_order_acq_rel));
  return -1;
}

template <typename T, typename S>
void ConcurrentExecutionQueue<T, S>::consume_until_empty() noexcept {
  size_t events = _events.load(::std::memory_order_acquire);
  while (true) {
    auto poped = _queue.template try_pop_n<false, false>(_consume_function,
                                                         _queue.capacity());
    if (poped != 0) {
      events = _events.load(::std::memory_order_acquire);
    } else if (_events.compare_exchange_strong(events, 0,
                                               ::std::memory_order_acq_rel)) {
      break;
    }
  }
}

BABYLON_NAMESPACE_END
