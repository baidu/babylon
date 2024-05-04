#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

using ::babylon::anyflow::Committer;
using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::ThreadPoolGraphExecutor;

using ::babylon::Any;
using ::babylon::TypeId;

class MockProcessor : public GraphProcessor {
 public:
  virtual int process() noexcept override {
    ::std::lock_guard<::std::mutex> lock(**option<::std::mutex*>());
    *x.emit() = *a;
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(::std::string, a)
                        ANYFLOW_EMIT_DATA(::std::string, x))
};

struct DataTest : public ::testing::Test {
  virtual void SetUp() override {
    auto creator = [] {
      return ::std::unique_ptr<GraphProcessor>(new MockProcessor);
    };
    {
      auto& vertex = builder.add_vertex(creator);
      vertex.option(&mx);
      vertex.named_depend("a").to("A");
      vertex.named_emit("x").to("B");
    }
    {
      auto& vertex = builder.add_vertex(creator);
      vertex.option(&my);
      vertex.named_depend("a").to("B");
      vertex.named_emit("x").to("C");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<GraphProcessor>(new GraphProcessor);
      });
      vertex.anonymous_emit().to("D");
    }
    executor.initialize(4, 128);
    builder.set_executor(executor);
    ASSERT_EQ(0, builder.finish());
    graph = builder.build();
    ASSERT_TRUE(graph);

    a = graph->find_data("A");
    b = graph->find_data("B");
    c = graph->find_data("C");
    d = graph->find_data("D");
  }

  ThreadPoolGraphExecutor executor;
  GraphBuilder builder;
  ::std::unique_ptr<Graph> graph;

  GraphData* a;
  GraphData* b;
  GraphData* c;
  GraphData* d;
  ::std::mutex mx;
  ::std::mutex my;
};

/*
TEST_F(DataTest, emit_only_once) {
    auto c1 = a->emit<::std::string>();
    auto c2 = a->emit<::std::string>();
    ASSERT_TRUE(c1);
    ASSERT_NE(nullptr, c1.get());
    ASSERT_FALSE(c2);
    ASSERT_EQ(nullptr, c2.get());
}

TEST_F(DataTest, emit_make_data_ready) {
    ASSERT_FALSE(a->ready());
    a->emit<::std::string>();
    ASSERT_TRUE(a->ready());
    ASSERT_FALSE(b->ready());
    auto committer = b->emit<::std::string>();
    ASSERT_FALSE(b->ready());
    committer.release();
    ASSERT_TRUE(b->ready());
}

TEST_F(DataTest, emit_default_publish_empty_data) {
    a->emit<::std::string>();
    ASSERT_TRUE(a->ready());
    ASSERT_TRUE(a->empty());
}

TEST_F(DataTest, data_instance_create_on_touch) {
    {
        auto committer = a->emit<::std::string>();
        auto s = committer.get();
        ASSERT_NE(nullptr, s);
    }
    ASSERT_TRUE(a->ready());
    ASSERT_FALSE(a->empty());
}

TEST_F(DataTest, default_reset_destroy_value) {
    struct S {
        ::std::string s;
    };
    d->declare_type<S>();
    ASSERT_EQ(nullptr, d->value<S>());
    d->emit<S>()->s = "10086";
    ASSERT_TRUE(d->ready());
    ASSERT_FALSE(d->empty());
    ASSERT_EQ("10086", d->value<S>()->s);
    graph->reset();

    ASSERT_EQ(nullptr, d->value<S>());
    ASSERT_FALSE(d->ready());
    ASSERT_TRUE(d->emit<S>()->s.empty());
}

TEST_F(DataTest, default_reset_detect_reset) {
    struct S {
        void reset() {
            s = "10010";
        }
        ::std::string s;
    };
    d->declare_type<S>();
    ASSERT_EQ(nullptr, d->value<S>());
    d->emit<S>()->s = "10086";
    ASSERT_TRUE(d->ready());
    ASSERT_FALSE(d->empty());
    ASSERT_EQ("10086", d->value<S>()->s);
    graph->reset();

    ASSERT_EQ(nullptr, d->value<S>());
    ASSERT_FALSE(d->ready());
    ASSERT_EQ("10010", d->emit<S>()->s);
}

TEST_F(DataTest, default_reset_detect_clear) {
    struct S {
        void clear() {
            s = "10010";
        }
        ::std::string s;
    };
    d->declare_type<S>();
    ASSERT_EQ(nullptr, d->value<S>());
    d->emit<S>()->s = "10086";
    ASSERT_TRUE(d->ready());
    ASSERT_FALSE(d->empty());
    ASSERT_EQ("10086", d->value<S>()->s);
    graph->reset();

    ASSERT_EQ(nullptr, d->value<S>());
    ASSERT_FALSE(d->ready());
    ASSERT_EQ("10010", d->emit<S>()->s);
}

TEST_F(DataTest, reset_with_custom_function) {
    struct S {
        ::std::string s;
    };
    auto output = d->declare_type<S>();
    output.set_on_reset([] (S& s) {
        s.s = "10010";
    });
    ASSERT_EQ(nullptr, d->value<S>());
    d->emit<S>()->s = "10086";
    ASSERT_TRUE(d->ready());
    ASSERT_FALSE(d->empty());
    ASSERT_EQ("10086", d->value<S>()->s);
    graph->reset();

    ASSERT_EQ(nullptr, d->value<S>());
    ASSERT_FALSE(d->ready());
    ASSERT_EQ("10010", d->emit<S>()->s);
}

TEST_F(DataTest, committer_can_output_ref) {
    ::std::string value = "10086";
    a->emit<::std::string>().ref(value);
    ASSERT_TRUE(a->ready());
    ASSERT_FALSE(a->empty());
    ASSERT_EQ(&value, a->value<::std::string>());
}

TEST_F(DataTest, ref_dont_keep_as_value) {
    ::std::string value = "10086";
    a->emit<::std::string>().ref(value);
    ASSERT_TRUE(a->ready());
    ASSERT_FALSE(a->empty());
    ASSERT_EQ(&value, a->value<::std::string>());

    graph->reset();
    auto ptr = a->emit<::std::string>().get();
    ASSERT_NE(&value, ptr);
    ASSERT_TRUE(ptr->empty());
    ASSERT_TRUE(a->ready());
    ASSERT_FALSE(a->empty());
}

TEST_F(DataTest, committer_work_as_pointer) {
    {
        auto commiter = a->emit<::std::string>();
        commiter->assign("10086");
        ASSERT_EQ("10086", *commiter);
    }
    ASSERT_EQ("10086", *a->value<::std::string>());
}

TEST_F(DataTest, committer_support_move) {
    auto commiter = a->emit<::std::string>();
    {
        Committer<::std::string> moved_commiter(::std::move(commiter));
        ASSERT_FALSE(commiter);
        moved_commiter->assign("10086");
    }
    ASSERT_EQ("10086", *a->value<::std::string>());
}

TEST_F(DataTest, immediate_finish_when_data_already_released) {
    *a->emit<::std::string>() = "10086";
    auto closure = graph->run(a);
    ASSERT_TRUE(closure.finished());
    ASSERT_EQ(0, closure.get());
}
*/

TEST_F(DataTest, closure_finish_when_data_ready) {
  *a->emit<::std::string>() = "10086";
  // mx.lock();
  auto closure = graph->run(b);
  ::usleep(100000);
  // ASSERT_FALSE(closure.finished());
  // mx.unlock();
  ASSERT_EQ(0, closure.get());
}

/*
TEST_F(DataTest, activate_data_fail_for_no_producer) {
    auto closure = graph->run(a);
    ASSERT_TRUE(closure.finished());
    ASSERT_NE(0, closure.get());
    graph->reset();
    closure = graph->run(c);
    ASSERT_TRUE(closure.finished());
    ASSERT_NE(0, closure.get());
}

TEST_F(DataTest, support_instance_not_default_constructible) {
    struct A {
        A(int value) : value(value) {}
        int value {0};
    };
    *a->emit<::babylon::Any>() = A {10086};
    ASSERT_EQ(10086, a->value<A>()->value);
}

TEST_F(DataTest, preset_value_will_use_in_emit_once) {
    ::std::string str;
    a->preset(str);
    auto committer = a->emit<::std::string>();
    ASSERT_EQ(&str, committer.get());
    committer.release();
    graph->reset();
     committer = a->emit<::std::string>();
    ASSERT_NE(&str, committer.get());
}

TEST_F(DataTest, support_non_copyable_nor_moveable_class) {
    struct S {
        S() = default;
        S(const S&) = delete;
        S(S&&) = delete;
    } s;
    a->emit<S>().get();
    graph->reset();
    a->emit<S>().ref(s);
    graph->reset();
    a->emit<S>().get();
}

TEST_F(DataTest, can_get_declared_type) {
    ASSERT_EQ(TypeId<::std::string>::ID, a->declared_type());
    ASSERT_EQ(TypeId<::std::string>::ID, b->declared_type());
    ASSERT_EQ(TypeId<::std::string>::ID, c->declared_type());
    ASSERT_EQ(TypeId<Any>::ID, d->declared_type());
}
*/
