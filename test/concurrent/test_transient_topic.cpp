#include <babylon/concurrent/transient_topic.h>

#include <gtest/gtest.h>

#include <future>
#include <thread>

using ::babylon::ConcurrentTransientTopic;

TEST(concurrent_transient_topic, movable) {
  ConcurrentTransientTopic<::std::string> topic;
  topic.publish("1");
  topic.publish("2");
  ConcurrentTransientTopic<::std::string> moved_topic(::std::move(topic));
  ConcurrentTransientTopic<::std::string> move_assigned_topic;
  move_assigned_topic = ::std::move(moved_topic);
  move_assigned_topic.publish("3");
  move_assigned_topic.publish("4");
  move_assigned_topic.close();

  auto consumer = move_assigned_topic.subscribe();
  ASSERT_EQ("1", *consumer.consume());
  ASSERT_EQ("2", *consumer.consume());
  ASSERT_EQ("3", *consumer.consume());
  ASSERT_EQ("4", *consumer.consume());
  ASSERT_EQ(nullptr, consumer.consume());
}

TEST(concurrent_transient_topic, publish_consume_fifo) {
  ConcurrentTransientTopic<::std::string> topic;
  topic.publish("1");
  topic.publish("2");
  topic.publish("3");
  topic.publish("4");
  topic.publish("5");
  topic.close();

  auto consumer = topic.subscribe();
  auto* string = consumer.consume();
  ASSERT_NE(nullptr, string);
  ASSERT_EQ("1", *string);
  string = consumer.consume();
  ASSERT_NE(nullptr, string);
  ASSERT_EQ("2", *string);
  auto range = consumer.consume(2);
  ASSERT_EQ(2, range.size());
  ASSERT_EQ("3", range[0]);
  ASSERT_EQ("4", range[1]);
  range = consumer.consume(10);
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("5", range[0]);
  string = consumer.consume();
  ASSERT_EQ(nullptr, string);
  range = consumer.consume(2);
  ASSERT_EQ(0, range.size());
}

TEST(concurrent_transient_topic, consume_wait_publish) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  ::std::promise<::std::string*> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    promise.set_value(consumer.consume());
  }).detach();
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  topic.publish("10086");
  ASSERT_EQ("10086", *future.get());
}

TEST(concurrent_transient_topic, consume_stop_after_close) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  ::std::promise<::std::string*> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    promise.set_value(consumer.consume());
  }).detach();
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  topic.close();
  ASSERT_EQ(nullptr, future.get());
}

TEST(concurrent_transient_topic, consume_wait_for_full_batch) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  ::std::promise<ConcurrentTransientTopic<::std::string>::ConsumeRange> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    promise.set_value(consumer.consume(2));
  }).detach();
  topic.publish("10086");
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  topic.publish("10087");
  auto range = future.get();
  ASSERT_EQ(2, range.size());
  ASSERT_EQ("10086", range[0]);
  ASSERT_EQ("10087", range[1]);
}

TEST(concurrent_transient_topic,
     consume_stop_before_full_batch_get_small_range) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  ::std::promise<ConcurrentTransientTopic<::std::string>::ConsumeRange> promise;
  auto future = promise.get_future();
  ::std::thread([&] {
    promise.set_value(consumer.consume(2));
  }).detach();
  topic.publish("10086");
  ASSERT_EQ(::std::future_status::timeout,
            future.wait_for(::std::chrono::milliseconds(100)));
  topic.close();
  auto range = future.get();
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("10086", range[0]);
}

TEST(concurrent_transient_topic, const_topic_consume_const_item) {
  ConcurrentTransientTopic<::std::string> topic;
  topic.publish("10086");
  topic.publish("10010");
  auto consumer =
      static_cast<const ConcurrentTransientTopic<::std::string>&>(topic)
          .subscribe();
  ASSERT_EQ("10086", *consumer.consume());
  auto range = consumer.consume(1);
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("10010", range[0]);
}

TEST(concurrent_transient_topic, topic_can_reuse_after_clear) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  topic.publish("10086");
  topic.close();
  auto range = consumer.consume(2);
  ASSERT_EQ(1, range.size());
  ASSERT_EQ("10086", range[0]);
  topic.clear();
  topic.publish("107");
  consumer = topic.subscribe();
  auto string = consumer.consume();
  ASSERT_NE(nullptr, string);
  ASSERT_EQ("107", *string);
}

TEST(concurrent_transient_topic, publish_can_be_done_in_batch) {
  ConcurrentTransientTopic<::std::string> topic;
  auto consumer = topic.subscribe();
  topic.publish("0");
  using Iterator = ConcurrentTransientTopic<::std::string>::Iterator;
  size_t n = 1;
  topic.publish_n(4, [&](Iterator iter, Iterator end) {
    while (iter != end) {
      *iter++ = ::std::to_string(n++);
    }
  });
  topic.publish_n(5, [&](Iterator iter, Iterator end) {
    while (iter != end) {
      *iter++ = ::std::to_string(n++);
    }
  });
  auto range = consumer.consume(10);
  for (size_t i = 0; i < range.size(); ++i) {
    ASSERT_EQ(::std::to_string(i), range[i]);
  }
}

TEST(concurrent_transient_topic, support_concurrent_publish_consume) {
  ConcurrentTransientTopic<::std::string> topic;

  size_t rounds = 50;
  size_t publish_concurrents = 30;
  size_t consume_concurrents = 4;
  size_t expect_sum =
      consume_concurrents * (publish_concurrents - 1) * publish_concurrents / 2;
  for (size_t round = 0; round < rounds; ++round) {
    topic.clear();
    ::std::atomic<size_t> sum(0);

    ::std::vector<::std::thread> consume_threads;
    consume_threads.reserve(consume_concurrents);
    for (size_t i = 0; i < consume_concurrents; ++i) {
      consume_threads.emplace_back([&] {
        auto consumer = topic.subscribe();
        for (const ::std::string* string = consumer.consume();
             string != nullptr; string = consumer.consume()) {
          sum.fetch_add(atoi(string->data()), ::std::memory_order_relaxed);
        }
      });
    }

    ::std::vector<::std::thread> publish_threads;
    publish_threads.reserve(publish_concurrents);
    for (size_t i = 0; i < publish_concurrents; ++i) {
      publish_threads.emplace_back([&, i] {
        topic.publish(::std::to_string(i));
      });
    }
    for (auto& thread : publish_threads) {
      thread.join();
    }
    topic.close();
    for (auto& thread : consume_threads) {
      thread.join();
    }
    ASSERT_EQ(expect_sum, sum.load(::std::memory_order_relaxed));
  }
}
