#include "babylon/application_context.h"

#include "babylon/logging/logger.h" // BABYLON_LOG

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::OffsetDeleter begin
void ApplicationContext::OffsetDeleter::operator()(void* ptr) noexcept {
  if (deleter) {
    auto address = reinterpret_cast<intptr_t>(ptr) + offset;
    deleter(reinterpret_cast<void*>(address));
  }
}
// ApplicationContext::OffsetDeleter end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext begin
ApplicationContext& ApplicationContext::instance() noexcept {
  static ApplicationContext singleton;
  return singleton;
}

ApplicationContext::~ApplicationContext() noexcept {
  clear();
}

int ApplicationContext::register_component(
    ::std::unique_ptr<ComponentHolder>&& holder) noexcept {
  return register_component(::std::move(holder), "");
}

int ApplicationContext::register_component(
    ::std::unique_ptr<ComponentHolder>&& holder, StringView name) noexcept {
  if (!holder) {
    BABYLON_LOG(WARNING) << "register get null ComponentHolder";
    return -1;
  }

  auto pholder = holder.get();
  _holders.emplace_back(::std::move(holder));

  pholder->for_each_type([&](const Id* type) {
    {
      auto result = _holder_by_type.emplace(type, pholder);
      // type冲突，设置无法只按type获取
      if (result.second == false) {
        BABYLON_LOG(DEBUG) << "register different component of same type "
                           << *type << " will disable wireup by type";
        result.first->second = nullptr;
      }
    }
    if (!name.empty()) {
      ::std::tuple<const Id*, ::std::string> key {type, name};
      auto result = _holder_by_type_and_name.emplace(key, pholder);
      // type and name冲突
      if (result.second == false) {
        BABYLON_LOG(WARNING)
            << "register different component of same type " << *type
            << " with same name " << name << " will disable wireup";
        result.first->second = nullptr;
      }
    }
  });

  return 0;
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
  callback(&_type_id->type_id);
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

ApplicationContext::EmptyComponentHolder
    ApplicationContext::EMPTY_COMPONENT_HOLDER;

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::EmptyComponentHolder begin
ApplicationContext::EmptyComponentHolder::EmptyComponentHolder() noexcept
    : ComponentHolder {static_cast<void*>(nullptr)} {}

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
