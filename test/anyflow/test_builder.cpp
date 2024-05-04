#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::GraphVertexBuilder;
using ::babylon::anyflow::InplaceGraphExecutor;

struct BuilderTest : public ::testing::Test {
  virtual void SetUp() override {}

  GraphBuilder builder;
};

TEST_F(BuilderTest, empty_graph_is_ok_though_useless) {
  ASSERT_EQ(0, builder.finish());
  ASSERT_TRUE(static_cast<bool>(builder.build()));
}

TEST_F(BuilderTest, graph_has_a_name_for_print) {
  builder.set_name("10086");
  ASSERT_EQ("10086", builder.name());
  ::std::cerr << builder << ::std::endl;
}

TEST_F(BuilderTest, graph_has_executor) {
  InplaceGraphExecutor executor;
  builder.set_executor(executor);
  ASSERT_EQ(&executor, &builder.executor());
}

TEST_F(BuilderTest, vertex_builder_reference_keep_valid_after_add_vertex) {
  auto creator = [] {
    return ::std::unique_ptr<GraphProcessor>(new GraphProcessor);
  };

  ::std::vector<GraphVertexBuilder*> v1;
  v1.emplace_back(&builder.add_vertex(creator));
  v1.emplace_back(&builder.add_vertex(creator));
  v1.emplace_back(&builder.add_vertex(creator));
  v1.emplace_back(&builder.add_vertex(creator));

  ::std::vector<GraphVertexBuilder*> v2;
  builder.for_each_vertex([&](GraphVertexBuilder& v) {
    v2.emplace_back(&v);
  });

  ASSERT_EQ(v1, v2);
}

TEST_F(BuilderTest, null_graph_processor_report_fail_when_finish) {
  builder.add_vertex([] {
    return ::std::unique_ptr<GraphProcessor>();
  });
  ASSERT_NE(0, builder.finish());
}

TEST_F(BuilderTest, data_can_only_have_one_producer) {
  builder
      .add_vertex([] {
        return ::std::unique_ptr<GraphProcessor>();
      })
      .named_emit("a")
      .to("A");
  builder
      .add_vertex([] {
        return ::std::unique_ptr<GraphProcessor>();
      })
      .named_emit("b")
      .to("A");
  ASSERT_NE(0, builder.finish());
}
