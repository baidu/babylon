#include "babylon/environment.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/reusable/message.h"

#if GOOGLE_PROTOBUF_VERSION >= 3000000

#include "google/protobuf/generated_message_reflection.h" // google::protobuf::internal::GeneratedMessageReflection;
#if GOOGLE_PROTOBUF_VERSION >= 3018000
#include "google/protobuf/inlined_string_field.h" // google::protobuf::internal::InlinedStringField
#endif // GOOGLE_PROTOBUF_VERSION >= 3018000

using ::google::protobuf::FieldDescriptor;

#if GOOGLE_PROTOBUF_VERSION < 3009000
using ::google::protobuf::internal::GeneratedMessageReflection;
#endif // GOOGLE_PROTOBUF_VERSION < 3009000

#if GOOGLE_PROTOBUF_VERSION >= 3018000
using ::google::protobuf::internal::InlinedStringField;
#endif // GOOGLE_PROTOBUF_VERSION >= 3018000

using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

BABYLON_NAMESPACE_BEGIN

static void* get_raw_field(Message& message, const Reflection* reflection,
                           const FieldDescriptor* descriptor) {
#if GOOGLE_PROTOBUF_VERSION >= 3009000
  return reinterpret_cast<char*>(&message) +
         reflection->schema_.GetFieldOffset(descriptor);
#elif GOOGLE_PROTOBUF_VERSION >= 3002000 // && GOOGLE_PROTOBUF_VERSION < 3009000
  return reinterpret_cast<char*>(&message) +
         static_cast<const GeneratedMessageReflection*>(reflection)
             ->schema_.GetFieldOffset(descriptor);
#else                                    // GOOGLE_PROTOBUF_VERSION < 3002000
  return reinterpret_cast<char*>(&message) +
         static_cast<const GeneratedMessageReflection*>(reflection)
             ->offsets_[descriptor->index()];
#endif                                   // GOOGLE_PROTOBUF_VERSION < 3002000
}

#if GOOGLE_PROTOBUF_VERSION >= 3018000
static bool is_inlined(const Reflection* reflection,
                       const FieldDescriptor* descriptor) {
  return reflection->schema_.IsFieldInlined(descriptor);
}

static uint32_t* get_inlined_string_donated_array(
    Message& message, const Reflection* reflection) {
  return reinterpret_cast<uint32_t*>(
      reinterpret_cast<char*>(&message) +
      reflection->schema_.InlinedStringDonatedOffset());
}

static uint32_t get_inlined_string_index(const Reflection* reflection,
                                         const FieldDescriptor* descriptor) {
  return reflection->schema_.InlinedStringIndex(descriptor);
}
#endif // GOOGLE_PROTOBUF_VERSION < 3018000

static void reserve_arena_string_ptr(
    ::google::protobuf::internal::ArenaStringPtr& ptr, bool has_default_value,
    const ::std::string* default_string, size_t size,
    ::google::protobuf::Arena* arena) {
  using ::google::protobuf::internal::GetEmptyStringAlreadyInited;

  (void)has_default_value;
#if GOOGLE_PROTOBUF_VERSION >= 3014000
  // 从3.14开始Mutable接口对有无默认值情况的接口进行了分拆，需要判断情况调用
  if (!has_default_value) {
#if GOOGLE_PROTOBUF_VERSION >= 3020000
    // 从3.20开始Mutable接口在无默认值的情况下省去了标记参数
#if GOOGLE_PROTOBUF_HAS_DONATED_STRING
    // 对应用了补丁的版本，调用特殊接口保持Donated状态
    ptr.MutableAccessor(arena)->reserve(size);
#else  // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
    stable_reserve(*ptr.Mutable(arena), size);
#endif // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
#else  // GOOGLE_PROTOBUF_VERSION < 3020000
       // 在[3.14, 3.20)版本，有一个额外的标记参数
    ::google::protobuf::internal::ArenaStringPtr::EmptyDefault empty_default {};
#if GOOGLE_PROTOBUF_HAS_DONATED_STRING
    // 对应用了补丁的版本，调用特殊接口保持Donated状态
    ptr.MutableAccessor(empty_default, arena)->reserve(size);
#else  // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
    stable_reserve(*ptr.Mutable(empty_default, arena), size);
#endif // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
#endif // GOOGLE_PROTOBUF_VERSION < 3020000
  } else {
    using ::google::protobuf::internal::LazyString;
    LazyString non_empty_default {{{nullptr, 0}}, {default_string}};
#if GOOGLE_PROTOBUF_HAS_DONATED_STRING
    // 对应用了补丁的版本，调用特殊接口保持Donated状态
    ptr.MutableAccessor(non_empty_default, arena)->reserve(size);
#else  // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
    stable_reserve(*ptr.Mutable(non_empty_default, arena), size);
#endif // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
  }
#else  // GOOGLE_PROTOBUF_VERSION < 3014000
  // 3.14之前的版本Mutable接口统一传入默认值
  stable_reserve(*ptr.Mutable(default_string, arena), size);
#endif // GOOGLE_PROTOBUF_VERSION < 3014000
}

void MessageAllocationMetadata::FieldAllocationMetadata::reserve_string_field(
    ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection,
    ::google::protobuf::Arena* arena) const {
  auto* ptr = get_raw_field(message, reflection, descriptor);
#if GOOGLE_PROTOBUF_VERSION >= 3018000
  if (is_inlined(reflection, descriptor)) {
    auto* array = get_inlined_string_donated_array(message, reflection);
    auto index = get_inlined_string_index(reflection, descriptor);
    auto* donating_states = array + index / 32;
    auto selector = static_cast<uint32_t>(1) << (index % 32);
    bool donated = *donating_states & selector;
    auto mask = ~selector;
#if GOOGLE_PROTOBUF_HAS_DONATED_STRING
    auto string = reinterpret_cast<InlinedStringField*>(ptr)->MutableAccessor(
#else  // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
    auto& string = *reinterpret_cast<InlinedStringField*>(ptr)->Mutable(
#endif // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
#if GOOGLE_PROTOBUF_VERSION >= 3020000
        arena, donated, donating_states, mask, nullptr);
#else  // GOOGLE_PROTOBUF_VERSION < 3020000
        ArenaStringPtr::EmptyDefault {}, arena, donated, donating_states, mask);
#endif // GOOGLE_PROTOBUF_VERSION < 3020000
    stable_reserve(string, static_cast<size_t>(string_reserved));
  } else
#endif // GOOGLE_PROTOBUF_VERSION < 3018000
  {
    reserve_arena_string_ptr(*reinterpret_cast<ArenaStringPtr*>(ptr),
                             descriptor->has_default_value(), default_string,
                             static_cast<size_t>(string_reserved), arena);
  }
}

::google::protobuf::RepeatedField<int32_t>*
MessageAllocationMetadata::FieldAllocationMetadata::mutable_repeated_enum_field(
    ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection,
    const ::google::protobuf::FieldDescriptor* descriptor) {
  return reinterpret_cast<::google::protobuf::RepeatedField<int32_t>*>(
      reflection->MutableRawRepeatedField(
          &message, descriptor,
          ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM, -1, nullptr));
}

const ::google::protobuf::RepeatedField<int32_t>&
MessageAllocationMetadata::FieldAllocationMetadata::get_repeated_enum_field(
    const ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection,
    const ::google::protobuf::FieldDescriptor* descriptor) {
  return *reinterpret_cast<::google::protobuf::RepeatedField<int32_t>*>(
      reflection->MutableRawRepeatedField(
          &const_cast<::google::protobuf::Message&>(message), descriptor,
          ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM, -1, nullptr));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void MessageAllocationMetadata::FieldAllocationMetadata::update_repeated_field(
    const ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection) noexcept {
  switch (descriptor->cpp_type()) {
#define __BABYLON_TMP_CASE(case_enum, type)                                  \
  case ::google::protobuf::FieldDescriptor::case_enum: {                     \
    auto capacity = static_cast<size_t>(                                     \
        reflection->GetRepeatedField<type>(message, descriptor).Capacity()); \
    if (repeated_reserved < capacity) {                                      \
      repeated_reserved = capacity;                                          \
    }                                                                        \
    break;                                                                   \
  }
    __BABYLON_TMP_CASE(CPPTYPE_INT32, int32_t)
    __BABYLON_TMP_CASE(CPPTYPE_UINT32, uint32_t)
    __BABYLON_TMP_CASE(CPPTYPE_INT64, int64_t)
    __BABYLON_TMP_CASE(CPPTYPE_UINT64, uint64_t)
    __BABYLON_TMP_CASE(CPPTYPE_DOUBLE, double)
    __BABYLON_TMP_CASE(CPPTYPE_FLOAT, float)
    __BABYLON_TMP_CASE(CPPTYPE_BOOL, bool)
    case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
      auto capacity = static_cast<size_t>(
          get_repeated_enum_field(message, reflection, descriptor).Capacity());
      if (repeated_reserved < capacity) {
        repeated_reserved = capacity;
      }
      break;
    }
#undef __BABYLON_TMP_CASE
#define __BABYLON_TMP_CASE(case_enum, type)                              \
  case ::google::protobuf::FieldDescriptor::case_enum: {                 \
    auto& repeated_field =                                               \
        reflection->GetRepeatedPtrField<type>(message, descriptor);      \
    auto capacity = static_cast<size_t>(                                 \
        repeated_field.size() +                                          \
        reinterpret_cast<                                                \
            const ::google::protobuf::internal::RepeatedPtrFieldBase&>(  \
            repeated_field)                                              \
            .ClearedCount());                                            \
    if (repeated_reserved < capacity) {                                  \
      repeated_reserved = capacity;                                      \
    }                                                                    \
    auto begin = repeated_field.cbegin();                                \
    auto end = repeated_field.cbegin() + static_cast<ssize_t>(capacity); \
    for (; begin != end; ++begin) {                                      \
      update(*begin);                                                    \
    }                                                                    \
    break;                                                               \
  }
      __BABYLON_TMP_CASE(CPPTYPE_STRING, ::std::string)
      __BABYLON_TMP_CASE(CPPTYPE_MESSAGE, ::google::protobuf::Message)
#undef __BABYLON_TMP_CASE
    default:
      assert(false);
      break;
  }
}
#pragma GCC diagnostic pop

BABYLON_NAMESPACE_END

#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

#endif // BABYLON_USE_PROTOBUF
