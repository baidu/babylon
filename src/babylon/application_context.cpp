#include "babylon/application_context.h"

#include "babylon/logging/logger.h" // BABYLON_LOG

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::OffsetDeleter begin
void ApplicationContext::OffsetDeleter::operator()(void* ptr) noexcept {
  if (_deleter) {
    auto address = reinterpret_cast<intptr_t>(ptr) + _offset;
    _deleter(reinterpret_cast<void*>(address));
  }
}
// ApplicationContext::OffsetDeleter end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext begin
ApplicationContext& ApplicationContext::instance() noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static ApplicationContext singleton;
#pragma GCC diagnostic pop
  return singleton;
}

ApplicationContext::~ApplicationContext() noexcept {
  clear();
}

void ApplicationContext::register_component(
    ::std::unique_ptr<ComponentHolder>&& holder) noexcept {
  return register_component(::std::move(holder), "");
}

void ApplicationContext::register_component(
    ::std::unique_ptr<ComponentHolder>&& holder, StringView name) noexcept {
  if (!holder) {
    return;
  }

  auto pholder = holder.get();
  _holders.emplace_back(::std::move(holder));

  pholder->set_name(name);
  pholder->for_each_type([&](const Id* type) {
    {
      auto result = _holder_by_type.emplace(type, pholder);
      // Find one type only accessible path
      if (result.second) {
        pholder->increase_accessible_path();
      } else if (result.first->second) {
        // Remove one type only accessible path
        result.first->second->decrease_accessible_path();
        result.first->second = nullptr;
      }
    }
    if (!name.empty()) {
      ::std::tuple<const Id*, ::std::string> key {type, name};
      auto result = _holder_by_type_and_name.emplace(key, pholder);
      // Find one type name accessible path
      if (result.second) {
        pholder->increase_accessible_path();
      } else if (result.first->second) {
        // Remove one type name only accessible path
        result.first->second->decrease_accessible_path();
        result.first->second = nullptr;
      }
    }
  });
}

ApplicationContext::ComponentIterator ApplicationContext::begin() noexcept {
  return {_holders.begin()};
}

ApplicationContext::ComponentIterator ApplicationContext::end() noexcept {
  return {_holders.end()};
}

void ApplicationContext::clear() noexcept {
  struct SeqGt {
    using T = ::std::unique_ptr<ComponentHolder>;
    inline bool operator()(const T& left, const T& right) const noexcept {
      return left->sequence() > right->sequence();
    }
  };
  ::std::sort(_holders.begin(), _holders.end(), SeqGt());
  // 针对GLIBCXX，clear会按序析构
  // 但是LIBCPP行为并非如此，这里主动reset确保清理顺序
  for (auto& holder : _holders) {
    holder.reset(nullptr);
  }
  _holders.clear();
  _holder_by_type.clear();
  _holder_by_type_and_name.clear();
}
// ApplicationContext end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::ComponentHolder begin
Any ApplicationContext::ComponentHolder::create(
    ApplicationContext& context) noexcept {
  return create(context, _option);
}

Any ApplicationContext::ComponentHolder::create(ApplicationContext& context,
                                                const Any& option) noexcept {
  return create_instance(context, option);
}

void ApplicationContext::ComponentHolder::for_each_type(
    const ::std::function<void(const Id*)>& callback) const noexcept {
  for (auto& pair : _convert_offset) {
    callback(pair.first);
  }
}

void ApplicationContext::ComponentHolder::set_support_singleton(
    bool support) noexcept {
  _singleton_state =
      support ? SingletonState::UNINITIALIZED : SingletonState::DISABLED;
}

int ApplicationContext::ComponentHolder::autowire(
    Any& instance, ApplicationContext& context) noexcept {
  return _autowire_function(instance, context);
}

int ApplicationContext::ComponentHolder::initialize(
    Any& instance, ApplicationContext& context, const Any& option) noexcept {
  return _initialize_function(instance, context, option);
}

Any ApplicationContext::ComponentHolder::create_instance(
    ApplicationContext& context, const Any& option) noexcept {
  auto instance = create_instance();
  if (instance) {
    auto ret = autowire(instance, context);
    if (ret != 0) {
      BABYLON_LOG(WARNING) << "autowire failed for component of type "
                           << _type_id->type_id;
      instance.clear();
    } else {
      ret = initialize(instance, context, option);
      if (ret != 0) {
        BABYLON_LOG(WARNING)
            << "initialize failed for component of type " << _type_id->type_id;
        instance.clear();
      }
    }
  } else {
    BABYLON_LOG(WARNING) << "create instance failed for component of type "
                         << _type_id->type_id;
  }
  return instance;
}

void ApplicationContext::ComponentHolder::create_singleton(
    ApplicationContext& context) noexcept {
  ::std::lock_guard<std::recursive_mutex> lock(*_mutex);
  if (_singleton_state == ComponentHolder::SingletonState::INITIALIZING) {
    BABYLON_LOG(WARNING)
        << "initialize failed for recursive dependent component of type "
        << _type_id->type_id;
    return;
  }
  if (_singleton_state == ComponentHolder::SingletonState::UNINITIALIZED) {
    _singleton_state = ComponentHolder::SingletonState::INITIALIZING;
    _singleton = create(context);
    _sequence = next_sequence();
    atomic_singleton_state().store(ComponentHolder::SingletonState::INITIALIZED,
                                   ::std::memory_order_release);
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
#pragma GCC diagnostic ignored "-Wglobal-constructors"
ApplicationContext::EmptyComponentHolder
    ApplicationContext::EMPTY_COMPONENT_HOLDER;
#pragma GCC diagnostic pop

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::EmptyComponentHolder begin
ApplicationContext::EmptyComponentHolder::EmptyComponentHolder() noexcept
    : ComponentHolder {static_cast<Void*>(nullptr)} {}

Any ApplicationContext::EmptyComponentHolder::create_instance() noexcept {
  return {};
}
// ApplicationContext::EmptyComponentHolder end
////////////////////////////////////////////////////////////////////////////////

size_t ApplicationContext::ComponentHolder::next_sequence() noexcept {
  static ::std::atomic<size_t> counter {1};
  return counter.fetch_add(1, ::std::memory_order_relaxed);
}

ptrdiff_t ApplicationContext::ComponentHolder::convert_offset(
    const Id* type) const noexcept {
  auto iter = _convert_offset.find(type);
  if (iter != _convert_offset.end()) {
    return iter->second;
  }
  return PTRDIFF_MAX;
}

BABYLON_NAMESPACE_END
