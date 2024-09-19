#pragma once

#include "babylon/concurrent/id_allocator.h"
#include "babylon/concurrent/vector.h"

#pragma GCC diagnostic push
#if GCC_VERSION >= 120000
#pragma GCC diagnostic ignored "-Wtsan"
#endif // GCC_VERSION >= 120000

BABYLON_NAMESPACE_BEGIN

// Standalone epoch implementation as a part of EBR (Epoch-Based Reclamation)
// which described in https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-579.pdf
//
// With modifications below:
// - Spilt global epoch advance and local epoch check out of critical region
// - Support non-thread-local and long-term critical region
//
// This makes read-only access faster and decouple with reclaim operation.
// Also, this makes it possible to amortize overhead of read / modify / reclaim
// operation, like NEBR mentioned in
// https://sysweb.cs.toronto.edu/publication_files/0000/0159/jpdc07.pdf
class Epoch {
 public:
  // Accessor is an explicit handle for construct critical region.
  // An Accessor join epoch calculation just a thread in standard algorithm.
  //
  // Cannot mix usage of Accessor and implicit thread local style.
  class Accessor;
  inline Accessor create_accessor() noexcept;

  // Total num of Accessor created.
  // Contain released accessors, which may be reused in subsequent
  // create_accessor
  inline size_t accessor_number() const noexcept;

  // Standard thread local style to construct critical region.
  // Satisfy STL BasicLockable requirement.
  //
  // Cannot mix usage of Accessor and implicit thread local style.
  inline void lock() noexcept;
  inline void unlock() noexcept;

  // Advance global epoch and get epoch after increase.
  // This epoch value is used to check whether previous retired objects can be
  // safely reclaimed, if that value <= low_water_mark.
  inline uint64_t tick() noexcept;

  // Collect observations of all thread local or Accessor, calculate low water
  // mark
  inline uint64_t low_water_mark() const noexcept;

 private:
  class alignas(BABYLON_CACHELINE_SIZE) Slot {
   public:
    Slot() noexcept = default;
    Slot(Slot&&) = delete;
    Slot(const Slot&) = delete;
    Slot& operator=(Slot&&) = delete;
    Slot& operator=(const Slot&) = delete;
    ~Slot() noexcept = default;

    ::std::atomic<uint64_t> version {UINT64_MAX};
    size_t lock_times {0};
  };

  inline void unregister_accessor(size_t index) noexcept;

  inline void lock(size_t index) noexcept;
  inline void unlock(size_t index) noexcept;

  IdAllocator<uint32_t> _id_allocator;
  ConcurrentVector<Slot> _slots;
  ::std::atomic<uint64_t> _version {0};

  friend Accessor;
};

class Epoch::Accessor {
 public:
  inline Accessor() noexcept = default;
  inline Accessor(Accessor&&) noexcept;
  Accessor(const Accessor&) = delete;
  inline Accessor& operator=(Accessor&&) noexcept;
  Accessor& operator=(const Accessor&) = delete;
  inline ~Accessor() noexcept;

  // Whether this accessor is usable.
  inline operator bool() const noexcept;

  // Explicit construct critical region.
  // Satisfy STL BasicLockable requirement.
  //
  // Cannot mix usage of Accessor and implicit thread local style.
  inline void lock() noexcept;
  inline void unlock() noexcept;

  // Unbind this accessor. It is not usable anymore.
  inline void release() noexcept;

 private:
  inline Accessor(Epoch* epoch, size_t index) noexcept;
  inline void swap(Accessor& other) noexcept;

  Epoch* _epoch {nullptr};
  size_t _index {SIZE_MAX};

  friend Epoch;
};

////////////////////////////////////////////////////////////////////////////////
// Epoch begin
inline Epoch::Accessor Epoch::create_accessor() noexcept {
  auto index = _id_allocator.allocate().value;
  _slots.ensure(index);
  return {this, index};
}

inline size_t Epoch::accessor_number() const noexcept {
  return _id_allocator.end();
}

inline void Epoch::lock() noexcept {
  auto index = ThreadId::current_thread_id<Epoch>().value;
  _slots.ensure(index);
  lock(index);
}

inline void Epoch::unlock() noexcept {
  auto index = ThreadId::current_thread_id<Epoch>().value;
  unlock(index);
}

inline void Epoch::unregister_accessor(size_t index) noexcept {
  _id_allocator.deallocate(index);
}

inline void Epoch::lock(size_t index) noexcept {
  auto& slot = _slots[index];
  slot.lock_times += 1;
  if (slot.lock_times == 1) {
    auto global_version = _version.load(::std::memory_order_relaxed);
    slot.version.store(global_version, ::std::memory_order_relaxed);
    ::std::atomic_thread_fence(::std::memory_order_seq_cst);
  }
}

inline void Epoch::unlock(size_t index) noexcept {
  auto& slot = _slots[index];
  if (slot.lock_times == 1) {
    slot.version.store(UINT64_MAX, ::std::memory_order_release);
  }
  slot.lock_times -= 1;
}

inline uint64_t Epoch::tick() noexcept {
#if __x86_64__
  auto version = 1 + _version.fetch_add(1, ::std::memory_order_seq_cst);
#else  // !__x86_64__
  auto version = 1 + _version.fetch_add(1, ::std::memory_order_relaxed);
  ::std::atomic_thread_fence(::std::memory_order_seq_cst);
#endif // !__x86_64__
  return version;
}

inline uint64_t Epoch::low_water_mark() const noexcept {
  auto min_verison = UINT64_MAX;
  auto slots = _slots.snapshot();
  auto number = accessor_number();
  if (number == 0) {
    number = ThreadId::end<Epoch>();
  }
  slots.for_each(0, ::std::min(number, slots.size()),
                 [&](const Slot* iter, const Slot* end) {
                   while (iter != end) {
                     auto local_version =
                         (iter++)->version.load(::std::memory_order_acquire);
                     if (min_verison > local_version) {
                       min_verison = local_version;
                     }
                   }
                 });
  return min_verison;
}
// Epoch end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Epoch::Accessor begin
inline Epoch::Accessor::Accessor(Accessor&& other) noexcept {
  swap(other);
}

inline Epoch::Accessor& Epoch::Accessor::operator=(Accessor&& other) noexcept {
  swap(other);
  return *this;
}

inline Epoch::Accessor::~Accessor() noexcept {
  release();
}

inline Epoch::Accessor::operator bool() const noexcept {
  return _epoch != nullptr;
}

inline Epoch::Accessor::Accessor(Epoch* epoch, size_t index) noexcept
    : _epoch {epoch}, _index {index} {}

inline void Epoch::Accessor::swap(Accessor& other) noexcept {
  ::std::swap(_epoch, other._epoch);
  ::std::swap(_index, other._index);
}

inline void Epoch::Accessor::lock() noexcept {
  _epoch->lock(_index);
}

inline void Epoch::Accessor::unlock() noexcept {
  _epoch->unlock(_index);
}

inline void Epoch::Accessor::release() noexcept {
  if (_epoch != nullptr) {
    _epoch->unregister_accessor(_index);
    _epoch = nullptr;
  }
}
// Epoch::Accessor end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

#pragma GCC diagnostic pop
