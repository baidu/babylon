#pragma once

#include "babylon/basic_executor.h"           // BasicExecutor
#include "babylon/concurrent/bounded_queue.h" // ConcurrentBoundedQueue
#include "babylon/concurrent/thread_local.h"  // EnumerableThreadLocal
#include "babylon/coroutine/task.h"           // CoroutineTask
#include "babylon/future.h"                   // Future

#include <thread> // std::thread
#include <vector> // std::vector

BABYLON_NAMESPACE_BEGIN

// Unified interface to launch a task to run asynchronously. A task can be a
// closure of
// - normal function
// - member function
// - functor object, class or lambda
// - coroutine
//
// These various closure packing is done by babylon::Executor, and actual
// asynchronous execution mechanism is provide by interface of
// babylon::BasicExecutor.
class Executor : public BasicExecutor {
 private:
#if __cpp_concepts && __cpp_lib_coroutine
  // Coroutine only copy or move args... to internal state but ignore functor
  // object itself, lambda with capture for example. Distinguish functor object
  // out from others to properly handle them.
  template <typename C>
  static constexpr bool IsPlainFunction =
      ::std::is_function<
          ::std::remove_pointer_t<::std::remove_reference_t<C>>>::value ||
      ::std::is_member_function_pointer<C>::value;

  // Only CoroutineTask is directly support by executor, but other coroutine
  // could also be support by wrap in a CoroutineTask. Distinguish them out to
  // do that wrapping only when necessary.
  template <typename C, typename... Args>
  struct IsCoroutineTask;
  template <typename C, typename... Args>
  static constexpr bool IsCoroutineTaskValue =
      IsCoroutineTask<C, Args...>::value;
#endif // __cpp_concepts && __cpp_lib_coroutine

  // The **effective** result type of C(Args...), generally
  // std::invoke_result_t<C, Args...>.
  //
  // Coroutine is a special case, for that, in specification C(Args...) need
  // to return a task handle which more a future-like object than a
  // meaningful value co_return-ed by the coroutine task itself. So instead of
  // the handle, the **effective** result for a coroutine task is considered to
  // be type of object as if `co_await C(Args...)` in a coroutine context.
  template <typename C, typename... Args>
  struct Result;
  template <typename C, typename... Args>
  using ResultType = typename Result<C, Args...>::type;
#if __cpp_concepts && __cpp_lib_coroutine
  template <typename A>
  struct AwaitResult;
  template <typename A>
  using AwaitResultType = typename AwaitResult<A>::type;
#endif // __cpp_concepts && __cpp_lib_coroutine

 public:
  Executor() noexcept = default;
  Executor(Executor&&) noexcept = default;
  Executor(const Executor&) noexcept = default;
  Executor& operator=(Executor&&) noexcept = default;
  Executor& operator=(const Executor&) noexcept = default;
  virtual ~Executor() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////////
  // Execute a callable with executor, return a future object associate with
  // that execution.
  //
  // When enable -fcoroutines and -fconcepts or use -std=c++20, execute a
  // coroutine task is also supported. The returned future will associate with
  // that coroutine, and can be used to wait and get the co_return value just
  // like use co_await inside another coroutine.
  template <typename F = SchedInterface, typename C, typename... Args>
#if __cpp_concepts && __cpp_lib_coroutine
    requires(::std::invocable<C &&, Args && ...> &&
             !CoroutineInvocable<C &&, Args && ...>)
#endif // __cpp_concepts && __cpp_lib_coroutine
  inline Future<ResultType<C&&, Args&&...>, F> execute(C&& callable,
                                                       Args&&... args) noexcept;
#if __cpp_concepts && __cpp_lib_coroutine
  template <typename F = SchedInterface, typename C, typename... Args>
    requires CoroutineInvocable<C&&, Args&&...> && Executor::IsPlainFunction<C>
  inline Future<ResultType<C&&, Args&&...>, F> execute(C&& callable,
                                                       Args&&... args) noexcept;
  template <typename F = SchedInterface, typename C, typename... Args>
    requires CoroutineInvocable<C&&, Args&&...> &&
             (!Executor::IsPlainFunction<C>)
  inline Future<ResultType<C&&, Args&&...>, F> execute(C&& callable,
                                                       Args&&... args) noexcept;
#endif // __cpp_concepts && __cpp_lib_coroutine
  //////////////////////////////////////////////////////////////////////////////

#if __cpp_concepts && __cpp_lib_coroutine
  // Await a awaitable object, just like co_await it inside a coroutine context.
  // Return a future object to wait and get that result.
  template <typename F = SchedInterface, typename A>
    requires coroutine::Awaitable<A, CoroutineTask<>::promise_type>
  inline Future<AwaitResultType<A&&>, F> execute(A&& awaitable) noexcept;
#endif // __cpp_concepts && __cpp_lib_coroutine

  //////////////////////////////////////////////////////////////////////////////
  // Execute a callable with executor, return 0 if success scheduled.
  //
  // When enable -fcoroutines and -fconcepts or use -std=c++20, execute a
  // coroutine task is also supported.
  template <typename C, typename... Args>
#if __cpp_concepts && __cpp_lib_coroutine
    requires ::std::is_invocable<C&&, Args&&...>::value &&
             (!CoroutineInvocable<C &&, Args && ...>)
#endif // __cpp_concepts && __cpp_lib_coroutine
  inline int submit(C&& callable, Args&&... args) noexcept;
#if __cpp_concepts && __cpp_lib_coroutine
  template <typename C, typename... Args>
    requires Executor::IsCoroutineTaskValue<C&&, Args&&...> &&
             Executor::IsPlainFunction<C>
  inline int submit(C&& callable, Args&&... args) noexcept;
  template <typename C, typename... Args>
    requires CoroutineInvocable<C&&, Args&&...> &&
             (!Executor::IsCoroutineTaskValue<C &&, Args && ...>) &&
             Executor::IsPlainFunction<C>
  inline int submit(C&& callable, Args&&... args) noexcept;
  template <typename C, typename... Args>
    requires CoroutineInvocable<C&&, Args&&...> &&
             (!Executor::IsPlainFunction<C>)
  inline int submit(C&& callable, Args&&... args) noexcept;
#endif // __cpp_concepts && __cpp_lib_coroutine
  //////////////////////////////////////////////////////////////////////////////

 private:
#if __cpp_concepts && __cpp_lib_coroutine
  template <typename T>
  inline int submit(CoroutineTask<T>&& task) noexcept;
#endif // __cpp_concepts && __cpp_lib_coroutine

  template <typename P, typename C, typename... Args>
  inline static void apply_and_set_value(
      P& promise, C&& callable, ::std::tuple<Args...>&& args_tuple) noexcept;
  template <typename F, typename C, typename... Args>
  inline static void apply_and_set_value(
      Promise<void, F>& promise, C&& callable,
      ::std::tuple<Args...>&& args_tuple) noexcept;

#if __cpp_concepts && __cpp_lib_coroutine
  template <typename P, typename A>
  CoroutineTask<> await_and_set_value(P promise, A awaitable) noexcept;
  template <typename F, typename A>
  CoroutineTask<> await_and_set_value(Promise<void, F> promise,
                                      A awaitable) noexcept;

  template <typename T, typename P, typename C>
  CoroutineTask<> await_apply_and_set_value(P promise, C callable,
                                            T args_tuple) noexcept;
  template <typename T, typename F, typename C>
  CoroutineTask<> await_apply_and_set_value(Promise<void, F> promise,
                                            C callable, T args_tuple) noexcept;
#endif // __cpp_concepts && __cpp_lib_coroutine
};

#if __cpp_concepts && __cpp_lib_coroutine
template <typename C, typename... Args>
struct Executor::IsCoroutineTask {
  static constexpr bool value = false;
};
template <typename C, typename... Args>
  requires requires {
             typename ::std::remove_cvref_t<::std::invoke_result_t<C, Args...>>;
           }
struct Executor::IsCoroutineTask<C, Args...> {
  static constexpr bool value = IsSpecialization<
      ::std::remove_cvref_t<::std::invoke_result_t<C, Args...>>,
      CoroutineTask>::value;
};
#endif // __cpp_concepts && __cpp_lib_coroutine

template <typename C, typename... Args>
struct Executor::Result : public ::std::invoke_result<C, Args...> {};
#if __cpp_concepts && __cpp_lib_coroutine
template <typename C, typename... Args>
  requires CoroutineInvocable<C, Args...>
struct Executor::Result<C, Args...> {
  using type = Executor::AwaitResultType<::std::invoke_result_t<C, Args...>>;
};

template <typename A>
struct Executor::AwaitResult {};
template <typename A>
  requires requires {
             typename coroutine::AwaitResultType<A,
                                                 CoroutineTask<>::promise_type>;
           }
struct Executor::AwaitResult<A> {
  using AwaitResultType =
      coroutine::AwaitResultType<A, CoroutineTask<>::promise_type>;
  using type = typename ::std::conditional<
      ::std::is_rvalue_reference<AwaitResultType>::value,
      typename ::std::remove_reference<AwaitResultType>::type,
      AwaitResultType>::type;
};
#endif // __cpp_concepts && __cpp_lib_coroutine

// Just like std::async, but replace policy to a more flexible executor.
template <typename F = SchedInterface, typename C, typename... Args>
inline auto async(Executor& executor, C&& callable, Args&&... args) noexcept {
  return executor.execute<F>(::std::forward<C>(callable),
                             ::std::forward<Args>(args)...);
}

// Sync executor run task just inside invocation of execute/submit.
class InplaceExecutor : public Executor {
 public:
  static InplaceExecutor& instance() noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;

 private:
  InplaceExecutor() noexcept = default;
  InplaceExecutor(InplaceExecutor&&) = delete;
  InplaceExecutor(const InplaceExecutor&) = delete;
  InplaceExecutor& operator=(InplaceExecutor&&) = delete;
  InplaceExecutor& operator=(const InplaceExecutor&) = delete;
  virtual ~InplaceExecutor() noexcept override = default;
};

// Async executor launch new thread for every task execute/submit.
struct AlwaysUseNewThreadExecutor : public Executor {
 public:
  static AlwaysUseNewThreadExecutor& instance() noexcept;

  void join() noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;

 private:
  AlwaysUseNewThreadExecutor() noexcept = default;
  AlwaysUseNewThreadExecutor(AlwaysUseNewThreadExecutor&&) = delete;
  AlwaysUseNewThreadExecutor(const AlwaysUseNewThreadExecutor&) = delete;
  AlwaysUseNewThreadExecutor& operator=(AlwaysUseNewThreadExecutor&&) = delete;
  AlwaysUseNewThreadExecutor& operator=(const AlwaysUseNewThreadExecutor&) =
      delete;
  virtual ~AlwaysUseNewThreadExecutor() noexcept override;

  ::std::atomic<size_t> _running {0};
};

// Async executor use a thread pool as backend
class ThreadPoolExecutor : public Executor {
 public:
  // Use **this** in worker thread, so no copy nor move
  ThreadPoolExecutor() = default;
  ThreadPoolExecutor(ThreadPoolExecutor&&) noexcept = delete;
  ThreadPoolExecutor(const ThreadPoolExecutor&) noexcept = delete;
  ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) noexcept = delete;
  ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) noexcept = delete;
  virtual ~ThreadPoolExecutor() noexcept override;

  // Parameters
  // worker_number: Threads for running task.
  //
  // local_capacity: Task execute/submit inside another task will add to local
  // first, if not exceed this capacity.
  //
  // global_capacity: Task execute/submit to
  // global will wait in queue first. After waiting task exceed capacity, new
  // task execute/submit will block.
  //
  // enable_work_stealing: When enable, worker
  // thread will check other worker's local waiting task before trying wait and
  // get task from global queue.
  //
  // balance_interval: When set to positive, a
  // background thread will be used to **steal** all worker's local waiting task
  // periodically.
  void set_worker_number(size_t worker_number) noexcept;
  void set_local_capacity(size_t local_capacity) noexcept;
  void set_global_capacity(size_t global_capacity) noexcept;
  void set_enable_work_stealing(bool enable_work_stealing) noexcept;
  template <typename R, typename P>
  void set_balance_interval(
      ::std::chrono::duration<R, P> balance_interval) noexcept;

  int start() noexcept;

  // Even when work stealing is enabled, pending local task will not be consumed
  // if the idle worker is block waiting for global task. If schedule latency is
  // important, user can wakeup some worker actively to make the steal happen
  // eagerly.
  //
  // Just like backup-requesting mechanism, work stealing and proactive wakeup
  // is a trade-off of cpu usage for latency. So it is up to user to decide when
  // and how many idle workers need to wakeup.
  void wakeup_one_worker() noexcept;

  void stop() noexcept;

  int ABSL_DEPRECATED("Use start instead")
      initialize(size_t worker_num, size_t queue_capacity) noexcept;

 protected:
  virtual int invoke(MoveOnlyFunction<void(void)>&& function) noexcept override;

 private:
  enum class TaskType {
    FUNCTION,
    WAKEUP,
    STOP,
  };

  struct Task {
    TaskType type;
    MoveOnlyFunction<void(void)> function;
  };

  using TaskQueue = ConcurrentBoundedQueue<Task>;

  void keep_execute() noexcept;
  void keep_balance() noexcept;
  int enqueue_task(Task&& task) noexcept;

  size_t _worker_number {1};
  size_t _local_capacity {0};
  size_t _global_capacity {1};
  bool _enable_work_stealing {false};
  ::std::chrono::microseconds _balance_interval {-1};

  ::std::atomic<bool> _running {false};
  ::babylon::EnumerableThreadLocal<TaskQueue> _local_task_queues;
  TaskQueue _global_task_queue;
  ::std::vector<::std::thread> _threads;
  ::std::thread _balance_thread;
};

BABYLON_NAMESPACE_END

#include "babylon/executor.hpp"
