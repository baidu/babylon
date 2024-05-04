#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

using ::babylon::anyflow::Closure;
using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;

struct ConstProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *x.emit() = *a;
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(::std::string, a, 1)
                        ANYFLOW_EMIT_DATA(::std::string, x))
};

struct MutableProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *x.emit() = *a;
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_MUTABLE_DATA(::std::string, a)
                        ANYFLOW_EMIT_DATA(::std::string, x))
};

struct VariableProcessor : public GraphProcessor {
  virtual int setup() noexcept override {
    size_t anonymous_depend_sz = vertex().anonymous_dependency_size();
    for (size_t index = 0; index < anonymous_depend_sz; index++) {
      vertex().anonymous_dependency(index)->declare_essential(true);
    }
    return 0;
  }
  virtual int process() noexcept override {
    for (size_t i = 0; i < dv.size(); i++) {
      *ev[i].emit() = *dv[i];
    }
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_VA_DATA(::std::string, dv)
                        ANYFLOW_EMIT_VA_DATA(::std::string, ev))
};

struct DependencyTest : public ::testing::Test {
  virtual void SetUp() override {
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<ConstProcessor>(new ConstProcessor);
      });
      vertex.named_depend("a").to("A").on("C1");
      vertex.named_emit("x").to("X");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<ConstProcessor>(new ConstProcessor);
      });
      vertex.named_depend("a").to("A").on("C2");
      vertex.named_emit("x").to("Y");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<MutableProcessor>(new MutableProcessor);
      });
      vertex.named_depend("a").to("A").on("C3");
      vertex.named_emit("x").to("Z");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<VariableProcessor>(new VariableProcessor);
      });
      for (size_t index = 0; index < v.size(); index++) {
        vertex.anonymous_depend().to(v[index]).on("C4");
      }
      for (size_t index = 0; index < rv.size(); index++) {
        vertex.anonymous_emit().to(rv[index]);
      }
    }
    builder.finish();
    graph = builder.build();
    a = graph->find_data("A");
    x = graph->find_data("X");
    y = graph->find_data("Y");
    z = graph->find_data("Z");
    for (size_t index = 0; index < v.size(); index++) {
      gv.emplace_back(graph->find_data(v[index]));
    }
    for (size_t index = 0; index < rv.size(); index++) {
      grv.emplace_back(graph->find_data(rv[index]));
    }
    c1 = graph->find_data("C1");
    c2 = graph->find_data("C2");
    c3 = graph->find_data("C3");
    c4 = graph->find_data("C4");
  }

  GraphBuilder builder;
  ::std::unique_ptr<Graph> graph;

  GraphData* a;
  GraphData* x;
  GraphData* y;
  GraphData* z;
  GraphData* c1;
  GraphData* c2;
  GraphData* c3;
  GraphData* c4;

  ::std::vector<::std::string> v {"I", "J", "K"};
  ::std::vector<::std::string> rv {"RI", "RJ", "RK"};
  ::std::vector<GraphData*> gv;
  ::std::vector<GraphData*> grv;
};

TEST_F(DependencyTest,
       immediately_ready_when_target_ready_and_condition_established) {
  *c1->emit<bool>() = true;
  *a->emit<std::string>() = "10086";
  auto closure = graph->run(x);
  ASSERT_EQ(0, closure.get());
  ASSERT_EQ("10086", *x->value<::std::string>());
}

TEST_F(
    DependencyTest,
    anonymous_immediately_ready_when_target_ready_and_condition_established) {
  *c4->emit<bool>() = true;
  for (size_t index = 0; index < gv.size(); index++) {
    *gv[index]->emit<::std::string>() = std::to_string(index);
  }
  ::std::vector<Closure> cv;
  for (size_t index = 0; index < grv.size(); index++) {
    cv.emplace_back(graph->run(grv[index]));
  }
  for (size_t index = 0; index < cv.size(); index++) {
    ASSERT_EQ(0, cv[index].get());
    ASSERT_EQ(::std::to_string(index), *grv[index]->value<std::string>());
  }
}

TEST_F(DependencyTest, immediately_ready_when_condition_not_established) {
  *c1->emit<bool>() = false;
  auto closure = graph->run(x);
  ASSERT_EQ(0, closure.get());
  ASSERT_TRUE(x->empty());
}

TEST_F(DependencyTest,
       anonymous_immediately_ready_when_condition_not_established) {
  *c4->emit<bool>() = false;
  ::std::vector<Closure> cv;
  for (size_t index = 0; index < grv.size(); index++) {
    cv.emplace_back(graph->run(grv[index]));
  }
  for (size_t index = 0; index < cv.size(); index++) {
    ASSERT_EQ(0, cv[index].get());
    ASSERT_TRUE(grv[index]->empty());
  }
}

TEST_F(DependencyTest, ignore_target_when_condition_not_established) {
  *a->emit<std::string>() = "10086";
  *c1->emit<bool>() = false;
  auto closure = graph->run(x);
  ASSERT_EQ(0, closure.get());
  ASSERT_TRUE(x->empty());
}

TEST_F(DependencyTest, anonymous_ignore_target_when_condition_not_established) {
  *c4->emit<bool>() = false;
  for (size_t index = 0; index < gv.size(); index++) {
    *gv[index]->emit<std::string>() = std::to_string(index);
  }
  ::std::vector<Closure> cv;
  for (size_t index = 0; index < grv.size(); index++) {
    cv.emplace_back(graph->run(grv[index]));
  }
  for (size_t index = 0; index < cv.size(); index++) {
    ASSERT_EQ(0, cv[index].get());
    ASSERT_TRUE(grv[index]->empty());
  }
}

TEST_F(DependencyTest, empty_when_target_empty) {
  a->emit<std::string>();
  *c1->emit<bool>() = true;
  auto closure = graph->run(x);
  ASSERT_EQ(0, closure.get());
  ASSERT_TRUE(x->empty());
}

TEST_F(DependencyTest, anonymous_empty_when_target_empty) {
  for (size_t index = 0; index < gv.size(); index++) {
    gv[index]->emit<std::string>();
  }
  *c4->emit<bool>() = true;
  std::vector<Closure> cv;
  for (size_t index = 0; index < grv.size(); index++) {
    cv.emplace_back(graph->run(grv[index]));
  }
  for (size_t index = 0; index < cv.size(); index++) {
    ASSERT_EQ(0, cv[index].get());
    ASSERT_TRUE(grv[index]->empty());
  }
}

TEST_F(DependencyTest, single_mutable_is_ok) {
  *c2->emit<bool>() = false;
  *c3->emit<bool>() = true;
  *a->emit<std::string>() = "10086";
  auto closure = graph->run(y, z);
  ASSERT_EQ(0, closure.get());
  ASSERT_TRUE(y->empty());
  ASSERT_FALSE(z->empty());
  ASSERT_EQ("10086", *z->value<::std::string>());
}

TEST_F(DependencyTest, const_is_sharable) {
  *c1->emit<bool>() = true;
  *c2->emit<bool>() = true;
  *a->emit<std::string>() = "10086";
  auto closure = graph->run(x, y);
  ASSERT_EQ(0, closure.get());
  ASSERT_FALSE(x->empty());
  ASSERT_FALSE(y->empty());
  ASSERT_EQ("10086", *x->value<::std::string>());
  ASSERT_EQ("10086", *y->value<::std::string>());
}

TEST_F(DependencyTest, mutable_is_non_sharable) {
  *c2->emit<bool>() = true;
  *c3->emit<bool>() = true;
  *a->emit<std::string>() = "10086";
  auto closure = graph->run(y, z);
  ASSERT_NE(0, closure.get());
}

TEST_F(DependencyTest, mutable_need_non_const) {
  *c3->emit<bool>() = true;
  const ::std::string s = "10086";
  a->emit<std::string>().ref(s);
  auto closure = graph->run(z);
  ASSERT_NE(0, closure.get());
}
