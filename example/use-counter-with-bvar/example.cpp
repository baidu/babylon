#include "brpc/server.h"
#include "gflags/gflags.h"
#include "recorder.h"

#include <random>
#include <thread>

DEFINE_int32(dummy_port, 8000, "TCP Port of this dummy server");
DEFINE_uint64(concurrency, 8, "Concurrent counting thread num");
DEFINE_uint64(loop, 1024, "Counting times in one time measure scope");
DEFINE_string(mode, "latency_recorder",
              "One of adder/maxer/int_recorder/latency_recorder");
DEFINE_bool(use_counter, false, "Use babylon counter implemented bvar");

template <typename T>
ABSL_ATTRIBUTE_NOINLINE void run_once(T& var, uint32_t value) {
  var << value;
}

template <typename S>
void run_loop(::std::string prefix) {
  S s;
  s.expose("test-" + prefix + "_var");
  ::bvar::LatencyRecorder latency {"test-" + prefix};

  ::std::vector<uint32_t> values;
  values.resize(FLAGS_loop);
  ::std::mt19937_64 gen {::std::random_device {}()};
  ::std::normal_distribution<> dis {600, 100};
  for (auto& value : values) {
    value = dis(gen);
  }

  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < FLAGS_concurrency; ++i) {
    threads.emplace_back([&] {
      while (!::brpc::IsAskedToQuit()) {
        auto begin = ::butil::cpuwide_time_ns();
        for (auto value : values) {
          run_once(s.var, value);
        }
        auto end = ::butil::cpuwide_time_ns();
        latency << (end - begin) * 1000.0 / values.size();
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

template <typename V>
void run(::std::string prefix) {
  struct S {
    void expose(::std::string name) {
      win.expose(name);
    }
    V var;
    ::bvar::Window<V, ::bvar::SERIES_IN_SECOND> win {&var, -1};
  };
  run_loop<S>(prefix);
}

template <>
void run<::bvar::LatencyRecorder>(::std::string prefix) {
  struct S {
    void expose(::std::string name) {
      var.expose(name);
    }
    ::bvar::LatencyRecorder var;
  };
  run_loop<S>(prefix);
}

template <>
void run<::babylon::BvarLatencyRecorder>(::std::string prefix) {
  struct S {
    void expose(::std::string name) {
      var.expose(name);
    }
    ::babylon::BvarLatencyRecorder var;
  };
  run_loop<S>(prefix);
}

int main(int argc, char* argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  ::brpc::StartDummyServerAt(FLAGS_dummy_port);

  if (FLAGS_mode == "adder") {
    if (FLAGS_use_counter) {
      run<::babylon::BvarAdder>("babylon");
    } else {
      run<::bvar::Adder<ssize_t>>("bvar");
    }
  } else if (FLAGS_mode == "maxer") {
    if (FLAGS_use_counter) {
      run<::babylon::BvarMaxer>("babylon");
    } else {
      run<::bvar::Maxer<ssize_t>>("bvar");
    }
  } else if (FLAGS_mode == "int_recorder") {
    if (FLAGS_use_counter) {
      run<::babylon::BvarIntRecorder>("babylon");
    } else {
      run<::bvar::IntRecorder>("bvar");
    }
  } else if (FLAGS_mode == "latency_recorder") {
    if (FLAGS_use_counter) {
      run<::babylon::BvarLatencyRecorder>("babylon");
    } else {
      run<::bvar::LatencyRecorder>("bvar");
    }
  }

  return 0;
}
