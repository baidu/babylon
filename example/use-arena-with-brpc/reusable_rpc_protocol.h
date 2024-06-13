#pragma once

#include "babylon/concurrent/object_pool.h"
#include "babylon/concurrent/transient_hash_table.h"
#include "babylon/reusable/manager.h"

#include "brpc/policy/baidu_rpc_protocol.h"
#include "brpc/server.h"

BABYLON_NAMESPACE_BEGIN

// 特化支持server端的protocol
// 逻辑上等同于baidu_std，实际代码也是拷贝而来
// 主要针对reqeust和response的重复利用，以及生命周期做了扩展支持
class ReusableRPCProtocol : public ::brpc::Protocol {
 private:
  using Arena = ::google::protobuf::Arena;
  using Message = ::google::protobuf::Message;
  using MethodStatus = ::brpc::MethodStatus;
  using Controller = ::brpc::Controller;
  using MethodProperty = ::brpc::Server::MethodProperty;

 public:
  // 传入service代码的回调闭包
  // 内部持有一次请求处理使用的上下文信息
  // Run内部含【回包】和【释放资源】两部分
  // 分别对应send_response和release，即Run = send_response + release
  // 通过拆分出来支持两阶段调用，在高级场景可以先回包
  // 之后利用reqeust继续后台处理（异步打印、后台通信等）
  class Closure : public ::google::protobuf::Closure {
   public:
    inline Closure() = default;
    Closure(Closure&&) = delete;
    Closure(const Closure&) = delete;
    Closure& operator=(Closure&&) = delete;
    Closure& operator=(const Closure&) = delete;
    virtual ~Closure() noexcept = default;

    void set_page_allocator(PageAllocator& page_allocator) noexcept;

    void set_pool(ObjectPool<Closure>& pool) noexcept;
    void prepare(const ::brpc::Server::MethodProperty* property) noexcept;
    void set_correlation_id(int64_t correlation_id) noexcept;
    void set_received_us(int64_t received_us) noexcept;
    void set_server(const ::brpc::Server* server) noexcept;
    void set_controller(Controller* controller) noexcept;

    Message* request() noexcept;
    Message* response() noexcept;

    virtual void Run() noexcept override;

   private:
    SwissMemoryResource& resource() noexcept;

    // 实际的生命周期持有者
    ObjectPool<Closure>* _pool {nullptr};
    // 对应的RPC method
    MethodStatus* _method_status {nullptr};
    // 内存分配器
    SwissManager _manager;

    // 请求级信息
    int64_t _correlation_id {0};
    int64_t _received_us {0};
    const ::brpc::Server* _server {nullptr};
    Controller* _controller {nullptr};

    // 动态分配的实例
    Message* _request {nullptr};
    Message* _response {nullptr};

    // 复用模式的持有器
    ReusableAccessor<Message> _request_accessor;
    ReusableAccessor<Message> _response_accessor;
  };

  static int register_protocol() noexcept;
  static int register_protocol(int type, StringView name) noexcept;

  static Closure* create(const MethodProperty* property) noexcept;

 private:
  static ObjectPool<Closure>& closure_pool() noexcept;
  static PageAllocator& page_allocator() noexcept;
  static ObjectPool<Closure>& closure_pool(
      MethodStatus* method_status) noexcept;

  ReusableRPCProtocol(const char* name) noexcept;
};

BABYLON_NAMESPACE_END
