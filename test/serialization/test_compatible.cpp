#include <babylon/serialization.h>

#include <arena_example.pb.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <fstream>

using ::babylon::Serialization;
using ::babylon::TestMessage;

struct SerializeTest : public ::testing::Test {
    ::std::string string;
};

struct IntSerializable {
    IntSerializable() = default;
    IntSerializable(int v) : v{v} {}

    int v {0};

    BABYLON_COMPATIBLE((v, 1))
};
TEST_F(SerializeTest, int) {
    using S = IntSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_int", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, s.v);
        } else {
            s.v = 10086;
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_int", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct StringSerializable {
    ::std::string v;

    BABYLON_COMPATIBLE((v, 1))
};
TEST_F(SerializeTest, string) {
    using S = StringSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_string", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ("", s.v);
        } else {
            s.v.clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_string", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_string", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ("10086", s.v);
        } else {
            s.v = "10086";
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_string", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct ArraySerializable {
    int a[10];
    IntSerializable as[10];

    BABYLON_COMPATIBLE((a, 1)(as, 2))
};
TEST_F(SerializeTest, array) {
    using S = ArraySerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_array", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, s.a[1]);
            ASSERT_EQ(10010, s.as[1].v);
        } else {
            s.a[1] = 10086;
            s.as[1].v = 10010;
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_array", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct VectorSerializable {
    ::std::vector<int> v;
    ::std::vector<::std::string> vs;
    ::std::vector<::std::vector<int>> vv;
    ::std::vector<IntSerializable> vss;

    BABYLON_COMPATIBLE((v, 1)(vs, 2)(vv, 3)(vss, 4))
};
TEST_F(SerializeTest, vector) {
    using S = VectorSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_vector", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(0, s.v.size());
            ASSERT_EQ(0, s.vs.size());
            ASSERT_EQ(0, s.vv.size());
            ASSERT_EQ(0, s.vss.size());
        } else {
            s.v.clear();
            s.vs.clear();
            s.vv.clear();
            s.vss.clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_vector", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_vector", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, s.v[0]);
            ASSERT_EQ("10086", s.vs[0]);
            ASSERT_EQ(10010, s.vv[0][0]);
            ASSERT_EQ(10000, s.vss[0].v);
        } else {
            s.v.assign({10086});
            s.vs.assign({"10086"});
            s.vv.assign({{10010}});
            s.vss.assign({10000});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_vector", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct ListSerializable {
    ::std::list<int> l;
    ::std::list<::std::string> ls;
    ::std::list<::std::list<int>> ll;
    ::std::list<IntSerializable> lss;

    BABYLON_COMPATIBLE((l, 1)(ls, 2)(ll, 3)(lss, 4))
};
TEST_F(SerializeTest, list) {
    using S = ListSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_list", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(0, s.l.size());
            ASSERT_EQ(0, s.ls.size());
            ASSERT_EQ(0, s.ll.size());
            ASSERT_EQ(0, s.lss.size());
        } else {
            s.l.clear();
            s.ls.clear();
            s.ll.clear();
            s.lss.clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_list", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_list", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, s.l.front());
            ASSERT_EQ("10086", s.ls.front());
            ASSERT_EQ(10010, s.ll.front().front());
            ASSERT_EQ(10000, s.lss.front().v);
        } else {
            s.l.assign({10086});
            s.ls.assign({"10086"});
            s.ll.assign({{10010}});
            s.lss.assign({{10000}});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_list", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct IntSerializableHash {
    size_t operator()(IntSerializable s) const noexcept {
        return s.v;
    }
};
struct IntSerializableEqual {
    bool operator()(IntSerializable l, IntSerializable r) const noexcept {
        return l.v == r.v;
    }
};
struct SetSerializable {
    ::std::unordered_set<int> s;
    ::std::unordered_set<::std::string> ss;
    ::std::unordered_set<IntSerializable, IntSerializableHash, IntSerializableEqual> sss;

    BABYLON_COMPATIBLE((s, 1)(ss, 2)(sss, 3))
};
TEST_F(SerializeTest, set) {
    using S = SetSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_set", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(0, s.s.size());
            ASSERT_EQ(0, s.ss.size());
            ASSERT_EQ(0, s.sss.size());
        } else {
            s.s.clear();
            s.ss.clear();
            s.sss.clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_set", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_set", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, *s.s.begin());
            ASSERT_EQ("10086", *s.ss.begin());
            ASSERT_EQ(10010, s.sss.begin()->v);
        } else {
            s.s.insert(10086);
            s.ss.insert("10086");
            s.sss.insert({10010});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_set", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct MapSerializable {
    ::std::unordered_map<int, int> m;
    ::std::unordered_map<::std::string, ::std::string> ms;
    ::std::unordered_map<IntSerializable, IntSerializable, IntSerializableHash, IntSerializableEqual> mss;

    BABYLON_COMPATIBLE((m, 1)(ms, 2)(mss, 3))
};
TEST_F(SerializeTest, map) {
    using S = MapSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_map", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(0, s.m.size());
            ASSERT_EQ(0, s.ms.size());
            ASSERT_EQ(0, s.mss.size());
        } else {
            s.m.clear();
            s.ms.clear();
            s.mss.clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_map", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_map", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10010, s.m[10086]);
            ASSERT_EQ("10010", s.ms["10086"]);
            ASSERT_EQ(10086, s.mss[{10010}].v);
        } else {
            s.m.emplace(10086, 10010);
            s.ms.emplace("10086", "10010");
            s.mss.emplace(10010, 10086);
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_map", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct SharedPtrSerializable {
    ::std::shared_ptr<int> p;
    ::std::shared_ptr<::std::string> ps;
    ::std::shared_ptr<::std::shared_ptr<int>> pp;
    ::std::shared_ptr<IntSerializable> pss;

    BABYLON_COMPATIBLE((p, 1)(ps, 2)(pp, 3)(pss, 4))
};
TEST_F(SerializeTest, shared_ptr) {
    using S = SharedPtrSerializable;
    using SS = IntSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_shared_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_FALSE(s.p);
            ASSERT_FALSE(s.ps);
            ASSERT_FALSE(s.pp);
            ASSERT_FALSE(s.pss);
        } else {
            s.p.reset();
            s.ps.reset();
            s.pp.reset();
            s.pss.reset();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_shared_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_inner_empty_shared_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, *s.p);
            ASSERT_FALSE(s.ps);
            ASSERT_FALSE(s.pp);
            ASSERT_EQ(10000, s.pss->v);
        } else {
            s.p.reset(new int {10086});
            s.ps.reset(new ::std::string);
            s.pp.reset(new ::std::shared_ptr<int>);
            s.pss.reset(new SS {10000});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_inner_empty_shared_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_shared_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, *s.p);
            ASSERT_EQ("10086", *s.ps);
            ASSERT_EQ(10010, **s.pp);
            ASSERT_EQ(10000, s.pss->v);
        } else {
            s.p.reset(new int {10086});
            s.ps.reset(new ::std::string {"10086"});
            s.pp.reset(new ::std::shared_ptr<int> {new int {10010}});
            s.pss.reset(new SS {10000});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_shared_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct UniquePtrSerializable {
    ::std::unique_ptr<int> p;
    ::std::unique_ptr<::std::string> ps;
    ::std::unique_ptr<::std::unique_ptr<int>> pp;
    ::std::unique_ptr<IntSerializable> pss;

    BABYLON_COMPATIBLE((p, 1)(ps, 2)(pp, 3)(pss, 4))
};
TEST_F(SerializeTest, unique_ptr) {
    using S = UniquePtrSerializable;
    using SS = IntSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_unique_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_FALSE(s.p);
            ASSERT_FALSE(s.ps);
            ASSERT_FALSE(s.pp);
            ASSERT_FALSE(s.pss);
        } else {
            s.p.reset();
            s.ps.reset();
            s.pp.reset();
            s.pss.reset();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_unique_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_inner_empty_unique_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, *s.p);
            ASSERT_FALSE(s.ps);
            ASSERT_FALSE(s.pp);
            ASSERT_EQ(10000, s.pss->v);
        } else {
            s.p.reset(new int {10086});
            s.ps.reset(new ::std::string);
            s.pp.reset();
            s.pss.reset(new SS {10000});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_inner_empty_unique_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_unique_ptr", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, *s.p);
            ASSERT_EQ("10086", *s.ps);
            ASSERT_EQ(10010, **s.pp);
            ASSERT_EQ(10000, s.pss->v);
        } else {
            s.p.reset(new int {10086});
            s.ps.reset(new ::std::string {"10086"});
            s.pp.reset(new ::std::unique_ptr<int> {new int {10010}});
            s.pss.reset(new SS {10000});
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_unique_ptr", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}

struct MessageSerializable {
    TestMessage m;

    BABYLON_COMPATIBLE((m, 1))
};
TEST_F(SerializeTest, message) {
    using S = MessageSerializable;
    S s;

    {
        ::std::ifstream ifs("output/test/dump_empty_message", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(0, s.m.ByteSizeLong());
        } else {
            s.m.Clear();
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_empty_message", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }

    {
        ::std::ifstream ifs("output/test/dump_message", ::std::ios_base::in);
        if (ifs) {
            string.assign(::std::istreambuf_iterator<char>{ifs}, {});
            ASSERT_TRUE(Serialization::parse_from_string(string, s));
            ASSERT_EQ(10086, s.m.i32());
        } else {
            s.m.set_i32(10086);
        }
    }

    {
        ASSERT_TRUE(Serialization::serialize_to_string(s, string));
        ::std::ofstream ofs("output/test/dump_message", ::std::ios_base::out | ::std::ios_base::trunc);
        ofs << string;
    }
}
