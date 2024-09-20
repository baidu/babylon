#pragma once

#include "babylon/anyflow/data.h"
#include "babylon/anyflow/dependency.h"
// #include "babylon/anyflow/vertex.h"

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

void GraphDependency::source(GraphVertex& vertex) noexcept {
  _source = &vertex;
}

void GraphDependency::target(GraphData& data) noexcept {
  _target = &data;
}

void GraphDependency::condition(GraphData& data,
                                bool establish_value) noexcept {
  _condition = &data;
  _establish_value = establish_value;
}

void GraphDependency::declare_mutable() noexcept {
  _mutable = true;
}

template <typename T>
inline void GraphDependency::declare_type() noexcept {
  _target->declare_type<T>();
}

template <typename T>
inline void GraphDependency::check_declare_type() noexcept {
  _target->check_declare_type<T>();
}

bool GraphDependency::is_mutable() const noexcept {
  return _mutable;
}

void GraphDependency::declare_essential(bool is_essential) noexcept {
  _essential = is_essential;
}

bool GraphDependency::is_essential() const noexcept {
  return _essential;
}

template <typename T>
inline void GraphDependency::declare_channel() noexcept {
  declare_type<typename InputChannel<T>::Topic>();
}

void GraphDependency::reset() noexcept {
  _waiting_num.store(0, ::std::memory_order_relaxed);
  _established = false;
  _ready = false;
}

bool GraphDependency::ready() const noexcept {
  return _ready;
}

bool GraphDependency::established() const noexcept {
  return _established;
}

bool GraphDependency::empty() const noexcept {
  return _target->empty();
}

template <typename T>
const T* GraphDependency::value() const noexcept {
  if (ABSL_PREDICT_FALSE(!_ready || _target->empty())) {
    return nullptr;
  }
  return _target->cvalue<T>();
}

template <typename T>
T GraphDependency::as() const noexcept {
  if (ABSL_PREDICT_FALSE(!_ready || _target->empty())) {
    return static_cast<T>(0);
  }
  return _target->as<T>();
}

template <typename T>
T* GraphDependency::mutable_value() noexcept {
  if (ABSL_PREDICT_FALSE(!_ready || !_mutable)) {
    return nullptr;
  }
  return _target->mutable_value<T>();
}

GraphData* GraphDependency::target() noexcept {
  return _target;
}

const GraphData* GraphDependency::inner_target() const noexcept {
  return _target;
}

const GraphData* GraphDependency::inner_condition() const noexcept {
  return _condition;
}

bool GraphDependency::check_established() noexcept {
  if (_condition == nullptr) {
    _established = true;
  } else {
    bool value = _condition->as<bool>();
    if (value == _establish_value) {
      _established = true;
    }
  }
  return _established;
}

template <typename T>
inline InputChannel<T> GraphDependency::channel() noexcept {
  return InputChannel<T> {*this};
}

template <typename T>
inline MutableInputChannel<T> GraphDependency::mutable_channel() noexcept {
  return MutableInputChannel<T> {*this};
}

} // namespace anyflow
BABYLON_NAMESPACE_END
