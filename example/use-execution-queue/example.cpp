#include "babylon/concurrent/execution_queue.h"

#include "brpc/server.h"
#include "bthread/execution_queue.h"
#include "bthread_executor.h"
#include "gflags/gflags.h"

DEFINE_int32(dummy_port, 8000, "TCP Port of this dummy server");
DEFINE_uint64(concurrency, 7, "Concurrent logging thread num");
DEFINE_string(mode, "babylon", "");
DEFINE_uint64(qps, 100000, "");

namespace brpc {
bool IsAskedToQuit();
}

struct Task {
  int64_t begin;
};

::bthread::ExecutionQueueId<Task> bthread_queue_id;
::babylon::ConcurrentExecutionQueue<Task> babylon_queue;

::bvar::Adder<ssize_t> pending;
::bvar::LatencyRecorder latency;

void run_once_bthread(int64_t begin) {
  pending << 1;
  ::bthread::TaskOptions options;
  ::bthread::execution_queue_execute(bthread_queue_id, {begin}, &options);
}

void run_once_babylon(int64_t begin) {
  pending << 1;
  babylon_queue.execute({begin});
}

int bthread_queue_consume(void*, ::bthread::TaskIterator<Task>& iter) {
  if (iter.is_queue_stopped()) {
    return 0;
  }
  for (; iter; ++iter) {
    latency << ::butil::monotonic_time_ns() - iter->begin;
    pending << -1;
  }
  return 0;
}

void babylon_queue_consume(
    ::babylon::ConcurrentExecutionQueue<Task>::Iterator iter,
    ::babylon::ConcurrentExecutionQueue<Task>::Iterator end) {
  for (; iter != end; ++iter) {
    latency << ::butil::monotonic_time_ns() - iter->begin;
    pending << -1;
  }
}

void run_loop() {
  size_t times = 0;
  int64_t expect_us = 0;
  while (expect_us < 10000) {
    ++times;
    expect_us = 1000000L * times * FLAGS_concurrency / FLAGS_qps;
  }
  ::std::cerr << "expect_us " << expect_us << " times " << times << ::std::endl;

  void (*run_once)(int64_t);
  if (FLAGS_mode == "babylon") {
    run_once = run_once_babylon;
  } else if (FLAGS_mode == "bthread") {
    run_once = run_once_bthread;
  } else {
    return;
  }

  ::std::vector<::babylon::Future<void>> futures;
  for (size_t i = 0; i < FLAGS_concurrency; ++i) {
    futures.emplace_back(::babylon::BthreadExecutor::instance().execute([&] {
      while (!::brpc::IsAskedToQuit()) {
        auto begin = ::butil::monotonic_time_ns();
        for (size_t j = 0; j < times; ++j) {
          run_once(::butil::monotonic_time_ns());
        }
        auto end = ::butil::monotonic_time_ns();
        auto use_us = (end - begin) / 1000;
        if (use_us < expect_us) {
          ::usleep(expect_us - use_us);
        }
      }
    }));
  }

  for (auto& future : futures) {
    future.get();
  }
}

int main(int argc, char* argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  ::brpc::StartDummyServerAt(FLAGS_dummy_port);

  pending.expose("test_" + FLAGS_mode + "_pending");
  latency.expose("test_" + FLAGS_mode);

  if (FLAGS_mode == "babylon") {
    babylon_queue.initialize(1L << 18, ::babylon::BthreadExecutor::instance(),
                             babylon_queue_consume);
  } else if (FLAGS_mode == "bthread") {
    ::bthread::ExecutionQueueOptions options;
    ::bthread::execution_queue_start(&bthread_queue_id, &options,
                                     bthread_queue_consume, nullptr);
  } else {
    return 0;
  }

  run_loop();

  return 0;
}
