#include "brpc/server.h"
#include "gflags/gflags.h"
#include "recorder.h"

#include <random>
#include <thread>

DEFINE_int32(dummy_port, 8000, "TCP Port of this dummy server");
DEFINE_uint64(concurrency, 4, "Concurrent counting thread num");
DEFINE_uint64(vars, 10, "Counting bvar num");
DEFINE_bool(use_counter, false, "use babylon counter implemented bvar");
DEFINE_string(mode, "latency_recorder",
              "adder/maxer/int_recorder/latency_recorder");

template <typename T>
void __attribute__((noinline)) work(T& var, uint32_t value) {
  var << value;
}

template <typename S>
void run_loop(::std::string prefix) {
  ::std::vector<::std::unique_ptr<S>> vec;
  for (size_t i = 0; i < FLAGS_vars; ++i) {
    auto s = ::std::make_unique<S>();
    s->expose("test-" + prefix + "-" + ::std::to_string(i));
    vec.emplace_back(::std::move(s));
  }

  ::bvar::LatencyRecorder latency("test-" + prefix);
  ::std::vector<::std::thread> threads;
  for (size_t i = 0; i < FLAGS_concurrency; ++i) {
    threads.emplace_back([&] {
      ::std::mt19937_64 gen {::std::random_device {}()};
      ::std::normal_distribution<> dis(600, 100);
      while (true) {
        auto begin = ::butil::cpuwide_time_ns();
        auto v = dis(gen);
        for (size_t i = 0; i < 1000; ++i) {
          for (auto& s : vec) {
            work(s->var, v);
          }
        }
        auto use = (::butil::cpuwide_time_ns() - begin) / 1000 / vec.size();
        latency << use;
      }
    });
  }
  usleep(1000000000);
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

  /*
  ::babylon::BvarAdder adder;
  adder.expose("xxxx_adder");

  ::bvar::Window<::babylon::BvarAdder, ::bvar::SERIES_IN_SECOND> adder_window {
      &adder, -1};
  adder_window.expose("xxxx_adder_win");

  ::babylon::BvarMaxer maxer;
  maxer.expose("xxxx_maxer");

  ::bvar::Window<::babylon::BvarMaxer, ::bvar::SERIES_IN_SECOND> maxer_window {
      &maxer, -1};
  maxer_window.expose("xxxx_maxer_win");

  ::babylon::BvarIntRecorder int_recorder;
  int_recorder.expose("xxxx_int_recorder");

  ::bvar::Window<::babylon::BvarIntRecorder, ::bvar::SERIES_IN_SECOND>
      int_recorder_window {&int_recorder, -1};
  int_recorder_window.expose("xxxx_int_recorder_win");

  ::babylon::BvarPercentile percentile;
  ::bvar::Window<::babylon::BvarPercentile, ::bvar::SERIES_IN_SECOND>
      percentile_window {&percentile, -1};
  percentile_window.expose("xxxx_percentile_win");
  */

  return 0;
}
