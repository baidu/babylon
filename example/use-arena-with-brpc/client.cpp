#include "brpc/channel.h"
#include "butil/logging.h"
#include "butil/time.h"
#include "echo.pb.h"
#include "gflags/gflags.h"

#include <random>

DEFINE_string(connection_type, "",
              "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_int32(timeout_ms, 500, "RPC timeout in milliseconds");
DEFINE_uint64(qps, 100, "");

DEFINE_uint64(payload_scale, 10, "");

void finish(::example::EchoResponse* response, ::brpc::Controller* controller) {
  if (!controller->Failed()) {
    LOG_EVERY_SECOND(INFO) << "Received response from "
                           << controller->remote_side() << " to "
                           << controller->local_side()
                           << ": size=" << response->ByteSizeLong()
                           << " latency=" << controller->latency_us() << "us";
  } else {
    LOG(WARNING) << controller->ErrorText();
  }
  delete response;
  delete controller;
}

::std::mt19937 gen {::std::random_device {}()};

void fill(::std::string* term) {
  size_t num = gen() % (2 * FLAGS_payload_scale);
  for (size_t i = 0; i < num; ++i) {
    term->push_back(gen());
  }
}

void fill(::example::Item* item) {
  size_t num = gen() % (10 * FLAGS_payload_scale);
  for (size_t i = 0; i < num; ++i) {
    fill(item->mutable_term()->Add());
  }
}

void fill(::example::Result* result) {
  size_t num = gen() % (1 * FLAGS_payload_scale);
  for (size_t i = 0; i < num; ++i) {
    fill(result->mutable_item()->Add());
    fill(result->mutable_term()->Add());
  }
}

void fill(::example::ComplexPayload* payload) {
  size_t num = gen() % (10 * FLAGS_payload_scale);
  auto results = payload->mutable_result();
  results->Reserve(num);
  for (size_t i = 0; i < num; ++i) {
    fill(results->Add());
  }
}

int main(int argc, char* argv[]) {
  // Parse gflags. We recommend you to use gflags as well.
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // A Channel represents a communication line to a Server. Notice that
  // Channel is thread-safe and can be shared by all threads in your program.
  brpc::Channel channel;

  // Initialize the channel, NULL means using default options.
  brpc::ChannelOptions options;
  options.protocol = "baidu_std";
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms /*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), "", &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return -1;
  }

  // Normally, you should not call a Channel directly, but instead construct
  // a stub Service wrapping it. stub can be shared by all threads as well.
  example::EchoService_Stub stub(&channel);

  ::example::EchoRequest request;
  int64_t expected_us = 1000000 / FLAGS_qps;
  while (!brpc::IsAskedToQuit()) {
    auto begin_us = ::butil::gettimeofday_us();
    request.Clear();
    fill(request.mutable_payload());
    auto response = new ::example::EchoResponse;
    auto controller = new brpc::Controller;
    stub.Echo(controller, &request, response,
              ::google::protobuf::NewCallback(finish, response, controller));
    auto use_us = ::butil::gettimeofday_us() - begin_us;

    if (use_us < expected_us) {
      usleep(expected_us - use_us);
    }
  }

  LOG(INFO) << "EchoClient is going to quit";
  return 0;
}
