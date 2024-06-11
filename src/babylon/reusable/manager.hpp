#pragma once

#include "babylon/reusable/manager.h"

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ReusableAccessor begin
template <typename T>
inline ReusableAccessor<T>::ReusableAccessor(T** instance) noexcept
    : _instance(instance) {}

template <typename T>
inline T* ReusableAccessor<T>::operator->() const noexcept {
  return get();
}

template <typename T>
inline T* ReusableAccessor<T>::get() const noexcept {
  return *_instance;
}

template <typename T>
inline T& ReusableAccessor<T>::operator*() const noexcept {
  return *get();
}

template <typename T>
inline ReusableAccessor<T>::operator bool() const noexcept {
  return _instance != nullptr;
}
// ReusableAccessor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ReusableManager::TypedReusableUnit begin
template <typename R>
template <typename T>
ReusableManager<R>::TypedReusableUnit<T>::TypedReusableUnit(
    T* instance) noexcept
    : _instance {instance} {}

template <typename R>
template <typename T>
ReusableAccessor<T>
ReusableManager<R>::TypedReusableUnit<T>::accessor() noexcept {
  return {&_instance};
}

template <typename R>
template <typename T>
void ReusableManager<R>::TypedReusableUnit<T>::clear(R& resource) noexcept {
  Reuse::reconstruct(*_instance, MonotonicAllocator<T, R> {resource});
}

template <typename R>
template <typename T>
void ReusableManager<R>::TypedReusableUnit<T>::update() noexcept {
  Reuse::update_allocation_metadata(*_instance, _meta);
}

template <typename R>
template <typename T>
void ReusableManager<R>::TypedReusableUnit<T>::recreate(R& resource) noexcept {
  _instance = Reuse::create_with_allocation_metadata<T>(
      MonotonicAllocator<T, R> {resource}, _meta);
}
// ReusableManager::TypedReusableUnit end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ReusableManager begin
template <typename R>
R& ReusableManager<R>::resource() noexcept {
  return _resource;
}

template <typename R>
void ReusableManager<R>::set_recreate_interval(size_t interval) noexcept {
  _recreate_interval = interval;
}

template <typename R>
template <typename T, typename... Args>
ReusableAccessor<T> ReusableManager<R>::create_object(Args&&... args) noexcept {
  return create_object<T>([&](R& resource) {
    MonotonicAllocator<T, R> allocator {resource};
    return allocator.create(::std::forward<Args>(args)...);
  });
}

template <typename R>
template <typename T, typename C, typename>
ReusableAccessor<T> ReusableManager<R>::create_object(C&& creator) noexcept {
  auto instance = creator(_resource);
  return register_object(instance);
}

template <typename R>
void ReusableManager<R>::clear() noexcept {
  if (++_clear_times >= _recreate_interval) {
    _clear_times = 0;
    for (auto& unit : _units) {
      unit->update();
    }
    _resource.release();
    for (auto& unit : _units) {
      unit->recreate(_resource);
    }
  } else {
    for (auto& unit : _units) {
      unit->clear(_resource);
    }
  }
}

template <typename R>
template <typename T>
ReusableAccessor<T> ReusableManager<R>::register_object(T* instance) noexcept {
  auto unit = ::std::unique_ptr<TypedReusableUnit<T>>(
      new TypedReusableUnit<T> {instance});
  auto accessor = unit->accessor();
  {
    // 不在关键路径，简单用锁同步
    ::std::lock_guard<::std::mutex> lock(_mutex);
    _units.emplace_back(::std::move(unit));
  }
  return accessor;
}
// ReusableManager end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
