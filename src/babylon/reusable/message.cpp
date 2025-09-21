#include "babylon/environment.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/reusable/message.h"

#if GOOGLE_PROTOBUF_VERSION >= 3000000

BABYLON_NAMESPACE_BEGIN

void MessageAllocationMetadata::update(
    const ::google::protobuf::Message& message) noexcept {
  if (!_initialized) {
    initialize(message);
  }
  auto* reflection = message.GetReflection();
  for (auto& field : _fields) {
    field.update(message, reflection);
  }
}

void MessageAllocationMetadata::reserve(
    ::google::protobuf::Message& message) const noexcept {
  auto* reflection = message.GetReflection();
  auto* arena = message.GetArena();
  for (auto& field : _fields) {
    field.reserve(message, reflection, arena);
  }
}

MessageAllocationMetadata::FieldAllocationMetadata::FieldAllocationMetadata(
    const ::google::protobuf::FieldDescriptor* input_descriptor) noexcept
    : descriptor(input_descriptor) {}

MessageAllocationMetadata::FieldAllocationMetadata::FieldAllocationMetadata(
    const ::google::protobuf::FieldDescriptor* input_descriptor,
    const ::google::protobuf::Message* input_default_message) noexcept
    : descriptor(input_descriptor), default_message(input_default_message) {}

MessageAllocationMetadata::FieldAllocationMetadata::FieldAllocationMetadata(
    const ::google::protobuf::FieldDescriptor* input_descriptor,
    const ::std::string* input_default_string) noexcept
    : descriptor(input_descriptor), default_string(input_default_string) {}

void MessageAllocationMetadata::FieldAllocationMetadata::update(
    const ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection) noexcept {
  if (descriptor->is_repeated()) {
    update_repeated_field(message, reflection);
  } else if (descriptor->cpp_type() ==
             ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
    auto& string = reflection->GetStringReference(message, descriptor, nullptr);
    if (&string != default_string) {
      update(string);
    }
  } else {
    auto& sub_message = reflection->GetMessage(message, descriptor);
    update(sub_message);
  }
}

void MessageAllocationMetadata::FieldAllocationMetadata::update(
    const ::std::string& str) noexcept {
  string_reserved =
      ::std::max(string_reserved, static_cast<int64_t>(str.capacity()));
}

void MessageAllocationMetadata::FieldAllocationMetadata::update(
    const ::google::protobuf::Message& message) noexcept {
  if (&message != default_message) {
    if (!message_allocation_metadata) {
      message_allocation_metadata.reset(new MessageAllocationMetadata);
      message_allocation_metadata->initialize(message);
    }
    message_allocation_metadata->update(message);
  }
}

void MessageAllocationMetadata::FieldAllocationMetadata::reserve(
    ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection,
    ::google::protobuf::Arena* arena) const {
  if (descriptor->is_repeated() && repeated_reserved > 0) {
    reserve_repeated_field(message, reflection, arena);
  } else if (descriptor->cpp_type() ==
                 ::google::protobuf::FieldDescriptor::CPPTYPE_STRING &&
             string_reserved >= 0) {
    reserve_string_field(message, reflection, arena);
  } else if (message_allocation_metadata) {
    auto* sub_message = reflection->MutableMessage(&message, descriptor);
    message_allocation_metadata->reserve(*sub_message);
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
void MessageAllocationMetadata::initialize(
    const ::google::protobuf::Message& message) {
  auto* descriptor = message.GetDescriptor();
  auto* reflection = message.GetReflection();
  _default_instance = reflection->GetMessageFactory()->GetPrototype(descriptor);
  for (int32_t i = 0; i < descriptor->field_count(); ++i) {
    auto* field = descriptor->field(i);
    if (field->cpp_type() ==
        ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
      _fields.emplace_back(field, reflection->GetMessageFactory()->GetPrototype(
                                      field->message_type()));
    } else if (field->is_repeated()) {
      _fields.emplace_back(field);
    } else if (field->cpp_type() ==
               ::google::protobuf::FieldDescriptor::CPPTYPE_STRING) {
      _fields.emplace_back(field, &reflection->GetStringReference(
                                      *_default_instance, field, nullptr));
    }
  }
  _fields.shrink_to_fit();
  _initialized = true;
}

void MessageAllocationMetadata::FieldAllocationMetadata::reserve_repeated_field(
    ::google::protobuf::Message& message,
    const ::google::protobuf::Reflection* reflection,
    ::google::protobuf::Arena* arena) const {
  switch (descriptor->cpp_type()) {
#define __BABYLON_TMP_CASE(case_enum, type)                           \
  case ::google::protobuf::FieldDescriptor::case_enum: {              \
    auto* repeated_field =                                            \
        reflection->MutableRepeatedField<type>(&message, descriptor); \
    repeated_field->Reserve(repeated_reserved);                       \
    break;                                                            \
  }
    __BABYLON_TMP_CASE(CPPTYPE_INT32, int32_t)
    __BABYLON_TMP_CASE(CPPTYPE_UINT32, uint32_t)
    __BABYLON_TMP_CASE(CPPTYPE_INT64, int64_t)
    __BABYLON_TMP_CASE(CPPTYPE_UINT64, uint64_t)
    __BABYLON_TMP_CASE(CPPTYPE_DOUBLE, double)
    __BABYLON_TMP_CASE(CPPTYPE_FLOAT, float)
    __BABYLON_TMP_CASE(CPPTYPE_BOOL, bool)
    case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
      auto* repeated_field =
          mutable_repeated_enum_field(message, reflection, descriptor);
      repeated_field->Reserve(repeated_reserved);
      break;
    }
    case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      auto* repeated_field = reflection->MutableRepeatedPtrField<::std::string>(
          &message, descriptor);
      repeated_field->Reserve(repeated_reserved);
      for (size_t i = static_cast<size_t>(repeated_field->size());
           i < repeated_reserved; ++i) {
#if GOOGLE_PROTOBUF_HAS_DONATED_STRING
        repeated_field->AddAccessor()->reserve(
            static_cast<size_t>(string_reserved));
#else  // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
        stable_reserve(*repeated_field->Add(),
                       static_cast<size_t>(string_reserved));
#endif // !GOOGLE_PROTOBUF_HAS_DONATED_STRING
      }
      repeated_field->Clear();
      break;
    }
    case ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
      auto* repeated_field =
          reflection->MutableRepeatedPtrField<::google::protobuf::Message>(
              &message, descriptor);
      repeated_field->Reserve(repeated_reserved);
      for (size_t i = 0; i < repeated_reserved; ++i) {
        auto* sub_message = default_message->New(arena);
        message_allocation_metadata->reserve(*sub_message);
        repeated_field->UnsafeArenaAddAllocated(sub_message);
      }
      repeated_field->Clear();
      break;
    }
    default:
      assert(false);
      break;
  }
}
#pragma GCC diagnostic pop

BABYLON_NAMESPACE_END

#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

#endif // BABYLON_USE_PROTOBUF
