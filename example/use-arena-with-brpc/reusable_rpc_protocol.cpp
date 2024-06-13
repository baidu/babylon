#include "reusable_rpc_protocol.h"

#include "babylon/reusable/allocator.h"

DEFINE_uint64(
    babylon_rpc_closure_cache_num, 128,
    "max idle closure and babylon::SwissMemoryResource num cached for reuse");
DEFINE_uint64(babylon_rpc_page_size, 128UL << 10,
              "each memory block size for allocate request and response");
DEFINE_uint64(babylon_rpc_page_cache_num, 1024,
              "max idle page num cached for reuse");

DEFINE_bool(babylon_rpc_full_reuse, false,
            "reuse instance instead of only memory");

BABYLON_NAMESPACE_BEGIN

ReusableRPCProtocol::Closure* ReusableRPCProtocol::create(
    const MethodProperty* property) noexcept {
  ObjectPool<Closure>* pool;
  if (FLAGS_babylon_rpc_full_reuse) {
    pool = &closure_pool(property->status);
  } else {
    pool = &closure_pool();
  }

  auto closure = pool->pop().release();
  closure->set_pool(*pool);
  closure->prepare(property);
  return closure;
}

int ReusableRPCProtocol::register_protocol() noexcept {
  return register_protocol(72, "baidu_std_reuse");
}

int ReusableRPCProtocol::register_protocol(int type, StringView name) noexcept {
  static ::std::string protocol_name;

  static ::std::atomic_flag regisitered;
  if (regisitered.test_and_set()) {
    return -1;
  }

  auto protocol_type = ::brpc::ProtocolType(type);
  protocol_name = name;
  return ::brpc::RegisterProtocol(protocol_type,
                                  ReusableRPCProtocol(protocol_name.c_str()));
}

PageAllocator& ReusableRPCProtocol::page_allocator() noexcept {
  static struct S {
    S() noexcept {
      new_delete_page_allocator.set_page_size(FLAGS_babylon_rpc_page_size);
      cached_page_allocator.set_upstream(new_delete_page_allocator);
      cached_page_allocator.set_free_page_capacity(
          FLAGS_babylon_rpc_page_cache_num);
    }
    ::babylon::NewDeletePageAllocator new_delete_page_allocator;
    ::babylon::CachedPageAllocator cached_page_allocator;
  } singleton;

  struct SS {
    static size_t free_page_num(void*) noexcept {
      return singleton.cached_page_allocator.free_page_num();
    }
    static ::bvar::Stat hit_stat(void*) noexcept {
      auto summary = singleton.cached_page_allocator.cache_hit_summary();
      ::bvar::Stat result;
      result.sum = summary.sum;
      result.num = summary.num;
      return result;
    }
  };
  static ::bvar::PassiveStatus<size_t> free_page_num(
      "babylon_reusable_rpc_free_page_num", SS::free_page_num, nullptr);
  static ::bvar::PassiveStatus<::bvar::Stat> hit_stat(SS::hit_stat, nullptr);
  static ::bvar::Window<::bvar::PassiveStatus<::bvar::Stat>,
                        ::bvar::SERIES_IN_SECOND>
      window_hit_summary("babylon_reusable_rpc_page_cache_hit_ratio", &hit_stat,
                         -1);

  return singleton.cached_page_allocator;
}

ObjectPool<ReusableRPCProtocol::Closure>&
ReusableRPCProtocol::closure_pool() noexcept {
  static struct S {
    S() noexcept {
      pool.set_creator([&]() {
        auto closure = new Closure;
        closure->set_page_allocator(page_allocator());
        return ::std::unique_ptr<Closure> {closure};
      });
      pool.reserve_and_clear(FLAGS_babylon_rpc_closure_cache_num);
    }
    ObjectPool<Closure> pool;
  } singleton;
  return singleton.pool;
}

ObjectPool<ReusableRPCProtocol::Closure>& ReusableRPCProtocol::closure_pool(
    MethodStatus* method_status) noexcept {
  static ConcurrentTransientHashMap<MethodStatus*, ObjectPool<Closure>> pools {
      32};
  auto iter = pools.find(method_status);
  if (iter != pools.end()) {
    return iter->second;
  }

  ObjectPool<Closure> pool;
  pool.set_creator([&]() {
    auto closure = new Closure;
    closure->set_page_allocator(page_allocator());
    return ::std::unique_ptr<Closure> {closure};
  });
  pool.reserve_and_clear(FLAGS_babylon_rpc_closure_cache_num);

  auto result = pools.emplace(method_status, ::std::move(pool));
  return result.first->second;
}

void ReusableRPCProtocol::Closure::set_page_allocator(
    PageAllocator& page_allocator) noexcept {
  resource().set_page_allocator(page_allocator);
}

void ReusableRPCProtocol::Closure::set_pool(
    ObjectPool<Closure>& pool) noexcept {
  _pool = &pool;
}

void ReusableRPCProtocol::Closure::prepare(
    const ::brpc::Server::MethodProperty* property) noexcept {
  if (FLAGS_babylon_rpc_full_reuse) {
    if (_method_status == nullptr) {
      auto service = property->service;
      auto method = property->method;
      auto arena = &static_cast<Arena&>(resource());
      _request_accessor =
          _manager.create_object<Message>([&](SwissMemoryResource&) {
            return service->GetRequestPrototype(method).New(arena);
          });
      _response_accessor =
          _manager.create_object<Message>([&](SwissMemoryResource&) {
            return service->GetResponsePrototype(method).New(arena);
          });
      _method_status = property->status;
    }
    _request = _request_accessor.get();
    _response = _response_accessor.get();
  } else {
    auto service = property->service;
    auto method = property->method;
    auto arena = &static_cast<Arena&>(resource());
    _request = service->GetRequestPrototype(method).New(arena);
    _response = service->GetResponsePrototype(method).New(arena);
    _method_status = property->status;
  }
}

void ReusableRPCProtocol::Closure::set_correlation_id(
    int64_t correlation_id) noexcept {
  _correlation_id = correlation_id;
}

void ReusableRPCProtocol::Closure::set_received_us(
    int64_t received_us) noexcept {
  _received_us = received_us;
}

void ReusableRPCProtocol::Closure::set_server(
    const ::brpc::Server* server) noexcept {
  _server = server;
}

void ReusableRPCProtocol::Closure::set_controller(
    Controller* controller) noexcept {
  _controller = controller;
}

::google::protobuf::Message* ReusableRPCProtocol::Closure::request() noexcept {
  return _request;
}

::google::protobuf::Message* ReusableRPCProtocol::Closure::response() noexcept {
  return _response;
}

SwissMemoryResource& ReusableRPCProtocol::Closure::resource() noexcept {
  return _manager.resource();
}

BABYLON_NAMESPACE_END
