#include "babylon/anyflow/builder.h"
#include "babylon/reusable/string.h"
#include "babylon/reusable/vector.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::Any;
using ::babylon::SwissAllocator;
using ::babylon::SwissString;
using ::babylon::SwissVector;

using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::GraphVertex;
using ::babylon::anyflow::GraphVertexClosure;
using ::babylon::anyflow::ThreadPoolGraphExecutor;

struct MockProcessor : public GraphProcessor {
  virtual int config(const Any& raw, Any& option) const noexcept override {
    option = *raw.get<::std::string>() + option_append;
    config_called = true;
    return config_return;
  }

  virtual int setup() noexcept override {
    option_effect = *option<::std::string>();
    setup_called = true;
    return setup_return;
  }

  virtual int process() noexcept override {
    x.emit();
    process_called = true;
    return process_return;
  }

  virtual void reset() noexcept override {
    reset_called = true;
  }

  ANYFLOW_INTERFACE(ANYFLOW_EMIT_DATA(::std::string, x))

  static bool config_called;
  static ::std::string option_append;
  static int config_return;

  static bool setup_called;
  static ::std::string option_effect;
  static int setup_return;

  static bool process_called;
  static int process_return;

  static bool reset_called;
};
bool MockProcessor::config_called = false;
::std::string MockProcessor::option_append;
int MockProcessor::config_return = 0;

bool MockProcessor::setup_called = false;
::std::string MockProcessor::option_effect;
int MockProcessor::setup_return = 0;

bool MockProcessor::process_called = false;
int MockProcessor::process_return = 0;

bool MockProcessor::reset_called = false;

struct AddProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    *x.emit() = ::std::to_string((a == nullptr ? 100 : *a) + *b + *c);
    return 0;
  }

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a, 0)
                        ANYFLOW_DEPEND_DATA(int32_t, b, 1)
                            ANYFLOW_DEPEND_DATA(int32_t, c)
                                ANYFLOW_EMIT_DATA(::std::string, x))
};

struct ProcessorTest : public ::testing::Test {
  virtual void SetUp() override {
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<AddProcessor>(new AddProcessor);
      });
      vertex.named_depend("a").to("A");
      vertex.named_depend("b").to("B");
      vertex.named_depend("c").to("C");
      vertex.named_emit("x").to("D");
    }
    executor.initialize(4, 128);
    builder.set_executor(executor);
    builder.finish();
    graph = builder.build();
    a = graph->find_data("A");
    b = graph->find_data("B");
    c = graph->find_data("C");
    d = graph->find_data("D");

    MockProcessor::config_called = false;
    MockProcessor::option_append = "";
    MockProcessor::config_return = 0;

    MockProcessor::setup_called = false;
    MockProcessor::option_effect = "";
    MockProcessor::setup_return = 0;

    MockProcessor::process_called = false;
    MockProcessor::process_return = 0;

    MockProcessor::reset_called = false;
  }

  ThreadPoolGraphExecutor executor;
  GraphBuilder builder;
  ::std::unique_ptr<Graph> graph;
  GraphData* a;
  GraphData* b;
  GraphData* c;
  GraphData* d;
};

TEST_F(ProcessorTest, config_when_finish) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  ASSERT_FALSE(MockProcessor::config_called);
  ASSERT_EQ(0, builder.finish());
  ASSERT_TRUE(MockProcessor::config_called);
}

TEST_F(ProcessorTest, config_fail_finish) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  MockProcessor::config_return = -1;
  ASSERT_NE(0, builder.finish());
}

TEST_F(ProcessorTest, config_modify_option) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  MockProcessor::option_append = "-10010";
  ASSERT_EQ(0, builder.finish());
  ASSERT_TRUE(builder.build());
  ASSERT_EQ("10086-10010", MockProcessor::option_effect);
}

TEST_F(ProcessorTest, setup_when_build) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  ASSERT_EQ(0, builder.finish());
  ASSERT_FALSE(MockProcessor::setup_called);
  ASSERT_TRUE(builder.build());
  ASSERT_TRUE(MockProcessor::setup_called);
  ASSERT_EQ("10086", MockProcessor::option_effect);
}

TEST_F(ProcessorTest, setup_fail_build) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  ASSERT_EQ(0, builder.finish());
  ASSERT_FALSE(MockProcessor::setup_called);
  MockProcessor::setup_return = -1;
  ASSERT_FALSE(builder.build());
}

TEST_F(ProcessorTest, reset_with_graph) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_FALSE(MockProcessor::reset_called);
  graph->reset();
  ASSERT_TRUE(MockProcessor::reset_called);
}

TEST_F(ProcessorTest, process_when_run) {
  GraphBuilder builder;
  builder
      .add_vertex([] {
        return ::std::unique_ptr<MockProcessor>(new MockProcessor);
      })
      .option(::std::string("10086"))
      .named_emit("x")
      .to("A");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_FALSE(MockProcessor::process_called);
  ASSERT_EQ(0, graph->run(graph->find_data("A")).get());
  ASSERT_TRUE(MockProcessor::process_called);
}

TEST_F(ProcessorTest, fail_when_essential_level_2_depend_empty) {
  *a->emit<int32_t>() = 1;
  *b->emit<int32_t>() = 2;
  c->emit<int32_t>();
  ASSERT_NE(0, graph->run(d).get());
  graph->reset();
  *a->emit<int32_t>() = 1;
  *b->emit<int32_t>() = 2;
  *c->emit<int64_t>() = 3;
  ASSERT_NE(0, graph->run(d).get());
}

TEST_F(ProcessorTest, skip_when_essential_level_1_depend_empty) {
  *a->emit<int32_t>() = 1;
  b->emit<int32_t>();
  *c->emit<int32_t>() = 3;
  ASSERT_EQ(0, graph->run(d).get());
  ASSERT_EQ(nullptr, d->value<::std::string>());
  graph->reset();
  *a->emit<int32_t>() = 1;
  *b->emit<int64_t>() = 2;
  *c->emit<int32_t>() = 3;
  ASSERT_NE(0, graph->run(d).get());
}

TEST_F(ProcessorTest, essential_level_0_can_handle_empty_as_needed) {
  a->emit<int32_t>();
  *b->emit<int32_t>() = 2;
  *c->emit<int32_t>() = 3;
  ASSERT_EQ(0, graph->run(d).get());
  ASSERT_EQ("105", *d->value<::std::string>());
  graph->reset();
  *a->emit<int64_t>() = 1;
  *b->emit<int32_t>() = 2;
  *c->emit<int32_t>() = 3;
  ASSERT_EQ(0, graph->run(d).get());
  ASSERT_EQ("105", *d->value<::std::string>());
}

TEST_F(ProcessorTest, build_fail_if_type_conflict) {
  struct P : public GraphProcessor {
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(::std::string, x))
  };
  auto& vertex = builder.add_vertex([] {
    return ::std::unique_ptr<P>(new P);
  });
  vertex.named_depend("a").to("D");
  vertex.named_emit("x").to("E");
  ASSERT_EQ(0, builder.finish());
  ASSERT_FALSE(builder.build());
}

TEST_F(ProcessorTest, build_fail_if_mutable_non_exclusive) {
  struct P : public GraphProcessor {
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_MUTABLE_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(::std::string, x))
  };
  auto& vertex = builder.add_vertex([] {
    return ::std::unique_ptr<P>(new P);
  });
  vertex.named_depend("a").to("A");
  vertex.named_emit("x").to("E");
  ASSERT_EQ(0, builder.finish());
  ASSERT_FALSE(builder.build());
}

TEST_F(ProcessorTest, any_accept_any_type) {
  struct P : public GraphProcessor {
    virtual int process() noexcept override {
      x.emit().ref(*a->get<::std::string>());
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(::babylon::Any, a)
                          ANYFLOW_EMIT_DATA(::std::string, x))
  };
  auto& vertex = builder.add_vertex([] {
    return ::std::unique_ptr<P>(new P);
  });
  vertex.named_depend("a").to("D");
  vertex.named_emit("x").to("E");
  ASSERT_EQ(0, builder.finish());
  graph = builder.build();
  ASSERT_TRUE(graph);
  a = graph->find_data("A");
  b = graph->find_data("B");
  c = graph->find_data("C");
  auto* e = graph->find_data("E");
  *a->emit<int32_t>() = 1;
  *b->emit<int32_t>() = 2;
  *c->emit<int32_t>() = 3;
  ASSERT_EQ(0, graph->run(e).get());
  ASSERT_EQ("6", *e->value<::std::string>());
}

TEST_F(ProcessorTest, mutable_data_get_non_const_pointer) {
  struct P : public GraphProcessor {
    virtual int process() noexcept override {
      a->append(" end");
      x.emit().ref(*a);
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_MUTABLE_DATA(::std::string, a)
                          ANYFLOW_EMIT_DATA(::std::string, x))
  };
  auto& vertex = builder.add_vertex([] {
    return ::std::unique_ptr<P>(new P);
  });
  vertex.named_depend("a").to("D");
  vertex.named_emit("x").to("E");
  ASSERT_EQ(0, builder.finish());
  graph = builder.build();
  ASSERT_TRUE(graph);
  a = graph->find_data("A");
  b = graph->find_data("B");
  c = graph->find_data("C");
  auto* e = graph->find_data("E");
  *a->emit<int32_t>() = 1;
  *b->emit<int32_t>() = 2;
  *c->emit<int32_t>() = 3;
  ASSERT_EQ(0, graph->run(e).get());
  ASSERT_EQ("6 end", *e->value<::std::string>());
}

TEST_F(ProcessorTest, downstream_function_run_before_current_one_return) {
  struct F1 : public GraphProcessor {
    virtual int process() noexcept override {
      *x.emit() = *a;
      f.get();
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(int32_t, x))
    ::std::promise<void> p;
    ::std::future<void> f = p.get_future();
  };
  struct F2 : public GraphProcessor {
    virtual int process() noexcept override {
      *x.emit() = *a;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(int32_t, x))
  };
  F1* f1 = nullptr;
  auto& v1 = builder.add_vertex([&] {
    f1 = new F1;
    return ::std::unique_ptr<F1>(f1);
  });
  v1.named_depend("a").to("X1");
  v1.named_emit("x").to("X2");
  auto& v2 = builder.add_vertex([] {
    return ::std::unique_ptr<F2>(new F2);
  });
  v2.named_depend("a").to("X2");
  v2.named_emit("x").to("X3");
  ASSERT_EQ(0, builder.finish());
  graph = builder.build();
  ASSERT_TRUE(graph);
  auto* x1 = graph->find_data("X1");
  auto* x3 = graph->find_data("X3");
  *x1->emit<int32_t>() = 1;
  auto closure = graph->run(x3);
  ASSERT_EQ(0, closure.get());
  f1->p.set_value();
}

TEST_F(ProcessorTest,
       downstream_function_run_aftert_return_to_avoid_stack_overflow) {
  struct F1 : public GraphProcessor {
    virtual int process() noexcept override {
      *x.emit() = *a;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(int32_t, x))
  };
  struct F2 : public GraphProcessor {
    virtual int setup() noexcept override {
      vertex().declare_trivial();
      return 0;
    }
    virtual int process() noexcept override {
      *x.emit() = *a;
      pe.set_value();
      f.get();
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(int32_t, x))
    bool emitted = false;
    ::std::promise<void> p;
    ::std::future<void> f = p.get_future();
    ::std::promise<void> pe;
    ::std::future<void> fe = pe.get_future();
  };
  struct F3 : public GraphProcessor {
    virtual int process() noexcept override {
      *x.emit() = *a;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int32_t, a)
                          ANYFLOW_EMIT_DATA(int32_t, x))
  };
  auto& v1 = builder.add_vertex([] {
    return ::std::unique_ptr<F1>(new F1);
  });
  v1.named_depend("a").to("X1");
  v1.named_emit("x").to("X2");
  F2* f2 = nullptr;
  auto& v2 = builder.add_vertex([&] {
    f2 = new F2;
    return ::std::unique_ptr<F2>(f2);
  });
  v2.named_depend("a").to("X2");
  v2.named_emit("x").to("X3");
  auto& v3 = builder.add_vertex([] {
    return ::std::unique_ptr<F3>(new F3);
  });
  v3.named_depend("a").to("X3");
  v3.named_emit("x").to("X4");
  ASSERT_EQ(0, builder.finish());
  graph = builder.build();
  ASSERT_TRUE(graph);
  auto* x1 = graph->find_data("X1");
  auto* x4 = graph->find_data("X4");
  *x1->emit<int32_t>() = 1;
  auto closure = graph->run(x4);
  f2->fe.get();
  ASSERT_FALSE(closure.finished());
  f2->p.set_value();
  ASSERT_EQ(0, closure.get());
}

TEST_F(ProcessorTest, depend_essential) {
  struct F : public GraphProcessor {
    virtual int process() noexcept override {
      if (a) {
        *x.emit() = 1;
      } else {
        *x.emit() = 0;
      }
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a, 0) // essential = 0
                      ANYFLOW_EMIT_DATA(int, x))
  };
  // depend A
  auto& v1 = builder.add_vertex([] {
    return ::std::unique_ptr<F>(new F);
  });
  v1.named_depend("a").to("A");
  v1.named_emit("x").to("X1");
  // not depend A
  auto& v2 = builder.add_vertex([] {
    return ::std::unique_ptr<F>(new F);
  });
  v2.named_emit("x").to("X2");
  ASSERT_EQ(0, builder.finish());
  graph = builder.build();
  ASSERT_TRUE(graph);
  a = graph->find_data("A");
  auto* x1 = graph->find_data("X1");
  auto* x2 = graph->find_data("X2");
  *a->emit<int>() = 1;
  ASSERT_EQ(0, graph->run(x1, x2).get());
  ASSERT_EQ(1, *x1->value<int>());
  ASSERT_EQ(0, *x2->value<int>());
}

TEST_F(ProcessorTest, trivial_invoke_run_processor_inplace) {
  struct P : public GraphProcessor {
    virtual int process() noexcept override {
      ::usleep(100000);
      *x.emit() = "10086";
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_EMIT_DATA(::std::string, x))
  };
  struct TP : public GraphProcessor {
    virtual int setup() noexcept override {
      vertex().declare_trivial();
      return 0;
    }
    virtual int process() noexcept override {
      ::usleep(100000);
      *x.emit() = "10086";
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_EMIT_DATA(::std::string, x))
  };
  builder
      .add_vertex([] {
        return ::std::unique_ptr<P>(new P);
      })
      .named_emit("x")
      .to("E");
  builder
      .add_vertex([] {
        return ::std::unique_ptr<TP>(new TP);
      })
      .named_emit("x")
      .to("F");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  {
    auto closure = graph->run(graph->find_data("E"));
    ASSERT_FALSE(closure.finished());
    ASSERT_EQ(0, closure.get());
  }
  {
    auto closure = graph->run(graph->find_data("F"));
    ASSERT_TRUE(closure.finished());
    ASSERT_EQ(0, closure.get());
  }
}

TEST_F(ProcessorTest, support_async_processor) {
  struct P : public GraphProcessor {
    virtual void process(GraphVertexClosure&& closure) noexcept override {
      ::std::thread(
          [&](GraphVertexClosure&&) {
            ::usleep(100000);
            *x.emit() = "10086";
          },
          ::std::move(closure))
          .detach();
    }
    ANYFLOW_INTERFACE(ANYFLOW_EMIT_DATA(::std::string, x))
  };
  builder
      .add_vertex([] {
        return ::std::unique_ptr<P>(new P);
      })
      .named_emit("x")
      .to("E");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  auto closure = graph->run(graph->find_data("E"));
  ASSERT_EQ(0, closure.get());
  ASSERT_EQ("10086", *graph->find_data("E")->value<::std::string>());
}

TEST_F(ProcessorTest, auto_declare_done_when_no_emit_declare) {
  struct P : public GraphProcessor {
    virtual int process() noexcept override {
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(::std::string, a))
  };
  auto& v = builder.add_vertex([] {
    return ::std::unique_ptr<P>(new P);
  });
  v.named_depend("a").to("X");
  v.named_emit("done").to("Y");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  *graph->find_data("X")->emit<::std::string>() = "10086";
  ASSERT_EQ(0, graph->run(graph->find_data("Y")).get());
  ASSERT_TRUE(graph->find_data("Y")->ready());
  ASSERT_TRUE(graph->find_data("Y")->empty());
}

TEST_F(ProcessorTest, processor_can_proxy_to_other) {
  class Real : public GraphProcessor {
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call real processor";
      b = *a + 10086;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, b))
  };
  class Proxy : public GraphProcessor {
    virtual int setup() noexcept override {
      return real->setup(vertex());
    }
    virtual int process() noexcept override {
      return real->process(vertex());
    }
    ::std::unique_ptr<GraphProcessor> real {new Real};
  };
  auto& v = builder.add_vertex([] {
    return ::std::unique_ptr<Proxy>(new Proxy);
  });
  v.named_depend("a").to("X");
  v.named_emit("b").to("Y");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  *graph->find_data("X")->emit<int>() = 1000;
  ASSERT_EQ(0, graph->run(graph->find_data("Y")).get());
  ASSERT_TRUE(graph->find_data("Y")->ready());
  ASSERT_EQ(11086, *graph->find_data("Y")->value<int>());
}

TEST_F(ProcessorTest, processor_switch_vertex_correctly) {
  struct Real : public GraphProcessor {
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call real processor";
      b = *a + 10086;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, b))
  };
  class Proxy : public GraphProcessor {
    virtual int setup() noexcept override {
      return real().setup(vertex());
    }
    virtual int process() noexcept override {
      return real().process(vertex());
    }
    GraphProcessor& real() {
      static Real processor;
      return processor;
    }
  };
  auto& v = builder.add_vertex([] {
    return ::std::unique_ptr<Proxy>(new Proxy);
  });
  v.named_depend("a").to("X");
  v.named_emit("b").to("Y");
  ASSERT_EQ(0, builder.finish());
  auto graph1 = builder.build();
  *graph1->find_data("X")->emit<int>() = 1000;
  auto graph2 = builder.build();
  *graph2->find_data("X")->emit<int>() = 2000;
  ASSERT_EQ(0, graph2->run(graph2->find_data("Y")).get());
  ASSERT_EQ(0, graph1->run(graph1->find_data("Y")).get());
  ASSERT_TRUE(graph1->find_data("Y")->ready());
  ASSERT_EQ(11086, *graph1->find_data("Y")->value<int>());
  ASSERT_TRUE(graph2->find_data("Y")->ready());
  ASSERT_EQ(12086, *graph2->find_data("Y")->value<int>());
}

TEST_F(ProcessorTest, processor_can_combine_together) {
  struct One : public GraphProcessor {
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call one processor";
      b = *a + 10086;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, b))
  };
  struct Two : public GraphProcessor {
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call two processor";
      c = *a + 10010;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, c))
  };
  class Proxy : public GraphProcessor {
    virtual int setup() noexcept override {
      return one->setup(vertex()) + two->setup(vertex());
    }
    virtual int process() noexcept override {
      return one->process(vertex()) + two->process(vertex());
    }
    ::std::unique_ptr<GraphProcessor> one {new One};
    ::std::unique_ptr<GraphProcessor> two {new Two};
  };
  auto& v = builder.add_vertex([] {
    return ::std::unique_ptr<Proxy>(new Proxy);
  });
  v.named_depend("a").to("X");
  v.named_emit("b").to("Y");
  v.named_emit("c").to("Z");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  *graph->find_data("X")->emit<int>() = 1000;
  ASSERT_EQ(0, graph->run(graph->find_data("Y")).get());
  ASSERT_TRUE(graph->find_data("Y")->ready());
  ASSERT_EQ(11086, *graph->find_data("Y")->value<int>());
  ASSERT_EQ(11010, *graph->find_data("Z")->value<int>());
}

TEST_F(ProcessorTest, processor_can_chain_together) {
  struct Tail : public GraphProcessor {
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call tail processor";
      b = *a + 10086;
      return 0;
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, b))
  };
  class Head : public GraphProcessor {
    virtual int setup() noexcept override {
      return tail->setup(vertex());
    }
    virtual int process() noexcept override {
      BABYLON_LOG(INFO) << "call head processor";
      c = *a + 10010;
      return tail->process(vertex());
    }
    ANYFLOW_INTERFACE(ANYFLOW_DEPEND_DATA(int, a) ANYFLOW_EMIT_DATA(int, c))
    ::std::unique_ptr<GraphProcessor> tail {new Tail};
  };
  auto& v = builder.add_vertex([] {
    return ::std::unique_ptr<Head>(new Head);
  });
  v.named_depend("a").to("X");
  v.named_emit("b").to("Y");
  v.named_emit("c").to("Z");
  ASSERT_EQ(0, builder.finish());
  auto graph = builder.build();
  ASSERT_TRUE(graph);
  *graph->find_data("X")->emit<int>() = 1000;
  ASSERT_EQ(0, graph->run(graph->find_data("Y")).get());
  ASSERT_TRUE(graph->find_data("Y")->ready());
  ASSERT_EQ(11086, *graph->find_data("Y")->value<int>());
  ASSERT_EQ(11010, *graph->find_data("Z")->value<int>());
}

struct HookedProcessor;
struct ProcessorControl {
  HookedProcessor* processor;
};

struct HookedProcessor : public GraphProcessor {
  virtual int setup() noexcept override {
    auto& control = **option<ProcessorControl*>();
    control.processor = this;
    return 0;
  }
  virtual int process() noexcept override {
    return 0;
  }
  using GraphProcessor::create_object;
  using GraphProcessor::create_reusable_object;
  using GraphProcessor::memory_resource;
};

struct ProcessorRunTest : public ::testing::Test {
  virtual void SetUp() override {
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<GraphProcessor>(new HookedProcessor);
      });
      vertex.option(&control);
      vertex.named_emit("x").to("A");
    }
    executor.initialize(4, 128);
    builder.set_executor(executor);
    builder.finish();
    graph = builder.build();
    a = graph->find_data("A");
    p = control.processor;
  }

  ThreadPoolGraphExecutor executor;
  GraphBuilder builder;
  ::std::unique_ptr<Graph> graph;
  ProcessorControl control;

  GraphData* a;
  HookedProcessor* p;
};

TEST_F(ProcessorRunTest, create_object_live_until_reset) {
  graph->run(a);
  auto v = p->create_object<
      ::std::vector<::std::string, SwissAllocator<::std::string>>>();
  v->assign({"012", "345"});
  v->insert(v->end(), {"679", "901"});
}

TEST_F(ProcessorRunTest, memory_resource_usable_until_reset) {
  graph->run(a);
  auto& resource = p->memory_resource();
  {
    ::std::vector<::std::string, SwissAllocator<::std::string>> v(resource);
    v.assign({"012", "345"});
    v.insert(v.end(), {"679", "901"});
  }
}

TEST_F(ProcessorRunTest, reusable_object_clear_with_graph) {
  auto rs = p->create_reusable_object<SwissVector<SwissString>>();
  rs->assign({"10086", "10010"});
  graph->reset();
  ASSERT_TRUE(rs->empty());
  rs->insert(rs->end(), {"8610086", "8610010"});
  graph->reset();
  ASSERT_TRUE(rs->empty());
}
