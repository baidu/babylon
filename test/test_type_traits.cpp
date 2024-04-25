#include "babylon/type_traits.h"

#include <gtest/gtest.h>

using ::babylon::Id;
using ::babylon::TypeId;

template <typename T>
struct S {
};

static const Id& F() {
    auto lambda = [&] (int) {};
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
#else // !__clang__
    ASSERT_EQ(type_name, "const babylon::Id&");
#endif // !__clang__
    ss.clear();
    ss.str("");
    ss << TypeId<S<TypeId<::std::string>>>::ID;
    type_name = ss.str();
#ifdef _GLIBCXX_RELEASE
#if _GLIBCXX_USE_CXX11_ABI
    ASSERT_EQ(type_name, "S<babylon::TypeId<std::__cxx11::basic_string<char> > >");
#else
    ASSERT_EQ(type_name, "S<babylon::TypeId<std::basic_string<char> > >");
#endif
#else // !_GLIBCXX_RELEASE
    ASSERT_EQ(type_name, "S<babylon::TypeId<std::string>>");
#endif // !_GLIBCXX_RELEASE
    ss.clear();
    ss.str("");
    ss << F();
    type_name = ss.str();
#ifdef __clang__
    ASSERT_EQ(type_name, "(lambda at baidu/third-party/babylon/test/test_type_traits.cpp:13:19)");
#else // !__clang__
    ASSERT_EQ(type_name, "F()::<lambda(int)>");
#endif // !__clang__
}
