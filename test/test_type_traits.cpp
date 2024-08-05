#include "babylon/type_traits.h"

#include <gtest/gtest.h>

using ::babylon::Id;
using ::babylon::TypeId;

template <typename T>
struct S {};

static const Id& F() {
  auto lambda = [&](int) {};
  return TypeId<decltype(lambda)>::ID;
}

TEST(type_traits, id_is_readable) {
  ::std::string type_name;
  ::std::stringstream ss;
  ss << TypeId<int32_t>::ID;
  type_name = ss.str();
  ASSERT_EQ(type_name, "int");
  ss.clear();
  ss.str("");
  ss << TypeId<const Id&>::ID;
  type_name = ss.str();
#ifdef __clang__
  ASSERT_EQ(type_name, "const babylon::Id &");
#else  // !__clang__
  ASSERT_EQ(type_name, "const babylon::Id&");
#endif // !__clang__
  ss.clear();
  ss.str("");
  ss << TypeId<S<TypeId<::std::string>>>::ID;
  type_name = ss.str();
#ifdef _GLIBCXX_RELEASE
#ifdef __clang__
  ASSERT_EQ(type_name, "S<babylon::TypeId<std::basic_string<char>>>");
#elif _GLIBCXX_USE_CXX11_ABI
  ASSERT_EQ(type_name,
            "S<babylon::TypeId<std::__cxx11::basic_string<char> > >");
#else
  ASSERT_EQ(type_name, "S<babylon::TypeId<std::basic_string<char> > >");
#endif
#else  // !_GLIBCXX_RELEASE
  ASSERT_EQ(type_name, "S<babylon::TypeId<std::string>>");
#endif // !_GLIBCXX_RELEASE
  ss.clear();
  ss.str("");
  ss << F();
  type_name = ss.str();
#ifdef __clang__
  ASSERT_EQ(type_name, "(lambda at test/test_type_traits.cpp:12:17)");
#else  // !__clang__
  ASSERT_EQ(type_name, "F()::<lambda(int)>");
#endif // !__clang__
}

TEST(type_traits, report_real_copyable_for_stl_containers) {
  using V = ::std::vector<::std::vector<int>>;
  using NV = ::std::vector<::std::vector<::std::unique_ptr<int>>>;
  ASSERT_TRUE(::babylon::IsCopyConstructible<V>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NV>::value);

  using L = ::std::list<::std::list<int>>;
  using NL = ::std::list<::std::list<::std::unique_ptr<int>>>;
  ASSERT_TRUE(::babylon::IsCopyConstructible<L>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NL>::value);

  using S = ::std::set<::std::set<int>>;
  using NS = ::std::set<::std::set<::std::unique_ptr<int>>>;
  ASSERT_TRUE(::babylon::IsCopyConstructible<S>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NS>::value);

  struct USH {
    size_t operator()(const ::std::unordered_set<int>&) const {
      return 0;
    }
  };
  using US = ::std::unordered_set<::std::unordered_set<int>, USH>;
  struct NUSH {
    size_t operator()(
        const ::std::unordered_set<::std::unique_ptr<int>>&) const {
      return 0;
    }
  };
  using NUS =
      ::std::unordered_set<::std::unordered_set<::std::unique_ptr<int>>, NUSH>;
  US {};
  NUS {};
  ASSERT_TRUE(::babylon::IsCopyConstructible<US>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NUS>::value);

  using MK = ::std::map<int, int>;
  using MT = ::std::map<int, int>;
  using M = ::std::map<MK, MT>;
  using NMK = ::std::map<::std::unique_ptr<int>, int>;
  using NMT = ::std::map<int, ::std::unique_ptr<int>>;
  using NM = ::std::map<NMK, NMT>;
  ASSERT_TRUE(::babylon::IsCopyConstructible<M>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NM>::value);

  using UMK = ::std::unordered_map<int, int>;
  using UMT = ::std::unordered_map<int, int>;
  struct UMH {
    size_t operator()(const UMK&) const {
      return 0;
    }
  };
  using UM = ::std::unordered_map<UMK, UMT, UMH>;
  using NUMK = ::std::unordered_map<::std::unique_ptr<int>, int>;
  using NUMT = ::std::unordered_map<int, ::std::unique_ptr<int>>;
  struct NUMH {
    size_t operator()(const NUMK&) const {
      return 0;
    }
  };
  using NUM = ::std::unordered_map<NUMK, NUMT, NUMH>;
  UM {};
  NUM {};
  ASSERT_TRUE(::babylon::IsCopyConstructible<UM>::value);
  ASSERT_FALSE(::babylon::IsCopyConstructible<NUM>::value);
}
