#include "babylon/concurrent/object_pool.h"
#include "babylon/concurrent/transient_hash_table.h"
#include "babylon/reusable/manager.h"

#include "brpc/rpc_pb_message_factory.h"

class SwissRpcPBMessageFactory : public ::brpc::RpcPBMessageFactory {
 private:
  using Message = ::google::protobuf::Message;
  using Service = ::google::protobuf::Service;
  using MethodDescriptor = ::google::protobuf::MethodDescriptor;
  class Messages;

 public:
  SwissRpcPBMessageFactory() noexcept;
  SwissRpcPBMessageFactory(SwissRpcPBMessageFactory&&) = delete;
  SwissRpcPBMessageFactory(const SwissRpcPBMessageFactory&) = delete;
  SwissRpcPBMessageFactory& operator=(SwissRpcPBMessageFactory&&) = delete;
  SwissRpcPBMessageFactory& operator=(const SwissRpcPBMessageFactory&) = delete;

  void set_page_size(size_t page_size) noexcept;
  void set_free_page_capacity(size_t free_page_capacity) noexcept;
  void set_free_message_capacity(size_t free_message_capacity) noexcept;

 private:
  virtual ::brpc::RpcPBMessages* Get(const Service& service,
                                     const MethodDescriptor& method) override;
  virtual void Return(::brpc::RpcPBMessages* messages) override;

  size_t _free_message_capacity {128};

  ::babylon::ConcurrentTransientHashMap<const MethodDescriptor*,
                                        ::babylon::ObjectPool<Messages>>
      _pool_for_method {32};
  ::babylon::NewDeletePageAllocator _new_delete_page_allocator;
  ::babylon::CachedPageAllocator _cached_page_allocator;
  ::babylon::ObjectPool<Messages> _pool;
};

class SwissRpcPBMessageFactory::Messages : public ::brpc::RpcPBMessages {
 public:
  void set_page_allocator(::babylon::PageAllocator& page_allocator) noexcept;
  void set_pool(::babylon::ObjectPool<Messages>& pool) noexcept;
  ::babylon::ObjectPool<Messages>& pool() noexcept;

  void prepare(const Service& service, const MethodDescriptor& method) noexcept;
  void clear() noexcept;

  virtual Message* Request();
  virtual Message* Response();

 private:
  ::babylon::ObjectPool<Messages>* _pool {nullptr};
  Message* _request {nullptr};
  Message* _response {nullptr};
  ::babylon::SwissMemoryResource _resource;
};
