// #include "babylon/coroutine/yield_awaitable.h"
#include "babylon/coroutine/yield_awaitable.h"
#include "babylon/executor.h"
#include "babylon/io/network_service.h"
#include "babylon/logging/logger.h"
#include "babylon/move_only_function.h"

#include "brpc/policy/baidu_rpc_meta.pb.h"
#pragma push_macro("BLOCK_SIZE")
#undef BLOCK_SIZE
#include "brpc/server.h"
#pragma pop_macro("BLOCK_SIZE")

#include "absl/base/internal/endian.h"
#include "absl/strings/cord.h"
#include "echo.pb.h"
#include "gtest/gtest.h"
#include "liburing.h"
#include "tcmalloc/malloc_extension.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/eventfd.h>

::std::string FEATURE_NAME_FOR_OFFSET[] = {
    [__builtin_ctz(IORING_FEAT_SINGLE_MMAP)] = "IORING_FEAT_SINGLE_MMAP",
    [__builtin_ctz(IORING_FEAT_NODROP)] = "IORING_FEAT_NODROP",
    [__builtin_ctz(IORING_FEAT_SUBMIT_STABLE)] = "IORING_FEAT_SUBMIT_STABLE",
    [__builtin_ctz(IORING_FEAT_RW_CUR_POS)] = "IORING_FEAT_RW_CUR_POS",
    [__builtin_ctz(IORING_FEAT_CUR_PERSONALITY)] =
        "IORING_FEAT_CUR_PERSONALITY",
    [__builtin_ctz(IORING_FEAT_FAST_POLL)] = "IORING_FEAT_FAST_POLL",
    [__builtin_ctz(IORING_FEAT_POLL_32BITS)] = "IORING_FEAT_POLL_32BITS",
    [__builtin_ctz(IORING_FEAT_SQPOLL_NONFIXED)] =
        "IORING_FEAT_SQPOLL_NONFIXED",
    [__builtin_ctz(IORING_FEAT_EXT_ARG)] = "IORING_FEAT_EXT_ARG",
    [__builtin_ctz(IORING_FEAT_NATIVE_WORKERS)] = "IORING_FEAT_NATIVE_WORKERS",
    [__builtin_ctz(IORING_FEAT_RSRC_TAGS)] = "IORING_FEAT_RSRC_TAGS",
    [__builtin_ctz(IORING_FEAT_CQE_SKIP)] = "IORING_FEAT_CQE_SKIP",
    [__builtin_ctz(IORING_FEAT_LINKED_FILE)] = "IORING_FEAT_LINKED_FILE",
    [__builtin_ctz(IORING_FEAT_REG_REG_RING)] = "IORING_FEAT_REG_REG_RING",
    [__builtin_ctz(IORING_FEAT_RECVSEND_BUNDLE)] =
        "IORING_FEAT_RECVSEND_BUNDLE",
};

::std::string OP_NAME_FOR_OFFSET[] = {
    [IORING_OP_NOP] = "IORING_OP_NOP",
    [IORING_OP_READV] = "IORING_OP_READV",
    [IORING_OP_WRITEV] = "IORING_OP_WRITEV",
    [IORING_OP_FSYNC] = "IORING_OP_FSYNC",
    [IORING_OP_READ_FIXED] = "IORING_OP_READ_FIXED",
    [IORING_OP_WRITE_FIXED] = "IORING_OP_WRITE_FIXED",
    [IORING_OP_POLL_ADD] = "IORING_OP_POLL_ADD",
    [IORING_OP_POLL_REMOVE] = "IORING_OP_POLL_REMOVE",
    [IORING_OP_SYNC_FILE_RANGE] = "IORING_OP_SYNC_FILE_RANGE",
    [IORING_OP_SENDMSG] = "IORING_OP_SENDMSG",
    [IORING_OP_RECVMSG] = "IORING_OP_RECVMSG",
    [IORING_OP_TIMEOUT] = "IORING_OP_TIMEOUT",
    [IORING_OP_TIMEOUT_REMOVE] = "IORING_OP_TIMEOUT_REMOVE",
    [IORING_OP_ACCEPT] = "IORING_OP_ACCEPT",
    [IORING_OP_ASYNC_CANCEL] = "IORING_OP_ASYNC_CANCEL",
    [IORING_OP_LINK_TIMEOUT] = "IORING_OP_LINK_TIMEOUT",
    [IORING_OP_CONNECT] = "IORING_OP_CONNECT",
    [IORING_OP_FALLOCATE] = "IORING_OP_FALLOCATE",
    [IORING_OP_OPENAT] = "IORING_OP_OPENAT",
    [IORING_OP_CLOSE] = "IORING_OP_CLOSE",
    [IORING_OP_FILES_UPDATE] = "IORING_OP_FILES_UPDATE",
    [IORING_OP_STATX] = "IORING_OP_STATX",
    [IORING_OP_READ] = "IORING_OP_READ",
    [IORING_OP_WRITE] = "IORING_OP_WRITE",
    [IORING_OP_FADVISE] = "IORING_OP_FADVISE",
    [IORING_OP_MADVISE] = "IORING_OP_MADVISE",
    [IORING_OP_SEND] = "IORING_OP_SEND",
    [IORING_OP_RECV] = "IORING_OP_RECV",
    [IORING_OP_OPENAT2] = "IORING_OP_OPENAT2",
    [IORING_OP_EPOLL_CTL] = "IORING_OP_EPOLL_CTL",
    [IORING_OP_SPLICE] = "IORING_OP_SPLICE",
    [IORING_OP_PROVIDE_BUFFERS] = "IORING_OP_PROVIDE_BUFFERS",
    [IORING_OP_REMOVE_BUFFERS] = "IORING_OP_REMOVE_BUFFERS",
    [IORING_OP_TEE] = "IORING_OP_TEE",
    [IORING_OP_SHUTDOWN] = "IORING_OP_SHUTDOWN",
    [IORING_OP_RENAMEAT] = "IORING_OP_RENAMEAT",
    [IORING_OP_UNLINKAT] = "IORING_OP_UNLINKAT",
    [IORING_OP_MKDIRAT] = "IORING_OP_MKDIRAT",
    [IORING_OP_SYMLINKAT] = "IORING_OP_SYMLINKAT",
    [IORING_OP_LINKAT] = "IORING_OP_LINKAT",
    [IORING_OP_MSG_RING] = "IORING_OP_MSG_RING",
    [IORING_OP_FSETXATTR] = "IORING_OP_FSETXATTR",
    [IORING_OP_SETXATTR] = "IORING_OP_SETXATTR",
    [IORING_OP_FGETXATTR] = "IORING_OP_FGETXATTR",
    [IORING_OP_GETXATTR] = "IORING_OP_GETXATTR",
    [IORING_OP_SOCKET] = "IORING_OP_SOCKET",
    [IORING_OP_URING_CMD] = "IORING_OP_URING_CMD",
    [IORING_OP_SEND_ZC] = "IORING_OP_SEND_ZC",
    [IORING_OP_SENDMSG_ZC] = "IORING_OP_SENDMSG_ZC",
    [IORING_OP_READ_MULTISHOT] = "IORING_OP_READ_MULTISHOT",
    [IORING_OP_WAITID] = "IORING_OP_WAITID",
    [IORING_OP_FUTEX_WAIT] = "IORING_OP_FUTEX_WAIT",
    [IORING_OP_FUTEX_WAKE] = "IORING_OP_FUTEX_WAKE",
    [IORING_OP_FUTEX_WAITV] = "IORING_OP_FUTEX_WAITV",
    [IORING_OP_FIXED_FD_INSTALL] = "IORING_OP_FIXED_FD_INSTALL",
    [IORING_OP_FTRUNCATE] = "IORING_OP_FTRUNCATE",
    [IORING_OP_BIND] = "IORING_OP_BIND",
    [IORING_OP_LISTEN] = "IORING_OP_LISTEN",
};

struct IOTest : public ::testing::Test {
  virtual void SetUp() override {
    ::babylon::LoggerBuilder builder;
    builder.set_min_severity(::babylon::LogSeverity::INFO);
    // builder.set_min_severity(::babylon::LogSeverity::DEBUG);
    ::babylon::LoggerManager::instance().set_root_builder(::std::move(builder));
    ::babylon::LoggerManager::instance().apply();

    do_probe();
    executor.set_worker_number(6);
    executor.set_global_capacity(1024);
    executor.set_local_capacity(4096);
    // executor.set_enable_work_stealing(true);
    // executor.set_balance_interval(::std::chrono::milliseconds {1});
    executor.set_balance_interval(::std::chrono::microseconds {200});
    executor.start();
    new_delete_page_allocator.set_page_size(1024);
    cached_page_allocator.set_upstream(new_delete_page_allocator);
    cached_page_allocator.set_free_page_capacity(1024);
    buffer_allocator.set_upstream(cached_page_allocator);
  }

  virtual void TearDown() override {}

  void do_probe() {
    struct ::io_uring tmp_ring;
    auto ret = ::io_uring_queue_init(1, &tmp_ring, 0);
    if (ret != 0) {
      ::std::cerr << "io_uring_queue_init for probe failed" << ::std::endl;
      return;
    }
    ::std::cerr << "io_uring features " << (void*)(uint64_t)tmp_ring.features
                << " {" << ::std::endl;
    auto feature_name_number =
        sizeof(FEATURE_NAME_FOR_OFFSET) / sizeof(FEATURE_NAME_FOR_OFFSET[0]);
    for (size_t i = 0; tmp_ring.features >> i || i < feature_name_number; i++) {
      ::std::cerr << "  [" << i << "]: ";
      if (i < feature_name_number) {
        ::std::cerr << FEATURE_NAME_FOR_OFFSET[i] << ": ";
      }
      ::std::cerr << (((tmp_ring.features >> i) & 0x1) ? 1 : 0) << ::std::endl;
    }
    ::std::cerr << "}" << ::std::endl;
    auto probe = io_uring_get_probe();
    ::std::cerr << "io_uring_probe ops " << (size_t)probe->ops_len << " {"
                << ::std::endl;
    auto op_name_number =
        sizeof(OP_NAME_FOR_OFFSET) / sizeof(OP_NAME_FOR_OFFSET[0]);
    for (size_t i = 0; i < probe->ops_len || i < op_name_number; ++i) {
      auto probe_op = &probe->ops[i];
      ::std::cerr << "  [" << i << "]: ";
      if (i < op_name_number) {
        ::std::cerr << OP_NAME_FOR_OFFSET[i] << ": ";
      }
      ::std::cerr << (i < probe->ops_len ? probe_op->flags : 0) << ::std::endl;
    }
    ::std::cerr << "}" << ::std::endl;
    ::io_uring_free_probe(probe);
    ::io_uring_queue_exit(&tmp_ring);
  }

  void check(const ::std::string& prefix, int ret) {
    ::std::cerr << prefix << " ret " << ret << " : "
                << ::strerror(-::std::min(0, ret)) << ::std::endl;
    if (ret < 0) {
      ::abort();
    }
  }

  ::babylon::ThreadPoolExecutor executor;
  ::babylon::NewDeletePageAllocator new_delete_page_allocator;
  ::babylon::CachedPageAllocator cached_page_allocator;
  //::babylon::PageAllocator& buffer_allocator = new_delete_page_allocator;
  ::babylon::CountingPageAllocator buffer_allocator;
};

int ParseRpcMessage(::absl::Cord& source, ::absl::Cord& meta_data,
                    ::absl::Cord& message_data) {
  if (source.size() < 12) {
    return 1;
  }

  auto header = source.Subcord(0, 12);
  auto header_view = header.Flatten();
  // BABYLON_LOG(INFO) << "header_view " << header_view.size();

  if (!header_view.starts_with("PRPC")) {
    return -1;
  }

  auto body_size = ::absl::big_endian::ToHost(
      *reinterpret_cast<const uint32_t*>(header_view.data() + 4));
  auto meta_size = ::absl::big_endian::ToHost(
      *reinterpret_cast<const uint32_t*>(header_view.data() + 8));
  // BABYLON_LOG(INFO) << "body_size " << body_size << " meta_size " <<
  // meta_size;

  if (source.size() < 12 + body_size) {
    return 1;
  }

  meta_data = source.Subcord(12, meta_size);
  message_data = source.Subcord(12 + meta_size, body_size - meta_size);
  source.RemovePrefix(12 + body_size);
  return 0;
}

namespace example {
class EchoServiceImpl : public EchoService {
 public:
  EchoServiceImpl() {}
  virtual ~EchoServiceImpl() {}
  virtual void Echo(::google::protobuf::RpcController* cntl,
                    const EchoRequest* request, EchoResponse* response,
                    google::protobuf::Closure* done) {
    response->set_message(request->message());
    done->Run();
  }
};
} // namespace example

::absl::flat_hash_map<::std::string, ::brpc::Server::MethodProperty> method_map;

::babylon::io::EntryBuffer* this_buffer;
::std::vector<::babylon::io::Entry> this_buffers;

void SendRpcResponse(::babylon::io::NetworkIOService::SocketId socket_id,
                     const ::google::protobuf::MethodDescriptor* method,
                     int64_t correlation_id,
                     ::google::protobuf::RpcController* cntl,
                     ::google::protobuf::Message* request,
                     ::google::protobuf::Message* response) {
  // BABYLON_LOG(INFO) << "SendRpcResponse " << correlation_id;
  ::std::unique_ptr<::google::protobuf::Message> request_guard {request};
  ::std::unique_ptr<::google::protobuf::Message> response_guard {response};
  ::std::unique_ptr<::google::protobuf::RpcController> cntl_guard {cntl};

  ::brpc::policy::RpcMeta meta;
  auto response_meta = meta.mutable_response();
  response_meta->set_error_code(0);
  meta.set_correlation_id(correlation_id);

  uint32_t meta_size = meta.ByteSizeLong();
  uint32_t body_size = meta_size + response->ByteSizeLong();
  // BABYLON_LOG(INFO) << "meta_size " << meta_size;
  // BABYLON_LOG(INFO) << "body_size " << body_size;
  meta_size = ::absl::big_endian::FromHost(meta_size);
  body_size = ::absl::big_endian::FromHost(body_size);

  auto& network_service = ::babylon::io::NetworkIOService::instance();
  //::babylon::io::EntryBuffer& buffer = *this_buffer;
  ::babylon::io::EntryBuffer buffer;
  buffer.set_page_allocator(network_service.send_buffer_allocator());
  buffer.begin();
  buffer.sputn("PRPC", 4);
  buffer.sputn(reinterpret_cast<char*>(&body_size), 4);
  buffer.sputn(reinterpret_cast<char*>(&meta_size), 4);
  {
    ::std::ostream os {&buffer};
    ::google::protobuf::io::OstreamOutputStream oos {&os};
    ::google::protobuf::io::CodedOutputStream cos {&oos};
    meta.SerializeWithCachedSizes(&cos);
    response->SerializeWithCachedSizes(&cos);
  }
  ////service.submit_send_to_io_thread(socket_id, buffer.end());
  network_service.send(socket_id, buffer.end());
  // BABYLON_LOG(INFO) << "SendRpcResponse " << meta.DebugString();
  // this_buffers.push_back(buffer.end());
}

::babylon::CoroutineTask<> ProcessRpcRequest(
    ::babylon::io::NetworkIOService::SocketId socket_id, ::absl::Cord meta_data,
    ::absl::Cord message_data) {
  // BABYLON_LOG(INFO) << "ProcessRpcRequest " << socket_id << " meta " <<
  // meta_data.size() << " message " << message_data.size();

  ::brpc::policy::RpcMeta meta;
  if (ABSL_PREDICT_FALSE(!meta.ParseFromCord(meta_data))) {
    BABYLON_LOG(WARNING) << "ProcessRpcRequest parse meta failed";
    ::abort();
  }

  auto& request_meta = meta.request();
  // BABYLON_LOG(WARNING) << "ProcessRpcRequest meta " << meta.DebugString();
  ::std::string full_name;
  full_name.reserve(request_meta.service_name().size() +
                    request_meta.method_name().size() + 1);
  full_name.assign(request_meta.service_name());
  full_name.push_back('.');
  full_name.append(request_meta.method_name());
  auto iter = method_map.find(full_name);
  if (iter == method_map.end()) {
    BABYLON_LOG(WARNING) << "ProcessRpcRequest find method failed";
    co_return;
  }

  auto& mp = iter->second;
  auto request = mp.service->GetRequestPrototype(mp.method).New();
  auto response = mp.service->GetResponsePrototype(mp.method).New();

  if (ABSL_PREDICT_FALSE(!request->ParseFromCord(message_data))) {
    BABYLON_LOG(WARNING) << "ProcessRpcRequest parse request failed";
    ::abort();
  }

  auto cntl = ::std::make_unique<::brpc::Controller>();
  auto done = ::brpc::NewCallback<::babylon::io::NetworkIOService::SocketId,
                                  const ::google::protobuf::MethodDescriptor*,
                                  int64_t, ::google::protobuf::RpcController*,
                                  ::google::protobuf::Message*,
                                  ::google::protobuf::Message*>(
      &SendRpcResponse, socket_id, mp.method, meta.correlation_id(), cntl.get(),
      request, response);
  mp.service->CallMethod(mp.method, cntl.release(), request, response, done);
  co_return;
}

TEST_F(IOTest, tttt) {
  ::example::EchoServiceImpl echo_service;
  {
    ::brpc::Server::MethodProperty mp;
    mp.service = &echo_service;
    mp.method = echo_service.GetDescriptor()->FindMethodByName("Echo");
    method_map[mp.method->full_name()] = mp;
  }

  ::babylon::ConcurrentSummer send_summer;
  int ret = 0;
  auto& service = ::babylon::io::NetworkIOService::instance();
  service.set_executor(executor);
  service.set_page_allocator(buffer_allocator);
  service.set_on_accept(
      [](::babylon::io::NetworkIOService::SocketId socket_id) {
        int flag = 1;
        ::setsockopt(socket_id.fd, IPPROTO_TCP, TCP_NODELAY,
                     reinterpret_cast<char*>(&flag), sizeof(flag));
        BABYLON_LOG(INFO) << "accept socket " << socket_id
                          << " set TCP_NODELAY";
      });
  service.set_on_receive(
      [&](::babylon::io::NetworkIOService::SocketId socket_id,
          ::absl::Cord& input_data,
          bool finished) -> ::babylon::CoroutineTask<> {
        size_t x = 0;
        while (true) {
          ::absl::Cord meta_data;
          ::absl::Cord message_data;
          auto parse_result =
              ParseRpcMessage(input_data, meta_data, message_data);
          if (parse_result == -1) {
            BABYLON_LOG(WARNING) << "ParseRpcMessage failed close connection";
            service.submit_shutdown_and_close_to_io_thread(socket_id);
            break;
          }

          if (parse_result == 1) {
            break;
          }

          executor.submit(ProcessRpcRequest, socket_id, ::std::move(meta_data),
                          ::std::move(message_data));
          x++;
        }
        if (x > 0) {
          send_summer << x;
        }
        // if (finished) {
        //   service.submit_shutdown_and_close_to_io_thread(socket_id);
        // }

        co_return;
      });
  // service.set_on_error(
  //     [&](::babylon::io::NetworkIOService::SocketId socket_id,
  //     ::babylon::io::Error) {
  //       BABYLON_LOG(INFO) << "on_error submit close on socket " << socket_id;
  //       service.submit_shutdown_and_close_to_io_thread(socket_id);
  //     });
  ASSERT_EQ(0, service.start());

  auto listen_socket = ::socket(PF_INET, SOCK_STREAM, 0);
  ASSERT_LE(0, listen_socket);
  int enable = 1;
  ret = ::setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enable,
                     sizeof(enable));
  ASSERT_LE(0, ret);

  struct ::sockaddr_in listen_addr;
  ::memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = ::htons(8080);
  listen_addr.sin_addr.s_addr = ::htonl(INADDR_ANY);

  ret = ::bind(listen_socket, (struct ::sockaddr*)&listen_addr,
               sizeof(listen_addr));
  ASSERT_LE(0, ret);
  ret = ::listen(listen_socket, 10);
  ASSERT_LE(0, ret);

  ::std::cerr << "submit listen " << listen_socket << ::std::endl;
  service.accept(listen_socket);

  ::std::thread([&] {
    ::babylon::ConcurrentSummer::Summary last_summary;
    ::babylon::ConcurrentSummer::Summary last_merge_summary;
    ::babylon::ConcurrentSummer::Summary last_send_summary;
    while (true) {
      BABYLON_LOG(INFO) << "send buffer "
                        << ::babylon::io::NetworkIOService::send_buffer_num;
      BABYLON_LOG(INFO) << "allocate " << buffer_allocator.allocated_page_num();

      auto summary = cached_page_allocator.cache_hit_summary();
      auto num = summary.num - last_summary.num;
      auto sum = summary.sum - last_summary.sum;
      last_summary = summary;
      BABYLON_LOG(INFO) << "ratio " << sum << " / " << num << " = "
                        << (sum * 1.0 / (num == 0 ? 1 : num));

      summary = service.merge_summer.value();
      num = summary.num - last_merge_summary.num;
      sum = summary.sum - last_merge_summary.sum;
      last_merge_summary = summary;
      BABYLON_LOG(INFO) << "merge " << sum << " / " << num << " = "
                        << (sum * 1.0 / (num == 0 ? 1 : num));

      summary = send_summer.value();
      num = summary.num - last_send_summary.num;
      sum = summary.sum - last_send_summary.sum;
      last_send_summary = summary;
      BABYLON_LOG(INFO) << "send " << sum << " / " << num << " = "
                        << (sum * 1.0 / (num == 0 ? 1 : num));

      usleep(1000000);
    }
  }).detach();
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  usleep(300 * 1000000);
  // auto future = service.execute_keep_accept_and_recv(listen_socket);
  // ASSERT_EQ(0, future.get());

  /*
  {
    auto sqe = ::io_uring_get_sqe(&input_ring);
    ::io_uring_prep_multishot_accept(sqe,
               listen_socket, nullptr, nullptr, 0);
    ::io_uring_sqe_set_data64(sqe, UserData {.mode = 1, .fd = -1}.value);
    ret = ::io_uring_submit(&input_ring);
    ASSERT_LE(0, ret);
  }

  struct ::io_uring_cqe saved_cqe;
  while (true) {
    struct ::io_uring_cqe* cqe;
    ret = ::io_uring_wait_cqe(&input_ring, &cqe);
    check("io_uring_wait_cqe", ret);

    ::std::cerr << "io_uring_cqe " << cqe << " {" << ::std::endl;
    ::std::cerr << "  user_data: " << (int)(UserData {.value =
  cqe->user_data}.mode) << " " << UserData {.value = cqe->user_data}.fd <<
  ::std::endl;
    ::std::cerr << "  res: " << cqe->res << ::std::endl;
    ::std::cerr << "  flags: " << cqe->flags << ::std::endl;
    ::std::cerr << "}" << ::std::endl;
    check("io_uring_wait_cqe cqe->res", cqe->res);

    UserData user_data {.value = cqe->user_data};
    saved_cqe.user_data = cqe->user_data;
    saved_cqe.res = cqe->res;
    saved_cqe.flags = cqe->flags;
    ::io_uring_cqe_seen(&input_ring, cqe);

    if (user_data.mode == 1) {
      check("accept_multishot", saved_cqe.res);
      ::std::cerr << "accept once get socket " << saved_cqe.res << ::std::endl;
      auto sqe = ::io_uring_get_sqe(&input_ring);
      ::io_uring_prep_recv_multishot(sqe,
              saved_cqe.res, nullptr, 0, 0);
      sqe->buf_group = 0;
      sqe->flags |= IOSQE_BUFFER_SELECT;
      ::io_uring_sqe_set_data64(sqe, UserData {.mode = 2, .fd =
  saved_cqe.res}.value); ret = ::io_uring_submit(&input_ring); ASSERT_LE(0,
  ret); } else if (user_data.mode == 2) { check("recv_multishot",
  saved_cqe.res);

      if (saved_cqe.res == 0) {
        auto sqe = ::io_uring_get_sqe(&input_ring);
        ::io_uring_prep_shutdown(sqe, user_data.fd, SHUT_RDWR);
        ::io_uring_sqe_set_data64(sqe, UserData {.mode = 3, .fd =
  user_data.fd}.value); ret = ::io_uring_submit(&input_ring); ASSERT_LE(0, ret);
        continue;
      }

      if ((saved_cqe.flags & IORING_CQE_F_BUFFER)) {
        auto index = saved_cqe.flags >> IORING_CQE_BUFFER_SHIFT;
        ::std::cerr << "recv [" << ::std::string_view((char*)(buffer[index]),
  saved_cqe.res) << "]" << " size " << saved_cqe.res << ::std::endl;
        ::io_uring_buf_ring_add(input_buffer_ring,
               buffer[index], buffer.page_size,
               index, ::io_uring_buf_ring_mask(8), 0);
        ::io_uring_buf_ring_advance(input_buffer_ring, 1);
      }
    } else if (user_data.mode == 3) {
      check("shutdown", saved_cqe.res);
      close(user_data.fd);
    } else {
      ASSERT_TRUE(false);
    }
  }
  */
}

int main() {
  ::testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
