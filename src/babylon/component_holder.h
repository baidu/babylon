#ifndef BAIDU_FEED_MLARCH_BABYLON_COMPONENT_HOLDER_H
#define BAIDU_FEED_MLARCH_BABYLON_COMPONENT_HOLDER_H

#include <baidu/feed/mlarch/babylon/any.h>
#include <baidu/feed/mlarch/babylon/application_context.h>
#include <baidu/feed/mlarch/babylon/expect.h>

#include <base/logging.h>

#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/control/expr_if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <gflags/gflags.h>

#include <functional>
#include <inttypes.h>
#include <tuple>

namespace baidu {
namespace feed {
namespace mlarch {
namespace babylon {

// 应用于具体类型T的组件容器
template <typename T>
class TypedComponentHolder : public ApplicationContext::ComponentHolder {
public:
    inline TypedComponentHolder() noexcept;
};

// 单例组件容器，持有T类型组件，呈现为U类型
// 持有的单例组件在initialize时会完成组装
template <typename T, typename U = T>
class SingletonComponentHolder : public TypedComponentHolder<U> {
public:
    virtual int32_t initialize() override;
    virtual ::std::tuple<void*, bool> get_or_create() override;

protected:
    virtual void on_initialize();

    ::std::unique_ptr<T, ConditionalDeleter<T>> _instance {nullptr, false};
};

// 默认单例组件容器，使用默认构造函数创建组件
// 探测initialize函数进行初始化
// 支持最基础的组件模式
template <typename T, typename U = T>
class DefaultComponentHolder : public SingletonComponentHolder<T, U> {
protected:
    virtual void on_initialize() override;
};

// 自动使用默认单例组件容器模板生成一个ComponentHolder，并注册到context
// 可以为组件指定一个name，以及注册到哪个context
// 默认没有名字，注册到全局单例的context
template <typename T, typename U = T>
class DefaultComponentRegister {
public:
    inline DefaultComponentRegister(::std::string_view name = "",
        ApplicationContext& context = ApplicationContext::instance()) noexcept;
};

// 使用外部构造并初始化完成的实例的组件容器
// 实例通过移动或拷贝方式置入容器，并通过单例呈现
// 用于支持不能默认构造，和无法改造适配initialize机制的组件
template <typename T, typename U = T>
class InitializedComponentHolder : public SingletonComponentHolder<T, U> {
public:
    inline InitializedComponentHolder(::std::unique_ptr<T>&& component) noexcept;
    inline InitializedComponentHolder(T&& component);
    inline InitializedComponentHolder(const T& component);
};

// 使用外部构造并初始化完成的实例的组件容器
// 实例直接引用置入容器，本体生命周期由外部维护
// 用于支持粘合其他单例组件机制
template <typename T, typename U = T>
class ExternalComponentHolder : public SingletonComponentHolder<T, U> {
public:
    inline ExternalComponentHolder(T& component) noexcept;
};

// 自定义组件容器，使用自定义函数创建并初始化组件
// 用于支持不能显式构造的单例组件
template <typename T, typename U = T>
class CustomComponentHolder : public SingletonComponentHolder<T, U> {
public:
    inline CustomComponentHolder(::std::function<T*()>&& creator) noexcept;
    virtual void on_initialize() override;

private:
    ::std::function<T*()> _creator;
};

// 自动使用自定义组件容器模板生成一个ComponentHolder，并注册到context
// 可以为组件指定一个name，以及注册到那个context
// 默认没有名字，注册到全局单例的context
template <typename T, typename U = T>
class CustomComponentRegister {
public:
    inline CustomComponentRegister(::std::function<T*()>&& creator, ::std::string_view name = "",
        ApplicationContext& context = ApplicationContext::instance()) noexcept;
};

// 工厂组件容器，构造T类型组件，呈现为U类型
// 组件在每次get_or_create时动态构造，并完成组装
template <typename T, typename U = T>
class FactoryComponentHolder : public TypedComponentHolder<U> {
public:
    virtual ::std::tuple<void*, bool> get_or_create() override;
    virtual void destroy(void* component) noexcept override;

protected:
    virtual T* create() = 0;
};

// 默认工厂组件容器，使用默认构造函数创建组件
// 探测initialize函数进行初始化
// 支持最基础的工厂组件模式
template <typename T, typename U = T>
class DefaultFactoryComponentHolder : public FactoryComponentHolder<T, U> {
protected:
    virtual T* create() override;
};

// 自动使用默认单例组件容器模板生成一个ComponentHolder，并注册到context
// 可以为组件指定一个name，以及注册到哪个context
// 默认没有名字，注册到全局单例的context
template <typename T, typename U = T>
class DefaultFactoryComponentRegister {
public:
    inline DefaultFactoryComponentRegister(::std::string_view name = "",
        ApplicationContext& context = ApplicationContext::instance()) noexcept;
};

// 自定义工厂组件容器，使用自定义函数创建并初始化组件
// 用于支持不能显式构造的工厂组件
template <typename T, typename U = T>
class CustomFactoryComponentHolder : public FactoryComponentHolder<T, U> {
public:
    inline CustomFactoryComponentHolder(::std::function<T*()>&& creator) noexcept;

protected:
    virtual T* create() override;

private:
    ::std::function<T*()> _creator;
};

// 自动使用自定义组件容器模板生成一个ComponentHolder，并注册到context
// 可以为组件指定一个name，以及注册到那个context
// 默认没有名字，注册到全局单例的context
template <typename T, typename U = T>
class CustomFactoryComponentRegister {
public:
    inline CustomFactoryComponentRegister(::std::function<T*()>&& creator,
        ::std::string_view name = "",
        ApplicationContext& context = ApplicationContext::instance()) noexcept;
};

///////////////////////////////////////////////////////////////////////////////
// TypedComponentHolder begin
template <typename T>
inline TypedComponentHolder<T>::TypedComponentHolder() noexcept :
    ComponentHolder(TypeId<T>().ID) {}
// TypedComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// SingletonComponentHolder begin
template <typename T, typename U>
::std::tuple<void*, bool> SingletonComponentHolder<T, U>::get_or_create() {
    return ::std::tuple<void*, bool>(static_cast<U*>(this->_instance.get()), false);
}

template <typename T, typename U>
int32_t SingletonComponentHolder<T, U>::initialize() {
    on_initialize();
    if (!this->_instance) {
        return -1;
    }
    if (0 != this->auto_wireup_if_possible(this->_instance.get(), this->context())) {
        return -1;
    }
    if (0 != this->wireup_if_possible(this->_instance.get(), this->context())) {
        return -1;
    }
    return 0;
}

template <typename T, typename U>
void SingletonComponentHolder<T, U>::on_initialize() {}
// SingletonComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultComponentHolder begin
template <typename T, typename U>
void DefaultComponentHolder<T, U>::on_initialize() {
    ::std::unique_ptr<T, ConditionalDeleter<T>> instance(new T, true);
    if (0 == this->initialize_if_possible(instance.get())) {
        this->_instance = ::std::move(instance);
    }
}
// DefaultComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultComponentRegister begin
template <typename T, typename U>
inline DefaultComponentRegister<T, U>::DefaultComponentRegister(::std::string_view name,
        ApplicationContext& context) noexcept {
    context.register_component(DefaultComponentHolder<T, U>(), name);
}
// DefaultComponentRegister end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// InitializedComponentHolder begin
template <typename T, typename U>
inline InitializedComponentHolder<T, U>::InitializedComponentHolder(
        ::std::unique_ptr<T>&& component) noexcept {
    this->_instance = ::std::unique_ptr<T, ConditionalDeleter<T>>(component.release(), true);
}

template <typename T, typename U>
inline InitializedComponentHolder<T, U>::InitializedComponentHolder(T&& component) :
    InitializedComponentHolder<T, U>(::std::unique_ptr<T>(new T(::std::move(component)))) {}

template <typename T, typename U>
inline InitializedComponentHolder<T, U>::InitializedComponentHolder(const T& component) :
    InitializedComponentHolder<T, U>(::std::unique_ptr<T>(new T(component))) {}
// InitializedComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// InitializedComponentHolder begin
template <typename T, typename U>
inline ExternalComponentHolder<T, U>::ExternalComponentHolder(T& component) noexcept {
    this->_instance = ::std::unique_ptr<T, ConditionalDeleter<T>>(&component, false);
}
// InitializedComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CustomComponentHolder begin
template <typename T, typename U>
inline CustomComponentHolder<T, U>::CustomComponentHolder(
        ::std::function<T*()>&& creator) noexcept {
    _creator = ::std::move(creator);
}

template <typename T, typename U>
void CustomComponentHolder<T, U>::on_initialize() {
    auto* component = _creator();
    if (component == nullptr) {
        LOG(WARNING) << "initialize custom component of type " << this->type().name << " failed";
        return;
    }
    this->_instance = ::std::unique_ptr<T, ConditionalDeleter<T>>(component, true);
}
// CustomComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CustomComponentRegister begin
template <typename T, typename U>
inline CustomComponentRegister<T, U>::CustomComponentRegister(::std::function<T*()>&& creator,
        ::std::string_view name, ApplicationContext& context) noexcept {
    context.register_component(CustomComponentHolder<T, U>(::std::move(creator)), name);
}
// CustomComponentRegister end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// FactoryComponentHolder begin
template <typename T, typename U>
::std::tuple<void*, bool> FactoryComponentHolder<T, U>::get_or_create() {
    auto* instance = create();
    if (instance == nullptr) {
        return ::std::tuple<void*, bool>(nullptr, false);
    }
    if (0 != this->auto_wireup_if_possible(instance, this->context())) {
        return ::std::tuple<void*, bool>(nullptr, false);
    }
    if (0 != this->wireup_if_possible(instance, this->context())) {
        return ::std::tuple<void*, bool>(nullptr, false);
    }
    return ::std::tuple<void*, bool>(static_cast<U*>(instance), true);
}

template <typename T, typename U>
void FactoryComponentHolder<T, U>::destroy(void* component) noexcept {
    delete reinterpret_cast<U*>(component);
}
// FactoryComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultFactoryComponentHolder begin
template <typename T, typename U>
T* DefaultFactoryComponentHolder<T, U>::create() {
    ::std::unique_ptr<T> instance(new T);
    if (0 != this->initialize_if_possible(instance.get())) {
        return nullptr;
    }
    return instance.release();
}
// DefaultFactoryComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultFactoryComponentRegister begin
template <typename T, typename U>
inline DefaultFactoryComponentRegister<T, U>::DefaultFactoryComponentRegister(
        ::std::string_view name, ApplicationContext& context) noexcept {
    context.register_component(DefaultFactoryComponentHolder<T, U>(), name);
}
// DefaultFactoryComponentRegister end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultFactoryComponentHolder begin
template <typename T, typename U>
inline CustomFactoryComponentHolder<T, U>::CustomFactoryComponentHolder(
    ::std::function<T*()>&& creator) noexcept : _creator(::std::move(creator)) {}

template <typename T, typename U>
T* CustomFactoryComponentHolder<T, U>::create() {
    return _creator();
}
// DefaultFactoryComponentHolder end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DefaultFactoryComponentRegister begin
template <typename T, typename U>
inline CustomFactoryComponentRegister<T, U>::CustomFactoryComponentRegister(
        ::std::function<T*()>&& creator,
        ::std::string_view name, ApplicationContext& context) noexcept {
    context.register_component(CustomFactoryComponentHolder<T, U>(::std::move(creator)), name);
}
// DefaultFactoryComponentRegister end
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// 用匿名方式注册T类型的组件
// T为一个标准名，如果不满足，如包含模板参数等，需前置一个typedef
#define BABYLON_REGISTER_COMPONENT(T) \
    static ::baidu::feed::mlarch::babylon::DefaultComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__);

// 用名字name注册T类型的组件
#define BABYLON_REGISTER_COMPONENT_WITH_NAME(T, name) \
    static ::baidu::feed::mlarch::babylon::DefaultComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(#name);

// 用名字name注册实际T类型，暴露U类型的组件
#define BABYLON_REGISTER_COMPONENT_WITH_TYPE_NAME(T, U, name) \
    static ::baidu::feed::mlarch::babylon::DefaultComponentRegister<T, U> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(#name);

// 用匿名方式注册T类型的组件
// T为一个标准名，如果不满足，如包含模板参数等，需前置一个typedef
#define BABYLON_REGISTER_CUSTOM_COMPONENT(T, ...) \
    static ::baidu::feed::mlarch::babylon::CustomComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__);

// 用名字name注册T类型的组件
#define BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_NAME(T, name, ...) \
    static ::baidu::feed::mlarch::babylon::CustomComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__, #name);

// 用名字name注册T类型，暴露U类型的组件
#define BABYLON_REGISTER_CUSTOM_COMPONENT_WITH_TYPE_NAME(T, U, name, ...) \
    static ::baidu::feed::mlarch::babylon::CustomComponentRegister<T, U> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__, #name);

// 用匿名方式注册T类型的工厂组件
// T为一个标准名，如果不满足，如包含模板参数等，需前置一个typedef
#define BABYLON_REGISTER_FACTORY_COMPONENT(T) \
    static ::baidu::feed::mlarch::babylon::DefaultFactoryComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__);

// 用名字name注册T类型的工厂组件
#define BABYLON_REGISTER_FACTORY_COMPONENT_WITH_NAME(T, name) \
    static ::baidu::feed::mlarch::babylon::DefaultFactoryComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(#name);

// 用名字name注册实际T类型，暴露U类型的工厂组件
#define BABYLON_REGISTER_FACTORY_COMPONENT_WITH_TYPE_NAME(T, U, name) \
    static ::baidu::feed::mlarch::babylon::DefaultFactoryComponentRegister<T, U> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(#name);

// 用匿名方式注册T类型的工厂组件
// T为一个标准名，如果不满足，如包含模板参数等，需前置一个typedef
#define BABYLON_REGISTER_CUSTOM_FACTORY_COMPONENT(T, ...) \
    static ::baidu::feed::mlarch::babylon::CustomFactoryComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__);

// 用名字name注册T类型的组件
#define BABYLON_REGISTER_CUSTOM_FACTORY_COMPONENT_WITH_NAME(T, name, ...) \
    static ::baidu::feed::mlarch::babylon::CustomFactoryComponentRegister<T> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__, #name);

// 用名字name注册T类型，暴露U类型的组件
#define BABYLON_REGISTER_CUSTOM_FACTORY_COMPONENT_WITH_TYPE_NAME(T, U, name, ...) \
    static ::baidu::feed::mlarch::babylon::CustomFactoryComponentRegister<T, U> \
        BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__)(__VA_ARGS__, #name);

}  // babylon
}  // mlarch
}  // feed
}  // baidu

#endif //BAIDU_FEED_MLARCH_BABYLON_COMPONENT_HOLDER_H
