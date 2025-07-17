#pragma once

#include "babylon/concurrent/object_pool.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/base/optimization.h) // ABSL_PREDICT_FALSE
// clang-format on

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ObjectPool begin
template <typename T>
void ObjectPool<T>::reserve_and_clear(size_t capacity) noexcept {
  _free_objects.reserve_and_clear(capacity * 2);
  _capacity = capacity;
}

template <typename T>
template <typename C>
void ObjectPool<T>::set_creator(C&& create_callback) noexcept {
  _object_creator = ::std::forward<C>(create_callback);
}

template <typename T>
template <typename C>
void ObjectPool<T>::set_recycler(C&& recycle_callback) noexcept {
  _object_recycler = ::std::forward<C>(recycle_callback);
}

template <typename T>
::std::unique_ptr<T, typename ObjectPool<T>::Deleter>
ObjectPool<T>::pop() noexcept {
  ::std::unique_ptr<T, Deleter> result {nullptr, this};
  if (_object_creator) {
    using IT = typename ConcurrentBoundedQueue<::std::unique_ptr<T>>::Iterator;
    _free_objects.template pop_n(
        [&](IT iter, IT) {
          result.reset(iter->release());
        },
        [&](IT iter, IT end) {
          while (iter != end) {
            *iter++ = _object_creator();
          }
        },
        1);
  } else {
    _free_objects.template pop<true, true, false>(
        [&](::std::unique_ptr<T>& object) {
          result.reset(object.release());
        });
  }
  return result;
}

template <typename T>
::std::unique_ptr<T, typename ObjectPool<T>::Deleter>
ObjectPool<T>::try_pop() noexcept {
  ::std::unique_ptr<T, Deleter> result {nullptr, this};
  _free_objects.template try_pop<true, false>(
      [&](::std::unique_ptr<T>& object) {
        result.reset(object.release());
      });
  return result;
}

template <typename T>
void ObjectPool<T>::push(::std::unique_ptr<T>&& object) noexcept {
  _object_recycler(*object);
  if (_object_creator) {
    if (ABSL_PREDICT_FALSE(_capacity <= _free_objects.size())) {
      return;
    }
    using IT = typename ConcurrentBoundedQueue<::std::unique_ptr<T>>::Iterator;
    _free_objects.template push_n(
        [&](IT iter, IT) {
          *iter = ::std::move(object);
        },
        [](IT iter, IT end) {
          while (iter != end) {
            (*iter++).reset();
          }
        },
        1);
  } else {
    _free_objects.template push<true, false, true>(::std::move(object));
  }
}

template <typename T>
void ObjectPool<T>::push(::std::unique_ptr<T, Deleter>&& object) noexcept {
  push(::std::unique_ptr<T> {object.release()});
}

template <typename T>
inline size_t ObjectPool<T>::free_object_number() const noexcept {
  return _free_objects.size();
}
// ObjectPool end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ObjectPool::Deleter begin
template <typename T>
inline ObjectPool<T>::Deleter::Deleter(ObjectPool<T>* pool) noexcept
    : _pool {pool} {}

template <typename T>
inline ObjectPool<T>::Deleter::Deleter(Deleter&& other) noexcept {
  ::std::swap(_pool, other._pool);
}

template <typename T>
inline typename ObjectPool<T>::Deleter& ObjectPool<T>::Deleter::operator=(
    Deleter&& other) noexcept {
  ::std::swap(_pool, other._pool);
}

template <typename T>
inline void ObjectPool<T>::Deleter::operator()(T* ptr) noexcept {
  if (_pool != nullptr) {
    _pool->push(::std::unique_ptr<T> {ptr});
  }
}
// ObjectPool::Deleter end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
