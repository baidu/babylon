#include "babylon/serialization.h"

#if BABYLON_USE_PROTOBUF

#include "babylon/logging/logger.h"

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"

#include <arena_example.pb.h>

#include <random>

using ::babylon::Serialization;
using ::babylon::SerializeTraits;
using ::babylon::TestEnum;
using ::babylon::TestMessage;

struct TestSubObject {
  bool b {false};   // = 1;
  int8_t i8 {0};    // = 2;
  int16_t i16 {0};  // = 3;
  int32_t i32 {0};  // = 4;
  int64_t i64 {0};  // = 5;
  uint8_t u8 {0};   // = 6;
  uint16_t u16 {0}; // = 7;
  uint32_t u32 {0}; // = 8;
  uint64_t u64 {0}; // = 9;
  // optional sint32 s32 = 10;
  // optional sint64 s64 = 11;
  // optional fixed32 f32 = 12;
  // optional fixed64 f64 = 13;
  // optional sfixed32 sf32 = 14;
  // optional sfixed64 sf64 = 15;
  float f {0};                       // = 16;
  double d {0};                      // = 17;
  int e {int(::babylon::E1)};        // = 18;
  ::std::string s;                   // = 19;
  ::std::string by;                  // = 20;
  TestMessage m;                     // = 21;
  ::std::unique_ptr<TestMessage> pm; // = 22;

  ::std::vector<bool> rb;       // = 23;
  ::std::vector<int8_t> ri8;    // = 24;
  ::std::vector<int16_t> ri16;  // = 25;
  ::std::vector<int32_t> ri32;  // = 26;
  ::std::vector<int64_t> ri64;  // = 27;
  ::std::vector<uint8_t> ru8;   // = 28;
  ::std::vector<uint16_t> ru16; // = 29;
  ::std::vector<uint32_t> ru32; // = 30;
  ::std::vector<uint64_t> ru64; // = 31;
  // repeated sint32 rs32 = 32;
  // repeated sint64 rs64 = 33;
  // repeated fixed32 rf32 = 34;
  // repeated fixed64 rf64 = 35;
  // repeated sfixed32 rsf32 = 36;
  // repeated sfixed64 rsf64 = 37;
  ::std::vector<float> rf;  // = 38;
  ::std::vector<double> rd; // = 39;
  ::std::vector<int> re;    // = 40;
  // repeated string rs = 41;
  // repeated bytes rby = 42;
  // repeated TestMessage rm = 43;

  ::std::vector<bool> rpb;       // = 44 [packed = true];
  ::std::vector<int8_t> rpi8;    // = 45 [packed = true];
  ::std::vector<int16_t> rpi16;  // = 46 [packed = true];
  ::std::vector<int32_t> rpi32;  // = 47 [packed = true];
  ::std::vector<int64_t> rpi64;  // = 48 [packed = true];
  ::std::vector<uint8_t> rpu8;   // = 49 [packed = true];
  ::std::vector<uint16_t> rpu16; // = 50 [packed = true];
  ::std::vector<uint32_t> rpu32; // = 51 [packed = true];
  ::std::vector<uint64_t> rpu64; // = 52 [packed = true];
  // repeated sint32 rps32 = 53 [packed = true];
  // repeated sint64 rps64 = 54 [packed = true];
  // repeated fixed32 rpf32 = 55 [packed = true];
  // repeated fixed64 rpf64 = 56 [packed = true];
  // repeated sfixed32 rpsf32 = 57 [packed = true];
  // repeated sfixed64 rpsf64 = 58 [packed = true];
  ::std::vector<float> rpf;  // = 59 [packed = true];
  ::std::vector<double> rpd; // = 60 [packed = true];
  ::std::vector<int> rpe;    // = 61 [packed = true];

  BABYLON_COMPATIBLE((b, 1)(i8, 2)(i16, 3)(i32, 4)(i64, 5)(u8, 6)(u16, 7)(
      u32, 8)(u64, 9)(f, 16)(d, 17)(e, 18)(s, 19)(by, 20)(m, 21)(pm, 22)(
      rb, 23)(ri8, 24)(ri16, 25)(ri32, 26)(ri64, 27)(ru8, 28)(ru16, 29)(
      ru32, 30)(ru64, 31)(rf, 38)(rd, 39)(re, 40)(rpb, 44)(rpi8, 45)(rpi16, 46)(
      rpi32, 47)(rpi64, 48)(rpu8, 49)(rpu16, 50)(rpu32, 51)(rpu64, 52)(rpf, 59)(
      rpd, 60)(rpe, 61))
};

struct TestObject {
  bool b {false};   // = 1;
  int8_t i8 {0};    // = 2;
  int16_t i16 {0};  // = 3;
  int32_t i32 {0};  // = 4;
  int64_t i64 {0};  // = 5;
  uint8_t u8 {0};   // = 6;
  uint16_t u16 {0}; // = 7;
  uint32_t u32 {0}; // = 8;
  uint64_t u64 {0}; // = 9;
  // optional sint32 s32 = 10;
  // optional sint64 s64 = 11;
  // optional fixed32 f32 = 12;
  // optional fixed64 f64 = 13;
  // optional sfixed32 sf32 = 14;
  // optional sfixed64 sf64 = 15;
  float f {0};                         // = 16;
  double d {0};                        // = 17;
  TestEnum e {::babylon::E1};          // = 18;
  ::std::string s;                     // = 19;
  ::std::string by;                    // = 20;
  TestSubObject m;                     // = 21;
  ::std::unique_ptr<TestSubObject> pm; // = 22;

  ::std::vector<bool> rb;       // = 23;
  ::std::vector<int8_t> ri8;    // = 24;
  ::std::vector<int16_t> ri16;  // = 25;
  ::std::vector<int32_t> ri32;  // = 26;
  ::std::vector<int64_t> ri64;  // = 27;
  ::std::vector<uint8_t> ru8;   // = 28;
  ::std::vector<uint16_t> ru16; // = 29;
  ::std::vector<uint32_t> ru32; // = 30;
  ::std::vector<uint64_t> ru64; // = 31;
  // repeated sint32 rs32 = 32;
  // repeated sint64 rs64 = 33;
  // repeated fixed32 rf32 = 34;
  // repeated fixed64 rf64 = 35;
  // repeated sfixed32 rsf32 = 36;
  // repeated sfixed64 rsf64 = 37;
  ::std::vector<float> rf;    // = 38;
  ::std::vector<double> rd;   // = 39;
  ::std::vector<TestEnum> re; // = 40;
  // repeated string rs = 41;
  // repeated bytes rby = 42;
  // repeated TestMessage rm = 43;

  ::std::vector<bool> rpb;       // = 44 [packed = true];
  ::std::vector<int8_t> rpi8;    // = 45 [packed = true];
  ::std::vector<int16_t> rpi16;  // = 46 [packed = true];
  ::std::vector<int32_t> rpi32;  // = 47 [packed = true];
  ::std::vector<int64_t> rpi64;  // = 48 [packed = true];
  ::std::vector<uint8_t> rpu8;   // = 49 [packed = true];
  ::std::vector<uint16_t> rpu16; // = 50 [packed = true];
  ::std::vector<uint32_t> rpu32; // = 51 [packed = true];
  ::std::vector<uint64_t> rpu64; // = 52 [packed = true];
  // repeated sint32 rps32 = 53 [packed = true];
  // repeated sint64 rps64 = 54 [packed = true];
  // repeated fixed32 rpf32 = 55 [packed = true];
  // repeated fixed64 rpf64 = 56 [packed = true];
  // repeated sfixed32 rpsf32 = 57 [packed = true];
  // repeated sfixed64 rpsf64 = 58 [packed = true];
  ::std::vector<float> rpf;    // = 59 [packed = true];
  ::std::vector<double> rpd;   // = 60 [packed = true];
  ::std::vector<TestEnum> rpe; // = 61 [packed = true];

  BABYLON_COMPATIBLE((b, 1)(i8, 2)(i16, 3)(i32, 4)(i64, 5)(u8, 6)(u16, 7)(
      u32, 8)(u64, 9)(f, 16)(d, 17)(e, 18)(s, 19)(by, 20)(m, 21)(pm, 22)(
      rb, 23)(ri8, 24)(ri16, 25)(ri32, 26)(ri64, 27)(ru8, 28)(ru16, 29)(
      ru32, 30)(ru64, 31)(rf, 38)(rd, 39)(re, 40)(rpb, 44)(rpi8, 45)(rpi16, 46)(
      rpi32, 47)(rpi64, 48)(rpu8, 49)(rpu16, 50)(rpu32, 51)(rpu64, 52)(rpf, 59)(
      rpd, 60)(rpe, 61))
};

struct MessageTest : public ::testing::Test {
  ::std::string string;
  ::std::mt19937_64 gen {::std::random_device {}()};

  void fill_leaf(TestMessage& m, ::std::mt19937_64& gen) {
    m.set_b(gen() % 2 ? 1 : 0);
    m.set_i8(gen() % INT8_MAX);
    m.set_i16(gen() % INT16_MAX);
    m.set_i32(gen());
    m.set_i64(gen());
    m.set_u8(gen() % UINT8_MAX);
    m.set_u16(gen() % UINT16_MAX);
    m.set_u32(gen());
    m.set_u64(gen());
    m.set_s32(gen());
    m.set_s64(gen());
    m.set_f32(gen());
    m.set_f64(gen());
    m.set_sf32(gen());
    m.set_sf64(gen());
    m.set_f(static_cast<int32_t>(gen()));
    m.set_d(static_cast<int32_t>(gen()));
    m.set_e(gen() % 2 ? ::babylon::E1 : ::babylon::E2);
    m.set_s(::std::to_string(gen()));
    m.set_by(::std::to_string(gen()));
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rs32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rs64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rf32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rf64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rsf32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rsf64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rs(::std::to_string(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rby(::std::to_string(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpb(gen() % 2 ? 1 : 0);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpi8(gen() % INT8_MAX);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpi16(gen() % INT16_MAX);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpi32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpi64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpu8(gen() % UINT8_MAX);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpu16(gen() % INT16_MAX);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpu32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpu64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rps32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rps64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpf32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpf64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpsf32(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpsf64(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpf(static_cast<int32_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpd(static_cast<int64_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      m.add_rpe(gen() % 2 ? ::babylon::E1 : ::babylon::E2);
    }
  }

  void fill_middle(TestMessage& m, ::std::mt19937_64& gen) {
    fill_leaf(m, gen);
    fill_leaf(*m.mutable_m(), gen);
    if (gen() % 2) {
      fill_leaf(*m.mutable_pm(), gen);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      auto rm = m.add_rm();
      fill_leaf(*rm, gen);
    }
  }

  void fill(TestMessage& m, ::std::mt19937_64& gen) {
    fill_leaf(m, gen);
    fill_middle(*m.mutable_m(), gen);
    if (gen() % 2) {
      fill_middle(*m.mutable_pm(), gen);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      auto rm = m.add_rm();
      fill_middle(*rm, gen);
    }
  }

  template <typename T>
  void fill_leaf(T& o, ::std::mt19937_64& gen) {
    o.b = gen() % 2 ? 1 : 0;
    o.i8 = gen();
    o.i16 = gen();
    o.i32 = gen();
    o.i64 = gen();
    o.u8 = gen();
    o.u16 = gen();
    o.u32 = gen();
    o.u64 = gen();
    o.f = static_cast<int32_t>(gen());
    o.d = static_cast<int64_t>(gen());
    o.e = static_cast<TestEnum>(gen() % 2 ? ::babylon::E1 : ::babylon::E2);
    o.s = ::std::to_string(gen());
    o.by = ::std::to_string(gen());
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rb.push_back(gen() % 2 ? 1 : 0);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ri8.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ri16.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ri32.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ri64.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ru8.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ru16.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ru32.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.ru64.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rf.emplace_back(static_cast<int32_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rd.emplace_back(static_cast<int64_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.re.emplace_back(gen() % 2 ? ::babylon::E1 : ::babylon::E2);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpb.push_back(gen() % 2 ? 1 : 0);
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpi8.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpi16.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpi32.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpi64.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpu8.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpu16.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpu32.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpu64.emplace_back(gen());
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpf.emplace_back(static_cast<int32_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpd.emplace_back(static_cast<int64_t>(gen()));
    }
    for (size_t i = 0; i < gen() % 10; ++i) {
      o.rpe.emplace_back(gen() % 2 ? ::babylon::E1 : ::babylon::E2);
    }
  }

  void fill_middle(TestSubObject& o, ::std::mt19937_64& gen) {
    fill_leaf(o, gen);
    fill_leaf(o.m, gen);
    if (gen() % 2) {
      o.pm.reset(new TestMessage);
      fill_leaf(*o.pm, gen);
    }
  }

  void fill(TestObject& o, ::std::mt19937_64& gen) {
    fill_leaf(o, gen);
    fill_middle(o.m, gen);
    if (gen() % 2) {
      o.pm.reset(new TestSubObject);
      fill_middle(*o.pm, gen);
    }
  }

  void assert_eq(const TestMessage& m, const TestMessage& mm) {
    ASSERT_EQ(m.b(), mm.b());
    ASSERT_EQ(m.i8(), mm.i8());
    ASSERT_EQ(m.i16(), mm.i16());
    ASSERT_EQ(m.i32(), mm.i32());
    ASSERT_EQ(m.i64(), mm.i64());
    ASSERT_EQ(m.u8(), mm.u8());
    ASSERT_EQ(m.u16(), mm.u16());
    ASSERT_EQ(m.u32(), mm.u32());
    ASSERT_EQ(m.u64(), mm.u64());
    ASSERT_EQ(m.s32(), mm.s32());
    ASSERT_EQ(m.s64(), mm.s64());
    ASSERT_EQ(m.f32(), mm.f32());
    ASSERT_EQ(m.f64(), mm.f64());
    ASSERT_EQ(m.sf32(), mm.sf32());
    ASSERT_EQ(m.sf64(), mm.sf64());
    ASSERT_EQ(m.f(), mm.f());
    ASSERT_EQ(m.d(), mm.d());
    ASSERT_EQ(m.e(), mm.e());
    ASSERT_EQ(m.s(), mm.s());
    ASSERT_EQ(m.by(), mm.by());
    if (m.has_m()) {
      assert_eq(m.m(), mm.m());
    } else {
      ASSERT_FALSE(mm.has_m());
    }
    if (m.has_pm()) {
      assert_eq(m.pm(), mm.pm());
    } else {
      ASSERT_FALSE(mm.has_pm());
    }
#define ASSERT_FIELD(name)                      \
  ASSERT_EQ(m.name().size(), mm.name().size()); \
  for (int i = 0; i < mm.name().size(); ++i) {  \
    ASSERT_EQ(m.name(i), mm.name(i));           \
  }
    ASSERT_FIELD(rb)
    ASSERT_FIELD(ri8)
    ASSERT_FIELD(ri16)
    ASSERT_FIELD(ri32)
    ASSERT_FIELD(ri64)
    ASSERT_FIELD(ru8)
    ASSERT_FIELD(ru16)
    ASSERT_FIELD(ru32)
    ASSERT_FIELD(ru64)
    ASSERT_FIELD(rs32)
    ASSERT_FIELD(rs64)
    ASSERT_FIELD(rf32)
    ASSERT_FIELD(rf64)
    ASSERT_FIELD(rsf32)
    ASSERT_FIELD(rsf64)
    ASSERT_FIELD(rf)
    ASSERT_FIELD(rd)
    ASSERT_FIELD(re)
    ASSERT_FIELD(rpb)
    ASSERT_FIELD(rpi8)
    ASSERT_FIELD(rpi16)
    ASSERT_FIELD(rpi32)
    ASSERT_FIELD(rpi64)
    ASSERT_FIELD(rpu8)
    ASSERT_FIELD(rpu16)
    ASSERT_FIELD(rpu32)
    ASSERT_FIELD(rpu64)
    ASSERT_FIELD(rps32)
    ASSERT_FIELD(rps64)
    ASSERT_FIELD(rpf32)
    ASSERT_FIELD(rpf64)
    ASSERT_FIELD(rpsf32)
    ASSERT_FIELD(rpsf64)
    ASSERT_FIELD(rpf)
    ASSERT_FIELD(rpd)
    ASSERT_FIELD(rpe)
#undef ASSERT_FIELD
  }

  template <typename T>
  void assert_eq_leaf(const T& m, const T& mm) {
    ASSERT_EQ(m.b, mm.b);
    ASSERT_EQ(m.i8, mm.i8);
    ASSERT_EQ(m.i16, mm.i16);
    ASSERT_EQ(m.i32, mm.i32);
    ASSERT_EQ(m.i64, mm.i64);
    ASSERT_EQ(m.u8, mm.u8);
    ASSERT_EQ(m.u16, mm.u16);
    ASSERT_EQ(m.u32, mm.u32);
    ASSERT_EQ(m.u64, mm.u64);
    ASSERT_EQ(m.f, mm.f);
    ASSERT_EQ(m.d, mm.d);
    ASSERT_EQ(m.e, mm.e);
    ASSERT_EQ(m.s, mm.s);
    ASSERT_EQ(m.by, mm.by);
    ASSERT_EQ(m.rb, mm.rb);
    ASSERT_EQ(m.ri8, mm.ri8);
    ASSERT_EQ(m.ri16, mm.ri16);
    ASSERT_EQ(m.ri32, mm.ri32);
    ASSERT_EQ(m.ri64, mm.ri64);
    ASSERT_EQ(m.ru8, mm.ru8);
    ASSERT_EQ(m.ru16, mm.ru16);
    ASSERT_EQ(m.ru32, mm.ru32);
    ASSERT_EQ(m.ru64, mm.ru64);
    ASSERT_EQ(m.rf, mm.rf);
    ASSERT_EQ(m.rd, mm.rd);
    ASSERT_EQ(m.re, mm.re);
    ASSERT_EQ(m.rpb, mm.rpb);
    ASSERT_EQ(m.rpi8, mm.rpi8);
    ASSERT_EQ(m.rpi16, mm.rpi16);
    ASSERT_EQ(m.rpi32, mm.rpi32);
    ASSERT_EQ(m.rpi64, mm.rpi64);
    ASSERT_EQ(m.rpu8, mm.rpu8);
    ASSERT_EQ(m.rpu16, mm.rpu16);
    ASSERT_EQ(m.rpu32, mm.rpu32);
    ASSERT_EQ(m.rpu64, mm.rpu64);
    ASSERT_EQ(m.rpf, mm.rpf);
    ASSERT_EQ(m.rpd, mm.rpd);
    ASSERT_EQ(m.rpe, mm.rpe);
  }

  void assert_eq(const TestSubObject& m, const TestSubObject& mm) {
    assert_eq_leaf(m, mm);
    assert_eq(m.m, mm.m);
    if (m.pm) {
      ASSERT_TRUE(mm.pm);
      assert_eq(*m.pm, *mm.pm);
    } else {
      ASSERT_FALSE(mm.pm);
    }
  }

  void assert_eq(const TestObject& m, const TestObject& mm) {
    assert_eq_leaf(m, mm);
    assert_eq(m.m, mm.m);
    if (m.pm) {
      ASSERT_TRUE(mm.pm);
      assert_eq(*m.pm, *mm.pm);
    } else {
      ASSERT_FALSE(mm.pm);
    }
  }

  template <typename T>
  void assert_eq(const T& m, const TestMessage& mm) {
    ASSERT_EQ(m.b, mm.b());
    ASSERT_EQ(m.i8, mm.i8());
    ASSERT_EQ(m.i16, mm.i16());
    ASSERT_EQ(m.i32, mm.i32());
    ASSERT_EQ(m.i64, mm.i64());
    ASSERT_EQ(m.u8, mm.u8());
    ASSERT_EQ(m.u16, mm.u16());
    ASSERT_EQ(m.u32, mm.u32());
    ASSERT_EQ(m.u64, mm.u64());
    ASSERT_EQ(m.f, mm.f());
    ASSERT_EQ(m.d, mm.d());
    ASSERT_EQ(m.e, mm.e());
    ASSERT_EQ(m.s, mm.s());
    ASSERT_EQ(m.by, mm.by());
    assert_eq(m.m, mm.m());
    if (m.pm) {
      assert_eq(*m.pm, mm.pm());
    } else {
      ASSERT_FALSE(mm.has_pm());
    }
#define ASSERT_FIELD(name)                     \
  ASSERT_EQ(m.name.size(), mm.name().size());  \
  for (size_t i = 0; i < m.name.size(); ++i) { \
    ASSERT_EQ(m.name[i], mm.name(i));          \
  }
    ASSERT_FIELD(rb)
    ASSERT_FIELD(ri8)
    ASSERT_FIELD(ri16)
    ASSERT_FIELD(ri32)
    ASSERT_FIELD(ri64)
    ASSERT_FIELD(ru8)
    ASSERT_FIELD(ru16)
    ASSERT_FIELD(ru32)
    ASSERT_FIELD(ru64)
    ASSERT_FIELD(rf)
    ASSERT_FIELD(rd)
    ASSERT_FIELD(re)
    ASSERT_FIELD(rpb)
    ASSERT_FIELD(rpi8)
    ASSERT_FIELD(rpi16)
    ASSERT_FIELD(rpi32)
    ASSERT_FIELD(rpi64)
    ASSERT_FIELD(rpu8)
    ASSERT_FIELD(rpu16)
    ASSERT_FIELD(rpu32)
    ASSERT_FIELD(rpu64)
    ASSERT_FIELD(rpf)
    ASSERT_FIELD(rpd)
    ASSERT_FIELD(rpe)
#undef ASSERT_FIELD
  }
};

TEST_F(MessageTest, serializable) {
  ASSERT_TRUE(SerializeTraits<TestMessage>::SERIALIZABLE);
  TestMessage m;
  fill(m, gen);
  ASSERT_TRUE(Serialization::serialize_to_string(m, string));
  TestMessage mm;
  ASSERT_TRUE(Serialization::parse_from_string(string, mm));
  assert_eq(m, mm);
}

TEST_F(MessageTest, support_print) {
  TestMessage m;
  fill(m, gen);
  ASSERT_TRUE(Serialization::print_to_string(m, string));
  BABYLON_LOG(INFO) << string;
}

TEST_F(MessageTest, struct_to_message) {
  TestObject m;
  fill_leaf(m, gen);
  ASSERT_TRUE(Serialization::serialize_to_string(m, string));
  TestMessage mm;
  ASSERT_TRUE(Serialization::parse_from_string(string, mm));
  assert_eq(m, mm);
}

TEST_F(MessageTest, message_to_struct) {
  TestMessage m;
  fill_leaf(m, gen);
  ASSERT_TRUE(Serialization::serialize_to_string(m, string));
  TestObject mm;
  ASSERT_TRUE(Serialization::parse_from_string(string, mm));
  assert_eq(mm, m);
}

#endif // BABYLON_USE_PROTOBUF
