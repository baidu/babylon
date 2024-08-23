#include <babylon/move_only_function.h>

#include <gtest/gtest.h>

using ::babylon::MoveOnlyFunction;

TEST(move_only_function, default_construct_empty_function) {
  MoveOnlyFunction<void(::std::string)> function;
  ASSERT_FALSE(function);
}

TEST(move_only_function, construct_from_moveable_callable_only) {
  typedef MoveOnlyFunction<void(::std::string)> MOF;
  {
    struct S {
      S() = default;
      S(S&&) = default;
      S(const S&) = default;
      void operator()(::std::string) const {}
    } s;
    auto value = ::std::is_constructible<MOF, S>::value;
    ASSERT_TRUE(value);
    MOF(::std::move(s))("10086");
    ASSERT_TRUE(MOF(::std::move(s)));
  }
  {
    struct S {
      S() = default;
      S(S&&) = default;
      S(const S&) = delete;
      void operator()(::std::string) const {}
    } s;
    auto value = ::std::is_constructible<MOF, S>::value;
    ASSERT_TRUE(value);
    MOF(::std::move(s))("10086");
    ASSERT_TRUE(MOF(::std::move(s)));
  }
  {
    struct S {
      // S() = default;
      S(S&&) = delete;
      // S(const S&) = default;
      // void operator()(::std::string) const {}
    };
    auto value = ::std::is_constructible<MOF, S>::value;
    ASSERT_FALSE(value);
  }
  {
    struct S {
      // S() = default;
      S(S&&) = delete;
      S(const S&) = delete;
      // void operator()(::std::string) const {}
    };
    auto value = ::std::is_constructible<MOF, S>::value;
    ASSERT_FALSE(value);
  }
}

TEST(move_only_function, callable_move_with_function) {
  typedef MoveOnlyFunction<::std::string(::std::string)> MOF;
  struct S {
    S(::std::string prefix) : _prefix(prefix) {}
    ::std::string operator()(::std::string value) {
      _prefix += value;
      return _prefix;
    }
    ::std::string _prefix;
  };
  {
    S s("10086");
    ASSERT_EQ("1008610010", s("10010"));
    MOF function(::std::move(s));
    ASSERT_EQ("10010", s("10010"));
    ASSERT_EQ("100861001010016", function("10016"));
    MOF moved_function(::std::move(function));
    ASSERT_FALSE(function);
    ASSERT_EQ("10086100101001610017", moved_function("10017"));
    MOF move_assigned_function;
    ASSERT_FALSE(move_assigned_function);
    move_assigned_function = ::std::move(moved_function);
    ASSERT_FALSE(moved_function);
    ASSERT_EQ("1008610010100161001710018", move_assigned_function("10018"));
  }
  {
    S s("10086");
    MOF function;
    ASSERT_FALSE(function);
    function = ::std::move(s);
    ASSERT_EQ("10010", s("10010"));
    ASSERT_EQ("1008610010", function("10010"));
  }
}

TEST(move_only_function, args_pass_as_forward) {
  struct S {
    S() = default;
    S(S&& s) : copy(s.copy), move(s.move + 1) {}
    S(const S& s) : copy(s.copy + 1), move(s.move) {}
    size_t copy = 0;
    size_t move = 0;
  };
  typedef MoveOnlyFunction<void(S, const S&, S&, S&&)> MOF;
  {
    S s;
    MOF([](S s1, const S& s2, S& s3, S&& s4) {
      ASSERT_EQ(0, s1.copy);
      ASSERT_EQ(1, s1.move);
      ASSERT_EQ(0, s2.copy);
      ASSERT_EQ(0, s2.move);
      ASSERT_EQ(0, s3.copy);
      ASSERT_EQ(0, s3.move);
      ASSERT_EQ(0, s4.copy);
      ASSERT_EQ(0, s4.move);
    })
    (S(), S(), s, S());
  }
  {
    S s[4];
    MOF([](S s1, const S& s2, S& s3, S&& s4) {
      ASSERT_EQ(1, s1.copy);
      ASSERT_EQ(1, s1.move);
      ASSERT_EQ(0, s2.copy);
      ASSERT_EQ(0, s2.move);
      ASSERT_EQ(0, s3.copy);
      ASSERT_EQ(0, s3.move);
      ASSERT_EQ(0, s4.copy);
      ASSERT_EQ(0, s4.move);
    })
    (s[0], s[1], s[2], ::std::move(s[3]));
  }
  {
    S s[4];
    MOF([](S s1, const S& s2, S& s3, S&& s4) {
      ASSERT_EQ(0, s1.copy);
      ASSERT_EQ(2, s1.move);
      ASSERT_EQ(0, s2.copy);
      ASSERT_EQ(0, s2.move);
      ASSERT_EQ(0, s3.copy);
      ASSERT_EQ(0, s3.move);
      ASSERT_EQ(0, s4.copy);
      ASSERT_EQ(0, s4.move);
    })
    (::std::move(s[0]), ::std::move(s[1]), s[2], ::std::move(s[3]));
  }
}

TEST(move_only_function, result_forward_out) {
  struct S {
    S() = default;
    S(S&& s) : copy(s.copy), move(s.move + 1) {}
    S(const S& s) : copy(s.copy + 1), move(s.move) {}
    size_t copy = 0;
    size_t move = 0;
  };
  typedef MoveOnlyFunction<S(void)> MOF;
  // RVO
  {
    auto s = MOF([]() {
      return S();
    })();
    ASSERT_EQ(0, s.copy);
    ASSERT_EQ(0, s.move);
  }
  // NRVO
  {
    auto s = MOF([]() {
      S obj;
      return obj;
    })();
    ASSERT_EQ(0, s.copy);
    ASSERT_EQ(0, s.move);
  }
  // move out
  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wpessimizing-move"
    auto s = MOF([]() {
      S s;
      return ::std::move(s);
    })();
    ASSERT_EQ(0, s.copy);
    ASSERT_EQ(1, s.move);
#pragma GCC diagnostic pop
  }
  // copy out
  {
    auto s = MOF([]() {
      static S s;
      return s;
    })();
    ASSERT_EQ(1, s.copy);
    ASSERT_EQ(0, s.move);
  }
}

TEST(move_only_function, support_normal_function) {
  struct S {
    static ::std::string func(::std::string s) {
      return "10086" + s;
    }
  };
  auto ret = MoveOnlyFunction<::std::string(::std::string)>(S::func)("10010");
  ASSERT_EQ("1008610010", ret);
}

TEST(move_only_function, support_normal_function_pointer) {
  struct S {
    static ::std::string func(::std::string s) {
      return "10086" + s;
    }
  };
  auto ret = MoveOnlyFunction<::std::string(::std::string)>(&S::func)("10010");
  ASSERT_EQ("1008610010", ret);
}

TEST(move_only_function, support_bind) {
  struct S {
    ::std::string func(::std::string s) {
      return x + s;
    }
    ::std::string x = "10086";
  } s;
  auto ret = MoveOnlyFunction<::std::string(::std::string)>(
      ::std::bind(&S::func, &s, ::std::placeholders::_1))("10010");
  ASSERT_EQ("1008610010", ret);
}
