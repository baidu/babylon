#include "babylon/any.h"

#include <gtest/gtest.h>

using ::babylon::Any;
using ::babylon::TypeId;

template <int32_t I = 0>
struct Normal {
  Normal(int32_t) {
    construct_num++;
  }
  Normal(const Normal&) {
    copy_num++;
    construct_num++;
  }
  Normal(Normal&&) {
    move_num++;
    construct_num++;
  }
  virtual ~Normal() {
    destruct_num++;
  }
  static int64_t construct_num;
  static int64_t copy_num;
  static int64_t move_num;
  static int64_t destruct_num;
};
template <int32_t I>
int64_t Normal<I>::construct_num = 0;
template <int32_t I>
int64_t Normal<I>::copy_num = 0;
template <int32_t I>
int64_t Normal<I>::move_num = 0;
template <int32_t I>
int64_t Normal<I>::destruct_num = 0;

template <int32_t I = 0>
struct Sub1 : public Normal<I> {
  using Normal<I>::Normal;
};
template <int32_t I = 0>
struct Sub2 : public Normal<I> {
  using Normal<I>::Normal;
};

struct NonVirtual {
  NonVirtual(int32_t) {}
  NonVirtual(const NonVirtual&) {}
  NonVirtual(NonVirtual&&) {}
};
struct Sub3 : public NonVirtual {
  using NonVirtual::NonVirtual;
};

struct NonCopyNorMove {
  NonCopyNorMove(int32_t) {}
  NonCopyNorMove(const NonCopyNorMove&) = delete;
  NonCopyNorMove(NonCopyNorMove&&) = delete;
};

TEST(any, default_create_empty) {
  Any any;
  ASSERT_FALSE(any);
}

TEST(any, create_by_copy_instance) {
  Normal<0> obj(0);
  ASSERT_EQ(1, Normal<0>::construct_num);
  ASSERT_EQ(0, Normal<0>::copy_num);
  ASSERT_EQ(0, Normal<0>::destruct_num);
  {
    Any any(obj);
    ASSERT_TRUE(any);
    ASSERT_EQ(2, Normal<0>::construct_num);
    ASSERT_EQ(1, Normal<0>::copy_num);
    ASSERT_EQ(0, Normal<0>::destruct_num);
  }
  ASSERT_EQ(2, Normal<0>::construct_num);
  ASSERT_EQ(1, Normal<0>::copy_num);
  ASSERT_EQ(1, Normal<0>::destruct_num);
  {
    Any any(static_cast<const Normal<0>&>(obj));
    ASSERT_TRUE(any);
    ASSERT_EQ(3, Normal<0>::construct_num);
    ASSERT_EQ(2, Normal<0>::copy_num);
    ASSERT_EQ(1, Normal<0>::destruct_num);
  }
  ASSERT_EQ(3, Normal<0>::construct_num);
  ASSERT_EQ(2, Normal<0>::copy_num);
  ASSERT_EQ(2, Normal<0>::destruct_num);
}

#ifdef NDEBUG
TEST(any, copy_any_holding_non_copyable_instance_trigger_error) {
  struct S {
    S() {}
    S(const S&) = delete;
    S(S&&) = default;
  };
  {
    Any any((S()));
    Any other(any);
    ASSERT_TRUE(other);
  }
  {
    Any any(::std::unique_ptr<S>(new S));
    Any other(any);
    ASSERT_TRUE(other);
    ASSERT_EQ(nullptr, other.get<S>());
  }
}
#endif // NDEBUG

TEST(any, small_object_keep_inplace) {
  struct S {
    size_t value;
  };
  Any any((S()));
  ASSERT_LT(reinterpret_cast<intptr_t>(&any),
            reinterpret_cast<intptr_t>(any.get<S>()));
  ASSERT_GT(reinterpret_cast<intptr_t>(&any) + sizeof(any),
            reinterpret_cast<intptr_t>(any.get<S>()));
}

TEST(any, inplace_object_copy_right) {
  Any any(::std::string("10086"));
  ASSERT_EQ("10086", *Any(any).get<::std::string>());
}

TEST(any, create_with_or_without_value) {
  {
    ::std::unique_ptr<Normal<1>> obj(new Normal<1>(0));
    ASSERT_EQ(1, Normal<1>::construct_num);
    Any any(::std::move(obj)); // 指针move，本体无操作
    ASSERT_EQ(1, Normal<1>::construct_num);
  }
  ASSERT_EQ(1, Normal<1>::construct_num);
  ASSERT_EQ(1, Normal<1>::destruct_num);
  {
    Normal<1> obj(0);
    ASSERT_EQ(2, Normal<1>::construct_num);
    Any any(::std::move(obj)); // 本体move
    ASSERT_EQ(3, Normal<1>::construct_num);
    ASSERT_EQ(1, Normal<1>::move_num);
  }
  ASSERT_EQ(3, Normal<1>::construct_num);
  ASSERT_EQ(3, Normal<1>::destruct_num);
  {
    Any any(1); // primitive
  }
}

TEST(any, reusable) {
  struct S {
    S(size_t& destruct_num) : _destruct_num(destruct_num) {}
    S(const S& other) : _destruct_num(other._destruct_num) {}
    ~S() {
      _destruct_num++;
    }
    size_t& _destruct_num;
  };
  size_t destruct_num = 0;
  S obj(destruct_num);
  Any any;
  ASSERT_TRUE(!any);
  any = obj;
  ASSERT_TRUE(any);
  ASSERT_NE(nullptr, any.get<S>());
  any = 1;
  ASSERT_EQ(1, destruct_num);
  ASSERT_TRUE(any);
  ASSERT_EQ(1, any.as<int32_t>());
  any = ::std::move(obj);
  ASSERT_TRUE(any);
  ASSERT_NE(nullptr, any.get<S>());
  any.clear();
  ASSERT_EQ(2, destruct_num);
  ASSERT_FALSE(any);
}

TEST(any, get_only_support_exact_type_matching) {
  {
    Sub1<>* obj = new Sub1<>(0);
    Any any((::std::unique_ptr<Sub1<>>(obj)));
    ASSERT_EQ(obj, any.get<Sub1<>>());
    // 不同类型不可以
    ASSERT_EQ(nullptr, any.get<Sub2<>>());
    // 指定基类也不可以
    ASSERT_EQ(nullptr, any.get<Normal<>>());
  }
  {
    Sub1<>* obj = new Sub1<>(0);
    // 按照基类存入
    Any any((::std::unique_ptr<Normal<>>(obj)));
    // 就不能直接按照子类读出
    ASSERT_EQ(nullptr, any.get<Sub1<>>());
    ASSERT_EQ(nullptr, any.get<Sub2<>>());
    // 但是心里有数可以出来再转
    ASSERT_EQ(obj, dynamic_cast<Sub1<>*>(any.get<Normal<>>()));
  }
  {
    Any any(static_cast<int32_t>(1));
    ASSERT_EQ(nullptr, any.get<int64_t>());
    ASSERT_NE(nullptr, any.get<int32_t>());
  }
}

TEST(any, non_virtual_object_is_ok) {
  // 无需rtti支持（dynamic_cast & typeid）
  // 无需虚函数表
  Any any(Sub3(0));
  ASSERT_NE(nullptr, any.get<Sub3>());
  ASSERT_EQ(nullptr, any.get<NonVirtual>());
}

TEST(any, instance_type_same_means_identical) {
  Any any0(Normal<>(0));
  Any any1(Normal<>(1));
  Any any2(static_cast<int32_t>(1));
  ASSERT_TRUE(any0.instance_type() == any1.instance_type());
  ASSERT_TRUE(&any0.instance_type() == &any1.instance_type());
  ASSERT_FALSE(any0.instance_type() == any2.instance_type());
  ASSERT_TRUE(TypeId<Normal<>>::ID == any0.instance_type());
  ASSERT_TRUE(TypeId<int32_t>::ID == any2.instance_type());
}

TEST(any, copyable_when_object_is_copyable) {
  Any any(Normal<2>(0));
  ASSERT_NE(nullptr, any.get<Normal<2>>());
  ASSERT_EQ(2, Normal<2>::construct_num);
  ASSERT_EQ(0, Normal<2>::copy_num);
  ASSERT_EQ(1, Normal<2>::destruct_num);
  {
    // copy构造
    Any any2(any);
    ASSERT_NE(nullptr, any2.get<Normal<2>>());
    ASSERT_EQ(3, Normal<2>::construct_num);
    ASSERT_EQ(1, Normal<2>::copy_num);
  }
  ASSERT_EQ(2, Normal<2>::destruct_num);
  {
    // copy赋值
    Any any2;
    any2 = any;
    ASSERT_NE(nullptr, any2.get<Normal<2>>());
    ASSERT_EQ(4, Normal<2>::construct_num);
    ASSERT_EQ(2, Normal<2>::copy_num);
  }
  ASSERT_EQ(3, Normal<2>::destruct_num);
  ASSERT_NE(nullptr, any.get<Normal<2>>());
  {
    // const copy赋值
    Any any2;
    any2 = const_cast<const Any&>(any);
    ASSERT_NE(nullptr, any2.get<Normal<2>>());
    ASSERT_EQ(5, Normal<2>::construct_num);
    ASSERT_EQ(3, Normal<2>::copy_num);
  }
  ASSERT_EQ(4, Normal<2>::destruct_num);
  ASSERT_NE(nullptr, any.get<Normal<2>>());
}

TEST(any, movable_even_object_is_not_movable) {
  {
    // move构造
    ::std::unique_ptr<NonCopyNorMove> obj(new NonCopyNorMove(0));
    Any any(std::move(obj));
    ASSERT_NE(nullptr, any.get<NonCopyNorMove>());
    Any any2(::std::move(any));
    ASSERT_EQ(nullptr, any.get<NonCopyNorMove>());
    ASSERT_NE(nullptr, any2.get<NonCopyNorMove>());
  }
  {
    // move赋值
    ::std::unique_ptr<NonCopyNorMove> obj(new NonCopyNorMove(0));
    Any any(std::move(obj));
    ASSERT_NE(nullptr, any.get<NonCopyNorMove>());
    Any any2;
    any2 = ::std::move(any);
    ASSERT_EQ(nullptr, any.get<NonCopyNorMove>());
    ASSERT_NE(nullptr, any2.get<NonCopyNorMove>());
  }
}

TEST(any, keep_reference_instead_of_instance) {
  Normal<3> obj(0);
  ASSERT_EQ(1, Normal<3>::construct_num);
  {
    Any any;
    any.ref(obj);
    ASSERT_EQ(&obj, any.get<Normal<3>>());
  }
  ASSERT_EQ(1, Normal<3>::construct_num);
  ASSERT_EQ(0, Normal<3>::destruct_num);
}

TEST(any, const_ref_or_const_any_can_get_as_const_only) {
  Normal<> obj(0);
  {
    Any any;
    any.ref(obj);
    ASSERT_TRUE(!any.is_const_reference());
    ASSERT_EQ(&obj, any.get<Normal<>>());
    ASSERT_EQ(&obj, any.cget<Normal<>>());
  }
  {
    const Normal<>& cobj = obj;
    Any any;
    any.ref(cobj);
    ASSERT_TRUE(any.is_const_reference());
    ASSERT_EQ(nullptr, any.get<Normal<>>());
    ASSERT_EQ(&obj, any.cget<Normal<>>());
    const Any& cany = any;
    ASSERT_TRUE(cany.is_const_reference());
    ASSERT_EQ(&obj, cany.get<Normal<>>());
    ASSERT_EQ(&obj, cany.cget<Normal<>>());
  }
}

TEST(any, any_can_ref_to_other) {
  Normal<> obj(0);
  {
    Any any0(obj);
    Any any1;
    any1.ref(any0);
    ASSERT_TRUE(!any1.is_const_reference());
    ASSERT_TRUE(any1.is_reference());
    ASSERT_EQ(any0.get<Normal<>>(), any1.get<Normal<>>());
    ASSERT_EQ(any0.get<Normal<>>(), any1.cget<Normal<>>());
  }
  // ref chain
  {
    Any any0;
    any0.ref(obj);
    Any any1;
    any1.ref(any0);
    ASSERT_TRUE(!any1.is_const_reference());
    ASSERT_TRUE(any1.is_reference());
    ASSERT_EQ(&obj, any1.get<Normal<>>());
    ASSERT_EQ(&obj, any1.cget<Normal<>>());
  }
  // cref is const
  {
    Any any0;
    any0.ref(obj);
    Any any1;
    any1.cref(any0);
    ASSERT_TRUE(any1.is_const_reference());
    ASSERT_TRUE(any1.is_reference());
    ASSERT_EQ(nullptr, any1.get<Normal<>>());
    ASSERT_EQ(&obj, any1.cget<Normal<>>());
  }
  // cref会传播
  {
    Any any0;
    any0.cref(obj);
    Any any1;
    any1.ref(any0);
    ASSERT_TRUE(any0.is_const_reference());
    ASSERT_TRUE(any1.is_const_reference());
    ASSERT_TRUE(any1.is_reference());
    ASSERT_EQ(nullptr, any1.get<Normal<>>());
    ASSERT_EQ(&obj, any1.cget<Normal<>>());
  }
}

TEST(any, primitive_value_handle_separately) {
  {
    int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any(static_cast<int64_t>(0xFEFEFEFEFEFEFEFEL));
    ASSERT_TRUE(any);
    ASSERT_EQ(Any::Type::INT64, any.type());
    ASSERT_NE(nullptr, any.get<int64_t>());
    ASSERT_EQ(value, *any.get<int64_t>());
  }
  {
    int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any(value);
    ASSERT_TRUE(any);
    ASSERT_EQ(Any::Type::INT64, any.type());
    ASSERT_NE(nullptr, any.get<int64_t>());
    ASSERT_EQ(value, *any.get<int64_t>());
  }
  {
    const int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any(value);
    ASSERT_TRUE(any);
    ASSERT_EQ(Any::Type::INT64, any.type());
    ASSERT_NE(nullptr, any.get<int64_t>());
    ASSERT_EQ(value, *any.get<int64_t>());
  }
  {
    int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any;
    any = value;
    ASSERT_TRUE(any);
    ASSERT_EQ(Any::Type::INT64, any.type());
    ASSERT_NE(nullptr, any.get<int64_t>());
    ASSERT_EQ(value, *any.get<int64_t>());
  }
  {
    const int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any;
    any = value;
    ASSERT_TRUE(any);
    ASSERT_EQ(Any::Type::INT64, any.type());
    ASSERT_NE(nullptr, any.get<int64_t>());
    ASSERT_EQ(value, *any.get<int64_t>());
  }
  {
    int64_t value = 0xFEFEFEFEFEFEFEFEL;
    Any any;
    any = static_cast<int32_t>(value);
    ASSERT_EQ(Any::Type::INT32, any.type());
    ASSERT_NE(nullptr, any.get<int32_t>());
    any = static_cast<int16_t>(value);
    ASSERT_EQ(Any::Type::INT16, any.type());
    ASSERT_NE(nullptr, any.get<int16_t>());
    any = static_cast<int8_t>(value);
    ASSERT_EQ(Any::Type::INT8, any.type());
    ASSERT_NE(nullptr, any.get<int8_t>());
    any = static_cast<uint64_t>(value);
    ASSERT_EQ(Any::Type::UINT64, any.type());
    ASSERT_NE(nullptr, any.get<uint64_t>());
    any = static_cast<uint32_t>(value);
    ASSERT_EQ(Any::Type::UINT32, any.type());
    ASSERT_NE(nullptr, any.get<uint32_t>());
    any = static_cast<uint16_t>(value);
    ASSERT_EQ(Any::Type::UINT16, any.type());
    ASSERT_NE(nullptr, any.get<uint16_t>());
    any = static_cast<uint8_t>(value);
    ASSERT_EQ(Any::Type::UINT8, any.type());
    ASSERT_NE(nullptr, any.get<uint8_t>());
    any = static_cast<bool>(value);
    ASSERT_EQ(Any::Type::BOOLEAN, any.type());
    ASSERT_NE(nullptr, any.get<bool>());
    any = static_cast<double>(value);
    ASSERT_EQ(Any::Type::DOUBLE, any.type());
    ASSERT_NE(nullptr, any.get<double>());
    any = static_cast<float>(value);
    ASSERT_EQ(Any::Type::FLOAT, any.type());
    ASSERT_NE(nullptr, any.get<float>());
  }
}

TEST(any, primitive_value_implicit_type_cast_to_other) {
  {
    Any any;
    any = -1;
    int64_t out = 0;
    ASSERT_EQ(0, any.to(out));
    ASSERT_EQ(-1, out);
    any = 0;
    ASSERT_EQ(0, any.to(out));
    ASSERT_EQ(0, out);
    any = "str";
    ASSERT_EQ(-1, any.to(out));
  }
}

TEST(any, primitive_value_can_be_cast_to_other) {
  {
    int64_t value = -1;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFFFFFFFFFFFFFFL;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    int32_t value = -1;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFFFFFF;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    int16_t value = -1;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFF;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    int8_t value = -1;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7F;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    uint64_t value = 0xFFFFFFFFFFFFFFFFL;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFFFFFFFFFFFFFFL;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    uint32_t value = 0xFFFFFFFF;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFFFFFF;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    uint16_t value = 0xFFFF;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7FFF;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    uint8_t value = 0xFF;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0x7F;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    bool value = true;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = false;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    double value = 123.456;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0.123456;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
  {
    float value = 123.456;
    Any any(value);
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
    value = 0.123456;
    any = value;
    ASSERT_EQ(static_cast<uint64_t>(value), any.as<uint64_t>());
    ASSERT_EQ(static_cast<uint32_t>(value), any.as<uint32_t>());
    ASSERT_EQ(static_cast<uint16_t>(value), any.as<uint16_t>());
    ASSERT_EQ(static_cast<uint8_t>(value), any.as<uint8_t>());
    ASSERT_EQ(static_cast<int64_t>(value), any.as<int64_t>());
    ASSERT_EQ(static_cast<int32_t>(value), any.as<int32_t>());
    ASSERT_EQ(static_cast<int16_t>(value), any.as<int16_t>());
    ASSERT_EQ(static_cast<int8_t>(value), any.as<int8_t>());
    ASSERT_EQ(static_cast<bool>(value), any.as<bool>());
    ASSERT_EQ(static_cast<double>(value), any.as<double>());
    ASSERT_EQ(static_cast<float>(value), any.as<float>());
  }
}

TEST(any, instance_as_primitive_got_zero) {
  ::std::string str("10086");
  Any any(str);
  ASSERT_EQ(0, any.as<uint64_t>());
}

struct StaticInstance {
  inline StaticInstance() {
    string = ::std::string("123");
    pstring = string.get<::std::string>();
    primitive = -123;
    primitive_value = primitive.as<uint64_t>();
    ref.ref(*string.get<::std::string>());
    pref = ref.get<::std::string>();
  }

  Any string;
  ::std::string* pstring;
  Any primitive;
  uint64_t primitive_value;
  Any ref;
  ::std::string* pref;
};

static StaticInstance static_instance;

#if BABYLON_HAS_INIT_PRIORITY
TEST(any, static_initialize_works_fine) {
  ASSERT_NE(nullptr, static_instance.string.get<::std::string>());
  ASSERT_EQ(static_instance.pstring,
            static_instance.string.get<::std::string>());
  ASSERT_NE(0, static_instance.primitive.as<uint64_t>());
  ASSERT_EQ(static_instance.primitive_value,
            static_instance.primitive.as<uint64_t>());
  ASSERT_NE(nullptr, static_instance.ref.get<::std::string>());
  ASSERT_EQ(static_instance.pref, static_instance.ref.get<::std::string>());
  ASSERT_EQ(static_instance.pref, static_instance.pstring);
}
#endif // BABYLON_HAS_INIT_PRIORITY

TEST(any, primitive_pass_with_unique_ptr) {
  Any any(::std::unique_ptr<int32_t>(new int32_t(-10086)));
  ASSERT_EQ(-10086, *any.cget<int32_t>());
  ASSERT_EQ(-10086, any.as<int64_t>());
  ASSERT_EQ(-10086, *Any(any).cget<int32_t>());
  ASSERT_EQ(-10086, Any(any).as<int64_t>());
  ASSERT_EQ(-10086, *Any().ref(any).cget<int32_t>());
  ASSERT_EQ(-10086, Any().ref(any).as<int64_t>());
  ASSERT_EQ(-10086, *Any(::std::move(any)).cget<int32_t>());
  any = ::std::unique_ptr<int32_t>(new int32_t(-10086));
  ASSERT_EQ(-10086, Any(::std::move(any)).as<int64_t>());
}

TEST(any, construct_with_descriptor) {
  auto desc = Any::descriptor<::std::string>();
  auto str = new std::string;
  Any any {desc, str};
  auto ptr = any.get<::std::string>();
  *ptr = "10086";
  ASSERT_EQ(str, ptr);
  ASSERT_EQ("10086", *ptr);
}

TEST(any, assign_with_descriptor) {
  auto desc = Any::descriptor<::std::string>();
  auto str = new std::string;
  Any any;
  any.assign(desc, str);
  auto ptr = any.get<::std::string>();
  *ptr = "10086";
  ASSERT_EQ(str, ptr);
  ASSERT_EQ("10086", *ptr);
}

TEST(any, ref_with_descriptor) {
  auto desc = Any::descriptor<::std::string>();
  ::std::string s = "10086";
  void* ptr = &s;
  Any any;
  any.ref(desc, ptr);
  auto ps = any.get<::std::string>();
  ps->append("10010");
  ASSERT_EQ(ptr, ps);
  ASSERT_EQ("1008610010", *ps);
}

TEST(any, cref_with_descriptor) {
  auto desc = Any::descriptor<::std::string>();
  const ::std::string s = "10086";
  const void* ptr = &s;
  Any any;
  any.ref(desc, ptr);
  auto ps = any.cget<::std::string>();
  ASSERT_EQ(ptr, ps);
  ASSERT_EQ("10086", *ps);
}

struct AnyTest : public ::testing::Test {
  static size_t destruct_times;

  struct NormalClass {
    NormalClass() = default;
    NormalClass(const NormalClass&) = default;
    ~NormalClass() {
      destruct_times++;
    }
    size_t v1;
    size_t v2;
  };

  struct InplaceClass {
    InplaceClass() = default;
    InplaceClass(const InplaceClass&) = default;
    ~InplaceClass() {
      destruct_times++;
    }
    size_t v;
  };

  struct InplaceTrivialClass {
    size_t v;
  };
};
size_t AnyTest::destruct_times = 0;

TEST_F(AnyTest, release_instance_inside) {
  using S = NormalClass;
  {
    Any any(S {});
    auto ptr = any.release<S>();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
    destruct_times = 0;
    any.clear();
    ASSERT_EQ(0, destruct_times);
    ptr.reset();
    ASSERT_EQ(1, destruct_times);
  }
  {
    Any any(S {});
    auto ptr = any.release();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
    destruct_times = 0;
    any.clear();
    ASSERT_EQ(0, destruct_times);
    ptr.reset();
    ASSERT_EQ(1, destruct_times);
  }
}

TEST_F(AnyTest, release_inplace_instance_inside) {
  using S = InplaceClass;
  {
    Any any(::std::unique_ptr<S> {new S});
    auto ptr = any.release<S>();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
    destruct_times = 0;
    any.clear();
    ASSERT_EQ(0, destruct_times);
    ptr.reset();
    ASSERT_EQ(1, destruct_times);
  }
  {
    Any any(::std::unique_ptr<S> {new S});
    auto ptr = any.release();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
    destruct_times = 0;
    any.clear();
    ASSERT_EQ(0, destruct_times);
    ptr.reset();
    ASSERT_EQ(1, destruct_times);
  }
}

TEST_F(AnyTest, release_inplace_trivial_instance_inside) {
  using S = InplaceTrivialClass;
  {
    Any any(::std::unique_ptr<S> {new S});
    auto ptr = any.release<S>();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
  }
  {
    Any any(::std::unique_ptr<S> {new S});
    auto ptr = any.release();
    ASSERT_NE(nullptr, ptr);
    ASSERT_EQ(nullptr, any.get<S>());
  }
}

TEST_F(AnyTest, release_empty_get_nullptr) {
  Any any;
  ASSERT_EQ(nullptr, any.release<NormalClass>());
  ASSERT_EQ(nullptr, any.release());
}

#pragma GCC diagnostic push
#if BABYLON_GCC_VERSION >= 120100 && BABYLON_GCC_VERSION < 120300
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif // BABYLON_GCC_VERSION >= 120100 && BABYLON_GCC_VERSION < 120300
TEST_F(AnyTest, release_wrong_type_get_nullptr_and_keep_instance_inside) {
  Any any(::std::string {"10086"});
  ASSERT_EQ(nullptr, any.release<NormalClass>());
  ASSERT_EQ("10086", *any.get<::std::string>());
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#if BABYLON_GCC_VERSION >= 120100 && BABYLON_GCC_VERSION < 120300
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif // BABYLON_GCC_VERSION >= 120100 && BABYLON_GCC_VERSION < 120300
TEST_F(AnyTest, release_reference_get_nullptr_and_keep_reference_inside) {
  ::std::string s {"10086"};
  Any any;
  any.ref(s);
  ASSERT_EQ(nullptr, any.release<::std::string>());
  ASSERT_EQ(nullptr, any.release());
  ASSERT_EQ("10086", *any.get<::std::string>());
}
#pragma GCC diagnostic pop

TEST_F(AnyTest, release_inplace_get_nullptr_and_keep_inplace_inside) {
  {
    Any any(InplaceClass {});
    any.get<InplaceClass>()->v = 10086;
    ASSERT_EQ(nullptr, any.release<InplaceClass>());
    ASSERT_EQ(nullptr, any.release());
    ASSERT_EQ(10086, any.get<InplaceClass>()->v);
  }
  {
    Any any(InplaceTrivialClass {});
    any.get<InplaceTrivialClass>()->v = 10086;
    ASSERT_EQ(nullptr, any.release<InplaceTrivialClass>());
    ASSERT_EQ(nullptr, any.release());
    ASSERT_EQ(10086, any.get<InplaceTrivialClass>()->v);
  }
}

TEST_F(AnyTest, get_unchecked_work_with_correct_type) {
  {
    Any any(NormalClass {});
    any.get<NormalClass>()->v1 = 10086;
    any.get<NormalClass>()->v2 = 10010;
    ASSERT_EQ(10086, any.get_unchecked<NormalClass>().v1);
    ASSERT_EQ(10010, any.get_unchecked<NormalClass>().v2);
  }
  {
    Any any(InplaceClass {});
    any.get<InplaceClass>()->v = 11010;
    ASSERT_EQ(11010, any.get_unchecked<InplaceClass>().v);
  }
  {
    InplaceClass c;
    Any any;
    any.ref(c);
    any.get<InplaceClass>()->v = 10086;
    ASSERT_EQ(10086, any.get_unchecked<InplaceClass>().v);
  }
  {
    Any any {static_cast<int>(0)};
    *any.get<int>() = 10086;
    ASSERT_EQ(10086, any.get_unchecked<int>());
  }
}
