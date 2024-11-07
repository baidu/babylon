#include "brpc/server.h"

#include "butil/logging.h"
#include "echo.pb.h"
#include "gflags/gflags.h"

#if WITH_BABYLON
#include "swiss_message_factory.h"
#endif

#include "tcmalloc/malloc_extension.h"

#include <thread>

DEFINE_int32(port, 8000, "TCP Port of this server");
DEFINE_int32(mode, 0,
             "0: Use DefaultRpcPBMessageFactory\n"
             "1: Use ArenaRpcPBMessageFactory\n"
             "2: Use SwissRpcPBMessageFactory\n");

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
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  ::brpc::Server server;
  ::example::EchoServiceImpl echo_service_impl;
  if (server.AddService(&echo_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) !=
      0) {
    LOG(ERROR) << "Fail to add service";
    return -1;
  }

  ::butil::EndPoint point = ::butil::EndPoint(butil::IP_ANY, FLAGS_port);
  ::brpc::ServerOptions options;
  if (FLAGS_mode == 0) {
    LOG(INFO) << "Use DefaultRpcPBMessageFactory";
  } else if (FLAGS_mode == 1) {
    LOG(INFO) << "Use ArenaRpcPBMessageFactory";
    options.rpc_pb_message_factory = ::brpc::GetArenaRpcPBMessageFactory();
#if WITH_BABYLON
  } else if (FLAGS_mode == 2) {
    LOG(INFO) << "Use SwissRpcPBMessageFactory";
    auto factory = new SwissRpcPBMessageFactory;
    factory->set_page_size(4096);
    factory->set_free_page_capacity(2048);
    factory->set_free_message_capacity(128);
    options.rpc_pb_message_factory = factory;
#endif
  } else {
    LOG(ERROR) << "Invalid mode " << FLAGS_mode;
    return -1;
  }

  if (server.Start(point, &options) != 0) {
    LOG(ERROR) << "Fail to start EchoServer";
    return -1;
  }

  ::std::thread([] {
    ::tcmalloc::MallocExtension::SetBackgroundReleaseRate(
        ::tcmalloc::MallocExtension::BytesPerSecond(16 << 10));
    ::tcmalloc::MallocExtension::ProcessBackgroundActions();
  }).detach();
  server.RunUntilAskedToQuit();
  return 0;
}
