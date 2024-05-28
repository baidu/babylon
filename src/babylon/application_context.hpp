#ifndef BAIDU_FEED_MLARCH_BABYLON_APPLICATION_CONTEXT_HPP
#define BAIDU_FEED_MLARCH_BABYLON_APPLICATION_CONTEXT_HPP

#include <baidu/feed/mlarch/babylon/application_context.h>

namespace baidu {
namespace feed {
namespace mlarch {
namespace babylon {

inline ApplicationContext::ComponentHolder::ComponentHolder(const Id& type_id) noexcept :
    _type_id(&type_id) {}

template <typename T>
inline void ApplicationContext::destroy_by_holder(ComponentHolder* holder, T* component) {
    holder->destroy(component);
}

template <typename V, typename ::std::enable_if<
    ApplicationContext::ComponentHolder::Initializeable<V>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::initialize_if_possible(V* component) {
    return component->initialize();
}

template <typename V, typename ::std::enable_if<
    !ApplicationContext::ComponentHolder::Initializeable<V>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::initialize_if_possible(V*) {
    return 0;
}
        
template <typename V, typename ::std::enable_if<
    ApplicationContext::ComponentHolder::Wireable<V, ApplicationContext&>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::wireup_if_possible(V* component, ApplicationContext& context) {
    return component->wireup(context);
}

template <typename V, typename ::std::enable_if<
    !ApplicationContext::ComponentHolder::Wireable<V, ApplicationContext&>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::wireup_if_possible(V*, ApplicationContext&) {
    return 0;
}

template <typename V, typename ::std::enable_if<
    ApplicationContext::ComponentHolder::AutoWireable<V, ApplicationContext&>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::auto_wireup_if_possible(V* component, ApplicationContext& context) {
    return component->__babylon_auto_wireup(context);
}

template <typename V, typename ::std::enable_if<
    !ApplicationContext::ComponentHolder::AutoWireable<V, ApplicationContext&>::value, int32_t>::type>
inline int32_t ApplicationContext::ComponentHolder::auto_wireup_if_possible(V*, ApplicationContext&) {
    return 0;
}

inline int32_t ApplicationContext::ComponentHolder::ensure_initialized() {
    if (_status == ComponentHolder::StatusType::S_INITIALIZED) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(*_mutex);
    if (_status == ComponentHolder::StatusType::S_INITIALIZED) {
        return 0;
    }
    if (ABSL_PREDICT_FALSE(_status != ComponentHolder::StatusType::S_UNINITIALIZED)) {
        LOG(WARNING) << "initialize failed for recursive dependent component[" << _name << "] of type " << *_type_id;
        return -3;
    }
    _status = ComponentHolder::StatusType::S_INITIALIZING; 
    LOG(NOTICE) << "initializing component[" << _name << "] of type " << *_type_id;
    if (0 != initialize()) {
        LOG(WARNING) << "initialize failed component[" << _name << "] of type " << *_type_id;
        return -1;
    }
    auto component_and_need_destroy = get_or_create();
    if (nullptr == ::std::get<0>(component_and_need_destroy)) {
        LOG(WARNING) << "first get failed component[" << _name << "] of type " << *_type_id;
        return -1;
    }
    if (::std::get<1>(component_and_need_destroy)) {
        destroy(::std::get<0>(component_and_need_destroy));
    }
    LOG(NOTICE) << "initialize successful component[" << _name << "] of type " << *_type_id;
    _status = ComponentHolder::StatusType::S_INITIALIZED;
    _seq = _s_next_seq.fetch_add(1);
    return 0;
}

inline void ApplicationContext::ComponentHolder::set_name(StringView name) {
    _name.assign(name.data(), name.size());
}

inline void ApplicationContext::ComponentHolder::set_context(ApplicationContext& context) noexcept {
    _context = &context;
}

inline const Id& ApplicationContext::ComponentHolder::type() const noexcept {
    return *_type_id;
}

inline const ::std::string& ApplicationContext::ComponentHolder::name() const noexcept {
    return _name;
}

inline size_t ApplicationContext::ComponentHolder::seq() const noexcept {
    return _seq;
}

inline ApplicationContext& ApplicationContext::ComponentHolder::context() noexcept {
    return *_context;
}

template <typename T>
inline ApplicationContext::ScopedComponent<T>
ApplicationContext::ComponentHolder::get_or_create() {
    auto result = get_or_create();
    return ScopedComponent<T>(
        reinterpret_cast<T*>(::std::get<0>(result)),
        ::std::get<1>(result) ? this : nullptr);
}

inline ApplicationContext& ApplicationContext::instance() {
    static ApplicationContext _instance;
    return _instance;
}

inline ApplicationContext::ApplicationContext(ApplicationContext&& other) noexcept :
    _holders(::std::move(other._holders)),
    _holder_by_type(::std::move(other._holder_by_type)),
    _holder_by_name(::std::move(other._holder_by_name)) {}

inline int32_t ApplicationContext::initialize(bool is_lazy) {
    for (auto& holder : _holders) {
        if (0 != map_component(holder.get())) {
            return -2;
        }
    }
    for (auto it = _holder_by_type.begin(); it != _holder_by_type.end();) {
        auto it_copy = it++;
        if (it_copy->second == nullptr) {
            _holder_by_type.erase(it_copy);
        }
    }
	// lazy mode: no init or wireup happend
    if (!is_lazy) {
        for (auto& holder : _holders) {
        	if (0 != holder->ensure_initialized()) {
            	return -1;
        	}
		}
    }
    return 0;
}

template <typename T>
inline ApplicationContext::ComponentAccessor<T> ApplicationContext::get_component_accessor() {
    auto it = _holder_by_type.find(&TypeId<T>().ID);
    if (it != _holder_by_type.end() && 0 == it->second->ensure_initialized()) {
        return ComponentAccessor<T>(*it->second);
    }
    return ComponentAccessor<T>();
}

template <typename T>
inline ApplicationContext::ComponentAccessor<T> ApplicationContext::get_component_accessor(StringView name) {
    thread_local ::std::string real_name;
    build_real_name<T>(real_name, name);
    auto it = _holder_by_name.find(real_name);
    if (it != _holder_by_name.end()
            && 0 == it->second->ensure_initialized()) {
        return ComponentAccessor<T>(*it->second);
    }
    return ComponentAccessor<T>();
}

template <typename T>
inline ApplicationContext::ScopedComponent<T> ApplicationContext::get_or_create() {
    return get_component_accessor<T>().get_or_create();
}

template <typename T>
inline ApplicationContext::ScopedComponent<T> ApplicationContext::get_or_create(StringView name) {
    return get_component_accessor<T>(name).get_or_create();
}

template <typename T>
inline T* ApplicationContext::get() {
    auto result = get_or_create<T>();
    return !result.get_deleter().own() ? result.get() : nullptr;
}

template <typename T>
inline T* ApplicationContext::get(StringView name) {
    auto result = get_or_create<T>(name);
    return !result.get_deleter().own() ? result.get() : nullptr;
}

inline int32_t ApplicationContext::map_component(ComponentHolder* holder) noexcept {
    // 创建type映射
    {
        auto result = _holder_by_type.emplace(
            ::std::make_pair(&holder->type(), holder));
        // type冲突，设置无法只按type获取
        if (result.second == false) {
            result.first->second = nullptr;
        }
    }
    // 创建type + name映射
    {
        thread_local ::std::string real_name;
        build_real_name(real_name, holder->name(), holder->type());
        auto result = _holder_by_name.emplace(
            ::std::make_pair(real_name, holder));
        if (result.second == false) {
            LOG(WARNING) << "both name and type conflict component[" << holder->name() << "] of type "
                << holder->type().name;
            return -2;
        }
    }
    return 0;
}

inline void ApplicationContext::build_real_name(::std::string& real_name,
        StringView name, const Id& type) noexcept {
    real_name.clear();
    ::absl::Format(&real_name, "%lld", reinterpret_cast<uint64_t>(&type));
    real_name.append(1, '\t');
    real_name.append(name.data(), name.size());
}

template <typename T>
inline void ApplicationContext::build_real_name(::std::string& real_name,
        StringView name) noexcept {
    build_real_name(real_name, name, TypeId<T>().ID);
}

} // babylon
} // mlarch
} // feed
} // baidu

#endif // BAIDU_FEED_MLARCH_BABYLON_APPLICATION_CONTEXT_HPP
