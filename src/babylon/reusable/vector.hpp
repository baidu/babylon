#pragma once

#include <babylon/reusable/vector.h>

BABYLON_NAMESPACE_BEGIN

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    ReusableVector&& other) noexcept
    : ReusableVector(other.get_allocator()) {
  swap(other);
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    const ReusableVector& other) noexcept
    : ReusableVector(other, other.get_allocator()) {}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>&
ReusableVector<T, MonotonicAllocator<U, A>>::operator=(
    const ReusableVector& other) noexcept {
  assign(other.begin(), other.end());
  return *this;
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>&
ReusableVector<T, MonotonicAllocator<U, A>>::operator=(
    ReusableVector&& other) noexcept {
  if (_allocator == other.get_allocator()) {
    swap(other);
  } else {
    clear();
    reserve(other.size());
    for (auto& value : other) {
      emplace_back(::std::move(value));
    }
  }
  return *this;
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::~ReusableVector() noexcept {
  if CONSTEXPR_SINCE_CXX17 (!::std::is_trivially_destructible<
                                value_type>::value) {
    for (size_t i = 0; i < _constructed_size; ++i) {
      _data[i].~value_type();
    }
  }
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    allocator_type allocator) noexcept
    : _allocator {allocator} {}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    ReusableVector&& other, allocator_type allocator) noexcept
    : ReusableVector(allocator) {
  *this = ::std::move(other);
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    const ReusableVector& other, allocator_type allocator) noexcept
    : ReusableVector(other.begin(), other.end(), allocator) {}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    size_type count, allocator_type allocator) noexcept
    : _allocator(allocator),
      _data(_allocator.allocate(count)),
      _size(count),
      _constructed_size(count),
      _capacity(count) {
  for (size_t i = 0; i < _size; ++i) {
    _allocator.construct(&_data[i]);
  }
}

template <typename T, typename U, typename A>
template <typename V>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    size_type count, const V& value, allocator_type allocator) noexcept
    : _allocator(allocator),
      _data(_allocator.allocate(count)),
      _size(count),
      _constructed_size(count),
      _capacity(count) {
  for (size_t i = 0; i < _size; ++i) {
    _allocator.construct(&_data[i], value);
  }
}

template <typename T, typename U, typename A>
template <typename IT, typename>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    IT first, IT last, allocator_type allocator) noexcept
    : _allocator {allocator},
      _size(::std::distance(first, last)),
      _constructed_size {_size},
      _capacity {_size} {
  _data = _allocator.allocate(_size);
  for (size_t i = 0; i < _size; ++i) {
    _allocator.construct(&_data[i], *first++);
  }
}

template <typename T, typename U, typename A>
template <typename V>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    ::std::initializer_list<V> initializer_list,
    allocator_type allocator) noexcept
    : ReusableVector(initializer_list.begin(), initializer_list.end(),
                     allocator) {}

template <typename T, typename U, typename A>
template <typename V>
inline ReusableVector<T, MonotonicAllocator<U, A>>&
ReusableVector<T, MonotonicAllocator<U, A>>::operator=(
    ::std::initializer_list<V> initializer_list) noexcept {
  assign(initializer_list);
  return *this;
}

template <typename T, typename U, typename A>
template <typename V>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::assign(
    size_type count, const V& value) noexcept {
  clear();
  reserve(count);
  for (size_type i = 0; i < count; ++i) {
    emplace_back(value);
  }
}

template <typename T, typename U, typename A>
template <typename IT, typename>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::assign(
    IT first, IT last) noexcept {
  using V = decltype(*first);
  clear();
  reserve(::std::distance(first, last));
  ::std::for_each(first, last, [this](V value) {
    emplace_back(value);
  });
}

template <typename T, typename U, typename A>
template <typename V>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::assign(
    ::std::initializer_list<V> initializer_list) noexcept {
  assign(initializer_list.begin(), initializer_list.end());
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::allocator_type
ReusableVector<T, MonotonicAllocator<U, A>>::get_allocator() const noexcept {
  return _allocator;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::reference
ReusableVector<T, MonotonicAllocator<U, A>>::operator[](
    size_type pos) noexcept {
  assert(pos < _size && "index overflow");
  return _data[pos];
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reference
ReusableVector<T, MonotonicAllocator<U, A>>::operator[](
    size_type pos) const noexcept {
  assert(pos < _size && "index overflow");
  return _data[pos];
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::reference
ReusableVector<T, MonotonicAllocator<U, A>>::front() noexcept {
  return operator[](0);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reference
ReusableVector<T, MonotonicAllocator<U, A>>::front() const noexcept {
  return operator[](0);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::reference
ReusableVector<T, MonotonicAllocator<U, A>>::back() noexcept {
  return operator[](_size - 1);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reference
ReusableVector<T, MonotonicAllocator<U, A>>::back() const noexcept {
  return operator[](_size - 1);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::pointer
ReusableVector<T, MonotonicAllocator<U, A>>::data() noexcept {
  return _data;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_pointer
ReusableVector<T, MonotonicAllocator<U, A>>::data() const noexcept {
  return _data;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::begin() noexcept {
  return iterator(&_data[0]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::begin() const noexcept {
  return const_iterator(&_data[0]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::cbegin() const noexcept {
  return const_iterator(&_data[0]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::end() noexcept {
  return iterator(&_data[_size]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::end() const noexcept {
  return const_iterator(&_data[_size]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::const_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::cend() const noexcept {
  return const_iterator(&_data[_size]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::reverse_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::rbegin() noexcept {
  return reverse_iterator(end());
}

template <typename T, typename U, typename A>
inline
    typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reverse_iterator
    ReusableVector<T, MonotonicAllocator<U, A>>::rbegin() const noexcept {
  return const_reverse_iterator(end());
}

template <typename T, typename U, typename A>
inline
    typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reverse_iterator
    ReusableVector<T, MonotonicAllocator<U, A>>::crbegin() const noexcept {
  return const_reverse_iterator(end());
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::reverse_iterator
ReusableVector<T, MonotonicAllocator<U, A>>::rend() noexcept {
  return reverse_iterator(begin());
}

template <typename T, typename U, typename A>
inline
    typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reverse_iterator
    ReusableVector<T, MonotonicAllocator<U, A>>::rend() const noexcept {
  return const_reverse_iterator(begin());
}

template <typename T, typename U, typename A>
inline
    typename ReusableVector<T, MonotonicAllocator<U, A>>::const_reverse_iterator
    ReusableVector<T, MonotonicAllocator<U, A>>::crend() const noexcept {
  return const_reverse_iterator(begin());
}

template <typename T, typename U, typename A>
inline bool ReusableVector<T, MonotonicAllocator<U, A>>::empty()
    const noexcept {
  return _size == 0;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::size_type
ReusableVector<T, MonotonicAllocator<U, A>>::size() const noexcept {
  return _size;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::size_type
ReusableVector<T, MonotonicAllocator<U, A>>::constructed_size() const noexcept {
  return _constructed_size;
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::reserve(
    size_type min_capacity) noexcept {
  if (_capacity >= min_capacity) {
    return;
  }

  auto new_data = _allocator.allocate(min_capacity);
  for (size_type i = 0; i < _constructed_size; ++i) {
    _allocator.construct(&new_data[i], ::std::move(_data[i]));
    _allocator.destroy(&_data[i]);
  }
  _data = new_data;
  _capacity = min_capacity;
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::size_type
ReusableVector<T, MonotonicAllocator<U, A>>::capacity() const noexcept {
  return _capacity;
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::clear() noexcept {
  _size = 0;
}

template <typename T, typename U, typename A>
template <typename V>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::insert(const_iterator pos,
                                                    V&& value) noexcept {
  return emplace(pos, ::std::forward<V>(value));
}

template <typename T, typename U, typename A>
template <typename V>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::insert(const_iterator pos,
                                                    size_type count,
                                                    const V& value) noexcept {
  size_type index = &*pos - _data;
  auto reconstruct_end_size = prepare_for_insert(index, count);
  for (size_type i = index; i < reconstruct_end_size; ++i) {
    ValueReusableTraits::reconstruct(_data[i], _allocator, value);
  }
  for (size_type i = reconstruct_end_size; i < index + count; ++i) {
    _allocator.construct(&_data[i], value);
    ++_constructed_size;
  }
  return iterator(&_data[index]);
}

template <typename T, typename U, typename A>
template <typename IT, typename>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::insert(const_iterator pos,
                                                    IT first,
                                                    IT last) noexcept {
  size_type index = &*pos - _data;
  auto count = ::std::distance(first, last);
  auto reconstruct_end_size = prepare_for_insert(index, count);
  for (size_type i = index; i < reconstruct_end_size; ++i) {
    ValueReusableTraits::reconstruct(_data[i], _allocator, *first++);
  }
  for (size_type i = reconstruct_end_size; i < index + count; ++i) {
    _allocator.construct(&_data[i], *first++);
    ++_constructed_size;
  }
  return iterator(&_data[index]);
}

template <typename T, typename U, typename A>
template <typename V>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::insert(
    const_iterator pos, ::std::initializer_list<V> initializer_list) noexcept {
  return insert(pos, initializer_list.begin(), initializer_list.end());
}

template <typename T, typename U, typename A>
template <typename... Args, typename>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::emplace(const_iterator pos,
                                                     Args&&... args) noexcept {
  size_type index = &*pos - _data;
  auto reconstruct_end_size = prepare_for_insert(index, 1);
  if (index < reconstruct_end_size) {
    ValueReusableTraits::reconstruct(_data[index], _allocator,
                                     ::std::forward<Args>(args)...);
  } else {
    _allocator.construct(&_data[index], ::std::forward<Args>(args)...);
    ++_constructed_size;
  }
  return iterator(&_data[index]);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::erase(
    const_iterator pos) noexcept {
  return erase(pos, pos + 1);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::iterator
ReusableVector<T, MonotonicAllocator<U, A>>::erase(
    const_iterator first, const_iterator last) noexcept {
  if (first == last) {
    return iterator(const_cast<pointer>(&*first));
  }

  iterator dest(const_cast<pointer>(&*first));
  iterator src(const_cast<pointer>(&*last));
  while (src != end()) {
    *dest++ = ::std::move(*src++);
  }

  _size -= last - first;
  return iterator(const_cast<pointer>(&*first));
}

template <typename T, typename U, typename A>
template <typename V>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::push_back(
    V&& value) noexcept {
  emplace_back(::std::forward<V>(value));
}

template <typename T, typename U, typename A>
template <typename... Args, typename>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::emplace_back(
    Args&&... args) noexcept {
  if (_size == _capacity) {
    reserve(_capacity == 0 ? 4 : _capacity * 2);
  }
  if (_constructed_size > _size) {
    ValueReusableTraits::reconstruct(_data[_size++], _allocator,
                                     ::std::forward<Args>(args)...);
  } else {
    _allocator.construct(&_data[_size++], ::std::forward<Args>(args)...);
    ++_constructed_size;
  }
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::pop_back() noexcept {
  assert(_size > 0 && "pop empty vector");
  --_size;
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::resize(
    size_type count) noexcept {
  reserve(count);
  if (_size < count) {
    auto reconstruct_end_size = ::std::min(_constructed_size, count);
    for (auto i = _size; i < reconstruct_end_size; ++i) {
      ValueReusableTraits::reconstruct(_data[i], _allocator);
    }
    for (auto i = reconstruct_end_size; i < count; ++i) {
      _allocator.construct(&_data[i]);
      ++_constructed_size;
    }
  }
  _size = count;
}

template <typename T, typename U, typename A>
template <typename V>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::resize(
    size_type count, const V& value) noexcept {
  reserve(count);
  if (_size < count) {
    auto reconstruct_end_size = ::std::min(_constructed_size, count);
    for (auto i = _size; i < reconstruct_end_size; ++i) {
      ValueReusableTraits::reconstruct(_data[i], _allocator, value);
    }
    for (auto i = reconstruct_end_size; i < count; ++i) {
      _allocator.construct(&_data[i], value);
      ++_constructed_size;
    }
  }
  _size = count;
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::swap(
    ReusableVector& other) noexcept {
  assert(_allocator == other._allocator &&
         "can not swap vector with different allocator");
  ::std::swap(_data, other._data);
  ::std::swap(_capacity, other._capacity);
  ::std::swap(_size, other._size);
  ::std::swap(_constructed_size, other._constructed_size);
}

template <typename T, typename U, typename A>
inline ReusableVector<T, MonotonicAllocator<U, A>>::ReusableVector(
    const AllocationMetadata& metadata, allocator_type allocator) noexcept
    : _allocator(allocator),
      _data(_allocator.allocate(metadata.capacity)),
      _size(0),
      _constructed_size(metadata.capacity),
      _capacity(metadata.capacity) {
  for (size_type i = 0; i < _constructed_size; ++i) {
    ValueReusableTraits::construct_with_allocation_metadata(
        &_data[i], _allocator, metadata.value_metadata);
  }
}

template <typename T, typename U, typename A>
inline void
ReusableVector<T, MonotonicAllocator<U, A>>::update_allocation_metadata(
    AllocationMetadata& metadata) const noexcept {
  metadata.capacity = ::std::max(_constructed_size, metadata.capacity);
  for (size_type i = 0; i < _constructed_size; ++i) {
    ValueReusableTraits::update_allocation_metadata(_data[i],
                                                    metadata.value_metadata);
  }
}

template <typename T, typename U, typename A>
inline void ReusableVector<T, MonotonicAllocator<U, A>>::assign(
    size_type count) noexcept {
  clear();
  resize(count);
}

template <typename T, typename U, typename A>
inline typename ReusableVector<T, MonotonicAllocator<U, A>>::size_type
ReusableVector<T, MonotonicAllocator<U, A>>::prepare_for_insert(
    size_type index, size_type count) noexcept {
  reserve(_size + count);
  auto move_end_size = ::std::max(index + count, _constructed_size);
  auto reconstruct_end_size = ::std::min(index + count, _constructed_size);
  for (size_type i = _size + count; i > move_end_size;) {
    --i;
    _allocator.construct(&_data[i], ::std::move(_data[i - count]));
    ++_constructed_size;
  }
  for (size_type i = move_end_size; i > index + count;) {
    --i;
    _data[i] = ::std::move(_data[i - count]);
  }
  _size += count;
  return reconstruct_end_size;
}
// ReusableVector<T, MonotonicAllocator<U, A>> end
////////////////////////////////////////////////////////////////////////////////

template <typename T, typename TT, typename A, typename AA>
inline bool operator==(const ::std::vector<T, A>& left,
                       const ReusableVector<TT, AA>& right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

template <typename T, typename TT, typename A, typename AA>
inline bool operator==(const ReusableVector<T, A>& left,
                       const ::std::vector<TT, AA>& right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

template <typename T, typename TT, typename A, typename AA>
inline bool operator==(const ReusableVector<T, A>& left,
                       const ReusableVector<TT, AA>& right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i] != right[i]) {
      return false;
    }
  }
  return true;
}

template <typename T, typename TT, typename A, typename AA>
inline bool operator!=(const ::std::vector<T, A>& left,
                       const ReusableVector<TT, AA>& right) noexcept {
  return !(left == right);
}

template <typename T, typename TT, typename A, typename AA>
inline bool operator!=(const ReusableVector<T, A>& left,
                       const ::std::vector<TT, AA>& right) noexcept {
  return !(left == right);
}

template <typename T, typename TT, typename A, typename AA>
inline bool operator!=(const ReusableVector<T, A>& left,
                       const ReusableVector<TT, AA>& right) noexcept {
  return !(left == right);
}

BABYLON_NAMESPACE_END
