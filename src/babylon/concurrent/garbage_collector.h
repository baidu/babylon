#pragma once

#include "babylon/concurrent/bounded_queue.h"
#include "babylon/concurrent/epoch.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

#include <thread>

BABYLON_NAMESPACE_BEGIN

// Standalone reclaimer implementation as a part of EBR (Epoch-Based
// Reclamation) which described in
// https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-579.pdf
//
// With modifications below:
// - Delay and move the reclaim operation to a standalone thread
//
// Typical usage is:
// - Use Epoch to build critical region. All element of a lock-free structure
// accessed inside this critical region will keep valid.
// - After modify that lock-free structure. Some old element may be **retired**
// and collect by the GarbageCollector. They will finally be **reclaimed** when
// all critical region **previous to that modification** are closed.
template <typename R>
class GarbageCollector {
 public:
  // Capture this in gc thread. So no copy nor move is allowed
  GarbageCollector() noexcept = default;
  GarbageCollector(GarbageCollector&&) noexcept = delete;
  GarbageCollector(const GarbageCollector&) noexcept = delete;
  GarbageCollector& operator=(GarbageCollector&&) noexcept = delete;
  GarbageCollector& operator=(const GarbageCollector&) noexcept = delete;
  ~GarbageCollector() noexcept;

  // Reclaim task is queued waiting reach the low_water_mark expected.
  // Set how many task can be queued before finally blocking the retire function
  void set_queue_capacity(size_t min_capacity) noexcept;

  // Start background gc thread that consume and do reclaim task
  int start() noexcept;

  // Get underlying epoch instance
  inline Epoch& epoch() noexcept;

  // Queue a reclaim task in. This task will be called by background thread
  // when low_water_mark of epoch reach at least lowest_epoch.
  //
  // lowest_epoch is default set by Epoch::tick. User can do this tick outside
  // to do fast batch retirements
  inline void retire(R&& reclaimer) noexcept;
  inline void retire(R&& reclaimer, uint64_t lowest_epoch) noexcept;

  // Stop background gc thread after current reclaim tasks are all finished
  void stop() noexcept;

 private:
  class ReclaimTask {
   public:
    ReclaimTask() = default;
    ReclaimTask(ReclaimTask&&) = default;
    ReclaimTask(const ReclaimTask&) = delete;
    ReclaimTask& operator=(ReclaimTask&&) = default;
    ReclaimTask& operator=(const ReclaimTask&) = delete;
    ~ReclaimTask() noexcept = default;

    ReclaimTask(R&& reclaimer, uint64_t lowest_epoch) noexcept
        : reclaimer {::std::move(reclaimer)}, lowest_epoch {lowest_epoch} {}

    R reclaimer;
    uint64_t lowest_epoch {UINT64_MAX};
  };

  void keep_reclaim() noexcept;
  bool consume_reclaim_task(size_t batch,
                            ::std::vector<ReclaimTask>& tasks) noexcept;
  size_t reclaim_start_from(size_t index,
                            ::std::vector<ReclaimTask>& tasks) noexcept;

  Epoch _epoch;
  ConcurrentBoundedQueue<ReclaimTask> _queue;
  ::std::thread _gc_thread;
};

////////////////////////////////////////////////////////////////////////////////
// GarbageCollector begin
template <typename R>
ABSL_ATTRIBUTE_NOINLINE GarbageCollector<R>::~GarbageCollector() noexcept {
  stop();
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE void GarbageCollector<R>::set_queue_capacity(
    size_t min_capacity) noexcept {
  _queue.reserve_and_clear(min_capacity);
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE int GarbageCollector<R>::start() noexcept {
  if (!_gc_thread.joinable()) {
    _gc_thread = ::std::thread(&GarbageCollector<R>::keep_reclaim, this);
  }
  return 0;
}

template <typename R>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline Epoch&
GarbageCollector<R>::epoch() noexcept {
  return _epoch;
}

template <typename R>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void GarbageCollector<R>::retire(
    R&& reclaimer) noexcept {
  retire(::std::move(reclaimer), _epoch.tick());
}

template <typename R>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void GarbageCollector<R>::retire(
    R&& reclaimer, uint64_t lowest_epoch) noexcept {
  _queue.template push<true, false, false>(
      ReclaimTask {::std::forward<R>(reclaimer), lowest_epoch});
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE void GarbageCollector<R>::stop() noexcept {
  if (_gc_thread.joinable()) {
    _queue.template push<true, false, false>(ReclaimTask {});
    _gc_thread.join();
  }
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE void GarbageCollector<R>::keep_reclaim() noexcept {
  bool running = true;
  size_t batch = ::std::min<size_t>(1024, _queue.capacity());
  size_t index = 0;
  ::std::vector<ReclaimTask> tasks;
  size_t backoff_us = 1000;
  tasks.reserve(batch);
  while (running) {
    if (index == tasks.size()) {
      tasks.clear();
      running = consume_reclaim_task(batch, tasks);
      index = 0;
    }

    auto reclaimed = reclaim_start_from(index, tasks);
    index += reclaimed;

    if (reclaimed < 100) {
      backoff_us = ::std::min<size_t>(backoff_us + 10, 100000);
      ::usleep(backoff_us);
    } else if (reclaimed >= batch) {
      backoff_us >>= 1;
    }
  }
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE bool GarbageCollector<R>::consume_reclaim_task(
    size_t batch, ::std::vector<ReclaimTask>& tasks) noexcept {
  using Iterator = typename ConcurrentBoundedQueue<ReclaimTask>::Iterator;

  bool running = true;
  _queue.template try_pop_n<false, false>(
      [&](Iterator iter, Iterator end) {
        while (iter < end) {
          auto& task = *iter++;
          if (ABSL_PREDICT_FALSE(task.lowest_epoch == UINT64_MAX)) {
            running = false;
            break;
          }
          tasks.emplace_back(::std::move(task));
        }
      },
      batch);
  return running;
}

template <typename R>
ABSL_ATTRIBUTE_NOINLINE size_t GarbageCollector<R>::reclaim_start_from(
    size_t index, ::std::vector<ReclaimTask>& tasks) noexcept {
  size_t reclaimed = 0;
  auto low_water_mark = _epoch.low_water_mark();
  for (; index < tasks.size(); ++index) {
    auto& task = tasks[index];
    if (task.lowest_epoch > low_water_mark) {
      break;
    }
    {
      ReclaimTask t = ::std::move(task);
      t.reclaimer();
    }
    reclaimed++;
  }
  return reclaimed;
}
// GarbageCollector end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
