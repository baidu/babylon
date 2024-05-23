#pragma once

#include "babylon/any.h"

#include "babylon/logging/interface.h"

#include "absl/container/flat_hash_map.h"
#include "boost/preprocessor/comparison/equal.hpp"
#include "boost/preprocessor/control/expr_if.hpp"
#include "boost/preprocessor/facilities/expand.hpp"
#include "boost/preprocessor/seq/for_each.hpp"

#include <memory>
#include <mutex>

BABYLON_NAMESPACE_BEGIN

// IOC容器，实现组件的注册和获取
// 组件可以是任意类型，且不需要公共基类
// 支持组件组装，和宏支持下的自动组装
class ApplicationContext {
 public:
  /*
class ComponentHolder;
template <typename T>
inline static void destroy_by_holder(ComponentHolder*, T*);

template <typename T>
class ComponentDeleter {
public:
  // 构造时明确传入解分配器，传入nullptr视为非托管对象
  inline ConditionalDeleter(D* deallocator) noexcept;
  inline void operator()(T* ptr) const noexcept;
  // 查询是否是托管对象
  inline bool own() const noexcept;

private:
  D* _deallocator;
};

  template <typename T>
  using ScopedComponent =
      ::std::unique_ptr<T, ConditionalDeleter<T, ComponentHolder,
destroy_by_holder>>;
*/

  // 组件容器，包装了对具体组件进行初始化和组装的方法
  // 单例ComponentHolder会在initialize时创建全局单例并完成wireup
  // 工厂ComponentHolder在initialize时只会准备工厂本身
  // 实际在每次get时进行组件创建，并完成wireup
  class ComponentHolder {
   public:
    // 构造时需要给定必要的类型信息，禁用默认构造
    ComponentHolder() = delete;
    ComponentHolder(ComponentHolder&& holder) = delete;
    ComponentHolder(const ComponentHolder& holder) = delete;
    ComponentHolder& operator=(ComponentHolder&& holder) = default;
    ComponentHolder& operator=(const ComponentHolder& holder) = delete;
    virtual ~ComponentHolder() noexcept = default;

    /*
    // 获取一个组件实例，实际可能是单例或者工厂模式，由实现确定
    // 根据是否需要外部控制生命周期，返回对应的智能指针
    template <typename T>
    inline ScopedComponent<T> get_or_create();
    // 在需要进行生命周期管理的情况下，会通过destroy完成释放动作
    virtual void destroy(void* component) noexcept;
    // 组件的名字，在注册时由ApplicationContext统一设置
    // 不被具体的ComponentHolder所控制
    inline const ::std::string& name() const noexcept;
    // 组件的类型
    inline const Id& type() const noexcept;
    // 组件的初始化顺序序列号
    inline size_t seq() const noexcept;

    inline ComponentHolder(const Id& type_id) noexcept;
    */

    template <typename T>
    void set_option(T&& option) noexcept;

    template <typename T>
    ::std::unique_ptr<T> create(ApplicationContext& context) noexcept;
    template <typename T>
    ::std::unique_ptr<T> create(ApplicationContext& context, const Any& option) noexcept;

    template <typename T>
    inline T* get(ApplicationContext& context) noexcept;

    inline bool singleton_disabled() const noexcept;

   protected:
    // 构造时需要给定必要的类型信息
    // T: 应和create_instance返回实际类型一致
    // BS: 若干T的基类，可以不设置，设置后支持使用基类操作组件
    // 参数为对应类型的指针，实际值不会被使用，仅用于匹配得到对应的类型
    template <typename T, typename... BS>
    ComponentHolder(T*, BS*...) noexcept;

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
    int initialize(Any& instance, ApplicationContext& context, const Any& option) noexcept;

    // 使用空参create_instance构造实例，之后探测协议函数并调用
    // 一般情况下实现空参版本即可，特殊情况下例如想要更换或禁用探测协议函数，可以重写此入口
    virtual Any create_instance(ApplicationContext& context, const Any& option) noexcept;
    // 构造一个实例，需要子类实现
    virtual Any create_instance() noexcept = 0;

   private:
    BABYLON_DECLARE_MEMBER_INVOCABLE(initialize, Initializeable);
    BABYLON_DECLARE_MEMBER_INVOCABLE(__babylon_autowire, AutoWireable);

    // 单例的状态，未初始化、初始化中、初始化完成
    enum class SingletonState { DISABLED, UNINITIALIZED, INITIALIZING, INITIALIZED };

    static size_t next_sequence() noexcept;

    // 设置实例类型信息，用于支持构造函数实现
    template <typename T>
    void set_type() noexcept;
    template <typename T>
    void add_convertible_type() noexcept;
    template <typename T, typename B, typename... BS>
    void add_convertible_type() noexcept;

    template <typename T>
    T* create_and_then_get(ApplicationContext& context) noexcept;

    inline ::std::atomic<SingletonState>& atomic_singleton_state() noexcept;

    // 探测实例是否使用BABYLON_AUTOWIRE生成了自动初始化协议函数
    // int __babylon_autowire(ApplicationContext&);
    // 有则调用，否则为空实现恒定返回成功
    template <typename T, typename ::std::enable_if<AutoWireable<T, ApplicationContext&>::value, int>::type = 0>
    static int autowire(Any& instance, ApplicationContext& context) noexcept;
    template <typename T, typename ::std::enable_if<!AutoWireable<T, ApplicationContext&>::value, int>::type = 0>
    static int autowire(Any&, ApplicationContext&) noexcept;

    // 探测实例是否具备初始化协议函数
    // int initialize(ApplicationContext&, const Any&);
    // int initialize(ApplicationContext&);
    // int initialize(const Any&);
    // int initialize();
    // 有则调用，否则为空实现恒定返回成功
    template <typename T, typename ::std::enable_if<Initializeable<T, ApplicationContext&, const Any&>::value, int>::type = 0>
    static int initialize(Any& instance, ApplicationContext& context, const Any& option) noexcept;
    template <typename T, typename ::std::enable_if<!Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                    Initializeable<T, ApplicationContext&>::value, int>::type = 0>
    static int initialize(Any& instance, ApplicationContext& context, const Any& option) noexcept;
    template <typename T, typename ::std::enable_if<!Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                    !Initializeable<T, ApplicationContext&>::value &&
                                                    Initializeable<T, const Any&>::value, int>::type = 0>
    static int initialize(Any& instance, ApplicationContext& context, const Any&) noexcept;
    template <typename T, typename ::std::enable_if<!Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                    !Initializeable<T, ApplicationContext&>::value &&
                                                    !Initializeable<T, const Any&>::value &&
                                                    Initializeable<T>::value, int>::type = 0>
    static int initialize(Any& instance, ApplicationContext&, const Any& option) noexcept;
    template <typename T, typename ::std::enable_if<!Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                    !Initializeable<T, ApplicationContext&>::value &&
                                                    !Initializeable<T, const Any&>::value &&
                                                    !Initializeable<T>::value, int>::type = 0>
    static int initialize(Any&, ApplicationContext&, const Any&) noexcept;

    const Id* _type_id {&TypeId<void>::ID};
    ::absl::flat_hash_map<const Id*, ptrdiff_t> _convert_offset;
    int (*_autowire_function)(Any&, ApplicationContext&) {nullptr};
    int (*_initialize_function)(Any&, ApplicationContext&, const Any&) {nullptr};
    Any _option;

    ::std::unique_ptr<::std::recursive_mutex> _mutex {new ::std::recursive_mutex};
    SingletonState _singleton_state {SingletonState::UNINITIALIZED};
    Any _singleton;
    size_t _sequence {0};
  };

  /*
      template <typename T>
      class ComponentAccessor {
      public:
          inline ComponentAccessor() noexcept = default;
          inline ComponentAccessor(ComponentAccessor&&) noexcept = default;
          inline ComponentAccessor(const ComponentAccessor&) noexcept = default;
          inline ComponentAccessor& operator=(ComponentAccessor&&) noexcept =
     default; inline ComponentAccessor& operator=(const ComponentAccessor&)
     noexcept = default;

          inline operator bool() const noexcept {
              return _holder != &EMPTY_COMPONENT_HOLDER;
          }

          inline ScopedComponent<T> get_or_create() {
              return _holder->get_or_create<T>();
          }

      private:
          inline ComponentAccessor(ComponentHolder& holder) noexcept :
     _holder(&holder) {}

          ComponentHolder* _holder {&EMPTY_COMPONENT_HOLDER};

          friend class ApplicationContext;
      };

      // 单例context
      inline static ApplicationContext& instance();

      // 可以默认和移动构造，不可拷贝
      inline ApplicationContext() = default;
      inline ApplicationContext(const ApplicationContext&) = delete;
      inline ApplicationContext(ApplicationContext&& other) noexcept;
      // 析构函数会析构所有ComponentHolder
      inline ~ApplicationContext();

      // 注册一个组件容器，默认没有名字
      inline void register_component(::std::unique_ptr<ComponentHolder>&&
     holder, StringView name = ""); template <typename H> inline void
     register_component(H&& holder, StringView name = "");

      // 初始化整个ApplicationContext
      // 此时会校验所有组件是否有命名和类型冲突，有则返回非0值
      // 对于is_lazy = false的默认情况，会对所有组件进行初始化和组装
      // 任何组件初始化或组装失败直接终止initialize过程并返回非0值
      // 对于is_lazy = true的情况，实际不会初始化任何组件
      inline int32_t initialize(bool is_lazy = false);

      // 按照类型或者类型+名字获取组件访问器
      // 按类型访问仅在具有该类型的组件唯一的情况下可用，否则必须要依赖名字区分
      // 组件访问器可以保存下来，用于后续快速访问组件，而不需要再次查找
      // 对于单例类型，首次获取某个组件访问器时会完成初始化和装配
      // 对于工厂类型，首次获取某个组件访问器时会尝试创建和装配一次，用于验证
      // 在无法找到组件，创建、初始化或装配失败时，返回无效ComponentAccessor
      // 可以通过if (accessor)判断返回值的有效性
      //
     对无效的ComponentAccessor进行get_or_create操作，恒定返回无效的ScopedComponent
      template <typename T>
      inline ComponentAccessor<T> get_component_accessor();
      template <typename T>
      inline ComponentAccessor<T> get_component_accessor(StringView name);

      // 相当于
      // get_component_accessor().get_or_create()
      // get_component_accessor("name").get_or_create()
      // 失败返回的ScopedComponent携带nullptr
      // 如果是工厂组件，生命周期由返回的ScopedComponent控制
      template <typename T>
      inline ScopedComponent<T> get_or_create();
      template <typename T>
      inline ScopedComponent<T> get_or_create(StringView name);

      // 限定使用单例方式获取组件相当于
      // get_or_create().get()
      // 特殊点为，会检测是否组件为单例，非单例组件也会返回nullptr
      // 由于限定了单例，返回的指针无需关注生命周期，例如不应该主动delete
      template <typename T>
      inline T* get();
      template <typename T>
      inline T* get(StringView name);

      // 清空并析构所有ComponentHolder，一般用于支持优雅退出
      // 需要确保正确的调用时机
      // 调用后所有get返回的单例组件均不再可用
      // 工厂组件不保证后续能够正确使用和析构
      void clear() noexcept;
      */

 private:
  /*
      // 初始化单个Component，对注册过的某个组件进行初始化和组装
      inline int32_t initialize(ComponentHolder* holder) noexcept;

      // 创建组件映射
      inline int32_t map_component(ComponentHolder* holder) noexcept;
      // 创建type + name签名，给出TypeId
      inline static void build_real_name(::std::string& real_name,
          StringView name, const Id& type) noexcept;
      // 创建type + name签名，从模板T推导TypeId
      template <typename T>
      inline static void build_real_name(::std::string& real_name,
          StringView name) noexcept;

      static ComponentHolder EMPTY_COMPONENT_HOLDER;

      ::std::vector<::std::unique_ptr<ComponentHolder>> _holders;
      ::absl::flat_hash_map<const Id*, ComponentHolder*> _holder_by_type;
      ::absl::flat_hash_map<::std::string, ComponentHolder*> _holder_by_name;
      */
};

template <typename T, typename... BS>
ABSL_ATTRIBUTE_NOINLINE
ApplicationContext::ComponentHolder::ComponentHolder(T*, BS*...) noexcept {
  set_type<T>();
  add_convertible_type<T, BS...>();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
void ApplicationContext::ComponentHolder::set_option(T&& option) noexcept {
  _option = ::std::forward<T>(option);
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
::std::unique_ptr<T> ApplicationContext::ComponentHolder::create(ApplicationContext& context) noexcept {
  return create<T>(context, _option);
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
::std::unique_ptr<T> ApplicationContext::ComponentHolder::create(ApplicationContext& context, const Any& option) noexcept {
  return create_instance(context, option).release<T>();
}

template <typename T>
inline T* ApplicationContext::ComponentHolder::get(ApplicationContext& context) noexcept {
  if (ABSL_PREDICT_TRUE(atomic_singleton_state().load(::std::memory_order_acquire) == SingletonState::INITIALIZED)) {
    return _singleton.get<T>();
  }
  return create_and_then_get<T>(context);
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE
bool ApplicationContext::ComponentHolder::singleton_disabled() const noexcept {
  return _singleton_state == SingletonState::DISABLED;
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
void ApplicationContext::ComponentHolder::set_type() noexcept {
  _type_id = &TypeId<T>::ID;
  _autowire_function = &autowire<T>;
  _initialize_function = &initialize<T>;
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
void ApplicationContext::ComponentHolder::add_convertible_type() noexcept {
}

template <typename T, typename U, typename... US>
ABSL_ATTRIBUTE_NOINLINE
void ApplicationContext::ComponentHolder::add_convertible_type() noexcept {
  _convert_offset[&TypeId<U>::ID] =
      reinterpret_cast<ptrdiff_t>(static_cast<U*>(reinterpret_cast<T*>(0)));
  add_convertible_type<T, US...>();
}

template <typename T>
ABSL_ATTRIBUTE_NOINLINE
T* ApplicationContext::ComponentHolder::create_and_then_get(ApplicationContext& context) noexcept {
  ::std::lock_guard<std::recursive_mutex> lock(*_mutex);
  if (_singleton_state == ComponentHolder::SingletonState::INITIALIZING) {
    BABYLON_LOG(WARNING) << "initialize failed for recursive dependent component of type " << *_type_id;
    return nullptr;
  }
  if (_singleton_state == ComponentHolder::SingletonState::UNINITIALIZED) {
    _singleton_state = ComponentHolder::SingletonState::INITIALIZING; 
    _singleton = create<T>();
    atomic_singleton_state().store(ComponentHolder::SingletonState::INITIALIZED, ::std::memory_order_release);
    _sequence = next_sequence();
  }
  return _singleton.get<T>();
}

inline ABSL_ATTRIBUTE_ALWAYS_INLINE
::std::atomic<ApplicationContext::ComponentHolder::SingletonState>&
ApplicationContext::ComponentHolder::atomic_singleton_state() noexcept {
  return reinterpret_cast<::std::atomic<SingletonState>&>(_singleton_state);
}

template <typename T, typename ::std::enable_if<ApplicationContext::ComponentHolder::AutoWireable<T, ApplicationContext&>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::autowire(Any& instance, ApplicationContext& context) noexcept {
  return instance.get<T>()->__babylon_autowire(context);
}

template <typename T, typename ::std::enable_if<!ApplicationContext::ComponentHolder::AutoWireable<T, ApplicationContext&>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::autowire(Any&, ApplicationContext&) noexcept {
  return 0;
}

template <typename T, typename ::std::enable_if<ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&, const Any&>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::initialize(Any& instance, ApplicationContext& context, const Any& option) noexcept {
  return instance.get<T>()->initialize(context, option);
}

template <typename T, typename ::std::enable_if<!ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::initialize(Any& instance, ApplicationContext& context, const Any&) noexcept {
  return instance.get<T>()->initialize(context);
}

template <typename T, typename ::std::enable_if<!ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&>::value &&
                                                ApplicationContext::ComponentHolder::Initializeable<T, const Any&>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::initialize(Any& instance, ApplicationContext&, const Any& option) noexcept {
  return instance.get<T>()->initialize(option);
}

template <typename T, typename ::std::enable_if<!ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T, const Any&>::value &&
                                                ApplicationContext::ComponentHolder::Initializeable<T>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::initialize(Any& instance, ApplicationContext&, const Any&) noexcept {
  return instance.get<T>()->initialize();
}

template <typename T, typename ::std::enable_if<!ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&, const Any&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T, ApplicationContext&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T, const Any&>::value &&
                                                !ApplicationContext::ComponentHolder::Initializeable<T>::value, int>::type>
ABSL_ATTRIBUTE_NOINLINE
int ApplicationContext::ComponentHolder::initialize(Any&, ApplicationContext&, const Any&) noexcept {
  return 0;
}

template <typename T, typename... BS>
class DefaultComponentHolder : public ApplicationContext::ComponentHolder {
public:
  DefaultComponentHolder() noexcept : ApplicationContext::ComponentHolder {static_cast<T*>(nullptr), static_cast<BS*>(nullptr)...} {
  }

private:
  virtual Any create_instance() noexcept override {
    return Any {::std::unique_ptr<T>(new T)};
  }
};

// 辅助自动组装的宏
#define __BABYLON_DECLARE_AUTOWIRE_MEMBER(member_type, member, name)   \
  ::baidu::feed::mlarch::babylon::ApplicationContext::ScopedComponent< \
      ::std::decay<::std::remove_pointer<member_type>::type>::type>    \
      member {nullptr, nullptr};

#define __BABYLON_AUTOWIRE_MEMBER(member_type, member, name)           \
  member = context.get_or_create<                                      \
      ::std::decay<::std::remove_pointer<member_type>::type>::type>(); \
  if (member == nullptr) {                                             \
    LOG(WARNING) << "get component[]"                                  \
                 << " with type[" << #member_type << "] failed";       \
    return -1;                                                         \
  }

#define __BABYLON_AUTOWIRE_MEMBER_BY_NAME(member_type, member, name)        \
  member = context.get_or_create<                                           \
      ::std::decay<::std::remove_pointer<member_type>::type>::type>(#name); \
  if (member == nullptr) {                                                  \
    LOG(WARNING) << "get component[]"                                       \
                 << " with type[" << #member_type << "] failed";            \
    return -1;                                                              \
  }

#define __BABYLON_DECLARE_EACH_AUTOWIRE_MEMBER(r, data, args) \
  BOOST_PP_EXPAND(__BABYLON_DECLARE_AUTOWIRE_MEMBER BOOST_PP_SEQ_ELEM(1, args))

#define __BABYLON_AUTOWIRE_EACH_MEMBER(r, data, args)                      \
  BOOST_PP_EXPR_IIF(                                                       \
      BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(0, BOOST_PP_SEQ_ELEM(0, args)), 0), \
      __BABYLON_AUTOWIRE_MEMBER BOOST_PP_SEQ_ELEM(1, args))                \
  BOOST_PP_EXPR_IIF(                                                       \
      BOOST_PP_EQUAL(BOOST_PP_SEQ_ELEM(0, BOOST_PP_SEQ_ELEM(0, args)), 1), \
      __BABYLON_AUTOWIRE_MEMBER_BY_NAME BOOST_PP_SEQ_ELEM(1, args))

// 自动组装一个成员，按类型从context获取
#define BABYLON_MEMBER(type, member) (((0))((type, member, _)))
// 自动组装一个成员，按类型+名字从context获取
#define BABYLON_MEMBER_BY_NAME(type, member, name) (((1))((type, member, name)))
// 生成自动组装函数和成员，例如
// struct AutowireComponent {
//    // 生成自动组装
//    BABYLON_AUTOWIRE(
//        // 生成成员
//        // NormalComponent* nc;
//        // 并组装为nc = context.get<NormalComponent>();
//        BABYLON_MEMBER(NormalComponent*, nc)
//        BABYLON_MEMBER(const NormalComponent*, cnc)
//        // 生成成员
//        // AnotherNormalComponent* anc1;
//        // 并组装为anc1 = context.get<AnotherNormalComponent>("name1");
//        BABYLON_MEMBER_BY_NAME(AnotherNormalComponent*, anc1, name1)
//        BABYLON_MEMBER_BY_NAME(AnotherNormalComponent*, anc2, name2)
//    )
//};
#define BABYLON_AUTOWIRE(members)                                             \
  inline int32_t __babylon_auto_wireup(                                       \
      ::baidu::feed::mlarch::babylon::ApplicationContext& context) noexcept { \
    BOOST_PP_SEQ_FOR_EACH(__BABYLON_AUTOWIRE_EACH_MEMBER, _, members)         \
    return 0;                                                                 \
  }                                                                           \
  BOOST_PP_SEQ_FOR_EACH(__BABYLON_DECLARE_EACH_AUTOWIRE_MEMBER, _, members)   \
  friend class ::baidu::feed::mlarch::babylon::ApplicationContext::           \
      ComponentHolder;

BABYLON_NAMESPACE_END

//#include "baidu/feed/mlarch/babylon/application_context.hpp"
//#include "baidu/feed/mlarch/babylon/component_holder.h"
