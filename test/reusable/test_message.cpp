#include "babylon/reusable/message.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/reusable/manager.h"

#include "gtest/gtest.h"

#include <arena_example.pb.h>

using ::absl::strings_internal::STLStringResizeUninitialized;
using ::babylon::ArenaExample;
using ::babylon::SwissManager;

const ::std::string long_string =
    ::std::string(::std::string().capacity() + 10, 'x');
const size_t default_string_capacity = ::std::string().capacity();

struct MockPageAllocator : public ::babylon::NewDeletePageAllocator {
  virtual ~MockPageAllocator() noexcept override {
    for (size_t i = 0; i < free_pages.size(); ++i) {
      ::std::cerr << "free " << free_pages[i] << ::std::endl;
    }
    NewDeletePageAllocator::deallocate(free_pages.data(), free_pages.size());
  }
  using PageAllocator::deallocate;
  virtual void deallocate(void** pages, size_t num) noexcept override {
    for (size_t i = 0; i < num; ++i) {
      ::std::cerr << "deallocate " << pages[i] << ::std::endl;
    }
    free_pages.insert(free_pages.end(), pages, pages + num);
  }
  ::std::vector<void*> free_pages;
};

// 相当于一个特殊尺寸的NewDeletePageAllocator
struct ReusableMessage : public ::testing::Test {
  using Arena = ::google::protobuf::Arena;
  using Message = ::google::protobuf::Message;

  void SetUp() override {
    allocator.set_page_size(256);
    manager.resource().set_page_allocator(allocator);
    manager.set_recreate_interval(1);
  }

  MockPageAllocator allocator;
  SwissManager manager;
};

TEST_F(ReusableMessage, may_recreate_message_when_clear) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  message.get();
  message->set_p(1);
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_FALSE(message->has_p());
  message->set_p(1);
  manager.set_recreate_interval(3);
  manager.clear();
  pmessage = message.get();
  message->set_p(1);
  manager.clear();
  ASSERT_EQ(pmessage, message.get());
  ASSERT_FALSE(message->has_p());
}

TEST_F(ReusableMessage, keep_string_capacity) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  ASSERT_EQ(default_string_capacity, message->s().capacity());
  ASSERT_EQ("10086", message->ds());
  message->set_s(long_string);
  message->set_ds(long_string);
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_FALSE(message->has_s());
  ASSERT_TRUE(message->s().empty());
  ASSERT_LE(long_string.size(), message->s().capacity());
  ASSERT_FALSE(message->has_ds());
  ASSERT_EQ("10086", message->ds());
  ASSERT_LE(long_string.size(), message->ds().capacity());
}

TEST_F(ReusableMessage, keep_sub_message_field_capacity) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  ASSERT_EQ(default_string_capacity, message->m().s().capacity());
  message->mutable_m()->set_s(long_string);
  message->mutable_m()->set_ds(long_string);
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_FALSE(message->m().has_s());
  ASSERT_TRUE(message->m().s().empty());
  ASSERT_LE(long_string.size(), message->m().s().capacity());
  ASSERT_FALSE(message->m().has_ds());
  ASSERT_EQ("10086", message->m().ds());
  ASSERT_LE(long_string.size(), message->m().ds().capacity());
}

TEST_F(ReusableMessage, keep_repeated_primitive_field_capacity) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  ASSERT_EQ(0, message->rp().Capacity());
  message->add_rp(1);
  auto capacity = message->rp().Capacity();
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_EQ(capacity, message->rp().Capacity());
}

TEST_F(ReusableMessage, keep_repeated_enum_field_capacity) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  ASSERT_EQ(0, message->re().Capacity());
  message->add_re(ArenaExample::ENUM1);
  auto capacity = message->re().Capacity();
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_EQ(capacity, message->re().Capacity());
}

TEST_F(ReusableMessage, keep_repeated_string_field_capacity) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  message->add_rs(long_string);
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_EQ(1, message->rs().ClearedCount());
  message->add_rs("");
  ASSERT_LE(long_string.size(), message->rs(0).capacity());
  message->add_rs("");
  ASSERT_EQ(default_string_capacity, message->rs(1).capacity());
#pragma GCC diagnostic pop
}

TEST_F(ReusableMessage, keep_repeated_sub_message_field_capacity) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  message->add_rm()->add_rs(long_string);
  message->add_rm()->set_s(long_string);
  message->add_rm()->set_ds(long_string);
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_EQ(3, message->rm().ClearedCount());
  message->add_rm()->add_rs("");
  ASSERT_LE(long_string.size(), message->rm(0).rs(0).capacity());
  ASSERT_FALSE(message->rm(0).has_s());
  ASSERT_TRUE(message->rm(0).s().empty());
  ASSERT_LE(long_string.size(), message->rm(0).s().capacity());
  ASSERT_FALSE(message->rm(0).has_ds());
  ASSERT_EQ("10086", message->rm(0).ds());
  ASSERT_LE(long_string.size(), message->rm(0).ds().capacity());
#pragma GCC diagnostic pop
}

TEST_F(ReusableMessage, recreate_string_on_arena) {
  auto message = manager.create_object<ArenaExample>();
  auto* pmessage = message.get();
  message->set_s("1234567890");
  message->add_rs("1234567890");
  message->mutable_m()->set_s("1234567890");
  if (!manager.resource().contains(message->mutable_s()->data())) {
    return;
  }
  manager.clear();
  ASSERT_NE(pmessage, message.get());
  ASSERT_TRUE(manager.resource().contains(message->mutable_s()->data()));
  ASSERT_TRUE(manager.resource().contains(message->add_rs()->data()));
  ASSERT_TRUE(
      manager.resource().contains(message->mutable_m()->mutable_s()->data()));
}

TEST_F(ReusableMessage,
       usable_with_base_protobuf_message_type_when_reflection) {
  auto message = manager.create_object<Message>(
      [](::babylon::SwissMemoryResource& resource) {
        Arena& arena = resource;
#if GOOGLE_PROTOBUF_VERSION >= 5026000
        Message* result = Arena::Create<ArenaExample>(&arena);
#else  // GOOGLE_PROTOBUF_VERSION < 5026000
        Message* result = Arena::CreateMessage<ArenaExample>(&arena);
#endif // GOOGLE_PROTOBUF_VERSION < 5026000
        return result;
      });
  auto pmessage = static_cast<ArenaExample*>(message.get());
  ASSERT_EQ(default_string_capacity, pmessage->s().capacity());
  ASSERT_EQ("10086", pmessage->ds());
  pmessage->set_s(long_string);
  pmessage->set_ds(long_string);
  manager.clear();
  auto pmessage2 = static_cast<ArenaExample*>(message.get());
  ASSERT_NE(pmessage, pmessage2);
  ASSERT_FALSE(pmessage2->has_s());
  ASSERT_TRUE(pmessage2->s().empty());
  ASSERT_LE(long_string.size(), pmessage2->s().capacity());
  ASSERT_FALSE(pmessage2->has_ds());
  ASSERT_EQ("10086", pmessage2->ds());
  ASSERT_LE(long_string.size(), pmessage2->ds().capacity());
}

#endif // BABYLON_USE_PROTOBUF
