#pragma once

#include "babylon/anyflow/graph.h"
#include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

template <typename... D>
inline Closure Graph::run(D... data) noexcept {
  GraphData* root_data[] = {data...};
  return run(root_data, sizeof...(data));
}

template <typename T>
inline T* Graph::context() noexcept {
  if (ABSL_PREDICT_FALSE(!_context)) {
    _context = T();
  }
  return _context.get<T>();
}

template <>
inline ::babylon::Any* Graph::context<::babylon::Any>() noexcept {
  return &_context;
}

template <typename T, typename... Args>
inline T* Graph::create_object(Args&&... args) noexcept {
  ::babylon::SwissAllocator<T> allocator(_memory_resource);
  return allocator.create(::std::forward<Args>(args)...);
}

inline ::babylon::SwissMemoryResource& Graph::memory_resource() noexcept {
  return _memory_resource;
}

template <typename T, typename... Args>
inline ::babylon::ReusableAccessor<T> Graph::create_reusable_object(
    Args&&... args) noexcept {
  return _reusable_manager.create_object<T>(::std::forward<Args>(args)...);
}

} // namespace anyflow
BABYLON_NAMESPACE_END
