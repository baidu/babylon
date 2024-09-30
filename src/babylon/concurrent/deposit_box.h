#pragma once

#include "babylon/concurrent/id_allocator.h"

#include "absl/types/optional.h"

BABYLON_NAMESPACE_BEGIN

template <typename T>
class DepositBox {
 public:
  inline static DepositBox& instance() noexcept;

  template <typename... Args>
  inline VersionedValue<uint32_t> emplace(Args&&... args) noexcept;

  inline ::absl::optional<T> take(VersionedValue<uint32_t> id) noexcept;

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
inline DepositBox<T>& DepositBox<T>::instance() noexcept {
  static DepositBox<T> object;
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
inline ::absl::optional<T> DepositBox<T>::take(
    VersionedValue<uint32_t> id) noexcept {
  auto& slot = _slots.ensure(id.value);
  if (slot.version.compare_exchange_strong(id.version, id.version + 1,
                                           ::std::memory_order_relaxed)) {
    ::absl::optional<T> result = ::std::move(slot.object);
    _slot_id_allocator.deallocate(id.value);
    return result;
  }
  return {};
}

BABYLON_NAMESPACE_END
