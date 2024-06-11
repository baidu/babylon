#pragma once

#include "babylon/environment.h" // BABYLON_USE_PROTOBUF

#if BABYLON_USE_PROTOBUF

#include "babylon/reusable/allocator.h" // babylon::SwissAllocator
#include "babylon/reusable/traits.h"    // babylon::ReusableTraits

#if GOOGLE_PROTOBUF_VERSION >= 3000000
#include "google/protobuf/arenastring.h" // google::protobuf::internal::ArenaStringPtr
#include "google/protobuf/message.h"        // google::protobuf::Message
#include "google/protobuf/repeated_field.h" // google::protobuf::RepeatedField

BABYLON_NAMESPACE_BEGIN

// 利用反射功能从Message中提取容量的工具
// 用于支持ReusableTraits<Message>实现
class MessageAllocationMetadata {
 private:
  using Arena = ::google::protobuf::Arena;
  using Message = ::google::protobuf::Message;
  using ArenaStringPtr = ::google::protobuf::internal::ArenaStringPtr;

 public:
  // 从message中提取容量，多次提取会保留最大值
  void update(const Message& message) noexcept;
  // 操作message，递归恢复到之前记录的容量
  void reserve(Message& message) const noexcept;

 private:
  // 每个field的capacity信息
  struct FieldAllocationMetadata {
    FieldAllocationMetadata(
        const ::google::protobuf::FieldDescriptor* descriptor) noexcept;
    FieldAllocationMetadata(
        const ::google::protobuf::FieldDescriptor* descriptor,
        const ::google::protobuf::Message* default_message) noexcept;
    FieldAllocationMetadata(
        const ::google::protobuf::FieldDescriptor* descriptor,
        const ::std::string* default_string) noexcept;

    // field的更新入口函数，按字段类型分派给子函数
    void update(const ::google::protobuf::Message& message,
                const ::google::protobuf::Reflection* reflection) noexcept;

    // 更新repeated字段
    void update_repeated_field(
        const ::google::protobuf::Message& message,
        const ::google::protobuf::Reflection* reflection) noexcept;
    // 更新string字段
    void update(const ::std::string& str) noexcept;
    // 更新sum message字段
    void update(const ::google::protobuf::Message& message) noexcept;

    // field的调整入口函数，按字段类型分派给子函数
    inline void reserve(::google::protobuf::Message& message,
                        const ::google::protobuf::Reflection* reflection,
                        ::google::protobuf::Arena* arena) const;

    // 调整repeated字段
    void reserve_repeated_field(
        ::google::protobuf::Message& message,
        const ::google::protobuf::Reflection* reflection,
        ::google::protobuf::Arena* arena) const;

    // 特殊trick函数，调整string
    // field底层的ArenaStringPtr或InlinedStringField本体容量
    void reserve_string_field(::google::protobuf::Message& message,
                              const ::google::protobuf::Reflection* reflection,
                              ::google::protobuf::Arena* arena) const;

    // 特殊trick函数，用于获取可变repeated enum成员
    static ::google::protobuf::RepeatedField<int32_t>*
    mutable_repeated_enum_field(
        ::google::protobuf::Message& message,
        const ::google::protobuf::Reflection* reflection,
        const ::google::protobuf::FieldDescriptor* descriptor);

    // 特殊trick函数，用于获取只读repeated enum成员
    static const ::google::protobuf::RepeatedField<int32_t>&
    get_repeated_enum_field(
        const ::google::protobuf::Message& message,
        const ::google::protobuf::Reflection* reflection,
        const ::google::protobuf::FieldDescriptor* descriptor);

    const ::google::protobuf::FieldDescriptor* descriptor;
    size_t repeated_reserved {0};
    int64_t string_reserved {-1};
    ::std::unique_ptr<MessageAllocationMetadata> message_allocation_metadata {
        nullptr};
    const ::google::protobuf::Message* default_message {nullptr};
    const ::std::string* default_string {nullptr};
  };

  // 从Message初始化一个空的schema
  void initialize(const ::google::protobuf::Message& message);

  bool _initialized {false};
  const Message* _default_instance {nullptr};
  ::std::vector<FieldAllocationMetadata> _fields;
};

template <typename T>
class ReusableTraits<T, typename ::std::enable_if<::std::is_base_of<
                            ::google::protobuf::Message, T>::value>::type>
    : public BasicReusableTraits<ReusableTraits<T>> {
 private:
  using Message = ::google::protobuf::Message;
  using Base = BasicReusableTraits<ReusableTraits<T>>;

 public:
  static constexpr bool REUSABLE = true;

  using AllocationMetadata = MessageAllocationMetadata;

  static void update_allocation_metadata(const Message& message,
                                         AllocationMetadata& meta) noexcept {
    meta.update(message);
  }

  static void construct_with_allocation_metadata(
      T* ptr, SwissAllocator<> allocator,
      const AllocationMetadata& meta) noexcept {
    allocator.construct(ptr);
    meta.reserve(*ptr);
  }

  static void reconstruct_instance(T& message, SwissAllocator<>) {
    message.Clear();
  }
};
#if __cplusplus < 201703L
template <typename T>
constexpr bool ReusableTraits<
    T, typename ::std::enable_if<::std::is_base_of<::google::protobuf::Message,
                                                   T>::value>::type>::REUSABLE;
#endif // __cplusplus < 201703L

template <>
class ReusableTraits<::google::protobuf::Message>
    : public BasicReusableTraits<ReusableTraits<::google::protobuf::Message>> {
 private:
  using Arena = ::google::protobuf::Arena;
  using Message = ::google::protobuf::Message;
  using Base = BasicReusableTraits<ReusableTraits<::google::protobuf::Message>>;

 public:
  struct AllocationMetadata {
    MessageAllocationMetadata metadata;
    const Message* default_instance {nullptr};
  };

  static constexpr bool REUSABLE = true;

  static void update_allocation_metadata(
      const Message& message, AllocationMetadata& metadata) noexcept {
    if (metadata.default_instance == nullptr) {
      auto descriptor = message.GetDescriptor();
      auto reflection = message.GetReflection();
      metadata.default_instance =
          reflection->GetMessageFactory()->GetPrototype(descriptor);
    }
    metadata.metadata.update(message);
  }

  static Message* create_with_allocation_metadata(
      SwissAllocator<> allocator, const AllocationMetadata& metadata) {
    auto instance =
        metadata.default_instance->New(&(Arena&)*allocator.resource());
    metadata.metadata.reserve(*instance);
    return instance;
  }

  inline static void reconstruct_instance(Message& instance, SwissAllocator<>) {
    instance.Clear();
  }
};

BABYLON_NAMESPACE_END

#endif // GOOGLE_PROTOBUF_VERSION >= 3000000

#endif // BABYLON_USE_PROTOBUF
