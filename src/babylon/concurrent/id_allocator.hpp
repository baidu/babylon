#pragma once

#include "babylon/concurrent/id_allocator.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h) // ABSL_PREDICT_FALSE
// clang-format on

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// VersionedValue begin
template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE VersionedValue<T>::VersionedValue(
    VersionAndValue version_and_value) noexcept {
  this->version_and_value = version_and_value;
}
// VersionedValue end
////////////////////////////////////////////////////////////////////////////////

template <typename T>
ABSL_ATTRIBUTE_NOINLINE VersionedValue<T> IdAllocator<T>::allocate() {
  auto current_head = free_head().load(::std::memory_order_acquire);
  while (current_head.value != FREE_LIST_TAIL) {
    VersionedValue<T> new_head;
    new_head.value =
        _free_next_value[current_head.value].load(::std::memory_order_relaxed);
    new_head.version = current_head.version;
    if (free_head().compare_exchange_weak(current_head, new_head,
                                          ::std::memory_order_acq_rel)) {
      _free_next_value[current_head.value].store(ACTIVE_FLAG,
                                                 ::std::memory_order_relaxed);
      return current_head;
    }
  }
  current_head.version_and_value =
      next_value().fetch_add(1, ::std::memory_order_relaxed);
  _free_next_value.ensure(current_head.value)
      .store(ACTIVE_FLAG, ::std::memory_order_relaxed);
  return current_head;
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE void IdAllocator<T>::deallocate(VersionedValue<T> id) {
  auto current_head = free_head().load(::std::memory_order_acquire);
  do {
    id.version = current_head.version + 1;
    _free_next_value[id.value].store(current_head.value,
                                     ::std::memory_order_relaxed);
  } while (!free_head().compare_exchange_weak(current_head, id,
                                              ::std::memory_order_release,
                                              ::std::memory_order_acquire));
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE T IdAllocator<T>::end() const noexcept {
  return next_value().load(::std::memory_order_acquire);
}

template <typename T>
template <typename C, typename>
inline void IdAllocator<T>::for_each(C&& callback) const {
  T iter_value = 0;
  T start_value = FREE_LIST_TAIL;
  auto snapshot = _free_next_value.snapshot();
  snapshot.for_each(
      0,
      ::std::min<size_t>(snapshot.size(),
                         next_value().load(::std::memory_order_acquire)),
      [&](const ::std::atomic<T>* aiter, const ::std::atomic<T>* aend) {
        const T* iter = reinterpret_cast<const T*>(aiter);
        const T* end = reinterpret_cast<const T*>(aend);
        while (iter < end) {
          if (start_value != FREE_LIST_TAIL) {
            auto* stop = ::std::find_if(iter, end, [](T value) {
              return value != ACTIVE_FLAG;
            });
            auto stop_value = stop - iter + iter_value;
            if (stop != end) {
              callback(start_value, stop_value);
              start_value = FREE_LIST_TAIL;
              iter = stop + 1;
              iter_value = stop_value + 1;
            } else {
              iter = stop;
              iter_value = stop_value;
            }
          } else {
            auto* stop = ::std::find(iter, end, ACTIVE_FLAG);
            auto stop_value = stop - iter + iter_value;
            if (stop != end) {
              start_value = stop_value;
              iter = stop + 1;
              iter_value = stop_value + 1;
            } else {
              iter = stop;
              iter_value = stop_value;
            }
          }
        }
      });
  if (iter_value > start_value) {
    callback(start_value, iter_value);
  }
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE ::std::atomic<T>&
IdAllocator<T>::next_value() noexcept {
  return reinterpret_cast<::std::atomic<T>&>(_next_value);
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE const ::std::atomic<T>&
IdAllocator<T>::next_value() const noexcept {
  return reinterpret_cast<const ::std::atomic<T>&>(_next_value);
}

template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE ::std::atomic<VersionedValue<T>>&
IdAllocator<T>::free_head() noexcept {
  return reinterpret_cast<::std::atomic<VersionedValue<T>>&>(_free_head);
}

#if __cplusplus < 201703L
template <typename T>
constexpr T IdAllocator<T>::FREE_LIST_TAIL;
template <typename T>
constexpr T IdAllocator<T>::ACTIVE_FLAG;
#endif // __cplusplus < 201703L

template <typename C, typename T, typename I>
inline ::std::basic_ostream<C, T>& operator<<(
    ::std::basic_ostream<C, T>& os, VersionedValue<I> versioned_value) {
  return os << "VersionedValue[" << versioned_value.value << "@"
            << versioned_value.version << "]";
}

namespace internal {
namespace concurrent_id_allocator {
template <typename T>
struct IdAllocatorFotType {
  static IdAllocator<uint16_t>& instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
    static IdAllocator<uint16_t> object;
#pragma GCC diagnostic pop
    return object;
  }
};
} // namespace concurrent_id_allocator
} // namespace internal

template <typename T>
// The identity of thread_local within inline function has some bug as mentioned
// in https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85400
//
// disable inline hint for buggy versions of GCC
#if !__clang__ && BABYLON_GCC_VERSION < 80400
ABSL_ATTRIBUTE_NOINLINE
#else  // __clang__ || BABYLON_GCC_VERSION >= 80400
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
#endif // __clang__ || BABYLON_GCC_VERSION >= 80400
       // clang-format off
VersionedValue<uint16_t> ThreadId::current_thread_id() noexcept {
       // clang-format on
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  thread_local ThreadId id(
      internal::concurrent_id_allocator::IdAllocatorFotType<T>::instance());
#pragma GCC diagnostic pop
  return id._value;
}

template <typename T>
inline uint16_t ThreadId::end() noexcept {
  return internal::concurrent_id_allocator::IdAllocatorFotType<T>::instance()
      .end();
}

template <typename T, typename C, typename>
ABSL_ATTRIBUTE_NOINLINE void ThreadId::for_each(C&& callback) {
  internal::concurrent_id_allocator::IdAllocatorFotType<T>::instance().for_each(
      ::std::forward<C>(callback));
}

inline ThreadId::ThreadId(IdAllocator<uint16_t>& allocator)
    : _allocator(allocator), _value(allocator.allocate()) {}

inline ThreadId::~ThreadId() noexcept {
  _allocator.deallocate(_value);
}

BABYLON_NAMESPACE_END
