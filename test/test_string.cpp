#include "babylon/string.h"
#include "babylon/string_view.h"

#include <gtest/gtest.h>

#include <random>

using ::absl::strings_internal::STLStringResizeUninitialized;
using ::babylon::resize_uninitialized;
using ::babylon::stable_reserve;

TEST(string, absl_resize_uninitialized) {
  ::std::string origin = "10086";
  ::std::string str(origin);
  STLStringResizeUninitialized(&str, 4);
  auto* data = &str[0];
  ASSERT_EQ(4, str.size());
  ASSERT_EQ("1008", str);
  ASSERT_EQ(data, str.data());
  STLStringResizeUninitialized(&str, 2);
  data = &str[0];
  ASSERT_EQ(2, str.size());
  ASSERT_EQ("10", str);
  ASSERT_EQ(data, str.data());
  STLStringResizeUninitialized(&str, 4);
  data = &str[0];
  ASSERT_EQ(4, str.size());
  ASSERT_EQ(::std::string_view("10\08", 4), str);
  ASSERT_EQ(data, str.data());
  ASSERT_EQ("10086", origin);
}

TEST(string, resize_uninitialized) {
  ::std::string origin = "10086";
  ::std::string str(origin);
  auto* data = resize_uninitialized(str, 4);
  ASSERT_EQ(4, str.size());
  ASSERT_EQ("1008", str);
  ASSERT_EQ(data, str.data());
  data = resize_uninitialized(str, 2);
  ASSERT_EQ(2, str.size());
  ASSERT_EQ("10", str);
  ASSERT_EQ(data, str.data());
  data = resize_uninitialized(str, 4);
  ASSERT_EQ(4, str.size());
  ASSERT_EQ(::std::string_view("10\08", 4), str);
  ASSERT_EQ(data, str.data());
  ASSERT_EQ("10086", origin);
}

TEST(string, resize_uninitialized_default_to_resize) {
  ::std::vector<char> vec = {'1', '0', '0', '8', '6'};
  auto* data = resize_uninitialized(vec, 4);
  ASSERT_EQ(4, vec.size());
  ASSERT_EQ((::std::vector<char> {'1', '0', '0', '8'}), vec);
  ASSERT_EQ(data, vec.data());
  data = resize_uninitialized(vec, 2);
  ASSERT_EQ(2, vec.size());
  ASSERT_EQ((::std::vector<char> {'1', '0'}), vec);
  data = resize_uninitialized(vec, 4);
  ASSERT_EQ(4, vec.size());
  ASSERT_EQ((::std::vector<char> {'1', '0', '\0', '\0'}), vec);
}

TEST(string, resize_uninitialized_zero_work_on_default) {
  ::std::string str;
  ASSERT_EQ(str.data(), resize_uninitialized(str, 0));
  ASSERT_EQ(0, str.size());
}

TEST(string, recognize_string_with_resize) {
  struct S {
    typedef char* pointer;
    typedef size_t size_type;
    void resize(size_t size) {
      this->size = size + 25;
    }
    char& operator[](size_t) {
      return (char&)size;
    }
    size_t size {0};
  } s;
  ASSERT_EQ(7 + 25, *(size_t*)resize_uninitialized(s, 7));
  ASSERT_EQ(12 + 25, *(size_t*)resize_uninitialized(s, 12));
}

TEST(string, recognize_string_with_resize_default_init_function) {
  struct S {
    typedef char* pointer;
    typedef size_t size_type;
    void __resize_default_init(size_t size) {
      this->size = size;
    }
    char& operator[](int) {
      return *(char*)size;
    }
    size_t size;
  } s;
  ASSERT_EQ(7, (size_t)resize_uninitialized(s, 7));
  ASSERT_EQ(12, (size_t)resize_uninitialized(s, 12));
}

TEST(string, stable_reserve_keep_stable_when_recreate) {
  for (size_t i = 0; i < 256; ++i) {
    ::std::string s;
    s.reserve(i);
    auto cap = s.capacity();
    ::std::string ss;
    stable_reserve(ss, cap);
    ASSERT_EQ(cap, ss.capacity());
  }
  auto max = ::sysconf(_SC_PAGE_SIZE) * 2;
  ::std::mt19937_64 gen(::std::random_device {}());
  for (size_t i = 0; i < 1024; ++i) {
    auto j = gen() % max;
    ::std::string s;
    s.reserve(j);
    auto cap = s.capacity();
    ::std::string ss;
    stable_reserve(ss, cap);
    ASSERT_EQ(cap, ss.capacity());
  }
}
