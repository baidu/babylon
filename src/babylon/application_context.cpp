#include "babylon/application_context.h"

BABYLON_NAMESPACE_BEGIN

int ApplicationContext::ComponentHolder::autowire(Any& instance, ApplicationContext& context) noexcept {
  return _autowire_function(instance, context);
}

int ApplicationContext::ComponentHolder::initialize(Any& instance, ApplicationContext& context, const Any& option) noexcept {
  return _initialize_function(instance, context, option);
}

Any ApplicationContext::ComponentHolder::create_instance(ApplicationContext& context, const Any& option) noexcept {
  auto instance = create_instance();
  if (instance) {
    auto ret = autowire(instance, context);
    if (ret != 0) {
      BABYLON_LOG(WARNING) << "autowire failed for component of type " << *_type_id;
      instance.clear();
    } else {
      ret = initialize(instance, context, option);
      if (ret != 0) {
        BABYLON_LOG(WARNING) << "initialize failed for component of type " << *_type_id;
        instance.clear();
      }
    }
  } else {
    BABYLON_LOG(WARNING) << "create instance failed for component of type " << *_type_id;
  }
  return instance;
}

/*
int32_t ApplicationContext::ComponentHolder::initialize() {
    return 0;
}

::std::tuple<void*, bool> ApplicationContext::ComponentHolder::get_or_create() {
    return ::std::tuple<void*, bool>(nullptr, false);
}

void ApplicationContext::ComponentHolder::destroy(void*) noexcept {
}

::std::atomic<size_t> ApplicationContext::ComponentHolder::_s_next_seq;

void ApplicationContext::clear() noexcept {
    struct SeqGt {
        using T = ::std::unique_ptr<ComponentHolder>;
        inline bool operator()(const T& left, const T& right) const noexcept {
            return left->seq() > right->seq();
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
    _holder_by_name.clear();
}

ApplicationContext::ComponentHolder ApplicationContext::EMPTY_COMPONENT_HOLDER;
*/

size_t ApplicationContext::ComponentHolder::next_sequence() noexcept {
  static ::std::atomic<size_t> counter {1};
  return counter.fetch_add(1, ::std::memory_order_relaxed);
}

BABYLON_NAMESPACE_END
