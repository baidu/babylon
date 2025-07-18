#pragma once

#include "babylon/reusable/memory_resource.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/utility/utility.h)
// clang-format on

#include "google/protobuf/stubs/common.h" // GOOGLE_PROTOBUF_VERSION

#include <stddef.h> // ::max_align_t

#include <vector> // std::vector

BABYLON_NAMESPACE_BEGIN

// 从c++11开始，标准库设计了uses-allocator construction协议
// 该协议主要用于实现allocator级联，例如对于std::vector<T, A>类型
// 如果T实现了该协议，那么除了std::vector本身使用A作为allocator
// 内部构造每一个元素T时，也会进行透明allocator增补让T也使用A构造
// 例如，在std::vector::emplace(...)时，会实际采用T(..., A)替代T(...)来构造
// 为了方便这种探测，也提供了std::uses_allocator<T,
// A>用于判定T类型是否遵守了该协议 但是由于历史原因，这个协议设计很复杂
// 例如对于T(Args...)构造，提供前置或者后置A的版本都视为符合协议
// 即T(Args..., A)和T(std::allocator_arg_t, A, Args...)
// 而且还存在std::pair这一特例，即尽管std::pair未实现任意一种协议
// 其std::uses_allocator<std::pair<F, S>, A>判定也为false
// 但是实际上std系列容器依然提供了特殊支持
// 即尝试对std::pair的first和second分别应用A来构造
//
// 因此正确实现uses-allocator construction协议远比判定更复杂
// 以至于在c++20之后提供了std::make_obj_using_allocator专门支持这个协议
// 但是c++20前暂时未提供此类功能，这里专门实现一版用于兼容
class UsesAllocatorConstructor {
 private:
  template <typename P, typename A, typename... Args>
  struct PairConstructible;
  template <typename P, typename A, typename... Args>
  struct PairCopyOrMoveConstructible;

 public:
  // 判断是否可以采用uses-allocator construction协议
  // 使用A作为Allocator来构造T
  // 判定比std::uses_allocator更精细，具体到一套实际的Args...
  // 会依据T的实现情况判定是否满足，以及应该采用前置还是后置增补，即
  // T(std::allocator_arg_t, A, Args...)
  // 还是
  // T(Args..., A)
  // 此外，针对std::pair进行特化处理，不依赖原本的构造函数
  // 在std::pair的任意元素能够应用协议时，即视为满足协议
  //
  // 满足协议时：Constructible::USES_ALLOCATOR == true
  // 否则：Constructible::USES_ALLOCATOR == false
  //
  // 同时为了方便使用，也对齐make_obj_using_allocator实现
  // 增加了Constructible::value判定
  // 除了Constructible::USES_ALLOCATOR == true时Constructible::value == true
  // 在Constructible::USES_ALLOCATOR == false时，如果T(Args...)直接可用
  // 也会使Constructible::value == true
  //
  // 只要Constructible::value == true，则可以使用
  // UsesAllocatorConstructor::construct(T*, A, Args...)进行构造
  // 只是在不满足协议时，退化成直接使用Args...构造
  template <typename T, typename A, typename... Args>
  class Constructible;

  ////////////////////////////////////////////////////////////////////////////
  // 统一签名为construct(T*, A, Args...)
  // 优先尝试基于协议传递A进行构造
  // 否则采用T(Args...)完成构造

  // 可前缀增补构造T(allocator_arg, A, Args...)
  template <typename T, typename A, typename... Args,
            typename ::std::enable_if<Constructible<T, A, Args&&...>::PREFIXED,
                                      int>::type = 0>
  inline static void construct(T* ptr, const A& allocator,
                               Args&&... args) noexcept;

  // 可后缀增补构造T(Args..., A)
  template <typename T, typename A, typename... Args,
            typename ::std::enable_if<Constructible<T, A, Args&&...>::SUFFIXED,
                                      int>::type = 0>
  inline static void construct(T* ptr, const A& allocator,
                               Args&&... args) noexcept;

  // 不可增补，直接构造T(Args...)
  template <
      typename T, typename A, typename... Args,
      typename ::std::enable_if<!Constructible<T, A, Args...>::USES_ALLOCATOR &&
                                    !PairConstructible<T, A, Args...>::value,
                                int>::type = 0>
  inline static void construct(T* ptr, A allocator, Args&&... args) noexcept;

  // 特化支持std::pair()
  template <typename P, typename A,
            typename =
                typename ::std::enable_if<PairConstructible<P, A>::value>::type>
  inline static void construct(P* pair, A allocator) noexcept;

  // 特化支持std::pair(const std::pair&)以及std::pair(std::pair&&)
  template <typename P, typename A, typename PP,
            typename ::std::enable_if<
                PairCopyOrMoveConstructible<P, A, PP>::value, int>::type = 0>
  inline static void construct(P* pair, A allocator, PP&& other) noexcept;

  // 特化支持原本不存在的std::pair(F)，F仅用来构造pair.first
  // 这个特化主要能够方便类似::std::map中的emplace的实现
  template <
      typename P, typename A, typename FF,
      typename ::std::enable_if<!PairCopyOrMoveConstructible<P, A, FF>::value &&
                                    PairConstructible<P, A, FF>::value,
                                int>::type = 0>
  inline static void construct(P* pair, A allocator, FF&& first) noexcept;

  // 特化支持std::pair(F, S)
  template <typename P, typename A, typename FF, typename SS,
            typename = typename ::std::enable_if<
                PairConstructible<P, A, FF, SS>::value>::type>
  inline static void construct(P* pair, A allocator, FF&& first,
                               SS&& second) noexcept;

  // 特化支持std::pair(piecewise_construct_t, tuple, tuple)
  template <typename P, typename A, typename... FArgs, typename... SArgs,
            typename = typename ::std::enable_if<PairConstructible<
                P, A, ::std::piecewise_construct_t, ::std::tuple<FArgs...>,
                ::std::tuple<SArgs...>>::value>::type>
  inline static void construct(P* pair, A allocator,
                               ::std::piecewise_construct_t,
                               ::std::tuple<FArgs...> ftuple,
                               ::std::tuple<SArgs...> stuple) noexcept;
  ////////////////////////////////////////////////////////////////////////////

 private:
  template <typename T, typename... Args>
  struct AllocatorApplied;
  template <typename T, typename... Args>
  struct AllocatorSuffixed;
  template <typename T, typename... Args>
  struct AllocatorPrefixed;
  template <typename P, typename A, typename... Args>
  struct PairConstructible;
  template <typename P, typename A, typename... Args>
  struct PairCopyOrMoveConstructible;
  template <typename T>
  struct PairTraits;

  BABYLON_DECLARE_MEMBER_INVOCABLE(allocate, AllocatorChecker)

  // 功能类似std::apply(construct, tuple<ptr, A, Args...>)
  // 但是能够在低于-std=c++17的场景下使用
  template <typename T, typename A, typename... Args, size_t... I,
            typename = typename ::std::enable_if<
                Constructible<T, A, Args...>::value>::type>
  inline static void construct_from_tuple_with_indexes(
      T* ptr, A allocator, ::std::tuple<Args...>& args_tuple,
      ::absl::index_sequence<I...>) noexcept;
};

// 单调分配器
// 用来支持典型的零散申请，统一释放的使用模式
// T: 分配器的目标类型
// R: 分配器实际的内存分配执行者
//
// 基础用法可以直接使用MonotonicAllocator<T>
// 此时使用默认的R为单调内存资源MonotonicBufferResource
//
// 极简用法甚至可以直接使用MonotonicAllocator<>
// 利用rebind协议让容器转化到合适的T
// 这在T本身的书写复杂冗长的时候很有用
//
// 在只使用某个明确的内存资源子类时（最典型的是万能分配器SwissMemoryResource）
// 可以直接指定R为对应的子类，绕过虚函数机制进一步降低分配开销
//
// 通过偏特化方式，也可以支持非MonotonicBufferResource子类
// 但是也具备零散申请，统一释放类似特质的单调内存资源
// 在需要将通用Reusable容器，应用在某个业务自身设计的特化单调内存资源时
// 这个用法是一种简便的适配器模式
template <typename T = void, typename R = MonotonicBufferResource>
class MonotonicAllocator;

// 基础单调分配器
// 为了实现一个单调分配器，需要支持一些分配器的基础协议
// 这可以通过继承BasicMonotonicAllocator的方式来得到
//
// 典型用法为支持单调分配器的特化
// template <typename T>
// class MonotonicAllocator<T, CustumMemoryResource> : public
// BasicMonotonicAllocator<T, MonotonicAllocator<T, CustumMemoryResource>> {
//   ...
// };
//
// 特殊情况下，也支持非MonotonicAllocator类型使用
// template <typename T>
// class CustomAllocator<T> : public BasicMonotonicAllocator<T,
// CustomAllocator<T>> {
//   ...
// };
template <typename T, typename A>
class BasicMonotonicAllocator {
 private:
  // void&是非法类型，替换成char[0]&
  // using ValueTypeForReference = typename ::std::conditional<
  //    ::std::is_same<T, void>::value, char[0], T>::type;

  BABYLON_DECLARE_MEMBER_INVOCABLE(construct, ConstructInvocable)

  template <typename Alloc, typename V>
  struct Rebind {};

  template <template <typename, typename...> class AllocTemplate, typename V,
            typename U, typename... Args>
  struct Rebind<AllocTemplate<U, Args...>, V> {
    using type = AllocTemplate<V, Args...>;
  };

 public:
  // allocator必要的类型定义
  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  // using reference = ValueTypeForReference&;
  // using const_reference = const ValueTypeForReference&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  // allocator的rebind协议
  template <typename U>
  struct rebind {
    using other = typename Rebind<A, U>::type;
  };

  // 用于判定MonotonicAllocator<T, R>::construct(T*, Args...)是否可用
  // 对于UsesAllocatorConstructor::Constructible::value == true的场景默认可用
  // 实际的MonotonicAllocator<T, R>可以在此基础上进一步扩展
  // 例如支持protobuf message等其他容器的构造
  template <typename... Args>
  using Constructible = ConstructInvocable<A, T*, Args...>;

  // std::allocator_traits风格分配接口
  // 分配连续num个T实例的内存，此处占位用，应由实际子类实现
  inline pointer allocate() noexcept;
  inline pointer allocate(size_type num) noexcept;

  // 单调分配器不需要释放内存，固定为空实现
  inline void deallocate(T* ptr, size_type num) noexcept;

  // 构造接口默认支持uses-allocator construction协议
  template <typename U, typename... Args,
            typename = typename ::std::enable_if<
                UsesAllocatorConstructor::Constructible<
                    U, typename rebind<U>::other, Args...>::value>::type>
  inline void construct(U* ptr, Args&&... args) noexcept;

  // 析构接口默认标准实现一般无需修改
  template <typename U>
  inline void destroy(U* ptr) noexcept;

  // std::polymorphic_allocator风格分配接口
  // 分配不限定类型的nbytes内存，对齐到alignment
  // 此处占位用，应由实际子类实现
  inline void* allocate_bytes(size_t nbytes) noexcept;
  inline void* allocate_bytes(size_t nbytes, size_t alignment) noexcept;

  // std::polymorphic_allocator风格释放接口
  // 单调分配器不需要释放内存，固定为空实现
  inline void deallocate_bytes(void* ptr, size_t nbytes) noexcept;
  inline void deallocate_bytes(void* ptr, size_t nbytes,
                               size_t alignment) noexcept;

  // std::polymorphic_allocator风格跨类型分配接口
  // 默认rebind后转交allocate接口
  template <typename U>
  inline U* allocate_object() noexcept;
  template <typename U>
  inline U* allocate_object(size_t num) noexcept;

  // std::polymorphic_allocator风格跨类型释放接口
  // 单调分配器不需要释放内存，固定为空实现
  template <typename U>
  inline void deallocate_object(U* ptr, size_t num) noexcept;

  // std::polymorphic_allocator风格分配构造一体化接口
  // 默认实现组合allocate和construct完成
  template <typename U, typename... Args>
  inline U* new_object(Args&&... args) noexcept;

  // std::polymorphic_allocator风格析构释放一体化接口
  // 由于单调分配器不需要释放内存，默认实现等同于destroy
  template <typename U>
  inline void delete_object(U* ptr) noexcept;

  // MonotonicAllocator特有注册析构函数接口
  // 托管实例随内存资源重置时统一析构，应由实际子类实现
  // template <typename U>
  // inline void register_destructor(U* ptr) noexcept;

  // 集成allocate，construct和register_destructor动作的接口
  // 构造一个由底层内存池托管生命周期的实例
  template <typename... Args>
  inline T* create(Args&&... args) noexcept;

  // 使用其他类型进行create的接口
  template <typename U, typename... Args>
  inline U* create_object(Args&&... args) noexcept;

 protected:
  // 子类扩展更多construct接口时用到，用于判定是否基类已经支持
  // 即默认支持的UsesAllocatorConstructor协议
  template <typename U, typename... Args>
  using BaseConstructible =
      ConstructInvocable<BasicMonotonicAllocator<T, A>, U*, Args...>;

 private:
  template <typename U>
  inline typename rebind<U>::other rebind_to() noexcept;
};

// 单调分配器的默认实现
// M: 限定为单调内存资源MonotonicBufferResource的子类
//
// 最典型的情况是默认的MonotonicAllocator<T>
// 此时M == MonotonicBufferResource
// 由于MonotonicBufferResource支持子类多态
// 可以实现一套签名支持不同的单调分配器子类实现
// 类似std::pmr::polymorphic_allocator<T>
//
// 在不需要多态行为的情况下，可以明确指定一个子类实现
// MonotonicAllocator<T, Child>
// 此时可以绕过多态，直接使用Child的非虚函数进一步加速
template <typename T, typename M>
class MonotonicAllocator
    : public BasicMonotonicAllocator<T, MonotonicAllocator<T, M>> {
 private:
  using Base = BasicMonotonicAllocator<T, MonotonicAllocator<T, M>>;

 public:
  // libstdc++实现的std::string在一些特殊情况下对allocator的默认构造能力有依赖
  // - 使用旧ABI时，即-D_GLIBCXX_USE_CXX11_ABI=0
  // - 使用clang编译时
  // 特化支持一下，其他版本默认不提供避免误用
#if __GLIBCXX__ && (__clang__ || !_GLIBCXX_USE_CXX11_ABI)
  inline MonotonicAllocator() noexcept = default;
#endif // __GLIBCXX__ && (__clang__ || !_GLIBCXX_USE_CXX11_ABI)
  inline MonotonicAllocator(MonotonicAllocator&&) noexcept = default;
  inline MonotonicAllocator(const MonotonicAllocator&) noexcept = default;
  inline MonotonicAllocator& operator=(MonotonicAllocator&&) noexcept = default;
  inline MonotonicAllocator& operator=(const MonotonicAllocator&) noexcept =
      default;
  inline ~MonotonicAllocator() noexcept = default;

  // allocator的rebind协议
  template <typename U>
  inline MonotonicAllocator(const MonotonicAllocator<U, M>& other) noexcept;

  // 只是内存资源的轻量包装
  inline MonotonicAllocator(M& resource) noexcept;

  // 也提供std::pmr::polymorphic_allocator风格的指针构造接口
  inline MonotonicAllocator(M* resource) noexcept;

  // 提供std::pmr::polymorphic_allocator风格的内存资源获取接口
  inline M* resource() const noexcept;

  // std::allocator_traits风格分配接口
  inline T* allocate() noexcept;
  inline T* allocate(size_t num) noexcept;

  // std::pmr::polymorphic_allocator风格分配接口
  inline void* allocate_bytes(size_t nbytes) noexcept;
  inline void* allocate_bytes(size_t nbytes, size_t alignment) noexcept;

#if __cpp_lib_memory_resource >= 201603L
  // 支持pmr容器
  template <typename U, typename... Args,
            typename = typename ::std::enable_if<
                !UsesAllocatorConstructor::Constructible<
                    U, MonotonicAllocator<U, M>, Args...>::USES_ALLOCATOR &&
                UsesAllocatorConstructor::Constructible<
                    U, ::std::pmr::polymorphic_allocator<U>,
                    Args...>::USES_ALLOCATOR>::type>
  inline void construct(U* ptr, Args&&... args) noexcept;
#endif // __cpp_lib_memory_resource >= 201603L

  // 代理到默认实现
  template <
      typename U, typename... Args,
      typename ::std::enable_if<
          Base::template BaseConstructible<U, Args...>::value
#if __cpp_lib_memory_resource >= 201603L
              && !(!UsesAllocatorConstructor::Constructible<
                       U, MonotonicAllocator<U, M>, Args...>::USES_ALLOCATOR &&
                   UsesAllocatorConstructor::Constructible<
                       U, ::std::pmr::polymorphic_allocator<U>,
                       Args...>::USES_ALLOCATOR)
#endif // __cpp_lib_memory_resource >= 201603L
              ,
          int>::type = 0>
  inline void construct(U* ptr, Args&&... args) noexcept;

  template <typename U>
  inline void register_destructor(U* ptr) noexcept;

 private:
  static_assert(::std::is_base_of<MonotonicBufferResource, M>::value,
                "default implement support MonotonicBufferResource only");

  M* _resource {nullptr};
};
template <typename T = void>
using ExclusiveAllocator =
    MonotonicAllocator<T, ExclusiveMonotonicBufferResource>;
template <typename T = void>
using SharedAllocator = MonotonicAllocator<T, SharedMonotonicBufferResource>;
template <typename T = void>
using SwissAllocator = MonotonicAllocator<T, SwissMemoryResource>;

// 特化支持SwissMemoryResource，实现stl和protobuf的统一支持
template <typename T>
class MonotonicAllocator<T, SwissMemoryResource>
    : public BasicMonotonicAllocator<T, SwissAllocator<T>> {
 private:
  using Base = BasicMonotonicAllocator<T, SwissAllocator<T>>;

 public:
  // libstdc++实现的std::string在一些特殊情况下对allocator的默认构造能力有依赖
  // - 使用旧ABI时，即-D_GLIBCXX_USE_CXX11_ABI=0
  // - 使用clang编译时
  // 特化支持一下，其他版本默认不提供避免误用
#if __GLIBCXX__ && (__clang__ || !_GLIBCXX_USE_CXX11_ABI)
  inline MonotonicAllocator() noexcept = default;
#endif // __GLIBCXX__ && (__clang__ || !_GLIBCXX_USE_CXX11_ABI)
  inline MonotonicAllocator(MonotonicAllocator&&) noexcept = default;
  inline MonotonicAllocator(const MonotonicAllocator&) noexcept = default;
  inline MonotonicAllocator& operator=(MonotonicAllocator&&) noexcept = default;
  inline MonotonicAllocator& operator=(const MonotonicAllocator&) noexcept =
      default;
  inline ~MonotonicAllocator() noexcept = default;

  // allocator的rebind协议
  template <typename U>
  inline MonotonicAllocator(const SwissAllocator<U>& other) noexcept;

  // 只是内存资源的轻量包装
  inline MonotonicAllocator(SwissMemoryResource& resource) noexcept;

  // 提供类似std::pmr::polymorphic_allocator的指针构造接口，便于支持pmr容器
  inline MonotonicAllocator(SwissMemoryResource* resource) noexcept;

  // 提供std::pmr::polymorphic_allocator风格的内存资源获取接口
  inline SwissMemoryResource* resource() const noexcept;

  // std::allocator_traits风格分配接口
  inline T* allocate() noexcept;
  inline T* allocate(size_t num) noexcept;

  // std::pmr::polymorphic_allocator风格分配接口
  inline void* allocate_bytes(size_t nbytes) noexcept;
  inline void* allocate_bytes(size_t nbytes, size_t alignment) noexcept;

#if GOOGLE_PROTOBUF_VERSION >= 3000000
  // 支持protobuf::Message
  template <
      typename U,
      typename = typename ::std::enable_if<
          ::google::protobuf::Arena::is_arena_constructable<U>::value>::type>
  inline void construct(U* ptr);

  template <
      typename U, typename V,
      typename = typename ::std::enable_if<
          ::google::protobuf::Arena::is_arena_constructable<U>::value>::type>
  inline void construct(U* ptr, V&& other);
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

  // protobuf::Message对于Arena构造版本不能调用析构函数，需要特判
  template <typename U>
  inline void destroy(U* ptr) noexcept;

#if __cpp_lib_memory_resource >= 201603L
  // 支持pmr容器
  template <typename U, typename... Args,
            typename = typename ::std::enable_if<
                !UsesAllocatorConstructor::Constructible<
                    U, MonotonicAllocator<U, SwissMemoryResource>,
                    Args...>::USES_ALLOCATOR &&
                UsesAllocatorConstructor::Constructible<
                    U, ::std::pmr::polymorphic_allocator<U>,
                    Args...>::USES_ALLOCATOR>::type>
  inline void construct(U* ptr, Args&&... args) {
    UsesAllocatorConstructor::construct(
        ptr, ::std::pmr::polymorphic_allocator<U>(this->_resource),
        ::std::forward<Args>(args)...);
  }
#endif // __cpp_lib_memory_resource >= 201603L

  // 代理到默认实现
  template <typename U, typename... Args,
            typename ::std::enable_if<
                Base::template BaseConstructible<U, Args...>::value
#if __cpp_lib_memory_resource >= 201603L
                    && !(!UsesAllocatorConstructor::Constructible<
                             U, MonotonicAllocator<U, SwissMemoryResource>,
                             Args...>::USES_ALLOCATOR &&
                         UsesAllocatorConstructor::Constructible<
                             U, ::std::pmr::polymorphic_allocator<U>,
                             Args...>::USES_ALLOCATOR)
#endif // __cpp_lib_memory_resource >= 201603L
                    ,
                int>::type = 0>
  inline void construct(U* ptr, Args&&... args) noexcept {
    Base::construct(ptr, ::std::forward<Args>(args)...);
  }

  template <typename U>
  inline void register_destructor(U* ptr) noexcept;

 private:
  SwissMemoryResource* _resource {nullptr};
};

BABYLON_NAMESPACE_END

namespace std {

template <typename CH, typename CT, typename U, typename A>
struct is_trivially_destructible<
    ::std::basic_string<CH, CT, ::babylon::MonotonicAllocator<U, A>>> {
  static constexpr bool value = true;
};
#if __cplusplus < 201703L
template <typename CH, typename CT, typename U, typename A>
constexpr bool is_trivially_destructible<
    ::std::basic_string<CH, CT, ::babylon::MonotonicAllocator<U, A>>>::value;
#endif // __cplusplus < 201703L

template <typename T, typename U, typename A>
struct is_trivially_destructible<
    ::std::vector<T, ::babylon::MonotonicAllocator<U, A>>> {
  static constexpr bool value = ::std::is_trivially_destructible<T>::value;
};
#if __cplusplus < 201703L
template <typename T, typename U, typename A>
constexpr bool is_trivially_destructible<
    ::std::vector<T, ::babylon::MonotonicAllocator<U, A>>>::value;
#endif // __cplusplus < 201703L

} // namespace std

#include "babylon/reusable/allocator.hpp"
