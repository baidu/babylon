#pragma once

#include <babylon/reusable/allocator.h> // babylon::MonotonicAllocator
#include <babylon/string.h>             // babylon::stable_reserve
#include <babylon/type_traits.h>        // BABYLON_MEMBER_INVOCABLE_CHECKER

BABYLON_NAMESPACE_BEGIN

template <typename T, typename E = void>
class ReusableTraits;
template <typename T>
class BasicReusableTraits;

class Reuse {
 private:
  // stl风格的清理函数
  BABYLON_MEMBER_INVOCABLE_CHECKER(clear, ClearInvocable)
  // protobuf风格的清理函数
  BABYLON_MEMBER_INVOCABLE_CHECKER(Clear, ClearUpperInvocable)
  // 赋值操作符
  BABYLON_MEMBER_INVOCABLE_CHECKER(operator=, OpEqInvocable)
  // stl风格的赋值函数
  BABYLON_MEMBER_INVOCABLE_CHECKER(assign, AssignInvocable)
  // 检查T::AllocationMetadata是否定义
  BABYLON_TYPE_DEFINED_CHECKER(AllocationMetadata, AllocationMetadataDefined)

 public:
  template <typename T>
  using AllocationMetadata = typename ReusableTraits<T>::AllocationMetadata;

  // 实现『保留容量』的重用能力
  // 功能上等价于采用args构造新实例
  // 但区别是，需要依赖一个已经构造好的实例
  // 并会重用实例中已经申请过的资源
  //
  // 需要明确传入实例使用的allocator
  // 对于未使用allocator机制构造的实例，可以传入std::allocator
  template <typename T, typename A, typename... Args>
  static void reconstruct(T& value, A allocator, Args&&... args) noexcept {
    ReusableTraits<T>::reconstruct(value, allocator,
                                   ::std::forward<Args>(args)...);
  }

  // 提取实例容量，更新到AllocationMetadata
  template <typename T, typename... Args>
  static void update_allocation_metadata(const T& value,
                                         AllocationMetadata<T>& meta) noexcept {
    ReusableTraits<T>::update_allocation_metadata(value, meta);
  }

  // 根据AllocationMetadata重建一个实例
  template <typename T, typename A, typename... Args>
  static void construct_with_allocation_metadata(
      T* ptr, A allocator, const AllocationMetadata<T>& meta) noexcept {
    ReusableTraits<T>::construct_with_allocation_metadata(ptr, allocator, meta);
  }

  // 根据AllocationMetadata重建一个实例
  // 包含了从allocator分配空间，以及生命周期托管
  // 语义接近Allocator::create_object
  template <typename T, typename A, typename... Args>
  static T* create_with_allocation_metadata(
      A allocator, const AllocationMetadata<T>& meta) noexcept {
    return ReusableTraits<T>::create_with_allocation_metadata(allocator, meta);
  }

 private:
  // 采用给定参数重新构造实例，逻辑签名为
  // call_reconstruct(T&, Args...)
  // 默认采用真正的『重新构造』实现，即
  // ~T()
  // T(Args...)
  //
  // 但是在可以快速重新构造的情况下，采用简化方法实现
  // 1、空参数，且T实现了clear/Clear函数
  // 2、单参数U，且T实现了T = U赋值函数
  // 3、多参数Args，且T实现了assign(Args...)函数
  template <typename T, typename A,
            typename ::std::enable_if<ClearInvocable<T>::value, int>::type = 0>
  static void call_reconstruct(T& value, A) noexcept {
    value.clear();
  }
  template <typename T, typename A,
            typename ::std::enable_if<!ClearInvocable<T>::value &&
                                          ClearUpperInvocable<T>::value,
                                      int>::type = 0>
  static void call_reconstruct(T& value, A) noexcept {
    value.Clear();
  }
  template <
      typename T, typename A, typename U,
      typename ::std::enable_if<OpEqInvocable<T, U>::value, int>::type = 0>
  static void call_reconstruct(T& value, A, U&& other) noexcept {
    value = ::std::forward<U>(other);
  }
  template <typename T, typename A, typename... Args,
            typename ::std::enable_if<!OpEqInvocable<T, Args...>::value &&
                                          AssignInvocable<T, Args...>::value,
                                      int>::type = 0>
  static void call_reconstruct(T& value, A, Args&&... args) noexcept {
    value.assign(::std::forward<Args>(args)...);
  }
  template <typename T, typename A, typename... Args,
            typename ::std::enable_if<!OpEqInvocable<T, Args...>::value &&
                                          !AssignInvocable<T, Args...>::value,
                                      int>::type = 0>
  static void call_reconstruct(T& value, A allocator, Args&&... args) noexcept {
    allocator.destroy(&value);
    allocator.construct(&value, ::std::forward<Args>(args)...);
  }

  template <typename T, typename E>
  friend class ReusableTraits;
  template <typename T>
  friend class BasicReusableTraits;
};

template <typename T>
class BasicReusableTraits;
template <typename T>
class BasicReusableTraits<ReusableTraits<T>> {
 public:
  // 是否可重用，可重用意味着
  // 1、可以进行『保留容量』的逻辑清空
  // 2、可以『提取容量』以及根据提取的容量『重建』一个实例
  // 结合这两个特性，可以实现结合内存池与对象池优势的实例重用
  //
  // 默认trivial类型可重用
  // 除此以外，自定义类型可以通过定义T::AllocationMetadata的类型标识为可重用
  // 定义的T::AllocationMetadata被用来实现提取容量的功能
  static constexpr bool REUSABLE = ::std::is_trivial<T>::value ||
                                   Reuse::AllocationMetadataDefined::value<T>();

  // 提取容量用到的辅助结构
  // 默认取T::AllocationMetadata
  // 如果未定义或者定义为void，定义为空结构ZeroSized
  using AllocationMetadata = typename ::std::conditional<
      Reuse::AllocationMetadataDefined::value<T>(),
      typename ::std::conditional<
          !::std::is_same<void,
                          Reuse::AllocationMetadataDefined::type<T>>::value,
          Reuse::AllocationMetadataDefined::type<T>, ZeroSized>::type,
      ZeroSized>::type;

  // 实现『保留容量』的重用能力
  // 功能上等价于采用args构造新实例
  // 但区别是，需要依赖一个已经构造好的实例
  // 并会重用实例中已经申请过的资源
  //
  // 需要明确传入实例使用的allocator
  // 对于未使用allocator机制构造的实例，可以传入std::allocator
  template <typename A, typename... Args>
  static void reconstruct(T& value, A allocator, Args&&... args) noexcept {
    Reuse::call_reconstruct(value, allocator, ::std::forward<Args>(args)...);
  }

  // 提取实例容量，更新到AllocationMetadata
  template <typename U = T,
            typename ::std::enable_if<
                (sizeof(typename ReusableTraits<U>::AllocationMetadata) > 0),
                int>::type = 0>
  static void update_allocation_metadata(
      const U& instance,
      typename ReusableTraits<U>::AllocationMetadata& meta) noexcept {
    instance.update_allocation_metadata(meta);
  }
  template <typename U = T,
            typename ::std::enable_if<
                (sizeof(typename ReusableTraits<U>::AllocationMetadata) == 0),
                int>::type = 0>
  static void update_allocation_metadata(
      const U&, typename ReusableTraits<U>::AllocationMetadata&) noexcept {}

  // 根据AllocationMetadata重建一个实例
  template <typename U = T, typename A,
            typename ::std::enable_if<
                (sizeof(typename ReusableTraits<U>::AllocationMetadata) > 0),
                int>::type = 0>
  static void construct_with_allocation_metadata(
      U* ptr, A allocator,
      const typename ReusableTraits<U, A>::AllocationMetadata& meta) noexcept {
    allocator.construct(ptr, meta);
  }
  template <typename U = T, typename A,
            typename ::std::enable_if<
                (sizeof(typename ReusableTraits<U>::AllocationMetadata) == 0),
                int>::type = 0>
  static void construct_with_allocation_metadata(
      U* ptr, A allocator,
      const typename ReusableTraits<U, A>::AllocationMetadata&) noexcept {
    allocator.construct(ptr);
  }

  // 根据AllocationMetadata重建一个实例
  // 包含了从allocator分配空间，以及生命周期托管
  // 语义接近Allocator::create_object
  template <typename TT = T, typename U, typename A>
  static TT* create_with_allocation_metadata(
      MonotonicAllocator<U, A> allocator,
      const typename ReusableTraits<TT>::AllocationMetadata& meta) noexcept {
    auto instance = allocator.template allocate_object<TT>();
    ReusableTraits<TT>::construct_with_allocation_metadata(instance, allocator,
                                                           meta);
    allocator.register_destructor(instance);
    return instance;
  }
};
#if __cplusplus < 201703L
template <typename T>
constexpr bool BasicReusableTraits<ReusableTraits<T>>::REUSABLE;
#endif // __cplusplus < 201703L

template <typename T, typename E>
class ReusableTraits : public BasicReusableTraits<ReusableTraits<T>> {};

////////////////////////////////////////////////////////////////////////////////
// std::basic_string begin
template <typename A>
class ReusableTraits<::std::basic_string<char, ::std::char_traits<char>, A>>
    : public BasicReusableTraits<ReusableTraits<
          ::std::basic_string<char, ::std::char_traits<char>, A>>> {
 private:
  using String = ::std::basic_string<char, ::std::char_traits<char>, A>;
  using Base = BasicReusableTraits<ReusableTraits<String>>;

 public:
  static constexpr bool REUSABLE = true;

  struct AllocationMetadata {
    size_t capacity {0};
  };

  static void update_allocation_metadata(const String& str,
                                         AllocationMetadata& meta) noexcept {
    meta.capacity = ::std::max(meta.capacity, str.capacity());
  }

  template <typename AA>
  static void construct_with_allocation_metadata(
      String* ptr, AA allocator, const AllocationMetadata& meta) noexcept {
    allocator.construct(ptr);
    stable_reserve(*ptr, meta.capacity);
  }
};
#if __cplusplus < 201703L
template <typename A>
constexpr bool ReusableTraits<
    ::std::basic_string<char, ::std::char_traits<char>, A>>::REUSABLE;
#endif // __cplusplus < 201703L
// std::basic_string end
////////////////////////////////////////////////////////////////////////////////

template <typename T, typename A>
class ReusableTraits<::std::vector<T, A>>
    : public BasicReusableTraits<ReusableTraits<::std::vector<T, A>>> {
 private:
  using Vector = ::std::vector<T, A>;
  using Base = BasicReusableTraits<ReusableTraits<Vector>>;

 public:
  static constexpr bool REUSABLE = ::std::is_trivial<T>::value;

  struct AllocationMetadata {
    size_t capacity {0};
  };

  static void update_allocation_metadata(const Vector& vec,
                                         AllocationMetadata& meta) noexcept {
    meta.capacity = ::std::max(meta.capacity, vec.capacity());
  }

  template <typename AA>
  static void construct_with_allocation_metadata(
      Vector* ptr, AA allocator, const AllocationMetadata& meta) noexcept {
    allocator.construct(ptr);
    ptr->reserve(meta.capacity);
  }
};
#if __cplusplus < 201703L
template <typename T, typename A>
constexpr bool ReusableTraits<::std::vector<T, A>>::REUSABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END

#include <babylon/reusable/message.h>
