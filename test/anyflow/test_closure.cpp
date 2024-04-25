#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::anyflow::Closure;
using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::ThreadPoolGraphExecutor;

struct DummyProcessor : public GraphProcessor {
    virtual int process() noexcept override {
        auto do_process = begin.get();
        begun.set_value();
        if (do_process) {
            *x.emit() = "10086";
        }
        auto ret = end.get();
        ended.set_value();
        return ret;
    }

    ANYFLOW_INTERFACE(
        ANYFLOW_EMIT_DATA(::std::string, x)
    )

    static ::std::future<bool> begin;
    static ::std::promise<void> begun;
    static ::std::future<int> end;
    static ::std::promise<void> ended;
};
::std::future<bool> DummyProcessor::begin;
::std::promise<void> DummyProcessor::begun;
::std::future<int> DummyProcessor::end;
::std::promise<void> DummyProcessor::ended;

struct ClosureTest : public ::testing::Test {
    virtual void SetUp() override {
        {
            auto& vertex = builder.add_vertex([] {
                return ::std::unique_ptr<DummyProcessor>(new DummyProcessor);
            });
            vertex.named_depend("a").to("A");
            vertex.named_emit("x").to("B");
        }
        executor.initialize(4, 128);
        builder.set_executor(executor);
        builder.finish();
        graph = builder.build();
        a = graph->find_data("A");
        b = graph->find_data("B");
        
        begin = ::std::promise<bool> {};
        DummyProcessor::begin = begin.get_future();
        DummyProcessor::begun = ::std::promise<void> {};
        begun = DummyProcessor::begun.get_future();
        end = ::std::promise<int> {};
        DummyProcessor::end = end.get_future();
        DummyProcessor::ended = ::std::promise<void> {};
        ended = DummyProcessor::ended.get_future();
    }

    ThreadPoolGraphExecutor executor;
    GraphBuilder builder;
    ::std::unique_ptr<Graph> graph;

    GraphData* a;
    GraphData* b;

    ::std::promise<bool> begin;
    ::std::future<void> begun;
    ::std::promise<int> end;
    ::std::future<void> ended;
};

TEST_F(ClosureTest, finish_when_data_ready) {
    a->emit<int>();
    auto closure = graph->run(b);
    ::std::thread([&] {
        ::usleep(100000);
        begin.set_value(true);
        end.set_value(0);
    }).detach();
    ASSERT_FALSE(closure.finished());
    ASSERT_EQ(0, closure.get());
    ASSERT_TRUE(closure.finished());
    ASSERT_EQ(0, closure.error_code());
    ASSERT_EQ("10086", *b->value<std::string>());
}

TEST_F(ClosureTest, finish_when_error_occur) {
    a->emit<int>();
    auto closure = graph->run(b);
    ::std::thread([&] {
        ::usleep(100000);
        begin.set_value(false);
        end.set_value(-1);
    }).detach();
    ASSERT_FALSE(closure.finished());
    ASSERT_NE(0, closure.get());
    ASSERT_TRUE(closure.finished());
    ASSERT_NE(0, closure.error_code());
    ASSERT_TRUE(b->empty());
}

TEST_F(ClosureTest, finish_when_idle_even_data_is_not_ready) {
    a->emit<int>();
    auto closure = graph->run(b);
    ::std::thread([&] {
        ::usleep(100000);
        begin.set_value(false);
        end.set_value(0);
    }).detach();
    ASSERT_FALSE(closure.finished());
    ASSERT_EQ(0, closure.get());
    ASSERT_TRUE(closure.finished());
    ASSERT_EQ(0, closure.error_code());
    ASSERT_TRUE(b->empty());
}

TEST_F(ClosureTest, wait_until_idle) {
    a->emit<int>();
    auto closure = graph->run(b);
    ::std::thread([&] {
        begin.set_value(true);
        ::usleep(100000);
        end.set_value(0);
    }).detach();
    ASSERT_EQ(0, closure.get());
    ASSERT_TRUE(closure.finished());
    ASSERT_EQ(0, closure.error_code());
    ASSERT_EQ("10086", *b->value<std::string>());
    ASSERT_EQ(::std::future_status::timeout, ended.wait_for(::std::chrono::milliseconds(0)));
    closure.wait();
    ASSERT_EQ(::std::future_status::ready, ended.wait_for(::std::chrono::milliseconds(0)));
}

TEST_F(ClosureTest, destroy_automatically_wait) {
    a->emit<int>();
    {
        auto closure = graph->run(b);
        ::std::thread([&] {
            begin.set_value(true);
            ::usleep(100000);
            end.set_value(0);
        }).detach();
        ASSERT_EQ(0, closure.get());
        ASSERT_TRUE(closure.finished());
        ASSERT_EQ(0, closure.error_code());
        ASSERT_EQ("10086", *b->value<std::string>());
        ASSERT_EQ(::std::future_status::timeout, ended.wait_for(::std::chrono::milliseconds(0)));
    }
    ASSERT_EQ(::std::future_status::ready, ended.wait_for(::std::chrono::milliseconds(0)));
}

TEST_F(ClosureTest, callback_invoke_on_finish) {
    a->emit<int>();
    auto closure = graph->run(b);
    ::std::thread([&] {
        ::usleep(100000);
        end.set_value(0);
        begin.set_value(true);
    }).detach();
    ::std::promise<void> p;
    auto f = p.get_future();
    closure.on_finish([&, graph = ::std::move(graph)] (Closure&&) {
        p.set_value();
    });
    ASSERT_EQ(::std::future_status::timeout, begun.wait_for(::std::chrono::milliseconds(0)));
    f.get();
    ASSERT_EQ(::std::future_status::ready, begun.wait_for(::std::chrono::milliseconds(0)));
}

TEST_F(ClosureTest, callback_invoke_in_place_after_finish) {
    a->emit<int>();
    auto closure = graph->run(b);
    begin.set_value(true);
    end.set_value(0);
    ASSERT_EQ(0, closure.get());
    bool called = false;
    closure.on_finish([&] (Closure&&) {
        called = true;
    });
    ASSERT_TRUE(called);
}

TEST_F(ClosureTest, moveable) {
    a->emit<int>();
    auto closure = graph->run(b);
    begin.set_value(true);
    end.set_value(0);
    Closure moved_closure(::std::move(closure));
    ASSERT_EQ(0, moved_closure.get());
}
