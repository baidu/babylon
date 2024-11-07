#include "swiss_message_factory.h"

SwissRpcPBMessageFactory::SwissRpcPBMessageFactory() noexcept {
  _cached_page_allocator.set_upstream(_new_delete_page_allocator);
}

void SwissRpcPBMessageFactory::set_page_size(size_t page_size) noexcept {
  _new_delete_page_allocator.set_page_size(page_size);
}

void SwissRpcPBMessageFactory::set_free_page_capacity(
    size_t free_page_capacity) noexcept {
  _cached_page_allocator.set_free_page_capacity(free_page_capacity);
}

void SwissRpcPBMessageFactory::set_free_message_capacity(
    size_t free_message_capacity) noexcept {
  _free_message_capacity = free_message_capacity;
  _pool.set_creator([&]() -> ::std::unique_ptr<Messages> {
    auto messages = new Messages;
    messages->set_page_allocator(_cached_page_allocator);
    return {messages, {}};
  });
  _pool.set_recycler([](Messages& messages) {
    messages.clear();
  });
  _pool.reserve_and_clear(_free_message_capacity);
}

::brpc::RpcPBMessages* SwissRpcPBMessageFactory::Get(
    const Service& service, const MethodDescriptor& method) {
  auto messages = _pool.pop().release();
  messages->prepare(service, method);
  messages->set_pool(_pool);
  return messages;
}

void SwissRpcPBMessageFactory::Return(::brpc::RpcPBMessages* messages) {
  auto reusable_messages = static_cast<Messages*>(messages);
  reusable_messages->pool().push(
      ::std::unique_ptr<Messages> {reusable_messages});
}

SwissRpcPBMessageFactory::Message*
SwissRpcPBMessageFactory::Messages::Request() {
  return _request;
}

SwissRpcPBMessageFactory::Message*
SwissRpcPBMessageFactory::Messages::Response() {
  return _response;
}

void SwissRpcPBMessageFactory::Messages::set_page_allocator(
    ::babylon::PageAllocator& page_allocator) noexcept {
  _resource.set_page_allocator(page_allocator);
}

void SwissRpcPBMessageFactory::Messages::set_pool(
    ::babylon::ObjectPool<Messages>& pool) noexcept {
  _pool = &pool;
}

::babylon::ObjectPool<SwissRpcPBMessageFactory::Messages>&
SwissRpcPBMessageFactory::Messages::pool() noexcept {
  return *_pool;
}

void SwissRpcPBMessageFactory::Messages::prepare(
    const Service& service, const MethodDescriptor& method) noexcept {
  using Arena = ::google::protobuf::Arena;
  auto arena = &static_cast<Arena&>(_resource);
  _request = service.GetRequestPrototype(&method).New(arena);
  _response = service.GetResponsePrototype(&method).New(arena);
}

void SwissRpcPBMessageFactory::Messages::clear() noexcept {
  _resource.release();
}
