#pragma once

#include "babylon/reusable/allocator.h" // babylon::MonotonicAllocator
#include "babylon/reusable/traits.h"    // babylon::ReusableTraits
#include "babylon/serialization.h"      // babylon::SerializationHelper
#include "babylon/string.h"             // absl::strings_internal::STLStringResizeUninitialized

BABYLON_NAMESPACE_BEGIN

template <typename C, typename CH = ::std::char_traits<C>, typename R = MonotonicBufferResource>
class MonotonicBasicString : public ::std::basic_string<
        C, CH, MonotonicAllocator<C, R>> {
private:
    using Base = ::std::basic_string<C, CH, MonotonicAllocator<C, R>>;

public:
    using typename Base::value_type;
    using typename Base::const_pointer;

    using Base::Base;
    using Base::operator=;

    using Base::append;
    using Base::clear;
    using Base::data;
    using Base::get_allocator;
    using Base::size;

    inline MonotonicBasicString(
            MonotonicBasicString&& other,
            typename Base::allocator_type allocator) noexcept :
                Base(allocator) {
        operator=(::std::move(other));
    }

    template <typename A>
    inline MonotonicBasicString(
            const ::std::basic_string<C, CH, A>& other,
            typename Base::allocator_type allocator) noexcept :
                Base(other.begin(), other.end(), allocator) {}

    template <typename A>
    inline MonotonicBasicString& operator=(
            const ::std::basic_string<C, ::std::char_traits<C>,
                                      A>& other) noexcept {
        static_cast<Base*>(this)->assign(other.c_str(), other.size());
        return *this;
    }

    inline MonotonicBasicString& operator=(
            const MonotonicBasicString& other) noexcept {
        *static_cast<Base*>(this) = other;
        return *this;
    }

    inline MonotonicBasicString& operator=(
            MonotonicBasicString&& other) noexcept {
        if (get_allocator() == other.get_allocator()) {
            // MonotonicAllocator不会释放空间，采用swap而非直接丢弃
            // 可以尽可能保留this上已经申请的空间，交给other尽可能复用
            swap(other);
        } else {
            *this = other;
        }
        return *this;
    }

    inline void swap(MonotonicBasicString& other) noexcept {
        assert(get_allocator() == other.get_allocator() &&
               "can not swap string with different allocator");
        Base::swap(other);
    }

    inline void __resize_default_init(size_t size) noexcept {
        ::absl::strings_internal::STLStringResizeUninitialized(
                static_cast<Base*>(this), size);
    }

private:
#if BABYLON_USE_PROTOBUF
    using CodedInputStream = ::google::protobuf::io::CodedInputStream;
    using CodedOutputStream = ::google::protobuf::io::CodedOutputStream;

    ////////////////////////////////////////////////////////////////////////////
    // 序列化支持函数
    inline static constexpr bool serialize_compatible() noexcept {
        return true;
    }
    inline static constexpr int serialized_size_complexity() noexcept {
        return SerializationHelper::SERIALIZED_SIZE_COMPLEXITY_SIMPLE;
    }
    inline void serialize(CodedOutputStream& os) const noexcept {
        os.WriteRaw(data(), size() * sizeof(value_type));
    }
    inline bool deserialize(CodedInputStream& is) noexcept {
        const void* data;
        int size;
        clear();
        while (is.GetDirectBufferPointer(&data, &size)) {
            append(reinterpret_cast<const_pointer>(data), size / sizeof(value_type));
            is.Skip(size);
        }
        return true;
    }
    inline size_t calculate_serialized_size() const noexcept {
        return size() * sizeof(value_type);
    }
    inline bool print(Serialization::PrintStream& ps) const noexcept {
        return ps.print_string(StringView(*this));
    }
    friend ::babylon::SerializationHelper;
    ////////////////////////////////////////////////////////////////////////////
#endif // BABYLON_USE_PROTOBUF
};

using MonotonicString = MonotonicBasicString<char>;

using SwissString = MonotonicBasicString<char, ::std::char_traits<char>,
                                         SwissMemoryResource>;

template <typename CH, typename A, typename R>
inline bool operator==(
        const ::std::basic_string<CH, ::std::char_traits<CH>, A>& left,
        const MonotonicBasicString<CH, ::std::char_traits<CH>, R>& right) noexcept {
    return BasicStringView<CH>(left) == BasicStringView<CH>(right);
}

template <typename CH, typename A, typename R>
inline bool operator==(
        const MonotonicBasicString<CH, ::std::char_traits<CH>, R>& left,
        const ::std::basic_string<CH, ::std::char_traits<CH>, A>& right) noexcept {
    return BasicStringView<CH>(left) == BasicStringView<CH>(right);
}

template <typename CH, typename R, typename RR>
inline bool operator==(
        const MonotonicBasicString<CH, ::std::char_traits<CH>, R>& left,
        const MonotonicBasicString<CH, ::std::char_traits<CH>, RR>& right) noexcept {
    return BasicStringView<CH>(left) == BasicStringView<CH>(right);
}

template <typename CH, typename R>
class ReusableTraits<MonotonicBasicString<CH, ::std::char_traits<CH>, R>> :
    public BasicReusableTraits<ReusableTraits<MonotonicBasicString<CH, ::std::char_traits<CH>, R>>> {
private:
    using String = MonotonicBasicString<CH, ::std::char_traits<CH>, R>;
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
            String* ptr, AA allocator,
            const AllocationMetadata& meta) noexcept {
        allocator.construct(ptr);
        stable_reserve(*ptr, meta.capacity);
    }
};
#if __cplusplus < 201703L
template <typename CH, typename R>
constexpr bool ReusableTraits<MonotonicBasicString<CH, ::std::char_traits<CH>, R>>::REUSABLE;
#endif // __cplusplus < 201703L

BABYLON_NAMESPACE_END

namespace std {

template <typename CH, typename R>
struct is_trivially_destructible<
        ::babylon::MonotonicBasicString<CH, ::std::char_traits<CH>, R>> {
    static constexpr bool value = true;
};
#if __cplusplus < 201703L
template <typename CH, typename R>
constexpr bool is_trivially_destructible<
    ::babylon::MonotonicBasicString<CH, ::std::char_traits<CH>, R>>::value;
#endif // __cplusplus < 201703L

} // std
