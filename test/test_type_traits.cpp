#include "babylon/type_traits.h"

#include <gtest/gtest.h>

using ::babylon::CallableArgs;
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
#if CLANG_VERSION >= 140000 // && _GLIBCXX_RELEASE
  ASSERT_EQ(type_name, "S<babylon::TypeId<std::basic_string<char>>>");
#elif __clang__ // && CLANG_VERSION < 140000 && _GLIBCXX_RELEASE
#if __cplusplus >= \
    201703L // && __clang__ && CLANG_VERSION < 140000 && _GLIBCXX_RELEASE
  ASSERT_EQ(type_name,
            "S<babylon::TypeId<std::__cxx11::basic_string<char, "
            "std::char_traits<char>, std::allocator<char>>>>");
#else       // __cplusplus < 201703L && __clang__ && CLANG_VERSION < 140000 &&
            // _GLIBCXX_RELEASE
  ASSERT_EQ(type_name, "S<babylon::TypeId<std::__cxx11::basic_string<char>>>");
#endif      // __cplusplus < 201703L && __clang__ && CLANG_VERSION < 140000 &&
            // _GLIBCXX_RELEASE
#elif _GLIBCXX_USE_CXX11_ABI // && !__clang__
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
  ASSERT_NE(type_name.npos, type_name.find("test/test_type_traits.cpp:13:17)"));
#else  // !__clang__
  ASSERT_EQ(type_name, "F()::<lambda(int)>");
#endif // !__clang__
}

TEST(type_traits, id_is_formatable) {
  ASSERT_EQ("int", ::fmt::format("{}", ::babylon::TypeId<int>::ID));
#if __cpp_lib_format >= 201907L
  ASSERT_EQ("int", ::std::format("{}", ::babylon::TypeId<int>::ID));
#endif
}

struct CallableArgsTest : public ::testing::Test {
  using E = ::std::tuple<int, int&, int&&>;
};

TEST_F(CallableArgsTest, support_normal_function) {
  {
    struct S {
      static int run(int, int&, int&&);
    };
    using F = decltype(S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F*>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&&>::type>::value));
  }
  {
    struct S {
      static int run(int, int&, int&&) noexcept;
    };
    using F = decltype(S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F*>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&&>::type>::value));
  }
}

TEST_F(CallableArgsTest, support_member_function) {
  {
    struct S {
      int run(int, int&, int&&);
    };
    using E = ::std::tuple<S*, int, int&, int&&>;
    using F = decltype(&S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F&>::type>::value));
  }
  {
    struct S {
      int run(int, int&, int&&) noexcept;
    };
    using E = ::std::tuple<S*, int, int&, int&&>;
    using F = decltype(&S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F>::type>::value));
  }
  {
    struct S {
      int run(int, int&, int&&) const;
    };
    using E = ::std::tuple<const S*, int, int&, int&&>;
    using F = decltype(&S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F&>::type>::value));
  }
  {
    struct S {
      int run(int, int&, int&&) const noexcept;
    };
    using E = ::std::tuple<const S*, int, int&, int&&>;
    using F = decltype(&S::run);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<F>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const F>::type>::value));
  }
}

TEST_F(CallableArgsTest, support_function_object) {
  {
    struct S {
      int operator()(int, int&, int&&);
    };
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    struct S {
      int operator()(int, int&, int&&) noexcept;
    };
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    struct S {
      int operator()(int, int&, int&&) const;
    };
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    struct S {
      int operator()(int, int&, int&&) const noexcept;
    };
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
}

TEST_F(CallableArgsTest, support_lambda) {
  {
    auto l = [](int, int&, int&&) {};
    using S = decltype(l);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    auto l = [](int, int&, int&&) noexcept {};
    using S = decltype(l);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    auto l = [](int, int&, int&&) mutable {};
    using S = decltype(l);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
  {
    auto l = [](int, int&, int&&) mutable noexcept {};
    using S = decltype(l);
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<S&&>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S>::type>::value));
    ASSERT_TRUE((::std::is_same<E, CallableArgs<const S&>::type>::value));
  }
}
