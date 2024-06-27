#include "brpc/server.h"

#include "butil/logging.h"
#include "echo.pb.h"
#include "gflags/gflags.h"
#include "reusable_rpc_protocol.h"

DEFINE_int32(port, 8000, "TCP Port of this server");
DEFINE_bool(use_arena, false, "use arena allocate request and response");

namespace example {
class EchoServiceImpl : public EchoService {
 public:
  EchoServiceImpl() {}
  virtual ~EchoServiceImpl() {}
  virtual void Echo(google::protobuf::RpcController* cntl_base,
                    const EchoRequest* request, EchoResponse* response,
                    google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    response->mutable_payload()->CopyFrom(request->payload());
    LOG_EVERY_SECOND(INFO) << "Request SpaceUsedLong = "
                           << request->SpaceUsedLong()
                           << " Response SpaceUsedLong = "
                           << response->SpaceUsedLong();
  }
};
} // namespace example

int main(int argc, char* argv[]) {
  // Parse gflags. We recommend you to use gflags as well.
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (0 != ::babylon::ReusableRPCProtocol::register_protocol()) {
    LOG(ERROR) << "register ReusableRPCProtocol failed";
    return -1;
  }

  // Generally you only need one Server.
  brpc::Server server;

  // Instance of your service.
  example::EchoServiceImpl echo_service_impl;

  // Add the service into server. Notice the second parameter, because the
  // service is put on stack, we don't want server to delete it, otherwise
  // use brpc::SERVER_OWNS_SERVICE.
  if (server.AddService(&echo_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) !=
      0) {
    LOG(ERROR) << "Fail to add service";
    return -1;
  }

  butil::EndPoint point = butil::EndPoint(butil::IP_ANY, FLAGS_port);

  // Start the server.
  brpc::ServerOptions options;
  if (FLAGS_use_arena) {
    options.enabled_protocols = "baidu_std_reuse";
  }
  if (server.Start(point, &options) != 0) {
    LOG(ERROR) << "Fail to start EchoServer";
    return -1;
  }

  // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
  server.RunUntilAskedToQuit();
  return 0;
}
