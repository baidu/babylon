#pragma once

#include "babylon/concurrent/id_allocator.h"
#include "babylon/concurrent/vector.h"

BABYLON_NAMESPACE_BEGIN

class GarbageCollector {
 public:
  class Accessor;
  class AccessScope;

  inline Accessor register_accessor() noexcept;

  inline AccessScope access(const Accessor&) noexcept;

 private:

  IdAllocator<uint32_t> id_allocator;
};

class GarbageCollector::Accessor {
 public:
  inline Accessor() noexcept = default;
  inline Accessor(Accessor&&) noexcept = default;
  Accessor(const Accessor&) = delete;
  inline Accessor& operator=(Accessor&&) noexcept = default;
  Accessor& operator=(const Accessor&) = delete;
  inline ~Accessor() noexcept = default;

  inline operator bool() const noexcept;

  inline size_t index() const noexcept;
  
 private:
  inline Accessor(size_t index) noexcept;

  size_t _index {SIZE_MAX};
};

inline GarbageCollector::operator bool() const noexcept {
  return _index != SIZE_MAX;
}

inline size_t GarbageCollector::index() const noexcept {
  return _index != SIZE_MAX;
}

inline GarbageCollector::Accessor(uint32_t index) noexcept :
  _index {index} {}

inline Accessor GarbageCollector::register_accessor() noexcept {
  return Accessor {id_allocator.allocate()};
}

BABYLON_NAMESPACE_END
