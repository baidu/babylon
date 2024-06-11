#pragma once

#include "babylon/type_traits.h"

#include <memory> // std::unique_ptr

BABYLON_NAMESPACE_BEGIN

// 类似std::any可以用于存放任何对象，并支持按原类型取回
// 在std::any的基础上，进一步支持不可拷贝，不可移动构造的对象
// 同时支持引用构造，可以支持同构处理引用和实体对象
class Any {
 public:
  enum class Type : uint8_t {
    EMPTY = 0,
    INSTANCE,
    INT64,
    INT32,
    INT16,
    INT8,
    UINT64,
    UINT32,
    UINT16,
    UINT8,
    BOOLEAN,
    DOUBLE,
    FLOAT,
  };

  struct HolderType {
    constexpr static uint8_t NON_TRIVIAL = 0x01;
    constexpr static uint8_t NON_INPLACE = 0x02;
    constexpr static uint8_t REFERENCE = 0x04;
    constexpr static uint8_t CONST = 0x08;

    constexpr static uint8_t INPLACE_TRIVIAL = 0;
    constexpr static uint8_t INPLACE_NON_TRIVIAL = NON_TRIVIAL;
    constexpr static uint8_t INSTANCE = NON_TRIVIAL | NON_INPLACE;
    constexpr static uint8_t CONST_REFERENCE = NON_INPLACE | REFERENCE | CONST;
    constexpr static uint8_t MUTABLE_REFERENCE = NON_INPLACE | REFERENCE;
  };

  struct Descriptor {
    const Id& type_id;
    void (*const destructor)(void*) noexcept;
    void (*const deleter)(void*) noexcept;
    void (*const copy_constructor)(void*, const void*) noexcept;
    void* (*const copy_creater)(const void*) noexcept;
  };

  // 默认构造，后续可用 = 赋值
  inline Any() noexcept;
  // 拷贝构造，如果other中已经持有一个对象实体
  // 需要确保具有拷贝构造能力，否则会强制abort退出
  // 如果other中持有对象引用，则会构造一个相同的对象引用
  inline Any(const Any& other);
  // 单独声明实现非常量引用拷贝，避免被Any<T>(T&&)匹配
  inline Any(Any& other);
  // 移动构造函数，直接移动other到自身，不依赖持有对象的移动构造
  inline Any(Any&& other) noexcept;
  inline ~Any() noexcept;

  // 从对象指针移动构造，直接转移value的控制权，可支持不能移动构造的对象
  template <typename T>
  inline explicit Any(::std::unique_ptr<T>&& value) noexcept;

  // 从对象拷贝或移动构造，自身会持有对象本体的拷贝或移动构造副本
  template <typename T, typename ::std::enable_if<sizeof(uint64_t) < sizeof(T),
                                                  int>::type = 0>
  inline explicit Any(T&& value) noexcept;
  // 特化支持8字节以内小对象，内联存储
  template <typename T, typename ::std::enable_if<sizeof(T) <= sizeof(uint64_t),
                                                  int>::type = 0>
  inline explicit Any(T&& value) noexcept;

  // 对数值类型特化支持
  inline explicit Any(int64_t value) noexcept;
  inline explicit Any(int32_t value) noexcept;
  inline explicit Any(int16_t value) noexcept;
  inline explicit Any(int8_t value) noexcept;
  inline explicit Any(uint64_t value) noexcept;
  inline explicit Any(uint32_t value) noexcept;
  inline explicit Any(uint16_t value) noexcept;
  inline explicit Any(uint8_t value) noexcept;
  inline explicit Any(bool value) noexcept;
  inline explicit Any(double value) noexcept;
  inline explicit Any(float value) noexcept;
  inline explicit Any(::std::unique_ptr<int64_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<int32_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<int16_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<int8_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<uint64_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<uint32_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<uint16_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<uint8_t>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<bool>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<double>&& value) noexcept;
  inline explicit Any(::std::unique_ptr<float>&& value) noexcept;

  // Type Erased版本的构造接口
  // 使用者需要确保instance的实际类型与descriptor相匹配
  // 即，假如instance的实际类型为T
  // 那么descriptor == Any::descriptor<T>()
  // 调用后instance的生命周期会转移给any管理，等效于Typed版本的
  // Any any {std::unique_ptr<T>(instance)};
  Any(const Descriptor* descriptor, void* instance) noexcept;

  // 赋值函数使用对应的构造函数构建新实例
  // 之后将当前实例析构，并使用新实例覆盖
  template <typename T>
  inline Any& operator=(T&& other);
  // 出于编译器要求，拷贝赋值函数单独声明实现
  // 内部直接传导到模板类型的赋值函数
  inline Any& operator=(const Any& other);

  // Type Erased版本的赋值接口
  // 使用者需要确保instance的实际类型与descriptor相匹配
  // 即，假如instance的实际类型为T
  // 那么descriptor == Any::descriptor<T>()
  // 调用后instance的生命周期会转移给any管理，等效于Typed版本的
  // any = std::unique_ptr<T>(instance);
  Any& assign(const Descriptor* descriptor, void* instance) noexcept;

  // 引用对象，本身不会进行计数等生命周期维护
  // 可以像持有对象本体一样使用const get取值
  template <typename T>
  inline Any& cref(const T& value) noexcept;
  template <typename T>
  inline Any& ref(const T& value) noexcept;

  // 引用对象的非常量版，支持const/非const get取值
  template <typename T>
  inline Any& ref(T& value) noexcept;

  // Type Erased版本的赋值接口
  // 使用者需要确保instance的实际类型与descriptor相匹配
  // 即，假如instance的实际类型为T
  // 那么descriptor == Any::descriptor<T>()
  // 调用后any会引用instance，等效于Typed版本的
  // any.cref(instance);
  // any.ref(instance);
  Any& cref(const Descriptor* descriptor, const void* instance) noexcept;
  Any& ref(const Descriptor* descriptor, const void* instance) noexcept;
  Any& ref(const Descriptor* descriptor, void* instance) noexcept;

  // 重置，释放持有的本体，恢复到empty状态
  inline void clear() noexcept;

  // 取值，需要指定和存入时一致的类型才可取出
  // empty或者类型不对都会取到nullptr
  // 尝试获取const ref也会取到nullptr
  template <typename T>
  inline T* get() noexcept;
  inline void* get() noexcept;

  // 常量取值
  template <typename T, typename ::std::enable_if<sizeof(size_t) < sizeof(T),
                                                  int>::type = 0>
  inline const T* cget() const noexcept;
  // 特化支持小对象内联存储
  template <typename T, typename ::std::enable_if<sizeof(T) <= sizeof(size_t),
                                                  int>::type = 0>
  inline const T* cget() const noexcept;
  template <typename T>
  inline const T* get() const noexcept;

  inline void* get(const Descriptor* descriptor) noexcept;

  // 辅助判断是否是常量引用
  inline bool is_const_reference() const noexcept;

  // 辅助判断是否是引用
  inline bool is_reference() const noexcept;

  // integer支持的转化功能
  template <typename T>
  inline T as() const noexcept;

  template <typename T>
  int to(T& value) const noexcept;

  // 辅助判断是否为空
  inline operator bool() const noexcept;

  // 辅助判断类型
  inline Type type() const noexcept;

  // 辅助判断对象类型，可通过字符串一致判断同一类型
  // 也可通过同地址判断同一类型
  // 可以产生人可读的类型表达
  inline const Id& instance_type() const noexcept;

  // 可以取得T类型对应的描述符，主要用于支持Type Erased模式
  // T* instance = new T {...};
  // any = std::unique_ptr<T>(instance);
  // 相当于
  // T* instance = new T {...};
  // const Descriptor* desc = Any::descriptor<T>();
  // void* ptr = instance;
  // any.assign(desc, ptr);
  template <typename T>
  inline static const Descriptor* descriptor() noexcept;

  // release managed instance inside
  // a successful release return std::unique_ptr of instance
  // and reset this Any to an initialize empty state
  //
  // when type not match or is reference, release will fail
  // and state of this Any is not changed
  template <typename T>
  inline ::std::unique_ptr<T> release() noexcept;
  inline ::std::unique_ptr<void, void (*)(void*)> release() noexcept;

 private:
  union Meta;

  template <typename T, typename E = void>
  struct TypeDescriptor {
    static void destructor(void* object) noexcept;
    static void deleter(void* object) noexcept;

    static Meta meta_for_instance;
    static Meta meta_for_inplace_trivial;
    static Meta meta_for_inplace_non_trivial;
  };

  template <typename T>
  struct TypeDescriptor<T, typename ::std::enable_if<
                               ::std::is_copy_constructible<T>::value>::type>
      : public TypeDescriptor<T, int> {
    static void copy_constructor(void* ptr, const void* object) noexcept;
    static void* copy_creater(const void* object) noexcept;

    static constexpr Descriptor descriptor {
        .type_id = TypeId<T>::ID,
        .destructor = TypeDescriptor<T, int>::destructor,
        .deleter = TypeDescriptor<T, int>::deleter,
        .copy_constructor = copy_constructor,
        .copy_creater = copy_creater,
    };
  };

  template <typename T>
  struct TypeDescriptor<T, typename ::std::enable_if<
                               !::std::is_copy_constructible<T>::value>::type>
      : public TypeDescriptor<T, int> {
    static void copy_constructor(void* ptr, const void* object) noexcept;
    static void* copy_creater(const void* object) noexcept;

    static constexpr Descriptor descriptor {
        .type_id = TypeId<T>::ID,
        .destructor = TypeDescriptor<T, int>::destructor,
        .deleter = TypeDescriptor<T, int>::deleter,
        .copy_constructor = copy_constructor,
        .copy_creater = copy_creater,
    };
  };

  inline static uint64_t meta_for_instance(
      const Descriptor* descriptor) noexcept;

  inline void destroy() noexcept;
  inline void* raw_pointer() noexcept;
  inline const void* const_raw_pointer() const noexcept;

  template <typename T, typename ::std::enable_if<::std::is_integral<T>::value,
                                                  int>::type = 0>
  inline void construct_inplace(T&& value) noexcept;
  template <typename T,
            typename ::std::enable_if<!::std::is_integral<T>::value &&
                                          (::std::is_array<T>::value ||
                                           ::std::is_function<T>::value),
                                      int>::type = 0>
  inline void construct_inplace(T&& value) noexcept;
  template <typename T,
            typename ::std::enable_if<!::std::is_integral<T>::value &&
                                          !::std::is_array<T>::value &
                                              !::std::is_function<T>::value,
                                      int>::type = 0>
  inline void construct_inplace(T&& value) noexcept;

  union Meta {
    inline const Descriptor* descriptor() const noexcept {
      return reinterpret_cast<const Descriptor*>(v & 0x0000FFFFFFFFFFFFL);
    }
    uint64_t v;
    struct M {
      uint32_t descriptor32;
      uint16_t descriptor16;
      Type type;
      uint8_t holder_type;
    } m;
  } _meta;
  union Holder {
    uint64_t uint64_v;
    int64_t int64_v;
    double double_v;
    float float_v;
    const void* const_pointer_value;
    void* pointer_value;
  } _holder;
};

BABYLON_NAMESPACE_END

#include "babylon/any.hpp"
