#pragma once

#include "babylon/reusable/allocator.h"

#include <memory> // std::allocator_traits

#if GOOGLE_PROTOBUF_VERSION >= 3000000
// protobuf为了避免用户错误地在非Arena内存上调用Arena版本的构造函数
// 将其设置了protected访问控制，并且从3.12开始将protoc生成的Message子类设置为final
// 但是为了实现SwissAllocator的兼容分配，需要在已经分配好的内存上调用Arena版本的构造函数
// 这里利用了Message子类对google::protobuf::Arena::InternalHelper<T>的friend声明来绕过访问了控制
// 采用了NeverUsed这个内部定义类型来特化GenericTypeHandler，确保这个特化不会产生副作用
template <>
class google::protobuf::Arena::InternalHelper<::babylon::NeverUsed> {
 public:
  template <typename T, typename... Args>
  inline static void construct(T* ptr, ::google::protobuf::Arena* arena,
                               Args&&... args) {
    new (ptr) T {arena, ::std::forward<Args>(args)...};
  }
};
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

BABYLON_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::Constructible begin
template <typename T, typename A, typename... Args>
class UsesAllocatorConstructor::Constructible {
 private:
  // 元素可能自身带cv修饰，但是构造时没有差别
  using U = typename ::std::remove_cv<T>::type;

  // 明确rebind到对应的类型，之后再使用std::uses_allocator来判定
  // 兼容采用特化而非定义allocator_type的方式
  using AA = typename ::std::allocator_traits<A>::template rebind_alloc<U>;

  // 满足前缀增补协议
  static constexpr bool PREFIXED =
      ::std::uses_allocator<U, AA>::value
      // Args本身并未满足协议
      && !AllocatorApplied<U, Args...>::value
      // 前置增补后可构造
      && ::std::is_constructible<U, ::std::allocator_arg_t, AA, Args...>::value;

  // 满足后缀增补协议
  static constexpr bool SUFFIXED =
      ::std::uses_allocator<U, AA>::value
      // Args本身并未满足协议
      && !AllocatorApplied<U, Args...>::value
      // 前置增补无效
      && !::std::is_constructible<U, ::std::allocator_arg_t, AA, Args...>::value
      // 后置增补后可构造
      && ::std::is_constructible<U, Args..., AA>::value;

 public:
  // 满足协议
  static constexpr bool USES_ALLOCATOR =
      // 满足前缀或者后缀协议
      PREFIXED ||
      SUFFIXED
      // 或者满足std::pair特化协议
      || PairConstructible<U, AA, Args...>::USES_ALLOCATOR ||
      PairCopyOrMoveConstructible<U, AA, Args...>::USES_ALLOCATOR;

  // 满足协议或者可以直接构造
  static constexpr bool value =
      USES_ALLOCATOR || PairConstructible<U, AA, Args...>::value ||
      PairCopyOrMoveConstructible<U, AA, Args...>::value ||
      ::std::is_constructible<U, Args...>::value;

  friend class UsesAllocatorConstructor;
};
#if __cplusplus < 201703L
template <typename T, typename A, typename... Args>
constexpr bool
    UsesAllocatorConstructor::Constructible<T, A, Args...>::USES_ALLOCATOR;
template <typename T, typename A, typename... Args>
constexpr bool UsesAllocatorConstructor::Constructible<T, A, Args...>::value;
#endif // __cplusplus < 201703L

// 特化NeverUsed对任意A都不满足协议
// std::pair特化中提取类型失败时通过NeverUsed表达
// 提供这个特化后可以统一处理
template <typename A, typename... Args>
struct UsesAllocatorConstructor::Constructible<NeverUsed, A, Args...> {
  static constexpr bool PREFIXED = false;
  static constexpr bool SUFFIXED = false;
  static constexpr bool USES_ALLOCATOR = false;
  static constexpr bool value = false;
};
#if __cplusplus < 201703L
template <typename A, typename... Args>
constexpr bool UsesAllocatorConstructor::Constructible<NeverUsed, A,
                                                       Args...>::USES_ALLOCATOR;
template <typename A, typename... Args>
constexpr bool
    UsesAllocatorConstructor::Constructible<NeverUsed, A, Args...>::value;
#endif // __cplusplus < 201703L
// UsesAllocatorConstructor::Constructible end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor begin
template <typename U, typename A, typename... Args,
          typename ::std::enable_if<UsesAllocatorConstructor::Constructible<
                                        U, A, Args&&...>::PREFIXED,
                                    int>::type>
inline void UsesAllocatorConstructor::construct(U* ptr, const A& allocator,
                                                Args&&... args) noexcept {
  new (const_cast<void*>(reinterpret_cast<const void*>(ptr)))
      U(::std::allocator_arg, allocator, ::std::forward<Args>(args)...);
}

template <typename U, typename A, typename... Args,
          typename ::std::enable_if<UsesAllocatorConstructor::Constructible<
                                        U, A, Args&&...>::SUFFIXED,
                                    int>::type>
inline void UsesAllocatorConstructor::construct(U* ptr, const A& allocator,
                                                Args&&... args) noexcept {
  new (const_cast<void*>(reinterpret_cast<const void*>(ptr)))
      U(::std::forward<Args>(args)..., allocator);
}

template <
    typename U, typename A, typename... Args,
    typename ::std::enable_if<
        !UsesAllocatorConstructor::Constructible<U, A,
                                                 Args...>::USES_ALLOCATOR &&
            !UsesAllocatorConstructor::PairConstructible<U, A, Args...>::value,
        int>::type>
inline void UsesAllocatorConstructor::construct(U* ptr, A,
                                                Args&&... args) noexcept {
  new (const_cast<void*>(reinterpret_cast<const void*>(ptr)))
      U(::std::forward<Args>(args)...);
}

template <typename P, typename A, typename>
inline void UsesAllocatorConstructor::construct(P* pair, A allocator) noexcept {
  construct(&pair->first, allocator);
  construct(&pair->second, allocator);
}

template <
    typename P, typename A, typename PP,
    typename ::std::enable_if<
        UsesAllocatorConstructor::PairCopyOrMoveConstructible<P, A, PP>::value,
        int>::type>
inline void UsesAllocatorConstructor::construct(P* pair, A allocator,
                                                PP&& other) noexcept {
  construct(&pair->first, allocator,
            ::std::forward<typename PairTraits<PP>::FirstType>(other.first));
  construct(&pair->second, allocator,
            ::std::forward<typename PairTraits<PP>::SecondType>(other.second));
}

template <typename P, typename A, typename FF,
          typename ::std::enable_if<
              !UsesAllocatorConstructor::PairCopyOrMoveConstructible<
                  P, A, FF>::value &&
                  UsesAllocatorConstructor::PairConstructible<P, A, FF>::value,
              int>::type>
inline void UsesAllocatorConstructor::construct(P* pair, A allocator,
                                                FF&& first) noexcept {
  construct(&pair->first, allocator, ::std::forward<FF>(first));
  construct(&pair->second, allocator);
}

template <typename P, typename A, typename FF, typename SS, typename>
inline void UsesAllocatorConstructor::construct(P* pair, A allocator,
                                                FF&& first,
                                                SS&& second) noexcept {
  construct(&pair->first, allocator, ::std::forward<FF>(first));
  construct(&pair->second, allocator, ::std::forward<SS>(second));
}

template <typename P, typename A, typename... FArgs, typename... SArgs,
          typename>
inline void UsesAllocatorConstructor::construct(
    P* pair, A allocator, ::std::piecewise_construct_t,
    ::std::tuple<FArgs...> ftuple, ::std::tuple<SArgs...> stuple) noexcept {
  construct_from_tuple_with_indexes(
      &pair->first, allocator, ftuple,
      ::absl::make_index_sequence<sizeof...(FArgs)> {});
  construct_from_tuple_with_indexes(
      &pair->second, allocator, stuple,
      ::absl::make_index_sequence<sizeof...(SArgs)> {});
}

template <typename T, typename A, typename... Args, size_t... I, typename>
inline void UsesAllocatorConstructor::construct_from_tuple_with_indexes(
    T* ptr, A allocator, ::std::tuple<Args...>& args_tuple,
    ::absl::index_sequence<I...>) noexcept {
  construct(ptr, allocator, ::std::forward<Args>(::std::get<I>(args_tuple))...);
}
// UsesAllocatorConstructor end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::AllocatorApplied begin
// 判断是否已经满足了前置或者后置协议，已经满足则无需增补
template <typename T, typename... Args>
struct UsesAllocatorConstructor::AllocatorApplied {
  static constexpr bool value = AllocatorPrefixed<T, Args...>::value ||
                                AllocatorSuffixed<T, Args...>::value;
};
// UsesAllocatorConstructor::AllocatorApplied end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::AllocatorPrefixed begin
// 缺省不满足前置协议
template <typename T, typename... Args>
struct UsesAllocatorConstructor::AllocatorPrefixed {
  static constexpr bool value = false;
};

// 双参数以上，看是否满足前缀特性
template <typename T, typename TAG, typename A, typename... Args>
struct UsesAllocatorConstructor::AllocatorPrefixed<T, TAG, A, Args...> {
  static constexpr bool value =
      // 第一个参数是std::allocator_arg_t
      ::std::is_convertible<TAG, ::std::allocator_arg_t>::value
      // 第二个参数和T满足协议，且具有allocate函数
      && ::std::uses_allocator<T, A>::value &&
      AllocatorChecker<A, size_t>::value;
};
// UsesAllocatorConstructor::AllocatorPrefixed end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::AllocatorSuffixed begin
// 缺省不满足后缀协议
template <typename T, typename... Args>
struct UsesAllocatorConstructor::AllocatorSuffixed {
  static constexpr bool value = false;
};

// 唯一参数情况下检测是否是满足协议的allocator
template <typename T, typename A>
struct UsesAllocatorConstructor::AllocatorSuffixed<T, A> {
  // 唯一参数和T满足协议，且具有allocate函数
  static constexpr bool value =
      ::std::uses_allocator<T, A>::value && AllocatorChecker<A, size_t>::value;
};

// 递归去掉前面的参数，直到终止在最后一个参数
// 再通过唯一参数版本检测是否是满足协议的allocator
template <typename T, typename A, typename... Args>
struct UsesAllocatorConstructor::AllocatorSuffixed<T, A, Args...> {
  static constexpr bool value = AllocatorSuffixed<T, Args...>::value;
};
// UsesAllocatorConstructor::AllocatorSuffixed end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::PairConstructible begin
// 缺省判定为不可构造，后面对每个符合协议的场景进行特化
template <typename P, typename A, typename... Args>
struct UsesAllocatorConstructor::PairConstructible {
  // 是否符合协议
  static constexpr bool USES_ALLOCATOR = false;
  // 符合协议，或者可以直接构造
  static constexpr bool value = false;
};

// 无参数构造
// 如果F()/S()任意一个符合协议，则整体符合协议
template <typename P, typename A>
struct UsesAllocatorConstructor::PairConstructible<P, A> {
  using PP = typename ::std::remove_cv<P>::type;
  using F = typename PairTraits<PP>::FirstType;
  using S = typename PairTraits<PP>::SecondType;
  static constexpr bool value =
      Constructible<F, A>::value && Constructible<S, A>::value;
  static constexpr bool USES_ALLOCATOR =
      value && (Constructible<F, A>::USES_ALLOCATOR ||
                Constructible<S, A>::USES_ALLOCATOR);
};

// 单参数构造
// 如果F(FF)/S任意一个符合协议，则整体符合协议
template <typename P, typename A, typename FF>
struct UsesAllocatorConstructor::PairConstructible<P, A, FF> {
  using F = typename PairTraits<typename ::std::remove_cv<P>::type>::FirstType;
  using S = typename PairTraits<typename ::std::remove_cv<P>::type>::SecondType;
  static constexpr bool value =
      Constructible<F, A, FF>::value && Constructible<S, A>::value;
  static constexpr bool USES_ALLOCATOR =
      value && (Constructible<F, A, FF>::USES_ALLOCATOR ||
                Constructible<S, A>::USES_ALLOCATOR);
};

// 双参数构造
// 如果F(FF)/S(SS)任意一个符合协议，则整体符合协议
template <typename P, typename A, typename FF, typename SS>
struct UsesAllocatorConstructor::PairConstructible<P, A, FF, SS> {
  using F = typename PairTraits<typename ::std::remove_cv<P>::type>::FirstType;
  using S = typename PairTraits<typename ::std::remove_cv<P>::type>::SecondType;
  static constexpr bool value =
      Constructible<F, A, FF>::value && Constructible<S, A, SS>::value;
  static constexpr bool USES_ALLOCATOR =
      value && (Constructible<F, A, FF>::USES_ALLOCATOR ||
                Constructible<S, A, SS>::USES_ALLOCATOR);
};

// 双tuple构造
// 如果F(FArgs...)/S(SArgs...)任意一个符合协议，则整体符合协议
template <typename P, typename A, typename... FArgs, typename... SArgs>
struct UsesAllocatorConstructor::PairConstructible<
    P, A, ::std::piecewise_construct_t, ::std::tuple<FArgs...>,
    ::std::tuple<SArgs...>> {
  using F = typename PairTraits<typename ::std::remove_cv<P>::type>::FirstType;
  using S = typename PairTraits<typename ::std::remove_cv<P>::type>::SecondType;
  static constexpr bool value = Constructible<F, A, FArgs...>::value &&
                                Constructible<S, A, SArgs...>::value;
  static constexpr bool USES_ALLOCATOR =
      value && (Constructible<F, A, FArgs...>::USES_ALLOCATOR ||
                Constructible<S, A, SArgs...>::USES_ALLOCATOR);
};
// UsesAllocatorConstructor::PairConstructible end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::PairCopyOrMoveConstructible begin
// 通过std::pair构造std::pair的场景，和单参数构造区分开
// 缺省判定为不可构造，后面对符合协议的场景进行特化
template <typename P, typename A, typename... Args>
struct UsesAllocatorConstructor::PairCopyOrMoveConstructible {
  static constexpr bool value = false;
  static constexpr bool USES_ALLOCATOR = false;
};

// 单一参数PP，且能够进行std::pair分解
template <typename P, typename A, typename PP>
struct UsesAllocatorConstructor::PairCopyOrMoveConstructible<P, A, PP> {
  using F = typename PairTraits<typename ::std::remove_cv<P>::type>::FirstType;
  using S = typename PairTraits<typename ::std::remove_cv<P>::type>::SecondType;
  using FF = typename PairTraits<PP>::FirstType;
  using SS = typename PairTraits<PP>::SecondType;
  // 尝试检查是否可以分解后对位满足协议
  static constexpr bool value =
      Constructible<F, A, FF>::value && Constructible<S, A, SS>::value;
  static constexpr bool USES_ALLOCATOR =
      value && (Constructible<F, A, FF>::USES_ALLOCATOR ||
                Constructible<S, A, SS>::USES_ALLOCATOR);
};
// UsesAllocatorConstructor::PairCopyOrMoveConstructible end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// UsesAllocatorConstructor::PairTraits begin
// 提取std::pair<F, S>类型中的F和S
// 缺省实现表示非std::pair类型，无法提取
template <typename P>
struct UsesAllocatorConstructor::PairTraits {
  using FirstType = NeverUsed;
  using SecondType = NeverUsed;
};

template <typename F, typename S>
struct UsesAllocatorConstructor::PairTraits<::std::pair<F, S>> {
  using FirstType = F;
  using SecondType = S;
};

template <typename F, typename S>
struct UsesAllocatorConstructor::PairTraits<::std::pair<F, S>&&> {
  using FirstType = F&&;
  using SecondType = S&&;
};

template <typename F, typename S>
struct UsesAllocatorConstructor::PairTraits<::std::pair<F, S>&> {
  using FirstType = F&;
  using SecondType = S&;
};

template <typename F, typename S>
struct UsesAllocatorConstructor::PairTraits<const ::std::pair<F, S>&> {
  using FirstType = const F&;
  using SecondType = const S&;
};
// UsesAllocatorConstructor::PairTraits end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// BaseMonotonicAllocator<T, A> begin
template <typename T, typename A>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T*
BasicMonotonicAllocator<T, A>::allocate() noexcept {
  auto& allocator = static_cast<A&>(*this);
  return ::std::allocator_traits<A>::allocate(allocator, 1);
}

template <typename T, typename A>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
BasicMonotonicAllocator<T, A>::deallocate(T*, size_type) noexcept {}

template <typename T, typename A>
template <typename U, typename... Args, typename>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
BasicMonotonicAllocator<T, A>::construct(U* ptr, Args&&... args) noexcept {
  UsesAllocatorConstructor::construct(ptr, rebind_to<U>(),
                                      ::std::forward<Args>(args)...);
}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void BasicMonotonicAllocator<T, A>::destroy(
    U* ptr) noexcept {
  // TODO(lijiang01): 由于单调分配器是不需要执行释放动作的，一个容器类型
  //                  无需通过析构来释放自身空间，如果其元素也不需要析构的话
  //                  那整个实例的析构动作就是不必要的
  //                  但是从简化通用书写方式的角度，容器一般还是会定义析构函数
  //                  这里约定容器根据元素实际情况
  //                  采用特化std::is_trivially_destructible的方式来表达
  //                  即便我定义了析构函数，但是无需被调用的含义
  //
  //                  尽管复用std::is_trivially_destructible可以天然支持本就
  //                  无需析构的简单类型，而且实践角度语义一致，也不容易引起误用
  //                  但是考虑标准约定std::is_trivially_destructible是不应该被特化的
  //                  后续还是需要改造成一个专用的萃取模板
  if (!::std::is_trivially_destructible<U>::value) {
    ptr->~U();
  }
}

template <typename T, typename A>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
BasicMonotonicAllocator<T, A>::deallocate_bytes(void*, size_t,
                                                size_t) noexcept {}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline U*
BasicMonotonicAllocator<T, A>::allocate_object() noexcept {
  return rebind_to<U>().allocate();
}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline U*
BasicMonotonicAllocator<T, A>::allocate_object(size_t num) noexcept {
  return rebind_to<U>().allocate(num);
}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
BasicMonotonicAllocator<T, A>::deallocate_object(U*, size_t) noexcept {}

template <typename T, typename A>
template <typename U, typename... Args>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline U*
BasicMonotonicAllocator<T, A>::new_object(Args&&... args) noexcept {
  auto allocator = rebind_to<U>();
  auto ptr = allocator.allocate();
  allocator.construct(ptr, ::std::forward<Args>(args)...);
  return ptr;
}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
BasicMonotonicAllocator<T, A>::delete_object(U* ptr) noexcept {
  auto& allocator = static_cast<A&>(*this);
  allocator.destroy(ptr);
}

template <typename T, typename A>
template <typename... Args>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T* BasicMonotonicAllocator<T, A>::create(
    Args&&... args) noexcept {
  auto& allocator = static_cast<A&>(*this);
  auto ptr = allocator.template new_object<T>(::std::forward<Args>(args)...);
  allocator.register_destructor(ptr);
  return ptr;
}

template <typename T, typename A>
template <typename U, typename... Args>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline U*
BasicMonotonicAllocator<T, A>::create_object(Args&&... args) noexcept {
  return rebind_to<U>().create(::std::forward<Args>(args)...);
}

template <typename T, typename A>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline
    typename BasicMonotonicAllocator<T, A>::template rebind<U>::other
    BasicMonotonicAllocator<T, A>::rebind_to() noexcept {
  using TA = ::std::allocator_traits<A>;
  return typename TA::template rebind_alloc<U> {*static_cast<A*>(this)};
}
// BasicMonotonicAllocator<T, A> end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// MonotonicAllocator<T, M> begin
template <typename T, typename M>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline MonotonicAllocator<
    T, M>::MonotonicAllocator(const MonotonicAllocator<U, M>& other) noexcept
    : MonotonicAllocator {other.resource()} {}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline MonotonicAllocator<
    T, M>::MonotonicAllocator(M& resource) noexcept
    : _resource {&resource} {}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline MonotonicAllocator<
    T, M>::MonotonicAllocator(M* resource) noexcept
    : _resource {resource} {}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline M* MonotonicAllocator<T, M>::resource()
    const noexcept {
  return _resource;
}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T*
MonotonicAllocator<T, M>::allocate() noexcept {
  return static_cast<T*>(_resource->template allocate<alignof(T)>(sizeof(T)));
}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T* MonotonicAllocator<T, M>::allocate(
    size_t num) noexcept {
  return static_cast<T*>(
      _resource->template allocate<alignof(T)>(sizeof(T) * num));
}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void*
MonotonicAllocator<T, M>::allocate_bytes(size_t nbytes) noexcept {
  return allocate_bytes(nbytes, alignof(::max_align_t));
}

template <typename T, typename M>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void*
MonotonicAllocator<T, M>::allocate_bytes(size_t nbytes,
                                         size_t alignment) noexcept {
  return _resource->allocate(nbytes, alignment);
}

#if __cpp_lib_memory_resource >= 201603L
template <typename T, typename M>
template <typename U, typename... Args, typename>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void MonotonicAllocator<T, M>::construct(
    U* ptr, Args&&... args) noexcept {
  ::std::pmr::polymorphic_allocator<U> allocator {_resource};
  UsesAllocatorConstructor::construct(ptr, allocator,
                                      ::std::forward<Args>(args)...);
}
#endif // __cpp_lib_memory_resource >= 201603L

template <typename T, typename M>
template <
    typename U, typename... Args,
    typename ::std::enable_if<
        BasicMonotonicAllocator<T, MonotonicAllocator<T, M>>::
                template BaseConstructible<U, Args...>::value
#if __cpp_lib_memory_resource >= 201603L
            && !(!UsesAllocatorConstructor::Constructible<
                     U, MonotonicAllocator<U, M>, Args...>::USES_ALLOCATOR &&
                 UsesAllocatorConstructor::Constructible<
                     U, ::std::pmr::polymorphic_allocator<U>,
                     Args...>::USES_ALLOCATOR)
#endif // __cpp_lib_memory_resource >= 201603L
            ,
        int>::type>
inline void MonotonicAllocator<T, M>::construct(U* ptr,
                                                Args&&... args) noexcept {
  Base::construct(ptr, ::std::forward<Args>(args)...);
}

template <typename T, typename M>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void
MonotonicAllocator<T, M>::register_destructor(U* ptr) noexcept {
  _resource->register_destructor(ptr);
}

template <typename T, typename U, typename M>
inline bool operator==(MonotonicAllocator<T, M> left,
                       MonotonicAllocator<U, M> right) noexcept {
  return left.resource() == right.resource();
}

template <typename T, typename U, typename M>
inline bool operator!=(MonotonicAllocator<T, M> left,
                       MonotonicAllocator<U, M> right) noexcept {
  return !(left == right);
}
// MonotonicAllocator<T, M> end
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// MonotonicAllocator<T, SwissMemoryResource> begin
template <typename T>
template <typename U>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline SwissAllocator<T>::MonotonicAllocator(
    const SwissAllocator<U>& other) noexcept
    : MonotonicAllocator {other.resource()} {}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline SwissAllocator<T>::MonotonicAllocator(
    SwissMemoryResource& resource) noexcept
    : _resource {&resource} {}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline SwissAllocator<T>::MonotonicAllocator(
    SwissMemoryResource* resource) noexcept
    : MonotonicAllocator {*resource} {}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline SwissMemoryResource*
SwissAllocator<T>::resource() const noexcept {
  return _resource;
}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T* SwissAllocator<T>::allocate() noexcept {
  return static_cast<T*>(_resource->template allocate<alignof(T)>(sizeof(T)));
}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline T* SwissAllocator<T>::allocate(
    size_t num) noexcept {
  return static_cast<T*>(
      _resource->template allocate<alignof(T)>(sizeof(T) * num));
}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void* SwissAllocator<T>::allocate_bytes(
    size_t nbytes) noexcept {
  return allocate_bytes(nbytes, alignof(::max_align_t));
}

template <typename T>
ABSL_ATTRIBUTE_ALWAYS_INLINE inline void* SwissAllocator<T>::allocate_bytes(
    size_t nbytes, size_t alignment) noexcept {
  return _resource->allocate(nbytes, alignment);
}

#if GOOGLE_PROTOBUF_VERSION >= 3000000
template <typename T>
template <typename U, typename>
inline void SwissAllocator<T>::construct(U* ptr) {
  ::google::protobuf::Arena& arena = *this->resource();
  ::google::protobuf::Arena::InternalHelper<NeverUsed>::construct(ptr, &arena);
}

template <typename T>
template <typename U, typename V, typename>
inline void SwissAllocator<T>::construct(U* ptr, V&& other) {
  ::google::protobuf::Arena& arena = *this->resource();
  ::google::protobuf::Arena::InternalHelper<NeverUsed>::construct(ptr, &arena);
  *ptr = ::std::forward<V>(other);
}
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

template <typename T>
template <typename U>
inline void SwissAllocator<T>::destroy(U* ptr) noexcept {
#if GOOGLE_PROTOBUF_VERSION >= 3000000
  if (::google::protobuf::Arena::is_arena_constructable<U>::value &&
      ::google::protobuf::Arena::is_destructor_skippable<U>::value) {
    return;
  }
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
  Base::destroy(ptr);
}

template <typename T>
template <typename U>
inline void SwissAllocator<T>::register_destructor(U* ptr) noexcept {
#if GOOGLE_PROTOBUF_VERSION >= 3000000
  if (::google::protobuf::Arena::is_arena_constructable<U>::value &&
      ::google::protobuf::Arena::is_destructor_skippable<U>::value) {
    return;
  }
#endif // GOOGLE_PROTOBUF_VERSION >= 3000000
  _resource->register_destructor(ptr);
}
// MonotonicAllocator<T, SwissMemoryResource> end
//////////////////////////////////////////////////////////////////////////////

BABYLON_NAMESPACE_END
