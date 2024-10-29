#include "babylon/io/network_service.h"

#include "babylon/coroutine/yield_awaitable.h"
#include "babylon/io/error.h"
#include "babylon/logging/logger.h"

#include <poll.h> // POLLIN

BABYLON_IO_NAMESPACE_BEGIN

NetworkIOService& NetworkIOService::instance() noexcept {
  static NetworkIOService service;
  return service;
}

void NetworkIOService::set_executor(Executor& executor) noexcept {
  _executor = &executor;
}

void NetworkIOService::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  _page_allocator = &page_allocator;
  _send_buffer_allocator.reset(new SendBufferAllocator);
  _send_buffer_allocator->set_upstream(page_allocator);
}

int NetworkIOService::start() noexcept {
  auto ret = ::io_uring_queue_init(_ring_capacity, &_input_ring,
                                   IORING_SETUP_SUBMIT_ALL);
  if (ret != 0) {
    BABYLON_LOG(WARNING) << "io_uring_queue_init failed with " << Error {-ret};
    return -1;
  }

  ret = ::io_uring_register_files_sparse(&_input_ring, 10);
  if (ret != 0) {
    BABYLON_LOG(WARNING) << "io_uring_register_files_sparse failed with "
                         << Error {-ret};
    return -1;
  }

  _submission_eventfd = ::eventfd(0, 0);
  if (_submission_eventfd < 0) {
    BABYLON_LOG(WARNING) << "create eventfd failed with " << Error {};
    ::io_uring_queue_exit(&_input_ring);
    return -1;
  }

  _buffer_ring = ::io_uring_setup_buf_ring(&_input_ring, _buffer_ring_capacity,
                                           0, 0, &ret);
  if (ret != 0) {
    BABYLON_LOG(WARNING) << "io_uring_setup_buf_ring failed with "
                         << Error {-ret};
    ::io_uring_queue_exit(&_input_ring);
    ::close(_submission_eventfd);
    return -1;
  }

  _page_size = _page_allocator->page_size();
  _send_buffer_size = _send_buffer_allocator->page_size();
  _buffers.resize(_buffer_ring_capacity);
  for (size_t i = 0; i < _buffer_ring_capacity; ++i) {
    fill_buffer(i);
  }

  _socket_data.ensure(0);

  _running.store(true, ::std::memory_order_relaxed);
  _keep_accept_and_receive_finished =
      _executor->execute(&NetworkIOService::keep_accept_and_receive, this);
  _keep_reclaim_send_buffer_finished =
      _executor->execute(&NetworkIOService::keep_reclaim_send_buffer, this);
  return 0;
}

void NetworkIOService::accept(int listen_socket) noexcept {
  _submission_queue.push<true, false, false>([&](UserData& user_data) {
    user_data = UserData {1, SocketId {listen_socket}};
  });
  signal_submission_events();
}

void NetworkIOService::send(SocketId socket_id, Entry& entry) noexcept {
  _output_queue.push<true, false, false>([&](OutputTask& task) {
    task.user_data = UserData {3, socket_id};
    task.entry = entry;
  });
  if (0 == _output_events.fetch_add(1, ::std::memory_order_acq_rel)) {
    _executor->submit(&NetworkIOService::consume_output_queue, this);
  }
}

NetworkIOService::NetworkIOService() noexcept
    : _on_accept {default_on_accept}, _on_error {default_on_error} {}

::babylon::CoroutineTask<>
NetworkIOService::keep_accept_and_receive() noexcept {
  // To be submitted operations. Both user accept receive operations or internal
  // ones are store to here first. Design to flatten some
  // submission-on-completion recursion. Initial with an event poll operation to
  // support user submission.
  ::std::vector<UserData> submissions {{0, _submission_eventfd}};
  size_t submited_number {0};

  // Last checked user submission_events. Check again before going into a
  // blocking wait to make it miss no events.
  uint64_t submission_events {0};

  ::std::vector<struct ::io_uring_cqe*> cqes {_ring_capacity, nullptr};
  while (_running.load(::std::memory_order_relaxed)) {
    if (submissions.empty()) {
      using Iterator = ::babylon::ConcurrentBoundedQueue<UserData>::Iterator;
      submission_events = _submission_events.load(::std::memory_order_acquire);
      _submission_queue.template try_pop_n<false, false>(
          [&](Iterator iter, Iterator end) {
            while (iter != end) {
              submissions.emplace_back(*iter);
              iter++;
            }
          },
          _submission_queue.capacity());
    }

    do_input_submission(submissions, submited_number);

    auto ret = ::io_uring_submit_and_get_events(&_input_ring);
    if (ret < 0) {
      BABYLON_LOG(WARNING) << "io_uring_submit_and_get_events failed with "
                           << Error {-ret};
      ::abort();
    }

    do_input_completion(cqes, submissions);

    if (submited_number != submissions.size()) {
      continue;
    } else {
      submissions.clear();
      submited_number = 0;
      if (!_submission_events.compare_exchange_strong(
              submission_events, 0, ::std::memory_order_acq_rel)) {
        continue;
      }
    }

    while (static_cast<::babylon::ThreadPoolExecutor*>(_executor)
               ->local_task_number() > 0) {
      co_await ::babylon::coroutine::yield();
    }

    local_output_ring();
    ::io_uring_wait_cqe_nr(&_input_ring, cqes.data(), 1);
  }
}

void NetworkIOService::do_input_submission(::std::vector<UserData>& submissions,
                                           size_t& submited_number) noexcept {
  for (; submited_number < submissions.size(); ++submited_number) {
    auto user_data = submissions[submited_number];
    switch (user_data.mode()) {
      case 0:
        if (prepare_poll(user_data)) {
          continue;
        }
        break;
      case 1:
        if (prepare_accept(user_data)) {
          continue;
        }
        break;
      case 2:
        if (prepare_receive(user_data)) {
          continue;
        }
        break;
      // case 4:
      //   if (prepare_shutdown_then_close_in_io_thread(sqe)) {
      //     continue;
      //   }
      //   break;
      // case 4:
      //  if (prepare_close_in_io_thread(sqe)) {
      //    continue;
      //  }
      //  break;
      default:
        ::std::cerr << "unknown submission mode " << user_data.mode()
                    << ::std::endl;
        ::abort();
    }
    break;
  }
}

bool NetworkIOService::prepare_poll(UserData user_data) noexcept {
  auto sqe = ::io_uring_get_sqe(&_input_ring);
  if (ABSL_PREDICT_FALSE(sqe == nullptr)) {
    return false;
  }
  ::io_uring_prep_poll_multishot(sqe, user_data.socket_id().fd, POLLIN);
  ::io_uring_sqe_set_data64(sqe, user_data);
  BABYLON_LOG(DEBUG) << "prepare poll on fd " << user_data.socket_id().fd;
  return true;
}

bool NetworkIOService::prepare_accept(UserData user_data) noexcept {
  auto sqe = ::io_uring_get_sqe(&_input_ring);
  if (ABSL_PREDICT_FALSE(sqe == nullptr)) {
    return false;
  }

  ::io_uring_prep_multishot_accept(sqe, user_data.socket_id().fd, nullptr,
                                   nullptr, 0);
  ::io_uring_sqe_set_data64(sqe, user_data);
  BABYLON_LOG(DEBUG) << "prepare accept on socket " << user_data.socket_id();
  return true;
}

bool NetworkIOService::prepare_receive(UserData user_data) noexcept {
  auto sqe = ::io_uring_get_sqe(&_input_ring);
  if (ABSL_PREDICT_FALSE(sqe == nullptr)) {
    return false;
  }
  ::io_uring_prep_recv(sqe, user_data.socket_id().fd, nullptr, 0, 0);
  sqe->buf_group = 0;
  sqe->flags |= IOSQE_BUFFER_SELECT;
  ::io_uring_sqe_set_data64(sqe, user_data);
  BABYLON_LOG(DEBUG) << "prepare receive on socket " << user_data.socket_id();
  return true;
}

bool NetworkIOService::prepare_send_in_io_thread(
    SQE& sqe, ::std::vector<struct ::iovec>& remained_iov) noexcept {
  auto& socket_data = _socket_data[sqe.socket_id().fd];
  if (ABSL_PREDICT_FALSE(socket_data.version != sqe.socket_id().version)) {
    BABYLON_LOG(DEBUG) << "discard expired send to socket " << sqe.socket_id();
    for (auto one_iov : remained_iov) {
      _send_buffer_allocator->deallocate(one_iov.iov_base);
    }
    remained_iov.clear();
    return true;
  }

  if (remained_iov.empty()) {
    sqe.entry.append_to_iovec(_send_buffer_size, remained_iov);
  }

  size_t i = 0;
  for (; i < remained_iov.size(); ++i) {
    auto& one_iov = remained_iov[i];
    auto iosqe = ::io_uring_get_sqe(&_input_ring);
    if (ABSL_PREDICT_FALSE(iosqe == nullptr)) {
      remained_iov.erase(remained_iov.begin(), remained_iov.begin() + i);
      return false;
    }
    auto& footer = _send_buffer_allocator->footer(one_iov.iov_base);
    footer.socket_id = sqe.socket_id();
    ::io_uring_prep_send(iosqe, sqe.socket_id().fd, one_iov.iov_base,
                         one_iov.iov_len, 0);
    ::io_uring_sqe_set_data64(iosqe, UserData {3, one_iov.iov_base});
    BABYLON_LOG(DEBUG) << "prepare send to socket " << sqe.socket_id()
                       << " with ["
                       << ::std::string_view((char*)one_iov.iov_base,
                                             one_iov.iov_len)
                       << "]";
  }
  remained_iov.clear();
  return true;
}

bool NetworkIOService::prepare_shutdown_then_close_in_io_thread(
    SQE& sqe) noexcept {
  auto& socket_data = _socket_data[sqe.socket_id().fd];
  if (ABSL_PREDICT_FALSE(socket_data.version != sqe.socket_id().version)) {
    BABYLON_LOG(DEBUG) << "discard expired shutdown then close to socket "
                       << sqe.socket_id();
    return true;
  }
  auto iosqe = ::io_uring_get_sqe(&_input_ring);
  if (ABSL_PREDICT_FALSE(iosqe == nullptr)) {
    return false;
  }
  socket_data.version += 1;
  ::io_uring_prep_shutdown(iosqe, sqe.socket_id().fd, SHUT_RDWR);
  iosqe->flags |= IOSQE_FIXED_FILE;
  ::io_uring_sqe_set_data64(iosqe, sqe.user_data);
  BABYLON_LOG(DEBUG) << "prepare shutdown then close on socket "
                     << sqe.socket_id();
  return true;
}

bool NetworkIOService::prepare_close_in_io_thread(SQE& sqe) noexcept {
  auto& socket_data = _socket_data[sqe.socket_id().fd];
  if (ABSL_PREDICT_FALSE(socket_data.version != sqe.socket_id().version)) {
    BABYLON_LOG(DEBUG) << "discard expired close to socket " << sqe.socket_id();
    return true;
  }
  auto iosqe = ::io_uring_get_sqe(&_input_ring);
  if (ABSL_PREDICT_FALSE(iosqe == nullptr)) {
    return false;
  }
  socket_data.version += 1;
  ::io_uring_prep_close(iosqe, sqe.socket_id().fd);
  ::io_uring_sqe_set_data64(iosqe, sqe.user_data);
  BABYLON_LOG(DEBUG) << "prepare close on socket " << sqe.socket_id();
  return true;
}

void NetworkIOService::do_input_completion(
    ::std::vector<struct ::io_uring_cqe*>& cqes,
    ::std::vector<UserData>& submissions) noexcept {
  auto peeked =
      ::io_uring_peek_batch_cqe(&_input_ring, cqes.data(), cqes.size());
  for (size_t i = 0; i < peeked; ++i) {
    auto cqe = cqes[i];
    auto mode = UserData::from(cqe).mode();
    switch (mode) {
      case 0:
        on_poll(cqe, submissions);
        break;
      case 1:
        on_accept(cqe, submissions);
        break;
      case 2:
        on_receive(cqe, submissions);
        break;
      // case 4:
      //   on_shutdown_then_close_in_io_thread(iocqe, sqes);
      //   break;
      // case 4:
      //  on_close_in_io_thread(iocqe, sqes);
      //  break;
      default:
        ::std::cerr << "unknown cqe mode" << ::std::endl;
        print_cqe(cqe);
        abort();
    }
  }
  ::io_uring_cq_advance(&_input_ring, peeked);
}

void NetworkIOService::on_poll(struct ::io_uring_cqe* cqe,
                               ::std::vector<UserData>& submissions) noexcept {
  if (!(cqe->flags & IORING_CQE_F_MORE)) {
    submissions.emplace_back(UserData::from(cqe));
  }

  if (ABSL_PREDICT_TRUE(cqe->res < 0)) {
    ::std::cerr << "poll failed" << ::std::endl;
    print_cqe(cqe);
    ::abort();
  }
}

void NetworkIOService::on_accept(
    struct ::io_uring_cqe* cqe, ::std::vector<UserData>& submissions) noexcept {
  if (!(cqe->flags & IORING_CQE_F_MORE)) {
    submissions.emplace_back(UserData::from(cqe));
  }

  if (ABSL_PREDICT_TRUE(cqe->res >= 0)) {
    auto& socket_data = _socket_data.ensure(cqe->res);
    SocketId socket_id;
    socket_id.fd = cqe->res;
    socket_id.version = socket_data.version;
    submissions.emplace_back(UserData {2, socket_id});
    _on_accept(socket_id);
    return;
  }

  BABYLON_LOG(WARNING) << "accept on socket " << UserData::from(cqe).socket_id()
                       << " failed with " << Error {-cqe->res};
  print_cqe(cqe);
  ::abort();
}

void NetworkIOService::on_receive(
    struct ::io_uring_cqe* cqe, ::std::vector<UserData>& submissions) noexcept {
  auto user_data = UserData::from(cqe);
  auto socket_id = user_data.socket_id();

  if (ABSL_PREDICT_TRUE(cqe->res > 0)) {
    auto buffer_index = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
    auto buffer = _buffers[buffer_index];
    auto buffer_size = cqe->res;
    auto buffer_view =
        ::absl::string_view(static_cast<const char*>(buffer), buffer_size);
    auto data =
        ::absl::MakeCordFromExternal(buffer_view, [](absl::string_view view) {
          auto& service = instance();
          service._page_allocator->deallocate(const_cast<char*>(view.data()));
        });
    add_to_input_queue(socket_id, ::std::move(data));
    fill_buffer(buffer_index);
    submissions.emplace_back(user_data);
    return;
  }

  if (ABSL_PREDICT_TRUE(cqe->res == 0)) {
    add_to_input_queue(socket_id, ::absl::Cord {});
    return;
  }

  if (cqe->res == -ECONNRESET) {
    _on_error(socket_id, Error {-cqe->res});
    return;
  }

  if (cqe->res == -ENOBUFS) {
    submissions.emplace_back(user_data);
    return;
  }

  BABYLON_LOG(WARNING) << "receive failed with " << Error {-cqe->res};
  print_cqe(cqe);
  ::abort();
}

void NetworkIOService::add_to_input_queue(
    SocketId socket_id, ::absl::Cord&& received_data) noexcept {
  if (ABSL_PREDICT_FALSE(!_on_receive)) {
    return;
  }

  auto& socket_data = _socket_data[socket_id.fd];
  ReceiveTask task;
  task.socket_id = socket_id;
  task.received_data = ::std::move(received_data);
  socket_data.input_queue.template push<false, false, false>(::std::move(task));
  if (0 == socket_data.input_events.fetch_add(1, ::std::memory_order_acq_rel)) {
    _executor->submit(&NetworkIOService::consume_input_queue, this,
                      socket_data);
  }
}

void NetworkIOService::on_shutdown_then_close_in_io_thread(
    struct ::io_uring_cqe* cqe, ::std::vector<SQE>& sqes) noexcept {
  if (ABSL_PREDICT_FALSE(cqe->res != 0 && cqe->res != -ENOTCONN)) {
    ::std::cerr << "shutdown failed with " << cqe->res << " : "
                << ::strerror(-::std::min(0, cqe->res)) << ::std::endl;
    ::abort();
  }
  auto socket_id = UserData::from(cqe).socket_id();
  sqes.emplace_back(SQE {.user_data = {5, socket_id}});
  BABYLON_LOG(INFO) << "shutdown finish socket "
                    << UserData::from(cqe).socket_id() << " submit close then";
}

void NetworkIOService::on_close_in_io_thread(struct ::io_uring_cqe* cqe,
                                             ::std::vector<SQE>&) noexcept {
  if (ABSL_PREDICT_FALSE(cqe->res < 0)) {
    ::std::cerr << "close failed with " << cqe->res << " : "
                << ::strerror(-::std::min(0, cqe->res)) << ::std::endl;
    ::abort();
  }
  BABYLON_LOG(INFO) << "close finish socket "
                    << UserData::from(cqe).socket_id();
}

struct ::io_uring& NetworkIOService::local_output_ring() noexcept {
  auto& ring = _output_rings.local();
  if (!ring.has_value.load(::std::memory_order_relaxed)) {
    auto ret = ::io_uring_queue_init(8, &ring.value,
                                     IORING_SETUP_SUBMIT_ALL |
                                         IORING_SETUP_SINGLE_ISSUER |
                                         IORING_SETUP_DEFER_TASKRUN);
    if (ret != 0) {
      BABYLON_LOG(WARNING) << "io_uring_queue_init failed with "
                           << Error {-ret};
      ::abort();
    }
    ring.has_value.store(true, ::std::memory_order_release);
  }
  return ring.value;
}

void NetworkIOService::submit_shutdown_and_close_to_io_thread(
    NetworkIOService::SocketId socket_id) noexcept {
  (void)socket_id;
  //_submission_queue.push<true, false, false>([&](SQE& sqe) noexcept {
  //  sqe.user_data = UserData {4, socket_id};
  //});
  // signal_io_thread();
}

::babylon::CoroutineTask<> NetworkIOService::consume_input_queue(
    SocketData& socket_data) noexcept {
  size_t events = socket_data.input_events.load(::std::memory_order_acquire);
  auto& task = socket_data.receive_task;
  bool finished = false;
  ReceiveTask next_task;
  bool next_finished = false;
  while (true) {
    auto poped = socket_data.input_queue.template try_pop_n<false, false>(
        [&](ReceiveTaskIterator iter, ReceiveTaskIterator end) {
          while (iter != end) {
            if (ABSL_PREDICT_FALSE(task.socket_id.fd == -1)) {
              task.socket_id = iter->socket_id;
            }
            if (ABSL_PREDICT_TRUE(!finished)) {
              if (!iter->received_data.empty()) {
                task.received_data.Append(::std::move(iter->received_data));
                iter->received_data.Clear();
              } else {
                finished = true;
              }
            } else {
              next_task.socket_id = iter->socket_id;
              if (!iter->received_data.empty()) {
                next_task.received_data.Append(
                    ::std::move(iter->received_data));
              } else {
                next_finished = true;
              }
            }
            iter++;
          }
        },
        socket_data.input_queue.capacity());
    if (poped != 0) {
      co_await _on_receive(task.socket_id, task.received_data, finished);
      if (finished) {
        task.socket_id.fd = -1;
        task.received_data.Clear();
      }
      if (ABSL_PREDICT_FALSE(next_task.socket_id.fd != -1)) {
        co_await _on_receive(next_task.socket_id, next_task.received_data,
                             next_finished);
        if (next_finished) {
          next_task.received_data.Clear();
        } else {
          task = ::std::move(next_task);
        }
        next_task.socket_id.fd = -1;
      }
      events = socket_data.input_events.load(::std::memory_order_acquire);
    } else if (socket_data.input_events.compare_exchange_strong(
                   events, 0, ::std::memory_order_acq_rel)) {
      break;
    }
  }
  co_return;
}

void NetworkIOService::keep_reclaim_send_buffer() noexcept {
  ::std::vector<struct ::io_uring_cqe*> cqes;
  cqes.resize(_ring_capacity);
  while (_running.load(::std::memory_order_relaxed)) {
    bool overflow = false;
    _output_rings.for_each([&](const OptionalRing* iter,
                               const OptionalRing* end) {
      while (iter != end) {
        auto& ring = const_cast<OptionalRing&>(*iter++);
        if (!ring.has_value.load(::std::memory_order_acquire)) {
          continue;
        }
        auto peeked =
            ::io_uring_peek_batch_cqe(&ring.value, cqes.data(), cqes.size());
        if (peeked == cqes.size()) {
          overflow = true;
        }
        if (peeked > 0) {
          for (size_t i = 0; i < peeked; ++i) {
            reclaim_send_buffer(cqes[i]);
          }
          ::io_uring_cq_advance(&ring.value, peeked);
        }
      }
    });
    if (!overflow) {
      ::usleep(1000);
    }
  }
}

void NetworkIOService::reclaim_send_buffer(
    struct ::io_uring_cqe* cqe) noexcept {
  auto buffer = UserData {cqe->user_data}.buffer();
  auto socket_id = _send_buffer_allocator->footer(buffer).socket_id;
  if (ABSL_PREDICT_FALSE(cqe->res < 0)) {
    _on_error(socket_id, Error {-cqe->res});
  }
  if (!(cqe->flags & IORING_CQE_F_MORE)) {
    _send_buffer_allocator->deallocate(buffer);
  }
}

void NetworkIOService::signal_submission_events() noexcept {
  if (0 == _submission_events.fetch_add(1, ::std::memory_order_acq_rel)) {
    uint64_t event = 1;
    auto ret = ::write(_submission_eventfd, &event, sizeof(event));
    if (ret != sizeof(event)) {
      BABYLON_LOG(WARNING)
          << "signal_submission_events write eventfd failed with " << Error {};
      ::abort();
    }
  }
}

void NetworkIOService::fill_buffer(size_t buffer_index) noexcept {
  _buffers[buffer_index] = _page_allocator->allocate();
  ::io_uring_buf_ring_add(_buffer_ring, _buffers[buffer_index],
                          _page_allocator->page_size(), buffer_index,
                          ::io_uring_buf_ring_mask(_buffer_ring_capacity), 0);
  ::io_uring_buf_ring_advance(_buffer_ring, 1);
}

void NetworkIOService::consume_output_queue() noexcept {
  auto& ring = local_output_ring();

  struct S {
    SocketId socket_id;
    ::std::vector<struct ::iovec> iovs;
  };
  thread_local ::std::vector<::std::unique_ptr<S>> reused_merged_tasks;
  thread_local ::std::vector<S*> merged_tasks;
  size_t merged_task_num = 0;

  size_t merge_num = 0;

  size_t events = 1;
  while (true) {
    auto poped =
        _output_queue.template try_pop<false, false>([&](OutputTask& task) {
          auto socket_id = task.user_data.socket_id();
          if (ABSL_PREDICT_FALSE(socket_id.fd >= merged_tasks.size())) {
            merged_tasks.resize(socket_id.fd + 1);
          }
          auto merged_task = merged_tasks[socket_id.fd];
          if (ABSL_PREDICT_FALSE(merged_task == nullptr)) {
            if (merged_task_num >= reused_merged_tasks.size()) {
              merged_task = new S;
              reused_merged_tasks.emplace_back(merged_task);
            }
            merged_task = merged_tasks[socket_id.fd] = reused_merged_tasks[merged_task_num++].get();
            merged_task->socket_id = socket_id;
          }
          if (ABSL_PREDICT_FALSE(socket_id.version < merged_task->socket_id.version)) {
            return;
          }
          task.entry.append_to_iovec(_send_buffer_size, merged_task->iovs);
          merge_num++;
        });

    if (poped) {
      continue;
    }

    for (size_t i = 0; i < merged_task_num; ++i) {
      auto& merged_task = reused_merged_tasks[i];
      auto socket_id = merged_task->socket_id;
      auto& iovs = merged_task->iovs;
      if (iovs.empty()) {
        continue;
      }

      size_t j = 0;
      for (size_t k = 1; k < iovs.size(); ++k) {
        auto& last_iov = iovs[j];
        auto& one_iov = iovs[k];
        if (last_iov.iov_len + one_iov.iov_len <= _send_buffer_size) {
          ::memcpy(static_cast<char*>(last_iov.iov_base) + last_iov.iov_len,
              static_cast<char*>(one_iov.iov_base), one_iov.iov_len);
          last_iov.iov_len += one_iov.iov_len;
          continue;
        }
        if (j + 1 < k) {
          ::std::swap(iovs[j + 1], iovs[k]);
        }
        ++j;
      }

      for (size_t k = j + 1; k < iovs.size(); ++k) {
        _send_buffer_allocator->deallocate(iovs[k].iov_base);
      }
      iovs.resize(j + 1);

      for (auto& one_iov : iovs) {
        auto& footer = _send_buffer_allocator->footer(one_iov.iov_base);
        footer.socket_id = socket_id;
        auto sqe = ::io_uring_get_sqe(&ring);
        if (ABSL_PREDICT_FALSE(sqe == nullptr)) {
          auto ret = ::io_uring_submit(&ring);
          if (ABSL_PREDICT_FALSE(ret < 0)) {
            BABYLON_LOG(INFO)
                << "consume_output_queue submit fail " << Error {-ret};
            ::abort();
          }
          sqe = ::io_uring_get_sqe(&ring);
        }
        ::io_uring_prep_send(sqe, socket_id.fd, one_iov.iov_base,
                             one_iov.iov_len, 0);
        ::io_uring_sqe_set_data64(sqe, UserData {3, one_iov.iov_base});
      }

      auto ret = ::io_uring_submit_and_get_events(&ring);
      if (ret < 0) {
        BABYLON_LOG(WARNING) << "io_uring_submit error " << Error {-ret};
        ::abort();
      }

      merged_tasks[socket_id.fd] = nullptr;
      iovs.clear();
    }

    merged_task_num = 0;

    merge_summer << merge_num;
    merge_num = 0;

    if (_output_events.compare_exchange_strong(
            events, 0, ::std::memory_order_acq_rel)) {
      break;
    }
  }
}

void NetworkIOService::default_on_accept(SocketId socket_id) noexcept {
  BABYLON_LOG(INFO) << "accept socket " << socket_id;
}

void NetworkIOService::default_on_error(SocketId socket_id,
                                        Error error) noexcept {
  BABYLON_LOG(WARNING) << "send failed to socket " << socket_id << " with "
                       << error;
}

BABYLON_IO_NAMESPACE_END
