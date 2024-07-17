#include "babylon/concurrent/bounded_queue.h"
#include "babylon/logging/logger.h" // BABYLON_LOG

#include "gtest/gtest.h"

#include <future>
#include <random>

using ::babylon::ConcurrentBoundedQueue;

TEST(concurrent_bounded_queue, default_constructed_with_capacity_one) {
  ConcurrentBoundedQueue<::std::string> queue;
  ASSERT_EQ(1, queue.capacity());
}

TEST(concurrent_bounded_queue, move_constructable) {
  ConcurrentBoundedQueue<::std::string> queue(1024);
  ConcurrentBoundedQueue<::std::string> moved_queue(::std::move(queue));
  ASSERT_EQ(1024, moved_queue.capacity());
}

TEST(concurrent_bounded_queue, capacity_ceil_to_pow2) {
  ConcurrentBoundedQueue<::std::string> queue;
  ASSERT_EQ(1, queue.capacity());
  ASSERT_EQ(1, queue.reserve_and_clear(0));
  ASSERT_EQ(1, queue.reserve_and_clear(1));
  ASSERT_EQ(2, queue.reserve_and_clear(2));
  ASSERT_EQ(4, queue.reserve_and_clear(3));
  ASSERT_EQ(4, queue.reserve_and_clear(4));
  ASSERT_EQ(8, queue.reserve_and_clear(5));
  ASSERT_EQ(8, queue.reserve_and_clear(6));
  ASSERT_EQ(8, queue.reserve_and_clear(7));
  ASSERT_EQ(8, queue.reserve_and_clear(8));
}

TEST(concurrent_bounded_queue, push_pop_with_value) {
  ConcurrentBoundedQueue<::std::string> queue;
  queue.push("10086");
  ASSERT_EQ(1, queue.size());
  ::std::string s;
  queue.pop(s);
  ASSERT_EQ("10086", s);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_pop_with_callback) {
  ConcurrentBoundedQueue<::std::string> queue;
  queue.push([](::std::string& s) {
    s = "10086";
  });
  ASSERT_EQ(1, queue.size());
  queue.pop([&](::std::string& s) {
    ASSERT_EQ("10086", s);
  });
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_pop_non_concurrent) {
  ConcurrentBoundedQueue<::std::string> queue;
  queue.push<false, false, false>("10086");
  ASSERT_EQ(1, queue.size());
  ::std::string s;
  queue.pop<false, false, false>(s);
  ASSERT_EQ("10086", s);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, try_pop_fail_on_empty) {
  ConcurrentBoundedQueue<::std::string> queue;
  ::std::string s;
  ASSERT_FALSE(queue.try_pop(s));
  ASSERT_TRUE(s.empty());
  queue.push("10086");
  ASSERT_TRUE(queue.try_pop(s));
  ASSERT_EQ("10086", s);
  bool called = false;
  ASSERT_FALSE(queue.try_pop([&](::std::string&) {
    called = true;
  }));
  ASSERT_FALSE(called);
  queue.push("10010");
  ASSERT_TRUE(queue.try_pop([&](::std::string& src) {
    s = src;
  }));
  ASSERT_EQ("10010", s);
}

TEST(concurrent_bounded_queue, try_pop_wakeup_blocking_push) {
  ConcurrentBoundedQueue<::std::string> queue;
  queue.push("10086");
  auto finished = ::std::async(::std::launch::async, [&] {
    queue.push("10010");
  });
  ASSERT_EQ(::std::future_status::timeout,
            finished.wait_for(::std::chrono::milliseconds(100)));
  ASSERT_TRUE(queue.try_pop([&](::std::string&) {}));
  finished.get();
}

TEST(concurrent_bounded_queue, push_pop_batch_with_value) {
  ConcurrentBoundedQueue<::std::string> queue(2);
  ::std::vector<::std::string> v = {"10086", "10010"};
  queue.push_n(v.begin(), v.end());
  ASSERT_EQ(2, queue.size());
  ::std::string s;
  queue.pop(s);
  queue.push("8610086");
  ASSERT_EQ(2, queue.size());
  v.clear();
  v.assign({"x", "x"});
  queue.pop_n(v.begin(), v.end());
  ASSERT_EQ("10010", v[0]);
  ASSERT_EQ("8610086", v[1]);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_pop_batch_with_value_wrapped) {
  ConcurrentBoundedQueue<::std::string> queue(2);
  ::std::vector<::std::string> v = {"10010", "10086"};
  queue.push("10000");
  ::std::string s;
  queue.pop(s);
  queue.push_n(v.begin(), v.end());
  ASSERT_EQ(2, queue.size());
  v.clear();
  v.assign({"x", "x"});
  queue.pop_n(v.begin(), v.end());
  ASSERT_EQ("10010", v[0]);
  ASSERT_EQ("10086", v[1]);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_pop_batch_with_value_non_concurrent) {
  ConcurrentBoundedQueue<::std::string> queue(2);
  ::std::vector<::std::string> v = {"10086", "10010"};
  queue.push_n<false, false, false>(v.begin(), v.end());
  ASSERT_EQ(2, queue.size());
  ::std::string s;
  queue.pop(s);
  queue.push("8610086");
  ASSERT_EQ(2, queue.size());
  v.clear();
  v.assign({"x", "x"});
  queue.pop_n<false, false, false>(v.begin(), v.end());
  ASSERT_EQ("10010", v[0]);
  ASSERT_EQ("8610086", v[1]);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_pop_batch_callback_with_iterator) {
  typedef ConcurrentBoundedQueue<::std::string>::Iterator IT;
  ConcurrentBoundedQueue<::std::string> queue(2);
  queue.push_n(
      [&](IT iter, IT end) {
        ASSERT_EQ(end, iter + 2);
        ASSERT_EQ(iter, end - 2);
        ASSERT_LT(iter, end);
        ASSERT_LE(iter, end);
        ASSERT_GT(end, iter);
        ASSERT_GE(end, iter);
        while (iter != end) {
          iter->assign("10010");
          ++iter;
        }
      },
      2);
  queue.pop_n(
      [&](IT iter, IT end) {
        while (iter != end) {
          ASSERT_EQ("10010", *iter++);
        }
      },
      2);
}

TEST(concurrent_bounded_queue, try_pop_n_like_normal_batch_before_end) {
  typedef ConcurrentBoundedQueue<::std::string>::Iterator IT;
  ConcurrentBoundedQueue<::std::string> queue(2);
  ::std::vector<::std::string> v = {"10086", "10010"};
  queue.push_n(v.begin(), v.end());
  ASSERT_EQ(2, queue.size());
  ::std::string s;
  queue.pop(s);
  queue.push("8610086");
  ASSERT_EQ(2, queue.size());
  v.clear();
  queue.try_pop_n<true, true>(
      [&](IT iter, IT end) {
        ::std::copy(iter, end, ::std::back_inserter(v));
      },
      2);
  ASSERT_EQ(2, v.size());
  ASSERT_EQ("10010", v[0]);
  ASSERT_EQ("8610086", v[1]);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, try_pop_n_cut_to_end) {
  typedef ConcurrentBoundedQueue<::std::string>::Iterator IT;
  ConcurrentBoundedQueue<::std::string> queue;
  queue.reserve_and_clear(4);
  ::std::vector<::std::string> v = {"a", "b", "10086", "10010"};
  queue.push_n(v.begin(), v.end());
  ASSERT_EQ(4, queue.size());
  ::std::string s;
  queue.pop(s);
  queue.pop(s);
  queue.push("8610086");
  ASSERT_EQ(3, queue.size());
  v.clear();
  auto ret = queue.try_pop_n<true, true>(
      [&](IT iter, IT end) {
        ::std::copy(iter, end, ::std::back_inserter(v));
      },
      4);
  ASSERT_EQ(3, ret);
  ASSERT_EQ(3, v.size());
  ASSERT_EQ("10086", v[0]);
  ASSERT_EQ("10010", v[1]);
  ASSERT_EQ("8610086", v[2]);
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, press_blocking_mpsc) {
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 200;
  ConcurrentBoundedQueue<size_t> queue {batch_concurrent + single_concurrent};

  ::std::vector<::std::future<size_t>> push_futures;
  push_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&] {
      ::std::mt19937 gen(::std::random_device {}());
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        size_t value = gen();
        ::std::fill_n(vec, batch_size, value);
        queue.push_n(vec, vec + batch_size);
        sum += value * batch_size;
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&] {
      ::std::mt19937 gen(::std::random_device {}());
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        size_t value = gen();
        queue.push(value);
        sum += value;
      }
      return sum;
    }));
  }

  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < batch_concurrent * times; ++j) {
      queue.pop_n<false, true, true>(vec, vec + batch_size);
      for (size_t i = 0; i < batch_size; ++i) {
        sum += vec[i];
      }
    }
    for (size_t j = 0; j < single_concurrent * times; ++j) {
      size_t v = 0;
      queue.pop<false, true, true>(v);
      sum += v;
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_blocking_mpsc_with_try_pop) {
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 200;
  ConcurrentBoundedQueue<size_t> queue {batch_concurrent + single_concurrent};

  ::std::vector<::std::future<size_t>> push_futures;
  push_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        ::std::fill_n(vec, batch_size, i * times + j);
        queue.push_n(vec, vec + batch_size);
        sum += (i * times + j) * batch_size;
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        queue.push(i * times + j);
        sum += i * times + j;
      }
      return sum;
    }));
  }

  typedef ConcurrentBoundedQueue<size_t>::Iterator IT;
  size_t total =
      batch_size * batch_concurrent * times + single_concurrent * times;
  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t count = 0;
    struct ::timespec timeout {
      0, 1000000
    };
    while (count < total) {
      size_t poped = queue.try_pop_n_exclusively_until<true>(
          [&](IT iter, IT end) {
            while (iter != end) {
              sum += *iter++;
            }
          },
          batch_size, &timeout);
      count += poped;
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_spining_mpsc) {
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 200;
  ConcurrentBoundedQueue<size_t> queue {batch_concurrent + single_concurrent};

  ::std::vector<::std::future<size_t>> push_futures;
  push_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        ::std::fill_n(vec, batch_size, i * times + j);
        queue.push_n<true, false, false>(vec, vec + batch_size);
        sum += (i * times + j) * batch_size;
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        queue.push<true, false, false>(i * times + j);
        sum += i * times + j;
      }
      return sum;
    }));
  }

  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < batch_concurrent * times; ++j) {
      queue.pop_n<false, false, false>(vec, vec + batch_size);
      for (size_t i = 0; i < batch_size; ++i) {
        sum += vec[i];
      }
    }
    for (size_t j = 0; j < single_concurrent * times; ++j) {
      size_t v = 0;
      queue.pop<false, false, false>(v);
      sum += v;
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_spining_mpsc_with_try_pop) {
  typedef ConcurrentBoundedQueue<size_t>::Iterator IT;
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 200;
  ConcurrentBoundedQueue<size_t> queue {batch_concurrent + single_concurrent};

  ::std::vector<::std::future<size_t>> push_futures;
  push_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        ::std::fill_n(vec, batch_size, i * times + j);
        queue.push_n<true, false, false>(vec, vec + batch_size);
        sum += (i * times + j) * batch_size;
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&, i] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        queue.push<true, false, false>(i * times + j);
        sum += i * times + j;
      }
      return sum;
    }));
  }

  size_t total =
      batch_size * batch_concurrent * times + single_concurrent * times;
  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t count = 0;
    while (count < total) {
      size_t poped = queue.try_pop_n<false, false>(
          [&](IT iter, IT end) {
            while (iter != end) {
              sum += *iter++;
            }
          },
          batch_size);
      if (poped == 0) {
        ::usleep(0);
      }
      count += poped;
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_blocking_spsc) {
  size_t batch_size = 10;
  size_t times = 1000;
  ConcurrentBoundedQueue<size_t> queue(64);
  ::std::vector<::std::future<size_t>> push_futures;
  ::std::vector<::std::future<size_t>> pop_futures;
  push_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < times; ++j) {
      ::std::fill_n(vec, batch_size, j);
      queue.push_n<true, true, true>(vec, vec + batch_size);
      sum += j * batch_size;
      queue.push<false, true, true>(j);
      sum += j;
    }
    return sum;
  }));
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < times; ++j) {
      size_t v = 0;
      queue.pop<false, true, true>(&v);
      sum += v;
      queue.pop_n<false, true, true>(vec, vec + batch_size);
      for (size_t i = 0; i < batch_size; ++i) {
        sum += vec[i];
      }
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_spining_spsc) {
  size_t batch_size = 10;
  size_t times = 1000;
  ConcurrentBoundedQueue<size_t> queue(64);
  ::std::vector<::std::future<size_t>> push_futures;
  ::std::vector<::std::future<size_t>> pop_futures;
  push_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < times; ++j) {
      ::std::fill_n(vec, batch_size, j);
      queue.push_n<false, false, false>(vec, vec + batch_size);
      sum += j * batch_size;
      queue.push<false, false, false>(j);
      sum += j;
    }
    return sum;
  }));
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t vec[batch_size];
    for (size_t j = 0; j < times; ++j) {
      size_t v = 0;
      queue.pop<false, false, false>(v);
      sum += v;
      queue.pop_n<false, false, false>(vec, vec + batch_size);
      for (size_t i = 0; i < batch_size; ++i) {
        sum += vec[i];
      }
    }
    return sum;
  }));

  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_nonblocking_spsc) {
  size_t times = 10000;
  ConcurrentBoundedQueue<size_t> queue(64);
  ::std::vector<::std::future<size_t>> push_futures;
  ::std::vector<::std::future<size_t>> pop_futures;
  push_futures.emplace_back(::std::async(::std::launch::async, [&] {
    ::std::mt19937 gen(::std::random_device {}());
    size_t sum = 0;
    for (size_t i = 0; i < times; ++i) {
      auto value = gen() % 1024;
      if (queue.try_push<false, false>(value)) {
        sum += value;
      }
    }
    return sum;
  }));
  ::std::atomic<bool> running {true};
  pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
    size_t sum = 0;
    size_t value = 0;
    auto is_running = true;
    while (is_running) {
      is_running = running.load(::std::memory_order_acquire);
      if (queue.try_pop<false, false>(value)) {
        is_running = true;
        sum += value;
      } else {
        ::sched_yield();
      }
    }
    return sum;
  }));
  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  running.store(false, ::std::memory_order_release);
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  BABYLON_LOG(INFO) << "push sum " << push_sum << " pop_sum " << pop_sum;
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, press_nonblocking_mpmc) {
  size_t concurrent = 32;
  size_t times = 10000;
  ConcurrentBoundedQueue<size_t> queue(64);
  ::std::vector<::std::future<size_t>> push_futures;
  ::std::vector<::std::future<size_t>> pop_futures;
  for (size_t j = 0; j < concurrent; ++j) {
    push_futures.emplace_back(::std::async(::std::launch::async, [&] {
      ::std::mt19937 gen(::std::random_device {}());
      size_t sum = 0;
      for (size_t i = 0; i < times; ++i) {
        auto value = gen() % 1024;
        if (queue.try_push<true, false>(value)) {
          sum += value;
        }
      }
      return sum;
    }));
  }
  ::std::atomic<bool> running {true};
  for (size_t j = 0; j < concurrent; ++j) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      size_t value = 0;
      auto is_running = true;
      while (is_running) {
        is_running = running.load(::std::memory_order_acquire);
        if (queue.try_pop<true, false>(value)) {
          is_running = true;
          sum += value;
        } else {
          ::sched_yield();
        }
      }
      return sum;
    }));
  }
  size_t push_sum = 0;
  for (auto& future : push_futures) {
    push_sum += future.get();
  }
  running.store(false, ::std::memory_order_release);
  size_t pop_sum = 0;
  for (auto& future : pop_futures) {
    pop_sum += future.get();
  }
  BABYLON_LOG(INFO) << "push sum " << push_sum << " pop_sum " << pop_sum;
  ASSERT_EQ(push_sum, pop_sum);
}

TEST(concurrent_bounded_queue, reusable_after_clear) {
  ConcurrentBoundedQueue<::std::string> queue(2);
  queue.push("10086");
  queue.push("10010");
  queue.pop([](::std::string& value) {
    ASSERT_EQ("10086", value);
  });
  ASSERT_EQ(1, queue.size());
  queue.clear();
  ASSERT_EQ(0, queue.size());
  queue.push("8610086");
  queue.pop([](::std::string& value) {
    ASSERT_EQ("8610086", value);
  });
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_can_move_object) {
  ConcurrentBoundedQueue<::std::string> queue;
  ::std::string s = "10086";
  s.reserve(32);
  auto* data = s.data();
  queue.push(::std::move(s));
  ASSERT_EQ(0, s.size());
  ASSERT_EQ(1, queue.size());
  queue.pop([&](::std::string& value) {
    ASSERT_EQ(data, value.data());
    ASSERT_EQ("10086", value);
  });
  ASSERT_EQ(0, queue.size());
}

TEST(concurrent_bounded_queue, push_can_fallback_to_pop_instead_of_wait) {
  typedef ConcurrentBoundedQueue<::std::string>::Iterator IT;
  ConcurrentBoundedQueue<::std::string> queue(2);
  queue.push("10086");
  size_t push_times = 0;
  size_t pop_times = 0;
  queue.push_n(
      [&](IT iter, IT end) {
        while (iter++ != end) {
          ++push_times;
        }
      },
      [&](IT iter, IT end) {
        while (iter++ != end) {
          ++pop_times;
        }
      },
      2);
  ASSERT_EQ(2, push_times);
  ASSERT_EQ(1, pop_times);
}

TEST(concurrent_bounded_queue, pop_can_fallback_to_push_instead_of_wait) {
  typedef ConcurrentBoundedQueue<::std::string>::Iterator IT;
  ConcurrentBoundedQueue<::std::string> queue(2);
  queue.push("10086");
  size_t push_times = 0;
  size_t pop_times = 0;
  queue.pop_n(
      [&](IT iter, IT end) {
        while (iter++ != end) {
          ++pop_times;
        }
      },
      [&](IT iter, IT end) {
        while (iter++ != end) {
          ++push_times;
        }
      },
      2);
  ASSERT_EQ(1, push_times);
  ASSERT_EQ(2, pop_times);
}
