#pragma once

#include "babylon/concurrent/id_allocator.h"

#include "absl/types/optional.h"

BABYLON_NAMESPACE_BEGIN

// DepositBox is a container design for multiple visitor compete ownership for
// item put into it beforehand
template <typename T>
class DepositBox {
 public:
  class Accessor;

  inline static DepositBox& instance() noexcept;

  // Construct a new item as T(args...) and get an id back. This id is used by
  // take function, to retrieve the item back, like a receipt.
  //
  // Object is reused between emplace -> take -> emplace cycle. Each time
  // reused, an id of same value but different version is returned. An already
  // taken id of old version is safely used for take function, and ensured to
  // get empty accessor back no matter the same slot is resued or not.
  template <typename... Args>
  inline VersionedValue<uint32_t> emplace(Args&&... args) noexcept;

  // Get an accessor by id returned by emplace. Same id can be safe to called
  // multiple times from different thread concurrently. Only the first call will
  // get a valid accessor back. Later ones all get empty accessor, even after
  // the slot is released and reused again.
  //
  // Slot to store item is keep valid in accesor's scope. After the accessor
  // is destructed, slot and stored item is recycled and should not be
  // accessed again for this round.
  inline Accessor take(VersionedValue<uint32_t> id) noexcept;

  // Used to retrieve item back by id just like take function, in a non RAII
  // way. Useful when take in batch.
  inline T* take_released(VersionedValue<uint32_t> id) noexcept;
  inline void finish_released(VersionedValue<uint32_t> id) noexcept;

  // The **unsafe** way to get item instead of take it. Caller must ensure that
  // no other visitor could exist right now.
  //
  // Useful when you need to modify item just after emplace, before the id
  // is shared to others.
  inline T& unsafe_get(VersionedValue<uint32_t> id) noexcept;

 private:
  struct Slot {
    ::std::atomic<uint32_t> version;
    ::absl::optional<T> object;
  };

  DepositBox() = default;
  DepositBox(DepositBox&&) = delete;
  DepositBox(const DepositBox&) = delete;
  DepositBox& operator=(DepositBox&&) = delete;
  DepositBox& operator=(const DepositBox&) = delete;
  ~DepositBox() noexcept = default;

  IdAllocator<uint32_t> _slot_id_allocator;
  ConcurrentVector<Slot> _slots;
};

template <typename T>
class DepositBox<T>::Accessor {
 public:
  inline Accessor() noexcept = default;
  inline Accessor(Accessor&&) noexcept;
  inline Accessor(const Accessor&) noexcept = delete;
  inline Accessor& operator=(Accessor&&) noexcept;
  inline Accessor& operator=(const Accessor&) noexcept = delete;
  inline ~Accessor() noexcept;

  inline operator bool() const noexcept;
  inline T* operator->() noexcept;
  inline T& operator*() noexcept;

 private:
  inline Accessor(DepositBox* box, T* object,
                  VersionedValue<uint32_t> id) noexcept;

  DepositBox* _box {nullptr};
  T* _object {nullptr};
  VersionedValue<uint32_t> _id {UINT64_MAX};

  friend DepositBox;
};

////////////////////////////////////////////////////////////////////////////////
// DepositBox::Accessor begin
template <typename T>
inline DepositBox<T>::Accessor::Accessor(Accessor&& other) noexcept
    : Accessor {other._box, ::std::exchange(other._object, nullptr),
                other._id} {}

template <typename T>
inline typename DepositBox<T>::Accessor& DepositBox<T>::Accessor::operator=(
    Accessor&& other) noexcept {
  ::std::swap(_box, other._box);
  ::std::swap(_object, other._object);
  ::std::swap(_id, other._id);
  return *this;
}

template <typename T>
inline DepositBox<T>::Accessor::~Accessor() noexcept {
  if (_object) {
    _box->finish_released(_id);
  }
}

template <typename T>
inline DepositBox<T>::Accessor::operator bool() const noexcept {
  return _object;
}

template <typename T>
inline T* DepositBox<T>::Accessor::operator->() noexcept {
  return _object;
}

template <typename T>
inline T& DepositBox<T>::Accessor::operator*() noexcept {
  return *_object;
}

template <typename T>
inline DepositBox<T>::Accessor::Accessor(DepositBox* box, T* object,
                                         VersionedValue<uint32_t> id) noexcept
    : _box {box}, _object {object}, _id {id} {}
// DepositBox::Accessor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// DepositBox begin
template <typename T>
inline DepositBox<T>& DepositBox<T>::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static DepositBox<T> object;
#pragma GCC diagnostic pop
  return object;
}

template <typename T>
template <typename... Args>
inline VersionedValue<uint32_t> DepositBox<T>::emplace(
    Args&&... args) noexcept {
  auto id = _slot_id_allocator.allocate();
  auto& slot = _slots.ensure(id.value);
  slot.version.store(id.version, ::std::memory_order_relaxed);
  slot.object.emplace(::std::forward<Args>(args)...);
  return id;
}

template <typename T>
inline typename DepositBox<T>::Accessor DepositBox<T>::take(
    VersionedValue<uint32_t> id) noexcept {
  return {this, take_released(id), id};
}

template <typename T>
inline T* DepositBox<T>::take_released(VersionedValue<uint32_t> id) noexcept {
  auto& slot = _slots[id.value];
  if (slot.version.compare_exchange_strong(id.version, id.version + 1,
                                           ::std::memory_order_relaxed)) {
    return &*(slot.object);
  }
  return nullptr;
}

template <typename T>
inline void DepositBox<T>::finish_released(
    VersionedValue<uint32_t> id) noexcept {
  _slot_id_allocator.deallocate(id.value);
}

template <typename T>
inline T& DepositBox<T>::unsafe_get(VersionedValue<uint32_t> id) noexcept {
  auto& slot = _slots[id.value];
  return *slot.object;
}
// DepositBox end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
