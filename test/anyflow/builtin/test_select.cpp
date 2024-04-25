#include "babylon/anyflow/builtin/select.h"

#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::builtin::SelectProcessor;

struct MutableProcessor : public GraphProcessor {
    virtual int process() noexcept override {
        x.emit().ref(*a);
        return 0;
    }

    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_MUTABLE_DATA(::std::string, a)
        ANYFLOW_EMIT_DATA(::std::string, x)
    )
};

struct ConstProcessor : public GraphProcessor {
    virtual int process() noexcept override {
        x.emit().ref(*a);
        return 0;
    }

    ANYFLOW_INTERFACE(
        ANYFLOW_DEPEND_DATA(::std::string, a)
        ANYFLOW_EMIT_DATA(::std::string, x)
    )
};

class SelectTest : public ::testing::Test {
public:
    virtual void SetUp() override {
        {
            auto& vertex = builder.add_vertex([] {
                return ::std::unique_ptr<ConstProcessor>(new ConstProcessor);
            });
            vertex.named_depend("a").to("CA");
            vertex.named_emit("x").to("CB");
        }
        {
            auto& vertex = builder.add_vertex([] {
                return ::std::unique_ptr<MutableProcessor>(new MutableProcessor);
            });
            vertex.named_depend("a").to("MA");
            vertex.named_emit("x").to("MB");
        }
        SelectProcessor::apply(builder, "CA", "C", "X", "Y");
        SelectProcessor::apply(builder, "MA", "C", "X", "Y");
        builder.finish();
        graph = builder.build();
        x = graph->find_data("X");
        y = graph->find_data("Y");
        c = graph->find_data("C");
        cb = graph->find_data("CB");
        mb = graph->find_data("MB");
    }
    
    GraphBuilder builder;
    ::std::unique_ptr<Graph> graph;

    GraphData* x;
    GraphData* y;
    GraphData* c;
    GraphData* cb;
    GraphData* mb;
};

TEST_F(SelectTest, forward_dependency_on_condition) {
    *x->emit<::std::string>() = "10086";
    *c->emit<bool>() = true;
    ASSERT_EQ(0, graph->run(cb).get());
    ASSERT_EQ("10086", *cb->value<::std::string>());
    graph->reset();
    *y->emit<::std::string>() = "10010";
    *c->emit<bool>() = false;
    ASSERT_EQ(0, graph->run(cb).get());
    ASSERT_EQ("10010", *cb->value<::std::string>());
}

TEST_F(SelectTest, forward_by_reference) {
    ::std::string s = "10086";
    x->emit<::std::string>().ref(s);
    *c->emit<bool>() = true;
    ASSERT_EQ(0, graph->run(cb).get());
    ASSERT_EQ(&s, cb->value<::std::string>());
    ASSERT_EQ("10086", *cb->value<::std::string>());
}

TEST_F(SelectTest, forward_mutable_as_mutable) {
    ::std::string s = "10086";
    x->emit<::std::string>().ref(s);
    *c->emit<bool>() = true;
    ASSERT_EQ(0, graph->run(mb).get());
    ASSERT_EQ(&s, mb->value<::std::string>());
    ASSERT_EQ("10086", *mb->value<::std::string>());
}

TEST_F(SelectTest, reject_const_as_mutable) {
    ::std::string s = "10086";
    x->emit<::std::string>().cref(s);
    *c->emit<bool>() = true;
    ASSERT_NE(0, graph->run(mb).get());
}
