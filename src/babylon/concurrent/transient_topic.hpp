#pragma once

#include "babylon/concurrent/transient_topic.h"

#if ABSL_HAVE_THREAD_SANITIZER
#include <sanitizer/tsan_interface.h> // ::__tsan_acquire/::__tsan_release
#endif                                // ABSL_HAVE_THREAD_SANITIZER

BABYLON_NAMESPACE_BEGIN
#pragma GCC diagnostic push
#if GCC_VERSION >= 120000
#pragma GCC diagnostic ignored "-Wtsan"
#endif // GCC_VERSION >= 120000

///////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic begin
template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::ConcurrentTransientTopic(
    ConcurrentTransientTopic&& other) noexcept
    : ConcurrentTransientTopic {} {
  *this = ::std::move(other);
}

template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>&
ConcurrentTransientTopic<T, S>::operator=(
    ConcurrentTransientTopic&& other) noexcept {
  _slots = ::std::move(other._slots);
  auto tmp = _next_event_index.load(::std::memory_order_relaxed);
  _next_event_index.store(
      other._next_event_index.load(::std::memory_order_relaxed),
      ::std::memory_order_relaxed);
  other._next_event_index.store(tmp, ::std::memory_order_relaxed);
  return *this;
}

template <typename T, typename S>
template <typename U, typename ::std::enable_if<
                          ::std::is_assignable<T&, U>::value, int>::type>
inline void ConcurrentTransientTopic<T, S>::publish(U&& value) noexcept {
  publish<true>(::std::forward<U>(value));
}

template <typename T, typename S>
template <
    bool CONCURRENT, typename U,
    typename ::std::enable_if<::std::is_assignable<T&, U>::value, int>::type>
inline void ConcurrentTransientTopic<T, S>::publish(U&& value) noexcept {
  publish_n<CONCURRENT>(1, [&](Iterator iter, Iterator) {
    *iter = ::std::forward<U>(value);
  });
}

template <typename T, typename S>
template <typename C, typename>
inline void ConcurrentTransientTopic<T, S>::publish_n(size_t num,
                                                      C&& callback) noexcept {
  publish_n<true>(num, ::std::forward<C>(callback));
}

template <typename T, typename S>
template <bool CONCURRENT, typename C, typename>
inline void ConcurrentTransientTopic<T, S>::publish_n(size_t num,
                                                      C&& callback) noexcept {
  size_t begin_index = 0;
  if (CONCURRENT) {
    begin_index = _next_event_index.fetch_add(num, ::std::memory_order_relaxed);
  } else {
    begin_index = _next_event_index.load(::std::memory_order_relaxed);
    _next_event_index.store(begin_index + num, ::std::memory_order_relaxed);
  }
  auto end_index = begin_index + num;
  auto snpashot = _slots.reserved_snapshot(end_index);
  snpashot.for_each(begin_index, end_index, [&](Slot* begin, Slot* end) {
    callback(Iterator(begin), Iterator(end));
    ::std::atomic_thread_fence(::std::memory_order_release);
#if ABSL_HAVE_THREAD_SANITIZER
    for (auto iter = begin; iter != end; ++iter) {
      iter->futex.mark_tsan_release();
    }
#endif // ABSL_HAVE_THREAD_SANITIZER
    for (auto iter = begin; iter != end; ++iter) {
      iter->futex.set_published();
    }
    ::std::atomic_thread_fence(::std::memory_order_seq_cst);
    for (auto iter = begin; iter != end; ++iter) {
      iter->futex.wakeup_waiters();
    }
  });
}

template <typename T, typename S>
inline void ConcurrentTransientTopic<T, S>::close() noexcept {
  auto index = _next_event_index.load(::std::memory_order_relaxed);
  auto& slot = _slots.ensure(index);
  slot.futex.set_closed();
  ::std::atomic_thread_fence(::std::memory_order_seq_cst);
  slot.futex.wakeup_waiters();
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::Consumer
ConcurrentTransientTopic<T, S>::subscribe() noexcept {
  return Consumer(this);
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::ConstConsumer
ConcurrentTransientTopic<T, S>::subscribe() const noexcept {
  return const_cast<ConcurrentTransientTopic<T, S>*>(this)->subscribe();
}

template <typename T, typename S>
inline void ConcurrentTransientTopic<T, S>::clear() noexcept {
  _slots.for_each(0, _slots.size(), [](Slot* iter, Slot* end) {
    while (iter != end) {
      (*iter++).futex.reset();
    }
  });
  _next_event_index.store(0, ::std::memory_order_relaxed);
}
// ConcurrentTransientTopic end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::SlotFutex begin
template <typename T, typename S>
inline void
ConcurrentTransientTopic<T, S>::SlotFutex::set_published() noexcept {
  auto& value = _futex.value();
  auto& status = reinterpret_cast<::std::atomic<uint16_t>&>(value);
  status.store(PUBLISHED, ::std::memory_order_relaxed);
}

template <typename T, typename S>
inline void ConcurrentTransientTopic<T, S>::SlotFutex::set_closed() noexcept {
  auto& value = _futex.value();
  auto& status = reinterpret_cast<::std::atomic<uint16_t>&>(value);
  status.store(CLOSED, ::std::memory_order_relaxed);
}

template <typename T, typename S>
inline void
ConcurrentTransientTopic<T, S>::SlotFutex::wakeup_waiters() noexcept {
  auto current_status_and_waiters =
      _futex.value().load(::std::memory_order_relaxed);
  if (current_status_and_waiters <= UINT16_MAX) {
    return;
  }
  wakeup_waiters_slow(current_status_and_waiters);
}

template <typename T, typename S>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentTransientTopic<T, S>::SlotFutex::wakeup_waiters_slow(
    uint32_t current_status_and_waiters) noexcept {
  uint16_t status = current_status_and_waiters;
  _futex.value().compare_exchange_weak(current_status_and_waiters, status,
                                       ::std::memory_order_relaxed);
  _futex.wake_all();
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::SlotFutex::is_published()
    const noexcept {
  auto& value = _futex.value();
  auto& status = reinterpret_cast<const ::std::atomic<uint16_t>&>(value);
  return PUBLISHED == status.load(::std::memory_order_relaxed);
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::SlotFutex::is_closed()
    const noexcept {
  auto& value = _futex.value();
  auto& status = reinterpret_cast<const ::std::atomic<uint16_t>&>(value);
  return CLOSED == status.load(::std::memory_order_relaxed);
}

template <typename T, typename S>
inline void
ConcurrentTransientTopic<T, S>::SlotFutex::wait_until_ready() noexcept {
  auto current_status_and_waiters =
      _futex.value().load(::std::memory_order_relaxed);
  uint16_t status = current_status_and_waiters;
  if (status != INITIAL) {
    return;
  }
  wait_until_ready_slow(current_status_and_waiters);
}

template <typename T, typename S>
ABSL_ATTRIBUTE_NOINLINE void
ConcurrentTransientTopic<T, S>::SlotFutex::wait_until_ready_slow(
    uint32_t current_status_and_waiters) noexcept {
  uint16_t status = current_status_and_waiters;
  while (status == INITIAL) {
    if (current_status_and_waiters <= UINT16_MAX) {
      uint32_t wait_status_and_waiters =
          current_status_and_waiters + UINT16_MAX + 1;
      if (_futex.value().compare_exchange_weak(current_status_and_waiters,
                                               wait_status_and_waiters,
                                               ::std::memory_order_relaxed)) {
        _futex.wait(wait_status_and_waiters, nullptr);
      }
    } else {
      _futex.wait(current_status_and_waiters, nullptr);
    }
    current_status_and_waiters =
        _futex.value().load(::std::memory_order_relaxed);
    status = current_status_and_waiters;
  }
}

template <typename T, typename S>
inline void ConcurrentTransientTopic<T, S>::SlotFutex::reset() noexcept {
  _futex.value().store(INITIAL, ::std::memory_order_relaxed);
}

template <typename T, typename S>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ConcurrentTransientTopic<T, S>::SlotFutex::mark_tsan_acquire() noexcept {
#if ABSL_HAVE_THREAD_SANITIZER
  __tsan_acquire(&_futex.value());
#endif // ABSL_HAVE_THREAD_SANITIZER
}

template <typename T, typename S>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE void
ConcurrentTransientTopic<T, S>::SlotFutex::mark_tsan_release() noexcept {
#if ABSL_HAVE_THREAD_SANITIZER
  __tsan_release(&_futex.value());
#endif // ABSL_HAVE_THREAD_SANITIZER
}
// ConcurrentTransientTopic::SlotFutex end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::Iterator begin
template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::Iterator::Iterator(Slot* slot) noexcept
    : _slot(slot) {}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::Iterator&
ConcurrentTransientTopic<T, S>::Iterator::operator++() noexcept {
  ++_slot;
  return *this;
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::Iterator
ConcurrentTransientTopic<T, S>::Iterator::operator++(int) noexcept {
  Iterator ret = *this;
  ++(*this);
  return ret;
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::Iterator
ConcurrentTransientTopic<T, S>::Iterator::operator+(
    ssize_t offset) const noexcept {
  return Iterator(_slot + offset);
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::Iterator
ConcurrentTransientTopic<T, S>::Iterator::operator-(
    ssize_t offset) const noexcept {
  return Iterator(_slot - offset);
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator==(
    Iterator other) const noexcept {
  return _slot == other._slot;
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator!=(
    Iterator other) const noexcept {
  return !(*this == other);
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator<(
    Iterator other) const noexcept {
  return _slot < other._slot;
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator<=(
    Iterator other) const noexcept {
  return _slot <= other._slot;
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator>(
    Iterator other) const noexcept {
  return _slot > other._slot;
}

template <typename T, typename S>
inline bool ConcurrentTransientTopic<T, S>::Iterator::operator>=(
    Iterator other) const noexcept {
  return _slot >= other._slot;
}

template <typename T, typename S>
inline T& ConcurrentTransientTopic<T, S>::Iterator::operator*() const noexcept {
  return _slot->value;
}

template <typename T, typename S>
inline T* ConcurrentTransientTopic<T, S>::Iterator::operator->()
    const noexcept {
  return &_slot->value;
}

template <typename T, typename S>
inline ssize_t ConcurrentTransientTopic<T, S>::Iterator::operator-(
    Iterator other) const noexcept {
  return _slot - other._slot;
}
// ConcurrentTransientTopic::Iterator end
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::ConsumeRange begin
template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::ConsumeRange::ConsumeRange(
    typename SlotVector::Snapshot snapshot, size_t begin, size_t size) noexcept
    : _snapshot(snapshot), _begin(begin), _size(size) {}

template <typename T, typename S>
inline size_t ConcurrentTransientTopic<T, S>::ConsumeRange::size()
    const noexcept {
  return _size;
}

template <typename T, typename S>
inline T& ConcurrentTransientTopic<T, S>::ConsumeRange::operator[](
    size_t index) noexcept {
  return _snapshot[_begin + index].value;
}

template <typename T, typename S>
inline const T& ConcurrentTransientTopic<T, S>::ConsumeRange::operator[](
    size_t index) const noexcept {
  return _snapshot[_begin + index].value;
}
// ConcurrentTransientTopic::ConsumeRange end
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::Consumer begin
template <typename T, typename S>
inline T* ConcurrentTransientTopic<T, S>::Consumer::consume() noexcept {
  auto range = consume(1);
  if (range.size() > 0) {
    return &range[0];
  }
  return nullptr;
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::ConsumeRange
ConcurrentTransientTopic<T, S>::Consumer::consume(size_t num) noexcept {
  auto begin_index = _next_consume_index;
  auto end_index = begin_index + num;
  auto snpashot = _queue->_slots.reserved_snapshot(end_index);
  size_t consumed = 0;
  bool closed = false;
  snpashot.for_each(begin_index, end_index, [&](Slot* iter, Slot* end) {
    if (closed) {
      return;
    }
    while (iter != end) {
      auto& slot = *iter;
      if (slot.futex.is_closed()) {
        closed = true;
        return;
      } else if (slot.futex.is_published()) {
        ++consumed;
        ++iter;
        continue;
      }
      slot.futex.wait_until_ready();
    }
  });
  _next_consume_index += consumed;
  ::std::atomic_thread_fence(::std::memory_order_acquire);
#if ABSL_HAVE_THREAD_SANITIZER
  for (size_t i = 0; i < consumed; ++i) {
    snpashot[begin_index + i].futex.mark_tsan_acquire();
  }
#endif // ABSL_HAVE_THREAD_SANITIZER
  return ConsumeRange {snpashot, begin_index, consumed};
}

template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::Consumer::Consumer(
    ConcurrentTransientTopic* queue) noexcept
    : _queue(queue) {}
// ConcurrentTransientTopic::Consumer end
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::ConstConsumeRange begin
template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::ConstConsumeRange::ConstConsumeRange(
    ConsumeRange other) noexcept
    : ConsumeRange(other) {}

template <typename T, typename S>
inline const T& ConcurrentTransientTopic<T, S>::ConstConsumeRange::operator[](
    size_t index) const noexcept {
  return ConsumeRange::operator[](index);
}
// ConcurrentTransientTopic::ConstConsumeRange end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ConcurrentTransientTopic::ConstConsumer begin
template <typename T, typename S>
inline ConcurrentTransientTopic<T, S>::ConstConsumer::ConstConsumer(
    const Consumer& other) noexcept
    : Consumer {other} {}

template <typename T, typename S>
inline const T*
ConcurrentTransientTopic<T, S>::ConstConsumer::consume() noexcept {
  return Consumer::consume();
}

template <typename T, typename S>
inline typename ConcurrentTransientTopic<T, S>::ConstConsumeRange
ConcurrentTransientTopic<T, S>::ConstConsumer::consume(size_t num) noexcept {
  return Consumer::consume(num);
}
// ConcurrentTransientTopic::ConstConsumer end
////////////////////////////////////////////////////////////////////////////////

#pragma GCC diagnostic pop
BABYLON_NAMESPACE_END
