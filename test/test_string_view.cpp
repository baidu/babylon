#include "test_string_view.h"

#include "gtest/gtest.h"

#include <sstream>

using ::babylon::StringView;

#if __cplusplus >= 201402L
inline constexpr StringView use_assing_operator_in_constexpr(
    StringView original) {
  StringView tmp;
  tmp = original;
  return tmp;
}

inline constexpr StringView use_remove_prefix_in_constexpr(StringView original,
                                                           size_t num) {
  original.remove_prefix(num);
  return original;
}

inline constexpr StringView use_remove_suffix_in_constexpr(StringView original,
                                                           size_t num) {
  original.remove_suffix(num);
  return original;
}

inline constexpr StringView use_swap_in_constexpr(StringView original) {
  StringView tmp;
  tmp.swap(original);
  return tmp;
}
#endif

TEST(string_view, construct_as_constexpr) {
  {
    constexpr StringView view {};
    ASSERT_TRUE(view.empty());
  }
  {
    constexpr StringView view = "10086";
    ASSERT_EQ(5, view.size());
  }
  {
    constexpr StringView view("10086");
    ASSERT_EQ(5, view.size());
  }
  {
    constexpr StringView view("10086", 4);
    ASSERT_EQ(4, view.length());
  }
  {
    constexpr StringView view("10086", 4);
    constexpr StringView same_view(view);
    ASSERT_EQ(4, same_view.length());
  }
#if __cplusplus >= 201402L
  {
    constexpr StringView view("10086", 4);
    constexpr StringView same_view = use_assing_operator_in_constexpr(view);
    ASSERT_EQ(4, same_view.length());
  }
#endif // __cplusplus
}

TEST(string_view, get_char_as_constexpr) {
  constexpr StringView view = "10086";
  {
    constexpr auto c = view[1];
    ASSERT_EQ('0', c);
  }
  {
    constexpr auto c = view.front();
    ASSERT_EQ('1', c);
  }
  {
    constexpr auto c = view.back();
    ASSERT_EQ('6', c);
  }
}

TEST(string_view, locate_char_report_error) {
  constexpr StringView view = "10086";
  char c = '\0';
  {
    c = view.at(1);
    ASSERT_EQ('0', c);
  }
#if __cplusplus < 202002L
  ASSERT_THROW(c = view.at(6), ::std::out_of_range);
#endif // __cplusplus < 202002L
}

TEST(string_view, get_data_and_size_as_constexpr) {
  constexpr auto& c_str = "10086";
  constexpr StringView view = c_str;
  ASSERT_EQ(c_str, view.data());
  ASSERT_EQ(strlen(c_str), view.size());
}

TEST(string_view, assign_get_same_pointer_and_size) {
  constexpr StringView view = "10086";
  {
    constexpr StringView same_view(view);
    ASSERT_EQ(view.data(), same_view.data());
    ASSERT_EQ(view.size(), same_view.size());
  }
  {
    StringView same_view;
    same_view = view;
    ASSERT_EQ(view.data(), same_view.data());
    ASSERT_EQ(view.size(), same_view.size());
  }
}

TEST(string_view, iterable) {
  constexpr StringView view = "10086";
  size_t all_count = 0;
  size_t zero_count = 0;
  for (const auto& c : view) {
    if (c == '0') {
      zero_count++;
    }
    all_count++;
  }
  ASSERT_EQ(2, zero_count);
  ASSERT_EQ(5, all_count);
}

TEST(string_view, reverse_iterable) {
  constexpr StringView view = "10086";
  size_t zero_count = 0;
  size_t all_count = 0;
  for (auto iter = view.rbegin(); iter != view.rend(); ++iter) {
    if (*iter == '0') {
      zero_count++;
    }
    all_count++;
  }
  ASSERT_EQ(2, zero_count);
  ASSERT_EQ(5, all_count);
}

TEST(string_view, cut_head_and_tail) {
  constexpr StringView view("10086");
  {
#if __cplusplus >= 201402L
    constexpr auto altered_view = use_remove_prefix_in_constexpr(view, 1);
#else
    StringView altered_view(view);
    altered_view.remove_prefix(1);
#endif
    ASSERT_STREQ("0086", altered_view.data());
    ASSERT_EQ(4, altered_view.size());
  }
  {
#if __cplusplus >= 201402L
    constexpr auto altered_view = use_remove_suffix_in_constexpr(view, 1);
#else
    StringView altered_view(view);
    altered_view.remove_suffix(1);
#endif
    ASSERT_STREQ("10086", altered_view.data());
    ASSERT_EQ(4, altered_view.size());
  }
}

TEST(string_view, swapable) {
#if __cplusplus >= 201402L
  {
    constexpr StringView view("10086");
    constexpr auto swapped_view = use_swap_in_constexpr(view);
    ASSERT_STREQ("10086", swapped_view.data());
  }
#endif
  StringView view("10086");
  StringView swapped_view("10087");
  swapped_view.swap(view);
  ASSERT_STREQ("10086", swapped_view.data());
  ASSERT_STREQ("10087", view.data());
}

TEST(string_view, get_sub_string) {
  constexpr StringView view = "10086";
  // 拷贝到buffer
  {
    char dest[100] = {'1', '2', '3', '4'};
    ASSERT_EQ(3, view.copy(dest, 3, 1));
    ASSERT_EQ(0, strncmp(dest, "0084", 4));
    ASSERT_EQ(1, view.copy(dest, 3, 4));
    ASSERT_EQ(0, strncmp(dest, "6084", 4));
#if __cplusplus < 202002L
    ASSERT_THROW(view.copy(dest, 3, 6), ::std::out_of_range);
#endif // __cplusplus < 202002L
  }
  // 取子view
  {
    constexpr StringView sub_view = view.substr(1, 3);
    ASSERT_EQ(0, strncmp(sub_view.data(), "008", sub_view.size()));
  }
}

TEST(string_view, comparable) {
  constexpr StringView view = "10086";
  {
    constexpr auto result = StringView("10086") > StringView("00086");
    ASSERT_TRUE(result);
    ASSERT_GE(StringView("10086"), StringView("00086"));
    ASSERT_NE(StringView("10086"), StringView("00086"));
    ASSERT_GT(StringView("10086", 4), StringView("00086"));
    ASSERT_GE(StringView("10086", 4), StringView("00086"));
    ASSERT_NE(StringView("10086", 4), StringView("00086"));
    ASSERT_GT(StringView("10086"), StringView("00086", 4));
    ASSERT_GE(StringView("10086"), StringView("00086", 4));
    ASSERT_NE(StringView("10086"), StringView("00086", 4));
  }
  {
    constexpr auto result = view == StringView("10086");
    ASSERT_TRUE(result);
    ASSERT_GE(StringView("10086"), StringView("10086"));
    ASSERT_LE(StringView("10086"), StringView("10086"));
    ASSERT_LT(StringView("10086", 4), StringView("10086"));
    ASSERT_LE(StringView("10086", 4), StringView("10086"));
    ASSERT_NE(StringView("10086", 4), StringView("10086"));
    ASSERT_GT(StringView("10086"), StringView("10086", 4));
    ASSERT_GE(StringView("10086"), StringView("10086", 4));
    ASSERT_NE(StringView("10086"), StringView("10086", 4));
  }
  {
    constexpr auto result = view < StringView("20086");
    ASSERT_TRUE(result);
    ASSERT_LE(StringView("10086"), StringView("20086"));
    ASSERT_NE(StringView("10086"), StringView("20086"));
    ASSERT_LT(StringView("10086", 4), StringView("20086"));
    ASSERT_LE(StringView("10086", 4), StringView("20086"));
    ASSERT_NE(StringView("10086", 4), StringView("20086"));
    ASSERT_LT(StringView("10086"), StringView("20086", 4));
    ASSERT_LE(StringView("10086"), StringView("20086", 4));
    ASSERT_NE(StringView("10086"), StringView("20086", 4));
  }
  {
    constexpr auto result = view.compare("10086");
    ASSERT_EQ(0, result);
  }
  {
    constexpr auto result = view.compare(1, 3, "008");
    ASSERT_EQ(0, result);
  }
  {
    constexpr auto result = view.compare(1, 3, "10087", 1, 3);
    ASSERT_EQ(0, result);
  }
  {
    constexpr auto result = view.compare(1, 3, "0087", 3);
    ASSERT_EQ(0, result);
  }
}

TEST(string_view, comparable_to_convertible_type) {
  ASSERT_LT("10085", StringView("10086"));
  ASSERT_LE("10086", StringView("10086"));
  ASSERT_EQ("10086", StringView("10086"));
  ASSERT_NE("10087", StringView("10086"));
  ASSERT_GE("10087", StringView("10086"));
  ASSERT_GT("10087", StringView("10086"));
  ASSERT_LT(StringView("10085"), "10086");
  ASSERT_LE(StringView("10086"), "10086");
  ASSERT_EQ(StringView("10086"), "10086");
  ASSERT_NE(StringView("10087"), "10086");
  ASSERT_GE(StringView("10087"), "10086");
  ASSERT_GT(StringView("10087"), "10086");
}

TEST(string_view, assign_and_append_to_string) {
  constexpr StringView v1 = "origin";
  constexpr StringView v2 = " append";
  constexpr StringView v3 = "assign";
  ::std::string string(v1);
  ASSERT_EQ("origin", string);
  string.append(v2);
  ASSERT_EQ("origin append", string);
  string.assign(v3);
  ASSERT_EQ("assign", string);
}

#ifndef DISABLE_SSTREAM
TEST(string_view, ostream_output) {
  constexpr StringView view = "10086";
  ::std::stringstream ss;
  ss << view;
  ASSERT_STREQ("10086", ss.str().c_str());
}
#endif // DISABLE_SSTREAM

TEST(string_view, hashable_as_string) {
  ASSERT_EQ(::std::hash<::std::string>()("10086"),
            ::std::hash<StringView>()(StringView("10086")));
  ASSERT_EQ(::std::hash<::std::string>()("10086"),
            ::std::hash<::std::string_view>()("10086"));
}

TEST(string_view, implicit_convert_from_string) {
  StringView view;
  auto need_string_view = [&](const StringView& input) {
    view = input;
  };
  ::std::string string = "10086";
  need_string_view(string);
  ASSERT_STREQ("10086", view.data());
}

TEST(string_view, explicit_convert_to_string) {
  {
    StringView sv = "10086";
    ::std::string s(sv);
    ASSERT_EQ("10086", s);
    s = "";
    ASSERT_EQ("", s);
    s = sv;
    ASSERT_EQ("10086", s);
  }
  {
    ::std::string_view sv = "10086";
    ::std::string s(sv);
    ASSERT_EQ("10086", s);
    s = "";
    ASSERT_EQ("", s);
    s = sv;
    ASSERT_EQ("10086", s);
  }
}

TEST(string_view, abi_compatible_for_different_cxx) {
  {
    ::std::string_view sv = "10086";
    ASSERT_EQ(5, ::get_std_string_view_size(sv));
    ASSERT_EQ(5, ::get_babylon_string_view_size(sv));
  }
  {
    StringView sv = "1008610010";
    ASSERT_EQ(10, ::get_std_string_view_size(sv));
    ASSERT_EQ(10, ::get_babylon_string_view_size(sv));
  }
  {
    ::std::string_view sv = "10086";
    ASSERT_EQ(5, ::get_std_string_view_ref_size(sv));
    ASSERT_EQ(5, ::get_babylon_string_view_ref_size(sv));
  }
  {
    StringView sv = "1008610010";
    ASSERT_EQ(10, ::get_std_string_view_ref_size(sv));
    ASSERT_EQ(10, ::get_babylon_string_view_ref_size(sv));
  }
}

TEST(string_view, std_and_babylon_string_view_convertible_to_each_other) {
  {
    ::std::string_view ssv = "10086";
    StringView v;
    ASSERT_EQ("", v);
    v = ssv;
    ASSERT_EQ("10086", v);
  }
  {
    StringView bsv = "10086";
    ::std::string_view v;
    ASSERT_EQ("", v);
    v = bsv;
    ASSERT_EQ("10086", v);
  }
}

TEST(string_view, formattable) {
  StringView view {"10086"};
  ASSERT_EQ("10086", ::fmt::format("{}", view));
#if __cpp_lib_format >= 201907L
  ASSERT_EQ("10086", ::std::format("{}", view));
#endif
}
