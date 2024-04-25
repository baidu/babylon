#pragma once

#include <babylon/reusable/allocator.h>
#include <babylon/reusable/traits.h>    // babylon::ReusableTraits
#include <babylon/serialization.h>      // babylon::SerializeTraits

BABYLON_NAMESPACE_BEGIN

// 对标std::vector，主要有以下几点点区别
// 1、仅支持MonotonicAllocator分配器
// 2、满足ReusableTraits::REUSABLE条件
// 3、为了更方便搭配allocator使用，增强了部分接口
//    需要传入T类型的构造或赋值放宽到可以构造T的V类型
//    主要因为元素T往往也需要使用分配器构造
//    采用类emplace语义的设计相比原接口可以更有效避免冗余拷贝
template <typename T, typename A = MonotonicAllocator<T>>
class ReusableVector;
template <typename T, typename U, typename A>
class ReusableVector<T, MonotonicAllocator<U, A>> {
private:
    using Vector = ::std::vector<T, MonotonicAllocator<T, A>>;
    using ValueReusableTraits = ReusableTraits<typename Vector::value_type>;

public:
    using value_type = typename Vector::value_type;
    using allocator_type = typename Vector::allocator_type;
    using size_type = typename Vector::size_type;
    using difference_type = typename Vector::difference_type;
    using reference = typename Vector::reference;
    using const_reference = typename Vector::const_reference;
    using pointer = typename Vector::pointer;
    using const_pointer = typename Vector::const_pointer;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = ::std::reverse_iterator<iterator>;
    using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

    static_assert(ValueReusableTraits::REUSABLE,
            "element of ReusableVector need reusable");

    struct AllocationMetadata {
        typename ValueReusableTraits::AllocationMetadata value_metadata;
        size_type capacity {0};
    };

    // 无默认构造，可以拷贝或移动
    ReusableVector() = delete;
    inline ReusableVector(ReusableVector&& other) noexcept;
    inline ReusableVector(const ReusableVector& other) noexcept;
    inline ReusableVector& operator=(ReusableVector&& other) noexcept;
    inline ReusableVector& operator=(const ReusableVector& other) noexcept;
    inline ~ReusableVector() noexcept;

    // 基础构造函数对应的带分配器版本
    inline explicit ReusableVector(allocator_type allocator) noexcept;
    inline ReusableVector(ReusableVector&& other,
                          allocator_type allocator) noexcept;
    inline ReusableVector(const ReusableVector& other,
                          allocator_type allocator) noexcept;

    // 其余构造函数均为带分配器版本
    inline ReusableVector(size_type count, allocator_type allocator) noexcept;
    template <typename V>
    inline ReusableVector(size_type count, const V& value,
                          allocator_type allocator) noexcept;
    template <typename IT, typename = typename ::std::enable_if<
        ::std::is_convertible<
            typename ::std::iterator_traits<IT>::iterator_category,
            ::std::input_iterator_tag>::value>::type>
    inline ReusableVector(IT first, IT last, allocator_type allocator) noexcept;
    template <typename V>
    inline ReusableVector(::std::initializer_list<V> initializer_list,
                          allocator_type allocator) noexcept;

    // 赋值
    template <typename V>
    inline ReusableVector& operator=(
        ::std::initializer_list<V> initializer_list) noexcept;
    template <typename V>
    inline void assign(size_type count, const V& value) noexcept;
    template <typename IT, typename = typename ::std::enable_if<
        ::std::is_convertible<
            typename ::std::iterator_traits<IT>::iterator_category,
            ::std::input_iterator_tag>::value>::type>
    inline void assign(IT first, IT last) noexcept;
    template <typename V>
    inline void assign(::std::initializer_list<V> initializer_list) noexcept;

    // 取得allocator
    inline allocator_type get_allocator() const noexcept;

    // 访问
    //inline reference at(size_type pos);
    //inline const_reference at(size_type pos) const;
    inline reference operator[](size_type pos) noexcept;
    inline const_reference operator[](size_type pos) const noexcept;
    inline reference front() noexcept;
    inline const_reference front() const noexcept;
    inline reference back() noexcept;
    inline const_reference back() const noexcept;
    inline pointer data() noexcept;
    inline const_pointer data() const noexcept;

    // 迭代器
    inline iterator begin() noexcept;
    inline const_iterator begin() const noexcept;
    inline const_iterator cbegin() const noexcept;
    inline iterator end() noexcept;
    inline const_iterator end() const noexcept;
    inline const_iterator cend() const noexcept;
    inline reverse_iterator rbegin() noexcept;
    inline const_reverse_iterator rbegin() const noexcept;
    inline const_reverse_iterator crbegin() const noexcept;
    inline reverse_iterator rend() noexcept;
    inline const_reverse_iterator rend() const noexcept;
    inline const_reverse_iterator crend() const noexcept;

    // 用量
    inline bool empty() const noexcept;
    inline size_type size() const noexcept;
    inline size_type constructed_size() const noexcept;
    //inline size_type max_size() const noexcept;
    inline void reserve(size_type min_capacity) noexcept;
    inline size_type capacity() const noexcept;
    //inline void shrink_to_fit();

    // 操作容器
    inline void clear() noexcept;
    template <typename V>
    inline iterator insert(const_iterator pos, V&& value) noexcept;
    template <typename V>
    inline iterator insert(const_iterator pos, size_type count,
                           const V& value) noexcept;
    template <typename IT, typename = typename ::std::enable_if<
        ::std::is_convertible<
            typename ::std::iterator_traits<IT>::iterator_category,
                 ::std::input_iterator_tag>::value>::type>
    inline iterator insert(const_iterator pos, IT first, IT last) noexcept;
    template <typename V>
    inline iterator insert(
        const_iterator pos,
        ::std::initializer_list<V> initializer_list) noexcept;
    template <typename... Args, typename = typename ::std::enable_if<
        UsesAllocatorConstructor::template Constructible<
            value_type, allocator_type, Args...>::value>::type>
    inline iterator emplace(const_iterator pos, Args&&... args) noexcept;
    inline iterator erase(const_iterator pos) noexcept;
    inline iterator erase(const_iterator first, const_iterator last) noexcept;
    template <typename V>
    inline void push_back(V&& value) noexcept;
    template <typename... Args, typename = typename ::std::enable_if<
        UsesAllocatorConstructor::template Constructible<
            value_type, allocator_type, Args...>::value>::type>
    inline void emplace_back(Args&&... args) noexcept;
    inline void pop_back() noexcept;
    inline void resize(size_type count) noexcept;
    template <typename V>
    inline void resize(size_type count, const V& value) noexcept;
    inline void swap(ReusableVector& other) noexcept;

    // 支持ReusableTraits重建
    inline ReusableVector(const AllocationMetadata& metadata,
                          allocator_type allocator) noexcept;
    inline void update_allocation_metadata(
            AllocationMetadata& meta) const noexcept;
    inline void assign(size_type count) noexcept;

private:
    // 为在index位置开始insert count个元素做准备
    // 调用后保证capacity和size已经正确调整，且
    // data[index, return)的元素已经构造完成，可以直接赋值
    // data[return, index + count)的元素尚未构造，需要赋值构造
    inline size_type prepare_for_insert(size_type index,
                                        size_type count) noexcept;

    friend inline void swap(ReusableVector& left,
                            ReusableVector& right) noexcept {
        left.swap(right);
    }

#if BABYLON_USE_PROTOBUF
    using CodedInputStream = ::google::protobuf::io::CodedInputStream;
    using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;

    ////////////////////////////////////////////////////////////////////////////
    // 序列化支持函数
    static constexpr int BABYLON_SERIALIZED_SIZE_COMPLEXITY =
        SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
            SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL ?
                SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE :
                SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_COMPLEX;
    inline void serialize(CodedOutputStream& os) const noexcept {
        for (const auto& one_value : *this) {
            SerializationHelper::serialize_packed_field(one_value, os);
        }
    }
    inline bool deserialize(CodedInputStream& is) noexcept {
        while (is.BytesUntilLimit() > 0) {
            emplace_back();
            if (ABSL_PREDICT_FALSE(
                    !SerializationHelper::deserialize_packed_field(is, back()))) {
                pop_back();
                return false;
            }
        }
        return true;
    }
    template <typename... Args, typename = typename ::std::enable_if<
        (sizeof...(Args) > 0) || SerializeTraits<T>::SERIALIZABLE>::type>
    inline size_t calculate_serialized_size(Args...) const noexcept {
        if CONSTEXPR_SINCE_CXX17 (
                SerializeTraits<T>::SERIALIZED_SIZE_COMPLEXITY ==
                    SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_TRIVIAL) {
            return size() *
                SerializationHelper::calculate_serialized_size_packed_field(_data[0]);
        }

        size_t size = 0;
        for (const auto& one_value : *this) {
            size += SerializationHelper::calculate_serialized_size_packed_field(one_value);
        }
        return size;
    }
    template <typename... Args, typename = typename ::std::enable_if<
        (sizeof...(Args) > 0) || SerializeTraits<T>::SERIALIZED_SIZE_CACHED>::type>
    inline size_t serialized_size_cached() const noexcept {
        size_t size = 0;
        for (const auto& one_value : *this) {
            size += SerializationHelper::serialized_size_cached_packed_field(one_value);
        }
        return size;
    }
    inline bool print(Serialization::PrintStream& ps) const noexcept {
        bool first = true;
        if (ABSL_PREDICT_FALSE(!ps.print_raw("["))) {
            return false;
        }
        for (const auto& one_value : *this) {
            if (first) {
                first = false;
            } else if (ABSL_PREDICT_FALSE(!ps.print_raw(", "))) {
                return false;
            }
            if (ABSL_PREDICT_FALSE(!SerializeTraits<T>::print(one_value, ps))) {
                return false;
            }
        }
        if (ABSL_PREDICT_FALSE(!ps.print_raw("]"))) {
            return false;
        }
        return true;
    }
    friend ::babylon::SerializationHelper;
    ////////////////////////////////////////////////////////////////////////////
#endif // BABYLON_USE_PROTOBUF

    allocator_type _allocator;
    pointer _data {nullptr};
    size_type _size {0};
    size_type _constructed_size {0};
    size_type _capacity {0};
};

template <typename T>
using SwissVector = ReusableVector<T, SwissAllocator<T>>;

BABYLON_NAMESPACE_END

namespace std {

template <typename T, typename A>
struct is_trivially_destructible<::babylon::ReusableVector<T, A>> {
    static constexpr bool value =
        ::std::is_trivially_destructible<T>::value;
};
#if __cplusplus < 201703L
template <typename T, typename A>
constexpr bool is_trivially_destructible<::babylon::ReusableVector<T, A>>::value;
#endif // __cplusplus < 201703L

} // std

#include <babylon/reusable/vector.hpp>
