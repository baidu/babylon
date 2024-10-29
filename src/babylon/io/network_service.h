// #include "babylon/coroutine/yield_awaitable.h"
#include "babylon/executor.h"
#include "babylon/io/entry.h"
#include "babylon/io/error.h"
#include "babylon/logging/logger.h"
// #include "babylon/move_only_function.h"

// clang-format off
#include BABYLON_EXTERNAL(absl/strings/cord.h)
// clang-format on

#include "liburing.h" // ::io_uring_*

#include <sys/eventfd.h> // ::eventfd

#include <future> // std::promise

BABYLON_IO_NAMESPACE_BEGIN

class NetworkIOService {
 private:
  class UserData;
  struct SQE;
  struct SocketData;
  struct ReceiveTask;

 public:
  struct SocketId;

  static NetworkIOService& instance() noexcept;

  void set_executor(Executor& executor) noexcept;
  void set_page_allocator(PageAllocator& page_allocator) noexcept;

  template <typename C>
    requires ::std::invocable<C&&, SocketId>
  void set_on_accept(C&& callback) noexcept;

  template <typename C>
    requires ::std::invocable<C&&, SocketId, ::absl::Cord&, bool>
  void set_on_receive(C&& callback) noexcept;

  template <typename C>
    requires ::std::invocable<C&&, SocketId, Error>
  void set_on_error(C&& callback) noexcept;

  int start() noexcept;

  void accept(int listen_socket) noexcept;

  inline PageAllocator& send_buffer_allocator() noexcept;
  void send(SocketId socket_id, Entry& entry) noexcept;

  int stop() noexcept;

  void add_to_input_queue(SocketId socket_id,
                          ::absl::Cord&& received_data) noexcept;
  void on_shutdown_then_close_in_io_thread(struct ::io_uring_cqe* cqe,
                                           ::std::vector<SQE>&) noexcept;
  void on_close_in_io_thread(struct ::io_uring_cqe* cqe,
                             ::std::vector<SQE>&) noexcept;

  void submit_shutdown_and_close_to_io_thread(
      NetworkIOService::SocketId socket_id) noexcept;

  struct ::io_uring& local_output_ring() noexcept;

  ::babylon::CoroutineTask<> consume_input_queue(
      SocketData& socket_data) noexcept;
  static ::std::atomic<ssize_t> send_buffer_num;
  ::babylon::ConcurrentSummer merge_summer;

 private:
  class UserData;
  class OutputTask;

  struct SQE;

  struct SendBufferFooter;
  class SendBufferAllocator;

  struct ReceiveTask;
  using ReceiveTaskIterator =
      ::babylon::ConcurrentBoundedQueue<ReceiveTask>::Iterator;

  struct SocketData;

  NetworkIOService() noexcept;

  ::babylon::CoroutineTask<> keep_accept_and_receive() noexcept;
  void do_input_submission(::std::vector<UserData>& submisions,
                           size_t& submited_number) noexcept;
  bool prepare_poll(UserData user_data) noexcept;
  bool prepare_accept(UserData user_data) noexcept;
  bool prepare_receive(UserData user_data) noexcept;
  void do_input_completion(::std::vector<struct ::io_uring_cqe*>& cqes,
                           ::std::vector<UserData>& submisions) noexcept;
  void on_poll(struct ::io_uring_cqe* cqe,
               ::std::vector<UserData>& submisions) noexcept;
  void on_accept(struct ::io_uring_cqe* cqe,
                 ::std::vector<UserData>& submisions) noexcept;
  void on_receive(struct ::io_uring_cqe* cqe,
                  ::std::vector<UserData>& submisions) noexcept;

  void keep_reclaim_send_buffer() noexcept;
  void reclaim_send_buffer(struct ::io_uring_cqe* cqe) noexcept;

  void signal_submission_events() noexcept;
  void fill_buffer(size_t buffer_index) noexcept;
  void consume_output_queue() noexcept;

  bool prepare_send_in_io_thread(
      SQE& sqe, ::std::vector<struct ::iovec>& remained_iov) noexcept;
  bool prepare_shutdown_then_close_in_io_thread(SQE& sqe) noexcept;
  bool prepare_close_in_io_thread(SQE& sqe) noexcept;

  ::babylon::Executor* _executor {&InplaceExecutor::instance()};
  ::babylon::PageAllocator* _page_allocator {&SystemPageAllocator::instance()};
  size_t _page_size {0};

 public:
  ::std::unique_ptr<SendBufferAllocator> _send_buffer_allocator;

 private:
  size_t _send_buffer_size {0};

  size_t _ring_capacity {4096};
  size_t _buffer_ring_capacity {256};

  static void default_on_accept(SocketId) noexcept;
  MoveOnlyFunction<void(SocketId)> _on_accept;

  MoveOnlyFunction<CoroutineTask<>(SocketId, ::absl::Cord&, bool)> _on_receive;

  static void default_on_error(SocketId, Error) noexcept;
  MoveOnlyFunction<void(SocketId, Error)> _on_error;

  struct ::io_uring _input_ring;
  struct ::io_uring_buf_ring* _buffer_ring {nullptr};
  ::std::vector<void*> _buffers;

  struct OptionalRing;
  ::babylon::EnumerableThreadLocal<OptionalRing> _output_rings;
  ::babylon::ConcurrentBoundedQueue<OutputTask> _output_queue {4096};
  ::std::atomic<size_t> _output_events {0};

  ::std::atomic<bool> _running {false};
  ::babylon::Future<void> _keep_accept_and_receive_finished;
  ::babylon::Future<void> _keep_reclaim_send_buffer_finished;

  ::babylon::ConcurrentBoundedQueue<UserData> _submission_queue {256};
  ::std::atomic<uint64_t> _submission_events {0};
  int _submission_eventfd {-1};

  ::babylon::ConcurrentVector<SocketData> _socket_data {128};

  void print_sqe(SQE& sqe);
  void print_cqe(struct ::io_uring_cqe* cqe);
};

::std::atomic<ssize_t> NetworkIOService::send_buffer_num {0};

struct NetworkIOService::OptionalRing {
  ::std::atomic<bool> has_value {false};
  struct ::io_uring value;
};

struct NetworkIOService::SocketId {
  union {
    uint64_t value;
    struct {
      int32_t fd;
      uint8_t version {0};
      uint8_t pad[0];
    };
  };

  SocketId() : fd {-1} {}

  SocketId(uint64_t value) : value {value} {}
  SocketId(int32_t fd) : fd {fd} {}
};

class NetworkIOService::UserData {
 public:
  static UserData from(struct ::io_uring_cqe* cqe) noexcept {
    return {::io_uring_cqe_get_data64(cqe)};
  }

  UserData() {
    _mode = UINT8_MAX;
  }

  UserData(uint64_t value) : value {value} {}

  UserData(uint8_t mode, SocketId socket_id)
      : payload {socket_id.value}, _mode {mode} {}

  UserData(uint8_t mode, const void* buffer)
      : payload {reinterpret_cast<uintptr_t>(buffer)}, _mode {mode} {}

  operator uint64_t() {
    return value;
  }

  inline uint8_t mode() const noexcept {
    return _mode;
  }

  void set_socket_id(SocketId socket_id) noexcept {
    payload = socket_id.value;
  }

  SocketId socket_id() const noexcept {
    return {payload};
  }

  void* buffer() const noexcept {
    return reinterpret_cast<void*>(payload);
  }

 private:
  union {
    uint64_t value;
    struct {
      uint64_t payload : 48;
      uint8_t padding[1];
      uint8_t _mode;
    };
  };
};

struct NetworkIOService::OutputTask {
  UserData user_data;
  Entry entry;
};

struct NetworkIOService::SQE {
  UserData user_data;
  ::babylon::io::Entry entry {};

  SocketId socket_id() const noexcept {
    return user_data.socket_id();
  }
};

struct NetworkIOService::SendBufferFooter {
  SocketId socket_id {};
};

class NetworkIOService::SendBufferAllocator : public ::babylon::PageAllocator {
 public:
  void set_upstream(::babylon::PageAllocator& upstream) noexcept {
    _upstream = &upstream;
  }

  SendBufferFooter& footer(void* page) noexcept {
    if (ABSL_PREDICT_FALSE(_page_size_cached == 0)) {
      _page_size_cached = page_size();
    }
    return *reinterpret_cast<SendBufferFooter*>(
        reinterpret_cast<uintptr_t>(page) + _page_size_cached);
  }

  inline size_t page_size_cached() const noexcept {
    return _page_size_cached;
  }

  virtual size_t page_size() const noexcept override {
    if (ABSL_PREDICT_FALSE(_page_size_cached == 0)) {
      _page_size_cached = _upstream->page_size() - sizeof(SendBufferFooter);
    }
    return _page_size_cached;
  }

  virtual void* allocate() noexcept override {
    send_buffer_num++;
    return _upstream->allocate();
  }
  virtual void allocate(void** pages, size_t num) noexcept override {
    send_buffer_num += num;
    return _upstream->allocate(pages, num);
  }
  virtual void deallocate(void* page) noexcept override {
    send_buffer_num--;
    return _upstream->deallocate(page);
  }
  virtual void deallocate(void** pages, size_t num) noexcept override {
    send_buffer_num -= num;
    return _upstream->deallocate(pages, num);
  }

 private:
  ::babylon::PageAllocator* _upstream {
      &::babylon::SystemPageAllocator::instance()};
  mutable size_t _page_size_cached {0};
};

struct NetworkIOService::ReceiveTask {
  SocketId socket_id;
  ::absl::Cord received_data;
};

struct NetworkIOService::SocketData {
  uint8_t version {0};

  ::babylon::ConcurrentBoundedQueue<ReceiveTask> input_queue {128};
  ::std::atomic<size_t> input_events {0};
  ReceiveTask receive_task;
};

template <typename C, typename T>
inline ::std::basic_ostream<C, T>& operator<<(
    ::std::basic_ostream<C, T>& os, NetworkIOService::SocketId socket_id) {
  return os << "SocketId[" << socket_id.fd << '@'
            << static_cast<int>(socket_id.version) << ']';
}

template <typename C>
  requires ::std::invocable<C&&, NetworkIOService::SocketId>
void NetworkIOService::set_on_accept(C&& callback) noexcept {
  _on_accept = ::std::forward<C>(callback);
}

template <typename C>
  requires ::std::invocable<C&&, NetworkIOService::SocketId, ::absl::Cord&,
                            bool>
void NetworkIOService::set_on_receive(C&& callback) noexcept {
  _on_receive = ::std::forward<C>(callback);
}

template <typename C>
  requires ::std::invocable<C&&, NetworkIOService::SocketId, Error>
void NetworkIOService::set_on_error(C&& callback) noexcept {
  _on_error = ::std::forward<C>(callback);
}

inline PageAllocator& NetworkIOService::send_buffer_allocator() noexcept {
  return *_send_buffer_allocator;
}

void NetworkIOService::print_sqe(SQE& sqe) {
  ::std::cerr << "SQE {" << ::std::endl;
  ::std::cerr << "  user_data: " << (void*)(uint64_t)sqe.user_data << " {"
              << ::std::endl;
  ::std::cerr << "    socket_id: " << sqe.user_data.socket_id() << ::std::endl;
  ::std::cerr << "    mode: " << (int)sqe.user_data.mode() << ::std::endl;
  ::std::cerr << "  }" << ::std::endl;
  ::std::cerr << "}" << ::std::endl;
}

void NetworkIOService::print_cqe(struct ::io_uring_cqe* cqe) {
  ::std::cerr << "io_uring_cqe " << cqe << " {" << ::std::endl;
  ::std::cerr << "  user_data: " << (void*)(uint64_t)cqe->user_data << " {"
              << ::std::endl;
  ::std::cerr << "    socket_id: " << UserData {cqe->user_data}.socket_id()
              << ::std::endl;
  ::std::cerr << "    mode: " << (int)UserData {cqe->user_data}.mode()
              << ::std::endl;
  ::std::cerr << "  }" << ::std::endl;
  ::std::cerr << "  res: " << cqe->res << ::std::endl;
  ::std::cerr << "  flags: " << (void*)(uint64_t)cqe->flags << ::std::endl;
  ::std::cerr << "}" << ::std::endl;
}

BABYLON_IO_NAMESPACE_END
