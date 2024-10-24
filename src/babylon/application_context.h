#pragma once

#include "babylon/any.h"            // babylon::Any
#include "babylon/logging/logger.h" // BABYLON_LOG

// clang-format off
#include BABYLON_EXTERNAL(absl/container/flat_hash_map.h) // absl::flat_hash_map
// clang-format on

#include "boost/preprocessor/comparison/less.hpp"   // BOOST_PP_LESS
#include "boost/preprocessor/control/if.hpp"        // BOOST_PP_IF
#include "boost/preprocessor/facilities/expand.hpp" // BOOST_PP_EXPAND
#include "boost/preprocessor/seq/for_each.hpp"      // BOOST_PP_SEQ_FOR_EACH
#include "boost/preprocessor/tuple/push_back.hpp"   // BOOST_PP_TUPLE_PUSH_BACK
#include "boost/preprocessor/tuple/size.hpp"        // BOOST_PP_TUPLE_SIZE

#include <atomic> // std::atomic
#include <memory> // std::unique_ptr
#include <mutex>  // std::recursive_mutex

BABYLON_NAMESPACE_BEGIN

// IOC模式的组件容器框架，主要实现
// - 标准化的单例和工厂设计模式
// - 声明式和可编程的依赖注入框架
// - 基于组件引用关系的声明周期托管，实现可靠优雅退出
// 基于babylon::Any以及静态反射机制实现，不限定组件基类，支持任意类型
class ApplicationContext {
 public:
  // 定制在组件场景使用的销毁器，主要支持
  // - 非虚继承模式下暴露父类型指针，但需要转换成子类型销毁
  // - 多继承模式下父子类型转换的偏移量管理
  // - 作为统一支持工厂和单例模式的接口返回值使用，按需销毁
  class OffsetDeleter;
  template <typename T>
  using ScopedComponent = ::std::unique_ptr<T, OffsetDeleter>;

  // 组件的提供侧接口，通过继承可以定制组件的初始化和组装行为
  class ComponentHolder;
  // 默认的组件机制DefaultComponentHolder
  // - 使用默认构造函数创建组件
  // - 探测BABYLON_AUTOWIRE协议进行自动组装
  // - 探测initialize协议函数进行初始化和复杂组装
  // - 支持按照类型T使用，也支持按照一系列基类BS使用
  template <typename T, typename... BS>
  class DefaultComponentHolder;
  // 基本等同于DefaultComponentHolder，但禁用了单例功能
  template <typename T, typename... BS>
  class FactoryComponentHolder;
  // 内部使用，永远返回失败
  class EmptyComponentHolder;

  template <typename T>
  class ComponentRegister;

  // 组件的使用侧接口，提供工厂和单例两种使用模式
  template <typename T>
  class ComponentAccessor;

  // 支持遍历组件的迭代器
  class ComponentIterator;

  // 可以默认构造，尽管大多情况下使用单例即可
  // 由于存在较多记录指针的场景，禁用移动和拷贝避免误用
  ApplicationContext() = default;
  ApplicationContext(ApplicationContext&&) = delete;
  ApplicationContext(const ApplicationContext&) = delete;
  ApplicationContext& operator=(ApplicationContext&&) = delete;
  ApplicationContext& operator=(const ApplicationContext&) = delete;
  ~ApplicationContext() noexcept;

  // 单例
  static ApplicationContext& instance() noexcept;

  // 注册一个组件，可以额外附加一个名称用于区分同类型的组件
  void register_component(::std::unique_ptr<ComponentHolder>&& holder) noexcept;
  void register_component(::std::unique_ptr<ComponentHolder>&& holder,
                          StringView name) noexcept;

  // 取得一个明确类型的组件访问器
  // 在需要反复使用工厂构造组件的情况下可以节省冗余寻址成本
  template <typename T>
  ComponentAccessor<T> component_accessor() noexcept;
  template <typename T>
  ComponentAccessor<T> component_accessor(StringView name) noexcept;

  // 简化表达component_accessor<T>().get();
  template <typename T>
  T* get() noexcept;
  // 简化表达component_accessor<T>(name).get();
  template <typename T>
  T* get(StringView name) noexcept;

  // 简化表达component_accessor<T>().get_or_create();
  template <typename T>
  ScopedComponent<T> get_or_create() noexcept;
  // 简化表达component_accessor<T>(name).get_or_create();
  template <typename T>
  ScopedComponent<T> get_or_create(StringView name) noexcept;

  // 支持range-based loop
  ComponentIterator begin() noexcept;
  ComponentIterator end() noexcept;

  // 清空并销毁所有ComponentHolder，在ApplicationContext自身析构时会默认调用
  // 主动使用场景主要用于明确销毁时机，支持优雅退出
  void clear() noexcept;

 private:
  static EmptyComponentHolder EMPTY_COMPONENT_HOLDER;

  ::std::vector<::std::unique_ptr<ComponentHolder>> _holders;
  ::absl::flat_hash_map<const Id*, ComponentHolder*> _holder_by_type;
  ::absl::flat_hash_map<::std::tuple<const Id*, ::std::string>,
                        ComponentHolder*>
      _holder_by_type_and_name;
};

class ApplicationContext::OffsetDeleter {
 public:
  // 默认构造的实例不会执行销毁动作
  // 用于支持单例模式使用统一返回类型
  inline OffsetDeleter() noexcept = default;
  inline OffsetDeleter(OffsetDeleter&&) noexcept = default;
  inline OffsetDeleter(const OffsetDeleter&) noexcept = default;
  inline OffsetDeleter& operator=(OffsetDeleter&&) noexcept = default;
  inline OffsetDeleter& operator=(const OffsetDeleter&) noexcept = default;
  inline ~OffsetDeleter() noexcept = default;

  // 在偏移offset后调用deleter来执行销毁
  // 使用在按基类销毁场景
  inline OffsetDeleter(void (*deleter)(void*)) noexcept;
  inline OffsetDeleter(void (*deleter)(void*), ptrdiff_t offset) noexcept;

  void operator()(void* ptr) noexcept;

 private:
  void (*_deleter)(void*) {nullptr};
  ptrdiff_t _offset {0};
};

class ApplicationContext::ComponentHolder {
 public:
  // 构造时需要给定必要的类型信息，禁用默认构造
  ComponentHolder() = delete;
  ComponentHolder(ComponentHolder&& holder) = delete;
  ComponentHolder(const ComponentHolder& holder) = delete;
  ComponentHolder& operator=(ComponentHolder&& holder) = delete;
  ComponentHolder& operator=(const ComponentHolder& holder) = delete;
  virtual ~ComponentHolder() noexcept = default;

  // 设置默认初始化选项，应用在后续组件初始化中
  template <typename T>
  void set_option(T&& option) noexcept;

  // ComponentHoler all support use as factory, but not all usable as singleton.
  inline bool support_singleton() const noexcept;

  // If component is convertible to type T, return address offset in convertion.
  // Return PTRDIFF_MAX if not convertible.
  template <typename T>
  inline ptrdiff_t offset() const noexcept;

  // Type-erased component as factory interface.
  Any create(ApplicationContext& context) noexcept;
  Any create(ApplicationContext& context, const Any& option) noexcept;

  // Type-erased component as singleton interface. Create and hold a singleton
  // component inside when first get request comes.
  // Return that singleton in a type-erased way, or empty any when creation
  // failed or not support_singleton.
  inline Any& get(ApplicationContext& context) noexcept;

  void for_each_type(
      const ::std::function<void(const Id*)>& callback) const noexcept;

  inline size_t sequence() const noexcept;

  // How many paths exist to access this component by type or name.
  // Use to detect orphan component cause by type and name conflict.
  inline size_t accessible_path_number() const noexcept;

  inline const ::std::string& name() const noexcept;
  inline const Id* type_id() const noexcept;

 protected:
  // 构造时需要给定必要的类型信息
  // T: 应和create_instance返回实际类型一致
  // BS: 若干T的基类，可以不设置，设置后支持使用基类操作组件
  // 参数为对应类型的指针，实际值不会被使用，仅用于匹配得到对应的类型
  template <typename T, typename... BS>
  ComponentHolder(T*, BS*...) noexcept;

  // 设置是否支持单例模式，默认支持
  void set_support_singleton(bool support) noexcept;

  // 探测实例是否使用BABYLON_AUTOWIRE生成了自动初始化协议函数
  // int __babylon_autowire(ApplicationContext&);
  // 有则调用，否则为空实现恒定返回成功
  int autowire(Any& instance, ApplicationContext& context) noexcept;

  // 探测实例是否具备初始化协议函数
  // int initialize(ApplicationContext&, const Any&);
  // int initialize(ApplicationContext&);
  // int initialize(const Any&);
  // int initialize();
  // 有则调用，否则为空实现恒定返回成功
  int initialize(Any& instance, ApplicationContext& context,
                 const Any& option) noexcept;

  // 设置实例类型信息，用于支持构造函数的实现
  template <typename T>
  void set_type() noexcept;
  template <typename T>
  void add_convertible_type() noexcept;
  template <typename T, typename B, typename... BS>
  void add_convertible_type() noexcept;
  template <typename T>
  void remove_convertible_type() noexcept;

  // 使用空参create_instance构造实例，之后探测协议函数并调用
  // 一般情况下实现空参版本即可，特殊情况下例如想要更换或禁用探测协议函数，可以重写此入口
  virtual Any create_instance(ApplicationContext& context,
                              const Any& option) noexcept;
  // 构造一个实例，需要子类实现
  virtual Any create_instance() noexcept = 0;

 private:
  BABYLON_DECLARE_MEMBER_INVOCABLE(initialize, Initializeable)
  BABYLON_DECLARE_MEMBER_INVOCABLE(__babylon_autowire, AutoWireable)

  // 单例的状态，未初始化、初始化中、初始化完成
  enum class SingletonState {
    DISABLED,
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED
  };

  // 取得下一个自增序号，用来标识初始化顺序实现逆序销毁
  static size_t next_sequence() noexcept;

  ptrdiff_t convert_offset(const Id* type) const noexcept;

  void create_singleton(ApplicationContext& context) noexcept;

  inline ::std::atomic<SingletonState>& atomic_singleton_state() noexcept;

  // 探测实例是否使用BABYLON_AUTOWIRE生成了自动初始化协议函数
  // int __babylon_autowire(ApplicationContext&);
  // 有则调用，否则为空实现恒定返回成功
  template <typename T,
            typename ::std::enable_if<
                AutoWireable<T, ApplicationContext&>::value, int>::type = 0>
  static int autowire(Any& instance, ApplicationContext& context) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !AutoWireable<T, ApplicationContext&>::value, int>::type = 0>
  static int autowire(Any&, ApplicationContext&) noexcept;

  // 探测实例是否具备初始化协议函数
  // int initialize(ApplicationContext&, const Any&);
  // int initialize(ApplicationContext&);
  // int initialize(const Any&);
  // int initialize();
  // 有则调用，否则为空实现恒定返回成功
  template <typename T,
            typename ::std::enable_if<
                Initializeable<T, ApplicationContext&, const Any&>::value,
                int>::type = 0>
  static int initialize(Any& instance, ApplicationContext& context,
                        const Any& option) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !Initializeable<T, ApplicationContext&, const Any&>::value &&
                    Initializeable<T, ApplicationContext&>::value,
                int>::type = 0>
  static int initialize(Any& instance, ApplicationContext& context,
                        const Any& option) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !Initializeable<T, ApplicationContext&, const Any&>::value &&
                    !Initializeable<T, ApplicationContext&>::value &&
                    Initializeable<T, const Any&>::value,
                int>::type = 0>
  static int initialize(Any& instance, ApplicationContext& context,
                        const Any&) noexcept;
  template <
      typename T,
      typename ::std::enable_if<
          !Initializeable<T, ApplicationContext&, const Any&>::value &&
              !Initializeable<T, ApplicationContext&>::value &&
              !Initializeable<T, const Any&>::value && Initializeable<T>::value,
          int>::type = 0>
  static int initialize(Any& instance, ApplicationContext&,
                        const Any& option) noexcept;
  template <typename T,
            typename ::std::enable_if<
                !Initializeable<T, ApplicationContext&, const Any&>::value &&
                    !Initializeable<T, ApplicationContext&>::value &&
                    !Initializeable<T, const Any&>::value &&
                    !Initializeable<T>::value,
                int>::type = 0>
  static int initialize(Any&, ApplicationContext&, const Any&) noexcept;

  inline void increase_accessible_path() noexcept;
  inline void decrease_accessible_path() noexcept;

  inline void set_name(StringView name) noexcept;

  ::std::string _name;
  const Any::Descriptor* _type_id {Any::descriptor<void>()};
  ::absl::flat_hash_map<const Id*, ptrdiff_t> _convert_offset;
  int (*_autowire_function)(Any&, ApplicationContext&) {nullptr};
  int (*_initialize_function)(Any&, ApplicationContext&, const Any&) {nullptr};
  Any _option;

  ::std::unique_ptr<::std::recursive_mutex> _mutex {new ::std::recursive_mutex};
  SingletonState _singleton_state {SingletonState::UNINITIALIZED};
  Any _singleton;
  size_t _sequence {0};
  size_t _accessible_path {0};

  friend ApplicationContext;
};

template <typename T, typename... BS>
class ApplicationContext::DefaultComponentHolder
    : public ApplicationContext::ComponentHolder {
 public:
  DefaultComponentHolder() noexcept;

  static ::std::unique_ptr<DefaultComponentHolder> create() noexcept;

 protected:
  virtual Any create_instance() noexcept override;
};

template <typename T, typename... BS>
class ApplicationContext::FactoryComponentHolder
    : public ApplicationContext::DefaultComponentHolder<T, BS...> {
 public:
  FactoryComponentHolder() noexcept;

  static ::std::unique_ptr<FactoryComponentHolder> create() noexcept;
};

class ApplicationContext::EmptyComponentHolder
    : public ApplicationContext::ComponentHolder {
 public:
  EmptyComponentHolder() noexcept;

 private:
  virtual Any create_instance() noexcept override;
};

template <typename T>
class ApplicationContext::ComponentRegister {
 public:
  ComponentRegister(StringView name) noexcept;
};

template <typename T>
class ApplicationContext::ComponentAccessor {
 public:
  // ComponentHolder的轻量级包装
  // 默认实现包装了可用且永远返回失败和nullptr
  inline ComponentAccessor() noexcept = default;
  inline ComponentAccessor(ComponentAccessor&&) noexcept = default;
  inline ComponentAccessor(const ComponentAccessor&) noexcept = default;
  inline ComponentAccessor& operator=(ComponentAccessor&&) noexcept = default;
  inline ComponentAccessor& operator=(const ComponentAccessor&) noexcept =
      default;
  inline ~ComponentAccessor() noexcept = default;

  // 作为ApplicationContext::component_accessor的返回值
  // 用自身的bool转换来表达是否成功找到可用的Component
  inline operator bool() const noexcept;

  // 设置默认初始化选项，应用在后续组件初始化中
  template <typename U>
  void set_option(U&& option) noexcept;

  // 按照工厂模式创建一个新的组件实例，由于组件实例可能存在非虚父类转换
  // 返回携带定制销毁器的智能指针来实现正确的销毁
  ScopedComponent<T> create() noexcept;
  ScopedComponent<T> create(const Any& option) noexcept;

  // 按照单例模式获取组件实例
  inline T* get() noexcept;

  // 综合组件获取接口
  // 优先使用单例模式，单例模式不可用时使用工厂模式
  inline ScopedComponent<T> get_or_create() noexcept;

 private:
  // 仅供ApplicationContext::component_accessor使用
  inline ComponentAccessor(ApplicationContext& context, ComponentHolder& holder,
                           ptrdiff_t type_offset) noexcept;

  ApplicationContext* _context {nullptr};
  ComponentHolder* _holder {&EMPTY_COMPONENT_HOLDER};
  ptrdiff_t _type_offset {0};

  friend class ApplicationContext;
};

class ApplicationContext::ComponentIterator {
 public:
  using difference_type = ssize_t;
  using value_type = ComponentHolder;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = ::std::input_iterator_tag;

  inline ComponentIterator() noexcept = default;
  inline ComponentIterator(ComponentIterator&&) noexcept = default;
  inline ComponentIterator(const ComponentIterator&) noexcept = default;
  inline ComponentIterator& operator=(ComponentIterator&&) noexcept = default;
  inline ComponentIterator& operator=(const ComponentIterator&) noexcept =
      default;
  inline ~ComponentIterator() noexcept = default;

  inline reference operator*() const noexcept;
  inline pointer operator->() const noexcept;
  inline bool operator==(ComponentIterator other) const noexcept;
  inline bool operator!=(ComponentIterator other) const noexcept;

  inline ComponentIterator& operator++() noexcept;

 private:
  using UnderlyingIterator =
      ::std::vector<::std::unique_ptr<ComponentHolder>>::iterator;
  inline ComponentIterator(UnderlyingIterator iter) noexcept;

  UnderlyingIterator _iter;

  friend class ApplicationContext;
};

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::OffsetDeleter begin
inline ApplicationContext::OffsetDeleter::OffsetDeleter(
    void (*deleter)(void*)) noexcept
    : OffsetDeleter {deleter, 0} {}

inline ApplicationContext::OffsetDeleter::OffsetDeleter(
    void (*deleter)(void*), ptrdiff_t offset) noexcept
    : _deleter {deleter}, _offset {offset} {}
// ApplicationContext::OffsetDeleter end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::DefaultComponentHolder begin
template <typename T, typename... BS>
ApplicationContext::DefaultComponentHolder<
    T, BS...>::DefaultComponentHolder() noexcept
    : ApplicationContext::ComponentHolder {static_cast<T*>(nullptr),
                                           static_cast<BS*>(nullptr)...} {}

template <typename T, typename... BS>
::std::unique_ptr<ApplicationContext::DefaultComponentHolder<T, BS...>>
ApplicationContext::DefaultComponentHolder<T, BS...>::create() noexcept {
  return {new DefaultComponentHolder, {}};
}

template <typename T, typename... BS>
Any ApplicationContext::DefaultComponentHolder<
    T, BS...>::create_instance() noexcept {
  return Any {::std::unique_ptr<T> {new T}};
};
// ApplicationContext::DefaultComponentHolder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::FactoryComponentHolder begin
template <typename T, typename... BS>
ApplicationContext::FactoryComponentHolder<
    T, BS...>::FactoryComponentHolder() noexcept
    : ApplicationContext::DefaultComponentHolder<T, BS...> {} {
  this->set_support_singleton(false);
}

template <typename T, typename... BS>
::std::unique_ptr<ApplicationContext::FactoryComponentHolder<T, BS...>>
ApplicationContext::FactoryComponentHolder<T, BS...>::create() noexcept {
  return {new FactoryComponentHolder, {}};
}
// ApplicationContext::FactoryComponentHolder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::ComponentRegister begin
template <typename T>
ApplicationContext::ComponentRegister<T>::ComponentRegister(
    StringView name) noexcept {
  ApplicationContext::instance().register_component(T::create(), name);
}
// ApplicationContext::ComponentRegister end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext begin
template <typename T>
ABSL_ATTRIBUTE_NOINLINE ApplicationContext::ComponentAccessor<T>
ApplicationContext::component_accessor() noexcept {
  auto it = _holder_by_type.find(&TypeId<T>().ID);
  if (it != _holder_by_type.end() && it->second != nullptr) {
    return {*this, *it->second, it->second->template offset<T>()};
  }
  return {};
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE ApplicationContext::ComponentAccessor<T>
ApplicationContext::component_accessor(StringView name) noexcept {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wexit-time-destructors"
  static thread_local ::std::tuple<const Id*, ::std::string> key;
#pragma GCC diagnostic pop
  ::std::get<0>(key) = &TypeId<T>().ID;
  ::std::get<1>(key) = name;
  auto it = _holder_by_type_and_name.find(key);
  if (it != _holder_by_type_and_name.end() && it->second != nullptr) {
    return {*this, *it->second, it->second->template offset<T>()};
  }
  return {};
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE T* ApplicationContext::get() noexcept {
  return component_accessor<T>().get();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE T* ApplicationContext::get(StringView name) noexcept {
  return component_accessor<T>(name).get();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE ApplicationContext::ScopedComponent<T>
ApplicationContext::get_or_create() noexcept {
  return component_accessor<T>().get_or_create();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE ApplicationContext::ScopedComponent<T>
ApplicationContext::get_or_create(StringView name) noexcept {
  return component_accessor<T>(name).get_or_create();
}
// ApplicationContext end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::ComponentHolder begin
template <typename T>
ABSL_ATTRIBUTE_NOINLINE void ApplicationContext::ComponentHolder::set_option(
    T&& option) noexcept {
  _option = ::std::forward<T>(option);
}

template <typename T>
inline ptrdiff_t ApplicationContext::ComponentHolder::offset() const noexcept {
  return convert_offset(&Any::descriptor<T>()->type_id);
}

inline Any& ApplicationContext::ComponentHolder::get(
    ApplicationContext& context) noexcept {
  if (ABSL_PREDICT_TRUE(
          atomic_singleton_state().load(::std::memory_order_acquire) ==
          SingletonState::INITIALIZED)) {
    return _singleton;
  }
  create_singleton(context);
  return _singleton;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE bool
ApplicationContext::ComponentHolder::support_singleton() const noexcept {
  return _singleton_state != SingletonState::DISABLED;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ApplicationContext::ComponentHolder::sequence() const noexcept {
  return _sequence;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE size_t
ApplicationContext::ComponentHolder::accessible_path_number() const noexcept {
  return _accessible_path;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE const ::std::string&
ApplicationContext::ComponentHolder::name() const noexcept {
  return _name;
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE const Id*
ApplicationContext::ComponentHolder::type_id() const noexcept {
  return &(_type_id->type_id);
}

template <typename T, typename... BS>
ABSL_ATTRIBUTE_NOINLINE ApplicationContext::ComponentHolder::ComponentHolder(
    T*, BS*...) noexcept {
  set_type<T>();
  add_convertible_type<T, T, BS...>();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE void
ApplicationContext::ComponentHolder::set_type() noexcept {
  _type_id = Any::descriptor<T>();
  _autowire_function = &autowire<T>;
  _initialize_function = &initialize<T>;
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE void
ApplicationContext::ComponentHolder::add_convertible_type() noexcept {}

template <typename T, typename U, typename... US>
ABSL_ATTRIBUTE_NOINLINE void
ApplicationContext::ComponentHolder::add_convertible_type() noexcept {
  _convert_offset[&TypeId<U>::ID] = reinterpret_cast<ptrdiff_t>(static_cast<U*>(
                                        reinterpret_cast<T*>(alignof(T)))) -
                                    alignof(T);
  add_convertible_type<T, US...>();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE void
ApplicationContext::ComponentHolder::remove_convertible_type() noexcept {
  _convert_offset.erase(&TypeId<T>::ID);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE ::std::atomic<
    ApplicationContext::ComponentHolder::SingletonState>&
ApplicationContext::ComponentHolder::atomic_singleton_state() noexcept {
  return reinterpret_cast<::std::atomic<SingletonState>&>(_singleton_state);
}

template <typename T, typename ::std::enable_if<
                          ApplicationContext::ComponentHolder::AutoWireable<
                              T, ApplicationContext&>::value,
                          int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::autowire(
    Any& instance, ApplicationContext& context) noexcept {
  return instance.get<T>()->__babylon_autowire(context);
}

template <typename T, typename ::std::enable_if<
                          !ApplicationContext::ComponentHolder::AutoWireable<
                              T, ApplicationContext&>::value,
                          int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::autowire(
    Any&, ApplicationContext&) noexcept {
  return 0;
}

template <typename T, typename ::std::enable_if<
                          ApplicationContext::ComponentHolder::Initializeable<
                              T, ApplicationContext&, const Any&>::value,
                          int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::initialize(
    Any& instance, ApplicationContext& context, const Any& option) noexcept {
  return instance.get<T>()->initialize(context, option);
}

template <typename T, typename ::std::enable_if<
                          !ApplicationContext::ComponentHolder::Initializeable<
                              T, ApplicationContext&, const Any&>::value &&
                              ApplicationContext::ComponentHolder::
                                  Initializeable<T, ApplicationContext&>::value,
                          int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::initialize(
    Any& instance, ApplicationContext& context, const Any&) noexcept {
  return instance.get<T>()->initialize(context);
}

template <typename T,
          typename ::std::enable_if<
              !ApplicationContext::ComponentHolder::Initializeable<
                  T, ApplicationContext&, const Any&>::value &&
                  !ApplicationContext::ComponentHolder::Initializeable<
                      T, ApplicationContext&>::value &&
                  ApplicationContext::ComponentHolder::Initializeable<
                      T, const Any&>::value,
              int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::initialize(
    Any& instance, ApplicationContext&, const Any& option) noexcept {
  return instance.get<T>()->initialize(option);
}

template <typename T,
          typename ::std::enable_if<
              !ApplicationContext::ComponentHolder::Initializeable<
                  T, ApplicationContext&, const Any&>::value &&
                  !ApplicationContext::ComponentHolder::Initializeable<
                      T, ApplicationContext&>::value &&
                  !ApplicationContext::ComponentHolder::Initializeable<
                      T, const Any&>::value &&
                  ApplicationContext::ComponentHolder::Initializeable<T>::value,
              int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::initialize(
    Any& instance, ApplicationContext&, const Any&) noexcept {
  return instance.get<T>()->initialize();
}

template <
    typename T,
    typename ::std::enable_if<
        !ApplicationContext::ComponentHolder::Initializeable<
            T, ApplicationContext&, const Any&>::value &&
            !ApplicationContext::ComponentHolder::Initializeable<
                T, ApplicationContext&>::value &&
            !ApplicationContext::ComponentHolder::Initializeable<
                T, const Any&>::value &&
            !ApplicationContext::ComponentHolder::Initializeable<T>::value,
        int>::type>
ABSL_ATTRIBUTE_NOINLINE int ApplicationContext::ComponentHolder::initialize(
    Any&, ApplicationContext&, const Any&) noexcept {
  return 0;
}

inline void
ApplicationContext::ComponentHolder::increase_accessible_path() noexcept {
  _accessible_path++;
}

inline void
ApplicationContext::ComponentHolder::decrease_accessible_path() noexcept {
  _accessible_path--;
}

inline void ApplicationContext::ComponentHolder::set_name(
    StringView name) noexcept {
  _name = name;
}
// ApplicationContext::ComponentHolder end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::ComponentAccessor begin
template <typename T>
inline ABSL_ATTRIBUTE_ALWAYS_INLINE
    ApplicationContext::ComponentAccessor<T>::operator bool() const noexcept {
  return _holder != &EMPTY_COMPONENT_HOLDER;
}

template <typename T>
template <typename U>
ABSL_ATTRIBUTE_NOINLINE void
ApplicationContext::ComponentAccessor<T>::set_option(U&& option) noexcept {
  if (operator bool()) {
    _holder->set_option(::std::forward<U>(option));
  }
}

template <typename T>
ApplicationContext::ScopedComponent<T>
ApplicationContext::ComponentAccessor<T>::create() noexcept {
  auto instance = _holder->create(*_context).release();
  if (instance) {
    auto address =
        reinterpret_cast<intptr_t>(instance.release()) + _type_offset;
    return {reinterpret_cast<T*>(address),
            {instance.get_deleter(), -_type_offset}};
  }
  return {};
}

template <typename T>
ApplicationContext::ScopedComponent<T>
ApplicationContext::ComponentAccessor<T>::create(const Any& option) noexcept {
  auto instance = _holder->create(*_context, option).release();
  if (instance) {
    auto address =
        reinterpret_cast<intptr_t>(instance.release()) + _type_offset;
    return {reinterpret_cast<T*>(address),
            {instance.get_deleter(), -_type_offset}};
  }
  return {};
}

template <typename T>
inline T* ApplicationContext::ComponentAccessor<T>::get() noexcept {
  auto& instance = _holder->get(*_context);
  if (instance) {
    auto address = reinterpret_cast<intptr_t>(instance.get()) + _type_offset;
    return reinterpret_cast<T*>(address);
  }
  return nullptr;
}

template <typename T>
inline ApplicationContext::ScopedComponent<T>
ApplicationContext::ComponentAccessor<T>::get_or_create() noexcept {
  if (_holder->support_singleton()) {
    return {get(), {}};
  }
  return create();
}

template <typename T>
inline ApplicationContext::ComponentAccessor<T>::ComponentAccessor(
    ApplicationContext& context, ComponentHolder& holder,
    ptrdiff_t type_offset) noexcept
    : _context {&context}, _holder {&holder}, _type_offset {type_offset} {}
// ApplicationContext::ComponentAccessor begin
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// ApplicationContext::ComponentIterator begin
inline ApplicationContext::ComponentIterator::reference
ApplicationContext::ComponentIterator::operator*() const noexcept {
  return **_iter;
}

inline ApplicationContext::ComponentIterator::pointer
ApplicationContext::ComponentIterator::operator->() const noexcept {
  return &**_iter;
}

inline bool ApplicationContext::ComponentIterator::operator==(
    ComponentIterator other) const noexcept {
  return _iter == other._iter;
}

inline bool ApplicationContext::ComponentIterator::operator!=(
    ComponentIterator other) const noexcept {
  return _iter != other._iter;
}

inline ApplicationContext::ComponentIterator&
ApplicationContext::ComponentIterator::operator++() noexcept {
  ++_iter;
  return *this;
}

inline ApplicationContext::ComponentIterator::ComponentIterator(
    UnderlyingIterator iter) noexcept
    : _iter {iter} {}
// ApplicationContext::ComponentIterator end
////////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END

// 自动组装宏，用于简化实现组件相互组装功能
// 根据描述最终生成__babylon_autowire协议成员函数
// 以及相应的一组自动组装成员，在组件初始化时完成依赖注入
//
// 例如
// struct AutowireComponent {
//   // 生成自动组装
//   BABYLON_AUTOWIRE(
//     // 生成成员
//     // ScopedComponent nc;
//     // 并组装为nc = context.get_or_create<NormalComponent>();
//     BABYLON_MEMBER(NormalComponent*, nc)
//     BABYLON_MEMBER(const NormalComponent*, cnc)
//     // 生成成员
//     // ScopedComponent anc1;
//     // 并组装为anc1 = context.get_or_create<AnotherNormalComponent>("name1");
//     BABYLON_MEMBER(AnotherNormalComponent*, anc1, "name1")
//     BABYLON_MEMBER(AnotherNormalComponent*, anc2, "name2")
//   )
// };
#define BABYLON_AUTOWIRE(members)                                           \
  int __babylon_autowire(::babylon::ApplicationContext& context) noexcept { \
    BOOST_PP_SEQ_FOR_EACH(__BABYLON_MEMBER_WIRE_EACH, _, members)           \
    return 0;                                                               \
  }                                                                         \
  BOOST_PP_SEQ_FOR_EACH(__BABYLON_MEMBER_DECLARE_EACH, _, members)          \
  BOOST_PP_SEQ_FOR_EACH(__BABYLON_MEMBER_DEFINE_EACH, _, members)           \
  friend class ::babylon::ApplicationContext::ComponentHolder;

// 自动组装一个成员，按类型从context获取
#define BABYLON_MEMBER(ctype, member, ...) \
  __BABYLON_MEMBER_FILL_ARGS((ctype, member, ##__VA_ARGS__))
#define __BABYLON_MEMBER_FILL_ARGS(args)                    \
  (BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 3), \
               BOOST_PP_TUPLE_PUSH_BACK(args, ""), args))

// 生成成员类型声明
#define __BABYLON_MEMBER_DECLARE_EACH(r, data, args) \
  BOOST_PP_EXPAND(__BABYLON_MEMBER_DECLARE args)
#define __BABYLON_MEMBER_DECLARE(ctype, member, name, ...) \
  using __babylon_autowire_type##member =                  \
      ::std::decay<::std::remove_pointer<ctype>::type>::type;

// 生成成员定义
#define __BABYLON_MEMBER_DEFINE_EACH(r, data, args) \
  BOOST_PP_EXPAND(__BABYLON_MEMBER_DEFINE args)
#define __BABYLON_MEMBER_DEFINE(ctype, member, name, ...) \
  ::babylon::ApplicationContext::ScopedComponent<         \
      __babylon_autowire_type##member>                    \
      member {nullptr};

// 生成成员组装代码
#define __BABYLON_MEMBER_WIRE_EACH(r, data, args) \
  BOOST_PP_EXPAND(__BABYLON_MEMBER_WIRE args)
#define __BABYLON_MEMBER_WIRE(ctype, member, name, ...)                    \
  if (::babylon::StringView(name).empty()) {                               \
    member = context.get_or_create<__babylon_autowire_type##member>();     \
  } else {                                                                 \
    member = context.get_or_create<__babylon_autowire_type##member>(name); \
  }                                                                        \
  if (!member) {                                                           \
    BABYLON_LOG(WARNING)                                                   \
        << "get component of type "                                        \
        << ::babylon::TypeId<__babylon_autowire_type##member>::ID          \
        << (::babylon::StringView(name).empty() ? "" : " with name " name) \
        << " failed";                                                      \
    return -1;                                                             \
  }

// 组件注册宏，用于随组件代码编译单元自动注册的去中心注册模式
//
// 典型使用方法
// class SomeComponent : public P1, public P2, ... {
//   ...
// };
//
// // 按类型注册
// BABYLON_REGISTER_COMPONENT(SomeComponent)
// // 按类型加名字注册
// BABYLON_REGISTER_COMPONENT(SomeComponent, "some name")
// // 按类型加名字注册，并附加可转换基类列表
// BABYLON_REGISTER_COMPONENT(SomeComponent, "some name", P1, P2, ...)
#define BABYLON_REGISTER_COMPONENT(T, ...)                                 \
  BOOST_PP_EXPAND(                                                         \
      __BABYLON_REGISTER_COMPONENT __BABYLON_REGISTER_COMPONENT_FILL_ARGS( \
          (T, ##__VA_ARGS__)))

#define __BABYLON_REGISTER_COMPONENT_FILL_ARGS(args)       \
  BOOST_PP_IF(BOOST_PP_LESS(BOOST_PP_TUPLE_SIZE(args), 2), \
              BOOST_PP_TUPLE_PUSH_BACK(args, ""), args)

#define __BABYLON_REGISTER_COMPONENT(T, name, ...)                             \
  static ::babylon::ApplicationContext::ComponentRegister<                     \
      ::babylon::ApplicationContext::DefaultComponentHolder<T, ##__VA_ARGS__>> \
      BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__) {name};

#define BABYLON_REGISTER_FACTORY_COMPONENT(T, ...) \
  BOOST_PP_EXPAND(                                 \
      __BABYLON_REGISTER_FACTORY_COMPONENT         \
          __BABYLON_REGISTER_COMPONENT_FILL_ARGS((T, ##__VA_ARGS__)))

#define __BABYLON_REGISTER_FACTORY_COMPONENT(T, name, ...)                     \
  static ::babylon::ApplicationContext::ComponentRegister<                     \
      ::babylon::ApplicationContext::FactoryComponentHolder<T, ##__VA_ARGS__>> \
      BOOST_PP_CAT(Babylon_Application_Context_Register, __COUNTER__) {name};
