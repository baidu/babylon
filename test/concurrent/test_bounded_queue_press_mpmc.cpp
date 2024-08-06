#include "babylon/concurrent/bounded_queue.h"
#include "babylon/logging/logger.h"

#include "gtest/gtest.h"

#include <future>
#include <random>

using ::babylon::ConcurrentBoundedQueue;

TEST(concurrent_bounded_queue, press_blocking_mpmc) {
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 10;
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
  pop_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        queue.pop_n(vec, vec + batch_size);
        for (size_t k = 0; k < batch_size; ++k) {
          sum += vec[k];
        }
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        size_t v = 0;
        queue.pop(v);
        sum += v;
      }
      return sum;
    }));
  }

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

TEST(concurrent_bounded_queue, press_spining_mpmc) {
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 10;
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
        queue.push_n<true, false, false>(vec, vec + batch_size);
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
        queue.push<true, false, false>(value);
        sum += value;
      }
      return sum;
    }));
  }

  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.reserve(batch_concurrent + single_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      size_t vec[batch_size];
      for (size_t j = 0; j < times; ++j) {
        queue.pop_n<true, false, false>(vec, vec + batch_size);
        for (size_t k = 0; k < batch_size; ++k) {
          sum += vec[k];
        }
      }
      return sum;
    }));
  }
  for (size_t i = 0; i < single_concurrent; ++i) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      for (size_t j = 0; j < times; ++j) {
        size_t v = 0;
        queue.pop<true, false, false>(v);
        sum += v;
      }
      return sum;
    }));
  }

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

TEST(concurrent_bounded_queue, press_spining_mpmc_with_try_pop) {
  typedef ConcurrentBoundedQueue<size_t>::Iterator IT;
  size_t batch_size = 10;
  size_t batch_concurrent = 32;
  size_t single_concurrent = 32;
  size_t times = 10;
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
  ::std::atomic<size_t> count(0);
  ::std::vector<::std::future<size_t>> pop_futures;
  pop_futures.reserve(batch_concurrent);
  for (size_t i = 0; i < batch_concurrent; ++i) {
    pop_futures.emplace_back(::std::async(::std::launch::async, [&] {
      size_t sum = 0;
      while (count < total) {
        size_t poped = queue.try_pop_n<true, false>(
            [&](IT iter, IT end) {
              while (iter != end) {
                sum += *iter++;
              }
            },
            batch_size);
        if (poped == 0) {
          ::usleep(0);
        }
        count.fetch_add(poped);
      }
      return sum;
    }));
  }

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
