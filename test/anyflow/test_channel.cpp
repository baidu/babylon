#include "babylon/anyflow/builder.h"

#include "gtest/gtest.h"

#include <future>

using ::babylon::anyflow::ChannelConsumer;
using ::babylon::anyflow::ChannelPublisher;
using ::babylon::anyflow::Graph;
using ::babylon::anyflow::GraphBuilder;
using ::babylon::anyflow::GraphData;
using ::babylon::anyflow::GraphProcessor;
using ::babylon::anyflow::GraphVertexClosure;
using ::babylon::anyflow::MutableChannelConsumer;
using ::babylon::anyflow::OutputChannel;
using ::babylon::anyflow::ThreadPoolGraphExecutor;

struct ChannelOutputProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    enter_promise.set_value(x);
    leave_future.get();
    return 0;
  }

  static ::std::promise<OutputChannel<::std::string>> enter_promise;
  static ::std::future<OutputChannel<::std::string>> enter_future;

  static ::std::promise<void> leave_promise;
  static ::std::future<void> leave_future;

  ANYFLOW_INTERFACE(ANYFLOW_EMIT_CHANNEL(::std::string, x))
};
::std::promise<OutputChannel<::std::string>>
    ChannelOutputProcessor::enter_promise;
::std::future<OutputChannel<::std::string>>
    ChannelOutputProcessor::enter_future;
::std::promise<void> ChannelOutputProcessor::leave_promise;
::std::future<void> ChannelOutputProcessor::leave_future;

struct ChannelInputProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    enter_promise.set_value(::std::move(a));
    leave_future.get();
    return 0;
  }

  static ::std::promise<ChannelConsumer<::std::string>> enter_promise;
  static ::std::future<ChannelConsumer<::std::string>> enter_future;

  static ::std::promise<void> leave_promise;
  static ::std::future<void> leave_future;

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_CHANNEL(::std::string, a))
};
::std::promise<ChannelConsumer<::std::string>>
    ChannelInputProcessor::enter_promise;
::std::future<ChannelConsumer<::std::string>>
    ChannelInputProcessor::enter_future;
::std::promise<void> ChannelInputProcessor::leave_promise;
::std::future<void> ChannelInputProcessor::leave_future;

struct MutableChannelInputProcessor : public GraphProcessor {
  virtual int process() noexcept override {
    enter_promise.set_value(::std::move(a));
    leave_future.get();
    return 0;
  }

  static ::std::promise<MutableChannelConsumer<::std::string>> enter_promise;
  static ::std::future<MutableChannelConsumer<::std::string>> enter_future;

  static ::std::promise<void> leave_promise;
  static ::std::future<void> leave_future;

  ANYFLOW_INTERFACE(ANYFLOW_DEPEND_MUTABLE_CHANNEL(::std::string, a))
};
::std::promise<MutableChannelConsumer<::std::string>>
    MutableChannelInputProcessor::enter_promise;
::std::future<MutableChannelConsumer<::std::string>>
    MutableChannelInputProcessor::enter_future;
::std::promise<void> MutableChannelInputProcessor::leave_promise;
::std::future<void> MutableChannelInputProcessor::leave_future;

struct ChannelTest : public ::testing::Test {
  virtual void SetUp() override {
    executor.initialize(4, 128);
    builder.set_executor(executor);
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<ChannelOutputProcessor>(
            new ChannelOutputProcessor);
      });
      vertex.named_emit("x").to("A");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<ChannelInputProcessor>(
            new ChannelInputProcessor);
      });
      vertex.named_depend("a").to("A");
      vertex.named_emit("done").to("B");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<ChannelOutputProcessor>(
            new ChannelOutputProcessor);
      });
      vertex.named_emit("x").to("MA");
    }
    {
      auto& vertex = builder.add_vertex([] {
        return ::std::unique_ptr<MutableChannelInputProcessor>(
            new MutableChannelInputProcessor);
      });
      vertex.named_depend("a").to("MA");
      vertex.named_emit("done").to("C");
    }
    ASSERT_EQ(0, builder.finish());
    graph = builder.build();
    ASSERT_TRUE(graph);

    a = graph->find_data("A");
    b = graph->find_data("B");
    c = graph->find_data("C");

    ChannelOutputProcessor::enter_promise =
        ::std::promise<OutputChannel<::std::string>>();
    ChannelOutputProcessor::enter_future =
        ChannelOutputProcessor::enter_promise.get_future();

    ChannelOutputProcessor::leave_promise = ::std::promise<void>();
    ChannelOutputProcessor::leave_future =
        ChannelOutputProcessor::leave_promise.get_future();

    ChannelInputProcessor::enter_promise =
        ::std::promise<ChannelConsumer<::std::string>>();
    ChannelInputProcessor::enter_future =
        ChannelInputProcessor::enter_promise.get_future();

    ChannelInputProcessor::leave_promise = ::std::promise<void>();
    ChannelInputProcessor::leave_future =
        ChannelInputProcessor::leave_promise.get_future();

    MutableChannelInputProcessor::enter_promise =
        ::std::promise<MutableChannelConsumer<::std::string>>();
    MutableChannelInputProcessor::enter_future =
        MutableChannelInputProcessor::enter_promise.get_future();

    MutableChannelInputProcessor::leave_promise = ::std::promise<void>();
    MutableChannelInputProcessor::leave_future =
        MutableChannelInputProcessor::leave_promise.get_future();
  }

  ThreadPoolGraphExecutor executor;
  GraphBuilder builder;
  ::std::unique_ptr<Graph> graph;

  GraphData* a;
  GraphData* b;
  GraphData* c;
};

TEST_F(ChannelTest, data_ready_after_channel_open) {
  auto closure = graph->run(b);
  auto channel = ChannelOutputProcessor::enter_future.get();
  ASSERT_EQ(::std::future_status::timeout,
            ChannelInputProcessor::enter_future.wait_for(
                ::std::chrono::milliseconds(100)));
  auto publisher = channel.open();
  ChannelInputProcessor::enter_future.get();
  publisher.close();
  ChannelOutputProcessor::leave_promise.set_value();
  ChannelInputProcessor::leave_promise.set_value();
}

TEST_F(ChannelTest, publish_consume_through_channel) {
  auto closure = graph->run(b);
  auto channel = ChannelOutputProcessor::enter_future.get();

  auto publisher = channel.open();
  publisher.publish("1");
  publisher.publish("2");
  publisher.publish("3");
  publisher.publish("4");
  publisher.close();

  auto consumer = ChannelInputProcessor::enter_future.get();
  ASSERT_EQ("1", *consumer.consume());
  auto range = consumer.consume(2);
  ASSERT_EQ(2, range.size());
  ASSERT_EQ("2", range[0]);
  ASSERT_EQ("3", range[1]);
  range = consumer.consume(2);
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("4", range[0]);
  ASSERT_EQ(nullptr, consumer.consume());

  ChannelOutputProcessor::leave_promise.set_value();
  ChannelInputProcessor::leave_promise.set_value();
}

TEST_F(ChannelTest, publisher_close_channel_when_destruct) {
  auto closure = graph->run(b);
  auto channel = ChannelOutputProcessor::enter_future.get();
  ::std::promise<void> promise;
  auto future = promise.get_future();
  ChannelConsumer<::std::string> consumer;
  {
    auto publisher = channel.open();
    consumer = ChannelInputProcessor::enter_future.get();
    ::std::thread([&] {
      ASSERT_EQ(nullptr, consumer.consume());
      promise.set_value();
    }).detach();
    ASSERT_EQ(::std::future_status::timeout,
              future.wait_for(::std::chrono::milliseconds(100)));
  }
  future.get();
  ChannelOutputProcessor::leave_promise.set_value();
  ChannelInputProcessor::leave_promise.set_value();
}

TEST_F(ChannelTest, publisher_movable) {
  auto closure = graph->run(b);
  auto channel = ChannelOutputProcessor::enter_future.get();
  ::std::promise<void> promise;
  auto future = promise.get_future();
  ChannelConsumer<::std::string> consumer;
  ChannelPublisher<::std::string> move_assigned_publisher;
  {
    auto publisher = channel.open();
    consumer = ChannelInputProcessor::enter_future.get();
    ::std::thread([&] {
      ASSERT_EQ(nullptr, consumer.consume());
      promise.set_value();
    }).detach();
    ChannelPublisher<::std::string> moved_publisher(::std::move(publisher));
    move_assigned_publisher = ::std::move(moved_publisher);
  }
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  move_assigned_publisher.close();
  future.get();
  ChannelOutputProcessor::leave_promise.set_value();
  ChannelInputProcessor::leave_promise.set_value();
}

TEST_F(ChannelTest, mutable_consumer_get_mutable_item) {
  auto closure = graph->run(c);
  auto channel = ChannelOutputProcessor::enter_future.get();

  auto publisher = channel.open();
  publisher.publish("1");
  publisher.publish("2");
  publisher.publish("3");
  publisher.publish("4");
  publisher.close();

  auto consumer = MutableChannelInputProcessor::enter_future.get();
  auto s = consumer.consume();
  s->append("x");
  ASSERT_EQ("1x", *s);
  auto range = consumer.consume(2);
  ASSERT_EQ(2, range.size());
  range[0].append("y");
  range[1].append("z");
  ASSERT_EQ("2y", range[0]);
  ASSERT_EQ("3z", range[1]);
  range = consumer.consume(2);
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("4", range[0]);
  ASSERT_EQ(nullptr, consumer.consume());

  ChannelOutputProcessor::leave_promise.set_value();
  MutableChannelInputProcessor::leave_promise.set_value();
}

TEST_F(ChannelTest, reject_illegal_type) {
  a->emit<int>();
  auto closure = graph->run(b);
  ASSERT_NE(0, closure.get());
}
