#pragma once

#include "babylon/anyflow/closure.h"
#include "babylon/anyflow/data.h"
#include "babylon/anyflow/dependency.h"

// clang-format off
#include "babylon/protect.h"
// clang-format on

BABYLON_NAMESPACE_BEGIN
namespace anyflow {

///////////////////////////////////////////////////////////////////////////////
// Committer begin
template <typename T>
inline Committer<T>::Committer(GraphData& data) noexcept
    : _data(&data),
      _valid(data.acquire()),
      _keep_reference(data.has_preset_value()) {}

template <typename T>
inline Committer<T>::Committer(Committer&& other) noexcept
    : _data(other._data),
      _valid(other._valid),
      _keep_reference(other._keep_reference) {
  other._data = nullptr;
  other._valid = false;
  other._keep_reference = false;
}

template <typename T>
inline Committer<T>& Committer<T>::operator=(Committer&& other) noexcept {
  ::std::swap(_data, other._data);
  ::std::swap(_valid, other._valid);
  ::std::swap(_keep_reference, other._keep_reference);
  other.release();
  return *this;
}

template <typename T>
inline Committer<T>::operator bool() const noexcept {
  return valid();
}

template <typename T>
inline bool Committer<T>::valid() const noexcept {
  return _valid;
}

template <typename T>
inline Committer<T>::~Committer() noexcept {
  release();
}

template <typename T>
inline T* Committer<T>::get() noexcept {
  if (ABSL_PREDICT_TRUE(_valid)) {
    _data->empty(false);
    if (_keep_reference) {
      return _data->mutable_value<T>();
    } else {
      return _data->certain_type_non_reference_mutable_value<T>();
    }
  } else {
    return nullptr;
  }
}

template <typename T>
inline T* Committer<T>::operator->() noexcept {
  return get();
}

template <typename T>
inline T& Committer<T>::operator*() noexcept {
  return *get();
}

template <typename T>
inline void Committer<T>::ref(T& value) noexcept {
  _data->empty(false);
  _data->ref(value);
  _keep_reference = true;
}

template <typename T>
inline void Committer<T>::ref(const T& value) noexcept {
  _data->empty(false);
  _data->ref(value);
  _keep_reference = true;
}

template <typename T>
inline void Committer<T>::cref(const T& value) noexcept {
  _data->empty(false);
  _data->cref(value);
  _keep_reference = true;
}

template <typename T>
inline void Committer<T>::clear() noexcept {
  if (ABSL_PREDICT_TRUE(_valid)) {
    return _data->empty(true);
  }
  _keep_reference = false;
}

template <typename T>
inline void Committer<T>::release() noexcept {
  if (ABSL_PREDICT_TRUE(_valid)) {
    _data->release();
    _valid = false;
  }
}

template <typename T>
inline void Committer<T>::cancel() noexcept {
  if (ABSL_PREDICT_TRUE(_valid)) {
    _valid = false;
    _data = nullptr;
  }
}
// Committer end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// OutputData begin
template <typename T>
inline OutputData<T>::operator bool() const noexcept {
  return _data != nullptr;
}

template <typename T>
template <typename U>
inline OutputData<T>& OutputData<T>::operator=(U&& value) noexcept {
  *_data->emit<T>() = ::std::forward<U>(value);
  return *this;
}

template <typename T>
inline Committer<T> OutputData<T>::emit() noexcept {
  return _data->emit<T>();
}

template <typename T>
inline OutputData<T>::OutputData(GraphData& data) noexcept : _data(&data) {}

template <typename T>
template <typename C>
inline void OutputData<T>::set_on_reset(C&& callback) noexcept {
  _data->set_on_reset<T>(::std::forward<C>(callback));
}
// OutputData end
///////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ChannelPublisher begin
template <typename T>
inline ChannelPublisher<T>::ChannelPublisher(ChannelPublisher&& other) noexcept
    : ChannelPublisher() {
  *this = ::std::move(other);
}

template <typename T>
inline ChannelPublisher<T>& ChannelPublisher<T>::operator=(
    ChannelPublisher&& other) noexcept {
  ::std::swap(_topic, other._topic);
  ::std::swap(_data, other._data);
  return *this;
}

template <typename T>
inline ChannelPublisher<T>::~ChannelPublisher() noexcept {
  close();
}

template <typename T>
template <typename U>
inline void ChannelPublisher<T>::publish(U&& value) noexcept {
  _topic->publish(value);
}

template <typename T>
template <typename C>
inline void ChannelPublisher<T>::publish_n(size_t num, C&& callback) noexcept {
  _topic->publish_n(num, ::std::forward<C>(callback));
}

template <typename T>
inline void ChannelPublisher<T>::close() noexcept {
  if (_topic != nullptr) {
    if (_data->check_last_producer()) {
      _topic->close();
      _topic = nullptr;
    }
  }
}

template <typename T>
inline ChannelPublisher<T>::ChannelPublisher(Topic& topic,
                                             GraphData& data) noexcept
    : _topic(&topic), _data(&data) {}
// ChannelPublisher end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// OutputChannel begin
template <typename T>
inline OutputChannel<T>::operator bool() const noexcept {
  return _data != nullptr;
}

template <typename T>
inline ChannelPublisher<T> OutputChannel<T>::open() noexcept {
  auto committer = _data->emit<Topic>();
  committer.get();
  return ChannelPublisher<T>(*committer, *_data);
}

template <typename T>
inline OutputChannel<T>::OutputChannel(GraphData& data) noexcept
    : _data(&data) {}
/*
_data->check_declare_type<Queue>();
_queue = _data->certain_type_non_reference_mutable_value<Queue>();
_queue->clear();

_data->on_reset(GraphData::OnResetFunction([](GraphData& data){
auto queue = data._data.get<Queue>();
if (nullptr != queue) {
    queue->clear();
}}));
*/
// OutputChannel end
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
// GraphData begin
template <typename T>
inline OutputChannel<T> GraphData::declare_channel() noexcept {
  return OutputChannel<T>(*this);
}

inline bool GraphData::ready() const noexcept {
  return SEALED_CLOSURE == _closure.load(::std::memory_order_acquire);
}

inline bool GraphData::empty() const noexcept {
  return _empty || !_data;
}

inline const ::std::string& GraphData::name() const noexcept {
  return _name;
}

inline const ::babylon::Id& GraphData::declared_type() const noexcept {
  return *_declared_type;
}

inline int32_t GraphData::error_code() const noexcept {
  return _error_code;
}

inline void GraphData::reset() noexcept {
  _acquired.store(false, ::std::memory_order_relaxed);
  _empty = true;
  _has_preset_value = false;
  _active = false;
  _closure.store(nullptr, ::std::memory_order_relaxed);
  _depend_state.store(0, ::std::memory_order_relaxed);
  _producer_done_num.store(0, ::std::memory_order_relaxed);
  _on_reset(_data);
}

inline GraphVertex* GraphData::producer() noexcept {
  return _producer;
}

inline const GraphVertex* GraphData::producer() const noexcept {
  return _producer;
}

inline std::vector<GraphVertex*>* GraphData::producers() noexcept {
  return &_producers;
}

inline const std::vector<GraphVertex*>* GraphData::producers() const noexcept {
  return &_producers;
}

inline void GraphData::producer(GraphVertex& producer) noexcept {
  if (_producers.empty()) {
    _producer = &producer;
  }
  _producers.emplace_back(&producer);
}

template <typename T>
inline OutputData<T> GraphData::output() noexcept {
  return OutputData<T>(*this);
}

template <typename T>
inline OutputChannel<T> GraphData::output_channel() noexcept {
  return OutputChannel<T>(*this);
}

template <typename T>
void GraphData::set_default_on_reset() noexcept {
  _on_reset = default_on_reset<T>;
}

template <typename T,
          typename ::std::enable_if<GraphData::Resetable<T>::value, int>::type>
void GraphData::default_on_reset(Any& data) noexcept {
  auto value = switch_to<T>(data);
  if (value != nullptr) {
    value->reset();
  }
}

template <typename T,
          typename ::std::enable_if<!GraphData::Resetable<T>::value &&
                                        GraphData::Clearable<T>::value,
                                    int>::type>
void GraphData::default_on_reset(Any& data) noexcept {
  auto value = switch_to<T>(data);
  if (value != nullptr) {
    value->clear();
  }
}

template <typename T,
          typename ::std::enable_if<!GraphData::Resetable<T>::value &&
                                        !GraphData::Clearable<T>::value,
                                    int>::type>
void GraphData::default_on_reset(Any& data) noexcept {
  data.clear();
}

template <typename T, typename C>
inline void GraphData::set_on_reset(C&& callback) noexcept {
  struct S {
    void operator()(Any& data) {
      auto value = switch_to<T>(data);
      if (value != nullptr) {
        callback(*value);
      }
    }
    C callback;
  };
  _on_reset = S {.callback {::std::forward<C>(callback)}};
}

template <typename T>
inline T* GraphData::switch_to(Any& data) noexcept {
  if (!data.is_reference()) {
    auto ptr = data.get<T>();
    if (ptr != nullptr) {
      return ptr;
    }
  }
  data.clear();
  return nullptr;
}

inline bool GraphData::bind(ClosureContext& closure) noexcept {
  // todo: 多次bind进行链式挂载
  ClosureContext* expected = nullptr;
  closure.depend_data_add();
  closure.add_waiting_data(this);
  if (ABSL_PREDICT_FALSE(!_closure.compare_exchange_strong(
          expected, &closure, ::std::memory_order_acq_rel))) {
    closure.depend_data_sub();
    return false;
  }
  return true;
}

inline void GraphData::data_num(size_t num) noexcept {
  _data_num = num;
}

inline void GraphData::vertex_num(size_t num) noexcept {
  _vertex_num = num;
}

inline void GraphData::executer(GraphExecutor& executer) noexcept {
  _executer = &executer;
}

inline void GraphData::add_successor(GraphDependency& successor) noexcept {
  _successors.push_back(&successor);
}

inline bool GraphData::acquire() noexcept {
  bool expected = false;
  if (ABSL_PREDICT_TRUE(_acquired.compare_exchange_strong(
          expected, true, ::std::memory_order_acq_rel))) {
    return true;
  }
  return false;
}

template <>
inline const ::babylon::Any* GraphData::cvalue<::babylon::Any>()
    const noexcept {
  if (ABSL_PREDICT_FALSE(_empty)) {
    return nullptr;
  }
  return &_data;
}

template <typename T>
inline const T* GraphData::cvalue() const noexcept {
  if (ABSL_PREDICT_FALSE(_empty)) {
    return nullptr;
  }
  return _data.get<T>();
}

template <typename T>
inline const T* GraphData::value() const noexcept {
  return cvalue<T>();
}

template <typename T>
inline T GraphData::as() const noexcept {
  if (ABSL_PREDICT_FALSE(_empty)) {
    return static_cast<T>(0);
  }
  return _data.as<T>();
}

template <typename T>
inline Committer<T> GraphData::emit() noexcept {
  return Committer<T>(*this);
}

template <>
inline OutputData<::babylon::Any>
GraphData::declare_type<::babylon::Any>() noexcept {
  return OutputData<::babylon::Any>(*this);
}

template <typename T>
inline OutputData<T> GraphData::declare_type() noexcept {
  if (_declared_type == &::babylon::TypeId<Any>::ID) {
    _declared_type = &::babylon::TypeId<T>::ID;
    set_default_on_reset<T>();
    return OutputData<T>(*this);
  } else if (_declared_type == &::babylon::TypeId<T>::ID) {
    return OutputData<T>(*this);
  } else {
    BABYLON_LOG(WARNING) << *this << " declare type["
                         << ::babylon::TypeId<T>().get_type_name()
                         << "] conflict with previous type[" << *_declared_type
                         << "]";
    _error_code = -1;
    return OutputData<T>();
  }
}

inline bool GraphData::forward(GraphDependency& dependency) noexcept {
  if (ABSL_PREDICT_FALSE(!dependency.ready())) {
    return false;
  }
  if (ABSL_PREDICT_FALSE(!acquire())) {
    return false;
  }
  auto& other = dependency.target()->_data;
  if (dependency.target()->need_mutable()) {
    if (dependency.is_mutable() && !other.is_const_reference()) {
      _data.ref(other);
    } else {
      _data = other;
    }
  } else if (dependency.is_mutable()) {
    _data.ref(dependency.target()->_data);
  } else {
    _data.cref(dependency.target()->_data);
  }
  _empty = false;
  release();
  return true;
}

template <typename T>
inline void GraphData::preset(T& value) noexcept {
  _data.ref(value);
  _has_preset_value = true;
}

inline bool GraphData::has_preset_value() const noexcept {
  return _has_preset_value;
}

inline bool GraphData::mark_active() noexcept {
  bool already_active = _active;
  _active = true;
  return already_active;
}

inline bool GraphData::acquire_immutable_depend() noexcept {
  auto state = _depend_state.exchange(1, ::std::memory_order_relaxed);
  return state != 2;
}

inline bool GraphData::acquire_mutable_depend() noexcept {
  auto state = _depend_state.exchange(2, ::std::memory_order_relaxed);
  return state == 0;
}

inline bool GraphData::need_mutable() const noexcept {
  return 2 == _depend_state.load(::std::memory_order_relaxed);
}

template <>
inline ::babylon::Any* GraphData::mutable_value<::babylon::Any>() noexcept {
  if (ABSL_PREDICT_FALSE(_empty)) {
    return nullptr;
  }
  return &_data;
}

template <typename T>
inline T* GraphData::mutable_value() noexcept {
  if (ABSL_PREDICT_FALSE(_empty)) {
    return nullptr;
  }
  return _data.get<T>();
}

template <>
inline ::babylon::Any*
GraphData::certain_type_non_reference_mutable_value<::babylon::Any>() noexcept {
  if (ABSL_PREDICT_FALSE(_data.is_reference())) {
    _data.clear();
  }
  return &_data;
}

template <typename T,
          typename ::std::enable_if<!::std::is_move_constructible<T>::value,
                                    int32_t>::type>
inline T* GraphData::certain_type_non_reference_mutable_value() noexcept {
  if (ABSL_PREDICT_FALSE(_data.is_reference())) {
    _data = ::std::unique_ptr<T>(new T);
    return _data.get<T>();
  }
  auto result = _data.get<T>();
  if (ABSL_PREDICT_FALSE(result == nullptr)) {
    _data = ::std::unique_ptr<T>(new T);
    result = _data.get<T>();
  }
  return result;
}

template <typename T,
          typename ::std::enable_if<::std::is_move_constructible<T>::value,
                                    int32_t>::type>
inline T* GraphData::certain_type_non_reference_mutable_value() noexcept {
  if (ABSL_PREDICT_FALSE(_data.is_reference())) {
    _data = T();
    return _data.get<T>();
  }
  auto result = _data.get<T>();
  if (ABSL_PREDICT_FALSE(result == nullptr)) {
    _data = T();
    result = _data.get<T>();
  }
  return result;
}

template <typename T>
inline void GraphData::ref(T& value) noexcept {
  _data.ref(value);
}

template <typename T>
inline void GraphData::cref(const T& value) noexcept {
  _data.cref(value);
}

inline void GraphData::empty(bool empty) noexcept {
  _empty = empty;
}

inline bool GraphData::check_last_producer() noexcept {
  return (_producers.empty() ||
          _producer_done_num.fetch_add(1) == (_producers.size() - 1));
}

inline void GraphData::trigger(DataStack& activating_data) noexcept {
  if (!mark_active()) {
    if (!ready()) {
      activating_data.emplace_back(this);
    }
  }
}

inline ::std::ostream& operator<<(::std::ostream& os, const GraphData& data) {
  os << "data[" << data._name << "]";
  return os;
}
// GraphData end
/////////////////////////////////////////////////////////////

} // namespace anyflow
BABYLON_NAMESPACE_END

#include "babylon/unprotect.h"
