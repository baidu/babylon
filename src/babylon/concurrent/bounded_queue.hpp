#pragma once

#include "babylon/absl_numeric_bits.h" // absl::bit_ceil
#include "babylon/concurrent/bounded_queue.h"
#include "babylon/new.h" // operator new

// clang-format off
#include BABYLON_EXTERNAL(absl/time/clock.h) // absl::GetCurrentTimeNanos
// clang-format on

#if ABSL_HAVE_THREAD_SANITIZER
#include <sanitizer/tsan_interface.h> // ::__tsan_acquire/::__tsan_release
#endif                                // ABSL_HAVE_THREAD_SANITIZER

#include <errno.h> // ::errno

#pragma GCC diagnostic push
// Thread Sanitizer目前无法支持std::atomic_thread_fence
// gcc-12之后增加了相应的报错，显示标记忽视并特殊处理相关段落
#if GCC_VERSION >= 120000
#pragma GCC diagnostic ignored "-Wtsan"
#endif // GCC_VERSION >= 120000

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ConcurrentBoundedQueue::SlotFutex begin
template <typename T, typename S>
inline uint16_t ConcurrentBoundedQueue<T, S>::SlotFutex::version(
    ::std::memory_order order) const noexcept {
  return _futex.value().load(order);
}

template <typename T, typename S>
template <bool USE_FUTEX_WAIT>
inline void
ConcurrentBoundedQueue<T, S>::SlotFutex::wait_until_reach_expected_version(
    uint16_t expected_version, const struct ::timespec* timeout,
    ::std::memory_order order) noexcept {
  auto current_version_and_waiters = _futex.value().load(order);
  uint16_t version = current_version_and_waiters;
  if (version == expected_version) {
    return;
  }
  if (USE_FUTEX_WAIT) {
    block_until_reach_expected_version_slow(current_version_and_waiters,
                                            expected_version, timeout, order);
  } else {
    spin_until_reach_expected_version_slow(current_version_and_waiters,
                                           expected_version, timeout, order);
  }
}

template <typename T, typename S>
inline void ConcurrentBoundedQueue<T, S>::SlotFutex::set_version(
    uint16_t version, ::std::memory_order order) noexcept {
  reinterpret_cast<::std::atomic<uint16_t>*>(&_futex.value())
      ->store(version, order);
}

template <typename T, typename S>
inline void ConcurrentBoundedQueue<T, S>::SlotFutex::wakeup_waiters(
    uint16_t current_version) noexcept {
  // 检测是否需要执行唤醒
  auto current_version_and_waiters =
      _futex.value().load(::std::memory_order_relaxed);
  if (current_version_and_waiters <= UINT16_MAX) {
    return;
  }
  // 检测一下版本，如果已经推进了说明无需执行唤醒了
  // 如果必要，后续的版本推进方会完成这个唤醒
  uint16_t version = current_version_and_waiters;
  if (version != current_version) {
    return;
  }
  // 清理等待标记并执行唤醒，如果清理时已经发生了版本推进
  // 则跳过本次唤醒，交给推进版本的后续发起方执行，减少重复
  if (_futex.value().compare_exchange_strong(
          current_version_and_waiters, version, ::std::memory_order_relaxed)) {
    _futex.wake_all();
  }
}

template <typename T, typename S>
inline void
ConcurrentBoundedQueue<T, S>::SlotFutex::set_version_and_wakeup_waiters(
    uint16_t next_version) noexcept {
  auto current_version_and_waiters =
      _futex.value().exchange(next_version, ::std::memory_order_release);
  if (current_version_and_waiters <= UINT16_MAX) {
    return;
  }
  _futex.wake_all();
}

template <typename T, typename S>
inline void ConcurrentBoundedQueue<T, S>::SlotFutex::reset() noexcept {
  _futex.value().store(0, ::std::memory_order_relaxed);
}

template <typename T, typename S>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ConcurrentBoundedQueue<T, S>::SlotFutex::mark_tsan_acquire() noexcept {
#if ABSL_HAVE_THREAD_SANITIZER
  __tsan_acquire(&_futex.value());
#endif // ABSL_HAVE_THREAD_SANITIZER
}

template <typename T, typename S>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ConcurrentBoundedQueue<T, S>::SlotFutex::mark_tsan_release() noexcept {
#if ABSL_HAVE_THREAD_SANITIZER
  __tsan_release(&_futex.value());
#endif // ABSL_HAVE_THREAD_SANITIZER
}

template <typename T, typename S>
ABSL_ATTRIBUTE_NOINLINE void ConcurrentBoundedQueue<T, S>::SlotFutex::
    block_until_reach_expected_version_slow(
        uint32_t current_version_and_waiters, uint16_t expected_version,
        const struct ::timespec* timeout, ::std::memory_order order) noexcept {
  int64_t begin_time_ns =
      timeout != nullptr ? ::absl::GetCurrentTimeNanos() : 0;
  struct ::timespec modified_timeout;
  uint16_t version = current_version_and_waiters;
  // 由于futex机制存在小概率非预期，需要循环兜底
  while (true) {
    // 高16bit用于标记是否有等待者
    // 首次进入等待的人完成这个标记动作
    if (current_version_and_waiters <= UINT16_MAX) {
      auto wait_version_and_waiters =
          current_version_and_waiters + UINT16_MAX + 1;
      if (!_futex.value().compare_exchange_strong(
              current_version_and_waiters, wait_version_and_waiters, order)) {
        // 设置失败重新校验版本
        version = current_version_and_waiters;
        if (version == expected_version) {
          break;
        }
        // 更新版本后依然可能需要进行首次标记等待者逻辑
        continue;
      }
      current_version_and_waiters = wait_version_and_waiters;
    }
    // 执行等待
    errno = 0;
    _futex.wait(current_version_and_waiters, timeout);
    if (errno == ETIMEDOUT) {
      break;
    }
    // 读取新值
    current_version_and_waiters = _futex.value().load(order);
    version = current_version_and_waiters;
    if (version == expected_version) {
      break;
    }
    // 必要的话，刷新等待时间
    if (timeout != nullptr) {
      int64_t end_time_ns = ::absl::GetCurrentTimeNanos();
      auto wait_duration = ::absl::DurationFromTimespec(*timeout);
      wait_duration -= ::absl::Nanoseconds(end_time_ns - begin_time_ns);
      if (wait_duration <= ::absl::Nanoseconds(0)) {
        break;
      }
      modified_timeout = ::absl::ToTimespec(wait_duration);
      timeout = &modified_timeout;
    }
  }
}

template <typename T, typename S>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentBoundedQueue<T, S>::SlotFutex::spin_until_reach_expected_version_slow(
    uint32_t current_version_and_waiters, uint16_t expected_version,
    const struct ::timespec* timeout, ::std::memory_order order) noexcept {
  int64_t begin_time_ns =
      timeout != nullptr ? ::absl::GetCurrentTimeNanos() : 0;
  int64_t end_time_ns =
      begin_time_ns +
      (timeout != nullptr
           ? ::absl::ToInt64Nanoseconds(::absl::DurationFromTimespec(*timeout))
           : 0);
  while (true) {
    // 执行等待
    S::usleep(1000);
    // 读取新值
    current_version_and_waiters = _futex.value().load(order);
    uint16_t version = current_version_and_waiters;
    if (version == expected_version) {
      break;
    }
    // 检测是否超时
    if (timeout != nullptr) {
      int64_t time_ns = ::absl::GetCurrentTimeNanos();
      if (time_ns > end_time_ns) {
        break;
      }
    }
  }
}
// ConcurrentBoundedQueue::SlotFutex end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentBoundedQueue::Iterator begin
template <typename T, typename S>
inline ConcurrentBoundedQueue<T, S>::Iterator::Iterator(Slot* slot) noexcept
    : _slot(slot) {}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::Iterator&
ConcurrentBoundedQueue<T, S>::Iterator::operator++() noexcept {
  ++_slot;
  return *this;
}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::Iterator
ConcurrentBoundedQueue<T, S>::Iterator::operator++(int) noexcept {
  Iterator ret = *this;
  ++(*this);
  return ret;
}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::Iterator
ConcurrentBoundedQueue<T, S>::Iterator::operator+(
    ssize_t offset) const noexcept {
  return Iterator(_slot + offset);
}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::Iterator
ConcurrentBoundedQueue<T, S>::Iterator::operator-(
    ssize_t offset) const noexcept {
  return Iterator(_slot - offset);
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator==(
    Iterator other) const noexcept {
  return _slot == other._slot;
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator!=(
    Iterator other) const noexcept {
  return !(*this == other);
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator<(
    Iterator other) const noexcept {
  return _slot < other._slot;
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator<=(
    Iterator other) const noexcept {
  return _slot <= other._slot;
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator>(
    Iterator other) const noexcept {
  return _slot > other._slot;
}

template <typename T, typename S>
inline bool ConcurrentBoundedQueue<T, S>::Iterator::operator>=(
    Iterator other) const noexcept {
  return _slot >= other._slot;
}

template <typename T, typename S>
inline T& ConcurrentBoundedQueue<T, S>::Iterator::operator*() const noexcept {
  return _slot->value;
}

template <typename T, typename S>
inline T* ConcurrentBoundedQueue<T, S>::Iterator::operator->() const noexcept {
  return &_slot->value;
}

template <typename T, typename S>
inline ssize_t ConcurrentBoundedQueue<T, S>::Iterator::operator-(
    Iterator other) const noexcept {
  return _slot - other._slot;
}
// ConcurrentBoundedQueue::Iterator end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentBoundedQueue::SlotVector begin
template <typename T, typename S>
ConcurrentBoundedQueue<T, S>::SlotVector::SlotVector(
    SlotVector&& other) noexcept
    : SlotVector() {
  *this = ::std::move(other);
}

template <typename T, typename S>
typename ConcurrentBoundedQueue<T, S>::SlotVector&
ConcurrentBoundedQueue<T, S>::SlotVector::operator=(
    SlotVector&& other) noexcept {
  swap(other);
  return *this;
}

template <typename T, typename S>
ConcurrentBoundedQueue<T, S>::SlotVector::~SlotVector() noexcept {
  for (size_t i = 0; i < _size; ++i) {
    _slots[i].~Slot();
  }
  ::operator delete(_slots, sizeof(Slot) * _size,
                    ::std::align_val_t(alignof(Slot)));
}

template <typename T, typename S>
ConcurrentBoundedQueue<T, S>::SlotVector::SlotVector(size_t size) noexcept
    : _slots {reinterpret_cast<Slot*>(::operator new(
          sizeof(Slot) * size, ::std::align_val_t(alignof(Slot))))},
      _size {size} {
  for (size_t i = 0; i < _size; ++i) {
    new (&_slots[i]) Slot();
  }
}

template <typename T, typename S>
void ConcurrentBoundedQueue<T, S>::SlotVector::resize(size_t size) noexcept {
  this->~SlotVector();
  new (this) SlotVector(size);
}

template <typename T, typename S>
inline size_t ConcurrentBoundedQueue<T, S>::SlotVector::size() const noexcept {
  return _size;
}

template <typename T, typename S>
inline T& ConcurrentBoundedQueue<T, S>::SlotVector::value(
    size_t index) noexcept {
  return _slots[index].value;
}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::Iterator
ConcurrentBoundedQueue<T, S>::SlotVector::value_iterator(
    size_t index) noexcept {
  return Iterator(&_slots[index]);
}

template <typename T, typename S>
inline typename ConcurrentBoundedQueue<T, S>::SlotFutex&
ConcurrentBoundedQueue<T, S>::SlotVector::futex(size_t index) noexcept {
  return _slots[index].futex;
}

template <typename T, typename S>
inline void ConcurrentBoundedQueue<T, S>::SlotVector::swap(
    SlotVector& other) noexcept {
  ::std::swap(_slots, other._slots);
  ::std::swap(_size, other._size);
}
// ConcurrentBoundedQueue::SlotVector end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentBoundedQueue begin
template <typename T, typename S>
ConcurrentBoundedQueue<T, S>::ConcurrentBoundedQueue(
    ConcurrentBoundedQueue&& other) noexcept
    : ConcurrentBoundedQueue() {
  swap(other);
}

template <typename T, typename S>
ConcurrentBoundedQueue<T, S>& ConcurrentBoundedQueue<T, S>::operator=(
    ConcurrentBoundedQueue&& other) noexcept {
  swap(other);
  return *this;
}

template <typename T, typename S>
ConcurrentBoundedQueue<T, S>::ConcurrentBoundedQueue(
    size_t min_capacity) noexcept
    : ConcurrentBoundedQueue() {
  reserve_and_clear(min_capacity);
}

template <typename T, typename S>
inline size_t ConcurrentBoundedQueue<T, S>::capacity() const noexcept {
  return _slots.size();
}

template <typename T, typename S>
inline size_t ConcurrentBoundedQueue<T, S>::size() const noexcept {
  auto next_pop_index = _next_pop_index.load(::std::memory_order_relaxed);
  auto next_push_index = _next_push_index.load(::std::memory_order_relaxed);
  // 由于没有同步机制，可能存在push < pop的情况，处理为0
  return next_push_index > next_pop_index ? next_push_index - next_pop_index
                                          : 0;
}

template <typename T, typename S>
size_t ConcurrentBoundedQueue<T, S>::reserve_and_clear(size_t min_capacity) {
  auto new_capacity = ::absl::bit_ceil(min_capacity);
  if (new_capacity != capacity()) {
    _slot_bits = static_cast<uint32_t>(__builtin_ctzll(new_capacity));
    _slot_mask = new_capacity - 1;
    _slots.resize(new_capacity);
    _next_push_index.store(0, ::std::memory_order_relaxed);
    _next_pop_index.store(0, ::std::memory_order_relaxed);
    for (size_t i = 0; i < new_capacity; ++i) {
      _slots.futex(i).reset();
    }
  } else {
    clear();
  }
  return capacity();
}

template <typename T, typename S>
template <typename U, typename ::std::enable_if<
                          ::std::is_assignable<T&, U>::value, int>::type>
inline void ConcurrentBoundedQueue<T, S>::push(U&& value) {
  push<true, true, true>(::std::forward<U>(value));
}

template <typename T, typename S>
template <typename C, typename>
inline void ConcurrentBoundedQueue<T, S>::push(C&& callback) {
  push<true, true, true>(::std::forward<C>(callback));
}

template <typename T, typename S>
template <
    bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename U,
    typename ::std::enable_if<::std::is_assignable<T&, U>::value, int>::type>
inline void ConcurrentBoundedQueue<T, S>::push(U&& value) {
  push<CONCURRENT, USE_FUTEX_WAIT, USE_FUTEX_WAKE>(
      [&](T& target) ABSL_ATTRIBUTE_ALWAYS_INLINE {
        target = ::std::forward<U>(value);
      });
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename C,
          typename>
inline void ConcurrentBoundedQueue<T, S>::push(C&& callback) {
  // 自增取得序号
  auto index = CONCURRENT
                   ? _next_push_index.fetch_add(1, ::std::memory_order_relaxed)
                   : _next_push_index.load(::std::memory_order_relaxed);
  if (!CONCURRENT) {
    _next_push_index.store(index + 1, ::std::memory_order_relaxed);
  }
  // 处理单个数据
  deal<USE_FUTEX_WAIT, USE_FUTEX_WAKE, true>(::std::forward<C>(callback),
                                             index);
}

template <typename T, typename S>
template <typename U, typename ::std::enable_if<
                          ::std::is_assignable<T&, U>::value, int>::type>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_push(U&& value) noexcept {
  return try_push<true, true, true>(::std::forward<U>(value));
}

template <typename T, typename S>
template <typename C, typename>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_push(C&& callback) noexcept {
  return try_push<true, true, true>(::std::forward<C>(callback));
}

template <typename T, typename S>
template <
    bool CONCURRENT, bool USE_FUTEX_WAKE, typename U,
    typename ::std::enable_if<::std::is_assignable<T&, U>::value, int>::type>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_push(U&& value) noexcept {
  return try_push<CONCURRENT, USE_FUTEX_WAKE>(
      [&](T& target) ABSL_ATTRIBUTE_ALWAYS_INLINE {
        target = ::std::forward<U>(value);
      });
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, typename C, typename>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_push(C&& callback) noexcept {
  return try_deal<CONCURRENT, USE_FUTEX_WAKE, true>(
      ::std::forward<C>(callback));
}

template <typename T, typename S>
template <typename IT>
inline void ConcurrentBoundedQueue<T, S>::push_n(IT begin, IT end) {
  push_n<true, true, true>(begin, end);
}

template <typename T, typename S>
template <typename C, typename>
inline void ConcurrentBoundedQueue<T, S>::push_n(C&& callback, size_t num) {
  push_n<true, true, true>(::std::forward<C>(callback), num);
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
          typename IT>
inline void ConcurrentBoundedQueue<T, S>::push_n(IT begin, IT end) {
  push_n<CONCURRENT, USE_FUTEX_WAIT, USE_FUTEX_WAKE>(
      [&](Iterator dest_begin, Iterator dest_end) ABSL_ATTRIBUTE_ALWAYS_INLINE {
        auto num = dest_end - dest_begin;
        ::std::copy(begin, begin + num, dest_begin);
        begin += num;
      },
      ::std::distance(begin, end));
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename C,
          typename>
inline void ConcurrentBoundedQueue<T, S>::push_n(C&& callback, size_t num) {
  auto index =
      CONCURRENT ? _next_push_index.fetch_add(num, ::std::memory_order_relaxed)
                 : _next_push_index.load(::std::memory_order_relaxed);
  if (!CONCURRENT) {
    _next_push_index.store(index + num, ::std::memory_order_relaxed);
  }
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (index + num <= next_round_begin_index) {
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, true>(
        ::std::forward<C>(callback), index, num);
  } else {
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, true>(
        ::std::forward<C>(callback), index, next_round_begin_index - index);
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, true>(
        ::std::forward<C>(callback), next_round_begin_index,
        index + num - next_round_begin_index);
  }
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, typename C, typename>
inline size_t ConcurrentBoundedQueue<T, S>::try_push_n(C&& callback,
                                                       size_t num) {
  auto index = _next_push_index.load(::std::memory_order_relaxed);
  auto end_index = index + num;
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (end_index <= next_round_begin_index) {
    return try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, true>(
        ::std::forward<C>(callback), index, end_index - index);
  } else {
    size_t continuous_num = next_round_begin_index - index;
    size_t pushed = try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, true>(
        ::std::forward<C>(callback), index, continuous_num);
    if (pushed < continuous_num) {
      return pushed;
    }
    return pushed + try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, true>(
                        ::std::forward<C>(callback), next_round_begin_index,
                        end_index - next_round_begin_index);
  }
}

template <typename T, typename S>
template <typename C, typename RC>
inline void ConcurrentBoundedQueue<T, S>::push_n(C&& callback,
                                                 RC&& reverse_callback,
                                                 size_t num) {
  auto index = _next_push_index.fetch_add(num, ::std::memory_order_relaxed);
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (index + num <= next_round_begin_index) {
    deal_n_continuously<true>(callback, reverse_callback, index, num);
  } else {
    deal_n_continuously<true>(callback, reverse_callback, index,
                              next_round_begin_index - index);
    deal_n_continuously<true>(callback, reverse_callback,
                              next_round_begin_index,
                              index + num - next_round_begin_index);
  }
}

template <typename T, typename S>
inline void ConcurrentBoundedQueue<T, S>::pop(T& value) {
  pop<true, true, true>(value);
}

template <typename T, typename S>
template <typename C, typename>
inline void ConcurrentBoundedQueue<T, S>::pop(C&& callback) {
  pop<true, true, true>(::std::forward<C>(callback));
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE>
inline void ConcurrentBoundedQueue<T, S>::pop(T* value) {
  pop<CONCURRENT, USE_FUTEX_WAIT, USE_FUTEX_WAKE>(*value);
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE>
inline void ConcurrentBoundedQueue<T, S>::pop(T& value) {
  pop<CONCURRENT, USE_FUTEX_WAIT, USE_FUTEX_WAKE>(
      [&value](T& source) ABSL_ATTRIBUTE_ALWAYS_INLINE {
        value = ::std::move(source);
      });
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename C,
          typename>
inline void ConcurrentBoundedQueue<T, S>::pop(C&& callback) {
  // 自增取得序号
  auto index = CONCURRENT
                   ? _next_pop_index.fetch_add(1, ::std::memory_order_relaxed)
                   : _next_pop_index.load(::std::memory_order_relaxed);
  if (!CONCURRENT) {
    _next_pop_index.store(index + 1, ::std::memory_order_relaxed);
  }
  // 处理单个数据
  deal<USE_FUTEX_WAIT, USE_FUTEX_WAKE, false>(::std::forward<C>(callback),
                                              index);
}

template <typename T, typename S>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_pop(T& value) noexcept {
  return try_pop<true, true>(value);
}

template <typename T, typename S>
template <typename C, typename>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_pop(C&& callback) noexcept {
  return try_pop<true, true>(::std::forward<C>(callback));
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_pop(T& value) noexcept {
  return try_pop<CONCURRENT, USE_FUTEX_WAKE>([&](T& source) {
    value = ::std::move(source);
  });
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, typename C, typename>
inline bool ABSL_ATTRIBUTE_ALWAYS_INLINE
ConcurrentBoundedQueue<T, S>::try_pop(C&& callback) noexcept {
  return try_deal<CONCURRENT, USE_FUTEX_WAKE, false>(
      ::std::forward<C>(callback));
}

template <typename T, typename S>
template <typename IT>
inline void ConcurrentBoundedQueue<T, S>::pop_n(IT begin, IT end) {
  pop_n<true, true, true>(begin, end);
}

template <typename T, typename S>
template <typename C, typename>
inline void ConcurrentBoundedQueue<T, S>::pop_n(C&& callback, size_t num) {
  pop_n<true, true, true>(::std::forward<C>(callback), num);
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE,
          typename IT>
inline void ConcurrentBoundedQueue<T, S>::pop_n(IT begin, IT end) {
  pop_n<CONCURRENT, USE_FUTEX_WAIT, USE_FUTEX_WAKE>(
      [&](Iterator src_begin, Iterator src_end) ABSL_ATTRIBUTE_ALWAYS_INLINE {
        ::std::copy(src_begin, src_end, begin);
        begin += src_end - src_begin;
      },
      ::std::distance(begin, end));
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, typename C,
          typename>
inline void ConcurrentBoundedQueue<T, S>::pop_n(C&& callback, size_t num) {
  auto index = CONCURRENT
                   ? _next_pop_index.fetch_add(num, ::std::memory_order_relaxed)
                   : _next_pop_index.load(::std::memory_order_relaxed);
  if (!CONCURRENT) {
    _next_pop_index.store(index + num, ::std::memory_order_relaxed);
  }
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (index + num <= next_round_begin_index) {
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, false>(
        ::std::forward<C>(callback), index, num);
  } else {
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, false>(
        ::std::forward<C>(callback), index, next_round_begin_index - index);
    deal_n_continuously<USE_FUTEX_WAIT, USE_FUTEX_WAKE, false>(
        ::std::forward<C>(callback), next_round_begin_index,
        index + num - next_round_begin_index);
  }
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, typename C, typename>
inline size_t ConcurrentBoundedQueue<T, S>::try_pop_n(C&& callback,
                                                      size_t num) {
  auto index = _next_pop_index.load(::std::memory_order_relaxed);
  auto end_index = index + num;
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (end_index <= next_round_begin_index) {
    return try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, false>(
        ::std::forward<C>(callback), index, end_index - index);
  } else {
    size_t continuous_num = next_round_begin_index - index;
    size_t poped = try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, false>(
        ::std::forward<C>(callback), index, continuous_num);
    if (poped < continuous_num) {
      return poped;
    }
    return poped + try_deal_n_continuously<CONCURRENT, USE_FUTEX_WAKE, false>(
                       ::std::forward<C>(callback), next_round_begin_index,
                       end_index - next_round_begin_index);
  }
}

template <typename T, typename S>
template <bool USE_FUTEX_WAKE, typename C, typename>
inline size_t ConcurrentBoundedQueue<T, S>::try_pop_n_exclusively_until(
    C&& callback, size_t num, const struct ::timespec* timeout) noexcept {
  auto index = _next_pop_index.load(::std::memory_order_relaxed) + num;
  auto expected_version = pop_version_for_index(index);
  auto slot_index = index & _slot_mask;
  auto& futex = _slots.futex(slot_index);
  futex.template wait_until_reach_expected_version<true>(
      expected_version, timeout, ::std::memory_order_relaxed);
  return try_pop_n<false, USE_FUTEX_WAKE>(::std::forward<C>(callback), num);
}

template <typename T, typename S>
template <typename C, typename RC>
inline void ConcurrentBoundedQueue<T, S>::pop_n(C&& callback,
                                                RC&& reverse_callback,
                                                size_t num) {
  auto index = _next_pop_index.fetch_add(num, ::std::memory_order_relaxed);
  auto next_round_begin_index = (index + _slot_mask + 1) & ~_slot_mask;
  if (index + num <= next_round_begin_index) {
    deal_n_continuously<false>(callback, reverse_callback, index, num);
  } else {
    deal_n_continuously<false>(callback, reverse_callback, index,
                               next_round_begin_index - index);
    deal_n_continuously<false>(callback, reverse_callback,
                               next_round_begin_index,
                               index + num - next_round_begin_index);
  }
}

template <typename T, typename S>
void ConcurrentBoundedQueue<T, S>::clear() {
  try_pop_n<true, true>([](Iterator, Iterator) {}, capacity());
}

template <typename T, typename S>
void ConcurrentBoundedQueue<T, S>::swap(
    ConcurrentBoundedQueue& other) noexcept {
  ::std::swap(_slots, other._slots);
  ::std::swap(_slot_mask, other._slot_mask);
  ::std::swap(_slot_bits, other._slot_bits);
  size_t next_push_index = _next_push_index;
  _next_push_index.store(other._next_push_index);
  other._next_push_index = next_push_index;
  size_t next_pop_index = _next_pop_index;
  _next_pop_index.store(other._next_pop_index);
  other._next_pop_index = next_pop_index;
}

template <typename T, typename S>
void ConcurrentBoundedQueue<T, S>::pop(T* value) {
  pop<true, true, true>(value);
}

template <typename T, typename S>
inline uint16_t ConcurrentBoundedQueue<T, S>::push_version_for_index(
    size_t index) noexcept {
  return (index >> _slot_bits) << 1;
}

template <typename T, typename S>
inline uint16_t ConcurrentBoundedQueue<T, S>::pop_version_for_index(
    size_t index) noexcept {
  return push_version_for_index(index) + 1;
}

template <typename T, typename S>
template <bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
          typename C>
inline void ConcurrentBoundedQueue<T, S>::deal(C&& callback,
                                               size_t index) noexcept {
  // 等待版本就绪
  auto expected_version = PUSH_OR_POP ? push_version_for_index(index)
                                      : pop_version_for_index(index);
  auto slot_index = index & _slot_mask;
  auto& futex = _slots.futex(slot_index);
  futex.template wait_until_reach_expected_version<USE_FUTEX_WAIT>(
      expected_version, nullptr, ::std::memory_order_acquire);
  // 执行回调
  callback(_slots.value(slot_index));
  if (USE_FUTEX_WAKE) {
    // 推进版本并唤醒等待者
    futex.set_version_and_wakeup_waiters(expected_version + 1);
  } else {
    // 推进版本
    futex.set_version(expected_version + 1, ::std::memory_order_release);
  }
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP, typename C>
inline bool ConcurrentBoundedQueue<T, S>::try_deal(C&& callback) noexcept {
  auto& next_index = PUSH_OR_POP ? _next_push_index : _next_pop_index;
  auto index = next_index.load(::std::memory_order_relaxed);
  while (true) {
    // 检测版本就绪，未就绪返回失败
    auto expected_version = PUSH_OR_POP ? push_version_for_index(index)
                                        : pop_version_for_index(index);
    auto slot_index = index & _slot_mask;
    auto& futex = _slots.futex(slot_index);
    if (expected_version != futex.version(::std::memory_order_acquire)) {
      auto current_index = next_index.load(::std::memory_order_relaxed);
      if (current_index == index) {
        return false;
      }
      index = current_index;
      continue;
    }
    if CONSTEXPR_SINCE_CXX17 (CONCURRENT) {
      // 竞争获取序号
      if (!next_index.compare_exchange_weak(index, index + 1,
                                            ::std::memory_order_relaxed)) {
        continue;
      }
    } else {
      next_index.store(index + 1, ::std::memory_order_relaxed);
    }
    // 执行回调
    callback(_slots.value(slot_index));
    if (USE_FUTEX_WAKE) {
      // 推进版本并唤醒等待者
      futex.set_version_and_wakeup_waiters(expected_version + 1);
    } else {
      // 推进版本
      futex.set_version(expected_version + 1, ::std::memory_order_release);
    }
    return true;
  }
}

template <typename T, typename S>
template <bool USE_FUTEX_WAIT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP,
          typename C>
inline void ConcurrentBoundedQueue<T, S>::deal_n_continuously(
    C&& callback, size_t index, size_t num) noexcept {
  // 等待整个区域版本就绪
  auto expected_version = PUSH_OR_POP ? push_version_for_index(index)
                                      : pop_version_for_index(index);
  auto slot_index = index & _slot_mask;
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i)
        .template wait_until_reach_expected_version<USE_FUTEX_WAIT>(
            expected_version, nullptr, ::std::memory_order_relaxed);
  }
  ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_acquire();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  // 批量执行回调
  callback(_slots.value_iterator(slot_index),
           _slots.value_iterator(slot_index + num));
  // 批量推进版本
  ::std::atomic_thread_fence(::std::memory_order_release);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_release();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i)
        .set_version(expected_version + 1, ::std::memory_order_relaxed);
  }
  // 批量唤醒等待者
  if (USE_FUTEX_WAKE) {
    // 对版本变更/检测和等待者注册/唤醒间建立全序关系
    // 用来保证成功注册的等待者一定会被唤醒
    ::std::atomic_thread_fence(::std::memory_order_seq_cst);
    for (size_t i = 0; i < num; ++i) {
      _slots.futex(slot_index + i).wakeup_waiters(expected_version + 1);
    }
  }
}

template <typename T, typename S>
template <bool PUSH_OR_POP, typename C, typename RC>
inline void ConcurrentBoundedQueue<T, S>::deal_n_continuously(
    C&& callback, RC&& reverse_callback, size_t index, size_t num) noexcept {
  // 等待整个区域版本就绪
  auto expected_version = PUSH_OR_POP ? push_version_for_index(index)
                                      : pop_version_for_index(index);
  auto slot_index = index & _slot_mask;
  for (size_t i = 0; i < num; ++i) {
    while (expected_version !=
           _slots.futex(slot_index + i).version(::std::memory_order_relaxed)) {
      // 未全部就绪，检查是否队空/满，是则执行补偿
      auto need_index =
          PUSH_OR_POP
              ? _next_pop_index.load(::std::memory_order_relaxed) + capacity()
              : _next_push_index.load(::std::memory_order_relaxed);
      if (need_index <= index + num) {
        if (PUSH_OR_POP) {
          try_pop_n<true, false>(::std::forward<RC>(reverse_callback), 1);
        } else {
          try_push_n<true, false>(::std::forward<RC>(reverse_callback), 1);
        }
        // 对应的生产/消费者已经在路上，让出cpu自旋一下
      } else {
        S::yield();
      }
    }
  }
  ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_acquire();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  // 批量执行回调
  callback(_slots.value_iterator(slot_index),
           _slots.value_iterator(slot_index + num));
  // 批量推进版本
  ::std::atomic_thread_fence(::std::memory_order_release);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_release();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i)
        .set_version(expected_version + 1, ::std::memory_order_relaxed);
  }
}

template <typename T, typename S>
template <bool CONCURRENT, bool USE_FUTEX_WAKE, bool PUSH_OR_POP, typename C>
inline size_t ConcurrentBoundedQueue<T, S>::try_deal_n_continuously(
    C&& callback, size_t index, size_t num) noexcept {
  // 检测版本就绪情况，调整num
  auto expected_version = PUSH_OR_POP ? push_version_for_index(index)
                                      : pop_version_for_index(index);
  auto slot_index = index & _slot_mask;
  for (size_t i = 0; i < num; ++i) {
    auto& futex = _slots.futex(slot_index + i);
    if (expected_version != futex.version(::std::memory_order_relaxed)) {
      num = i;
      break;
    }
  }
  if (num == 0) {
    return 0;
  }
  // 根据实际num推进生产/消费序号
  auto& next_index = PUSH_OR_POP ? _next_push_index : _next_pop_index;
  if (CONCURRENT) {
    if (!next_index.compare_exchange_strong(index, index + num,
                                            ::std::memory_order_relaxed)) {
      return 0;
    }
  } else {
    next_index.store(index + num, ::std::memory_order_relaxed);
  }
  ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_acquire();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  // 批量执行回调
  callback(_slots.value_iterator(slot_index),
           _slots.value_iterator(slot_index + num));
  // 批量推进版本
  ::std::atomic_thread_fence(::std::memory_order_release);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i).mark_tsan_release();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < num; ++i) {
    _slots.futex(slot_index + i)
        .set_version(expected_version + 1, ::std::memory_order_relaxed);
  }
  // 批量唤醒等待者
  if (USE_FUTEX_WAKE) {
    // 对版本变更/检测和等待者注册/唤醒间建立全序关系
    // 用来保证成功注册的等待者一定会被唤醒
    ::std::atomic_thread_fence(::std::memory_order_seq_cst);
    for (size_t i = 0; i < num; ++i) {
      _slots.futex(slot_index + i).wakeup_waiters(expected_version + 1);
    }
  }
  return num;
}

#pragma GCC diagnostic pop
BABYLON_NAMESPACE_END
