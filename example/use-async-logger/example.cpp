#include "babylon/logging/async_log_stream.h"
#include "babylon/logging/rolling_file_object.h"

#include "brpc/server.h"
#include "gflags/gflags.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include <filesystem>
#include <thread>

DEFINE_int32(dummy_port, 8000, "TCP Port of this dummy server");
DEFINE_uint64(concurrency, 7, "Concurrent logging thread num");
DEFINE_string(mode, "babylon", "");
DEFINE_uint64(qps, 10000, "");
DEFINE_uint64(batch, 1, "");
DEFINE_bool(benchmark, true, "");
DEFINE_uint64(payload, 50, "");

namespace brpc {
bool IsAskedToQuit();
}

void setup_babylon() {
  static ::babylon::RollingFileObject rolling_object;
  static ::babylon::NewDeletePageAllocator new_delete_allocator;
  static ::babylon::CachedPageAllocator cached_allocator;
  static ::babylon::BatchPageAllocator batch_allocator;
  static ::babylon::AsyncFileAppender appender;

  new_delete_allocator.set_page_size(256);
  cached_allocator.set_upstream(new_delete_allocator);
  cached_allocator.set_free_page_capacity(262144);
  batch_allocator.set_upstream(cached_allocator);
  batch_allocator.set_batch_size(64);

  appender.set_page_allocator(batch_allocator);
  appender.set_queue_capacity(262144);
  appender.initialize();

  rolling_object.set_directory("log");
  rolling_object.set_file_pattern("name.%Y%m%d-%H%M%S");
  if (FLAGS_benchmark) {
    rolling_object.set_directory("/dev");
    rolling_object.set_file_pattern("null");
  }
  rolling_object.scan_and_tracking_existing_files();

  ::babylon::LoggerBuilder builder;
  builder.set_log_stream_creator(
      ::babylon::AsyncLogStream::creator(appender, rolling_object));
  ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
  ::babylon::LoggerManager::instance().apply();

  ::std::thread([&] {
    while (true) {
      rolling_object.delete_expire_files();
      ::usleep(1000 * 1000);
    }
  }).detach();

  struct S {
    static size_t get_pending_size(void*) {
      return appender.pending_size();
    }
    static size_t get_free_page_num(void*) {
      return cached_allocator.free_page_num();
    }
    static ::bvar::Stat get_hit_summary(void*) {
      auto summary = cached_allocator.cache_hit_summary();
      return ::bvar::Stat {summary.sum, static_cast<ssize_t>(summary.num)};
    }
  };
  static ::bvar::PassiveStatus<size_t> pending_size {
      "test-babylon-pending", S::get_pending_size, nullptr};
  static ::bvar::PassiveStatus<size_t> free_page_num {
      "test-babylon-free", S::get_free_page_num, nullptr};
  static ::bvar::PassiveStatus<::bvar::Stat> hit_summary {S::get_hit_summary,
                                                          nullptr};
  static ::bvar::Window<::bvar::PassiveStatus<::bvar::Stat>> hit_summary_win {
      "test-babylon-hit", &hit_summary, -1};
}

void setup_brpc() {
  ::std::filesystem::create_directory("log");
  ::logging::LoggingSettings settings;
  settings.logging_dest = ::logging::LOG_TO_FILE;
  settings.delete_old = ::logging::DELETE_OLD_LOG_FILE;
  settings.log_file = "log/name.log";
  if (FLAGS_benchmark) {
    settings.log_file = "/dev/null";
  }
  ::logging::InitLogging(settings);
}

void setup_spdlog() {
  ::spdlog::set_pattern("%l %Y-%m-%d %H:%M:%S.%f %t %s:%#] %v");
  ::spdlog::init_thread_pool(262144, 1);
  auto async_file = ::spdlog::basic_logger_mt<::spdlog::async_factory>(
      "async_file_logger", FLAGS_benchmark ? "/dev/null" : "log/name.log");
  ::spdlog::set_default_logger(async_file);
}

::std::string payload;

void run_once_babylon(size_t round) {
  BABYLON_LOG(INFO) << "round " << round << " payload " << payload;
}

void run_once_brpc(size_t round) {
  LOG(INFO) << "round " << round << " payload " << payload;
}

void run_once_spdlog(size_t round) {
  SPDLOG_INFO("round {} payload {}", round, payload);
}

void run_loop() {
  ::bvar::LatencyRecorder latency {"test-" + FLAGS_mode};

  int64_t expect_us =
      1000.0 * 1000 / FLAGS_qps * FLAGS_batch * FLAGS_concurrency;

  void (*run_once)(size_t);
  if (FLAGS_mode == "babylon") {
    run_once = run_once_babylon;
  } else if (FLAGS_mode == "brpc") {
    run_once = run_once_brpc;
  } else if (FLAGS_mode == "spdlog") {
    run_once = run_once_spdlog;
  }
  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < FLAGS_concurrency; ++i) {
    threads.emplace_back([&] {
      size_t round = 0;
      while (!::brpc::IsAskedToQuit()) {
        auto round_begin = ::butil::cpuwide_time_ns();
        for (size_t j = 0; j < FLAGS_batch; ++j) {
          auto begin = ::butil::cpuwide_time_ns();
          run_once(round);
          auto end = ::butil::cpuwide_time_ns();
          latency << (end - begin);
        }
        auto round_end = ::butil::cpuwide_time_ns();
        auto use_us = (round_end - round_begin) / 1000;
        if (use_us < expect_us) {
          ::usleep(expect_us - use_us);
        }
        round++;
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

int main(int argc, char* argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  payload.resize(FLAGS_payload, 'x');

  ::brpc::StartDummyServerAt(FLAGS_dummy_port);

  ::std::filesystem::remove_all("log");
  if (FLAGS_mode == "babylon") {
    setup_babylon();
    run_loop();
  } else if (FLAGS_mode == "brpc") {
    setup_brpc();
    run_loop();
  } else if (FLAGS_mode == "spdlog") {
    setup_spdlog();
    run_loop();
  }

  return 0;
}
