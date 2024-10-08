#include "brpc/channel.h"
#include "gflags/gflags.h"
#include "search.pb.h"

DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");

int main(int argc, char* argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  ::brpc::Channel channel;
  ::brpc::ChannelOptions options;
  options.protocol = "baidu_std";
  options.timeout_ms = 1000;
  if (channel.Init(FLAGS_server.c_str(), "", &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return -1;
  }

  ::SearchService_Stub stub(&channel);
  while (!::brpc::IsAskedToQuit()) {
    ::brpc::Controller controller;
    ::SearchRequest request;
    ::SearchResponse response;
    stub.search(&controller, &request, &response, nullptr);
    LOG(INFO) << "Receive response " << response.ShortDebugString();
    ::usleep(1000000);
  }
  return 0;
}
