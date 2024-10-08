#include "brpc/server.h"

#include "gflags/gflags.h"
#include "graph_configurator.h"
#include "search.pb.h"

DEFINE_int32(port, 8000, "TCP Port of this server");

class SearchServiceImpl : public SearchService {
 public:
  SearchServiceImpl(GraphConfigurator& configurator)
      : _configurator {configurator} {}

  virtual void search(::google::protobuf::RpcController* controller_base,
                      const SearchRequest* request, SearchResponse* response,
                      ::google::protobuf::Closure* done) {
    ::brpc::ClosureGuard done_guard(done);
    auto controller = static_cast<::brpc::Controller*>(controller_base);

    auto graph = _configurator.get_graph();
    // ref request and preset response, make it zero-copy
    graph->find_data("request")->emit<SearchRequest>().ref(*request);
    auto data = graph->find_data("response");
    data->preset(*response);

    auto ret = graph->run(data).get();
    if (ret != 0) {
      LOG(WARNING) << "Run graph failed";
      controller->SetFailed("Run graph failed");
    }
  }

  GraphConfigurator& _configurator;
};

int main(int argc, char* argv[]) {
  ::gflags::ParseCommandLineFlags(&argc, &argv, true);

  GraphConfigurator configurator;
  if (0 != configurator.load("dag.yaml")) {
    LOG(ERROR) << "initialize failed";
    return 1;
  }

  ::brpc::Server server;
  SearchServiceImpl search_service_impl {configurator};
  if (server.AddService(&search_service_impl,
                        ::brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    LOG(ERROR) << "fail to add service";
    return -1;
  }

  ::butil::EndPoint point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
  ::brpc::ServerOptions options;
  if (server.Start(point, &options) != 0) {
    LOG(ERROR) << "fail to start server";
    return -1;
  }

  server.RunUntilAskedToQuit();
  return 0;
}
