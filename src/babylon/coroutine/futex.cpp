#include "babylon/coroutine/futex.h"

#if __cpp_concepts && __cpp_lib_coroutine

BABYLON_COROUTINE_NAMESPACE_BEGIN

int Futex::wake_one() noexcept {
  auto& box = DepositBox<Node>::instance();

  Node* node = nullptr;
  {
    ::std::lock_guard<::std::mutex> lock {_mutex};
    for (node = _awaiter_head.next; node != nullptr; node = node->next) {
      // Unconditionally remove node from list, even when we can not take
      // ownership of it.

      // Link prev->next to next and next->prev to prev
      auto next_node = node->next;
      if (next_node != nullptr) {
        next_node->prev = &_awaiter_head;
      }
      _awaiter_head.next = next_node;
      // Clear prev and next
      node->prev = nullptr;
      node->next = nullptr;

      // Try take ownership, and stop if success.
      // It is safe to just leave this node linger as it is when fail, because
      // the owner will handle it.
      if (box.take_released(node->id)) {
        break;
      }
    }
  }
  // Resume when remove one node and get it's ownership successfully
  if (node) {
    node->promise->resume(node->handle);
    box.finish_released(node->id);
    return 1;
  }
  return 0;
}

int Futex::wake_all() noexcept {
  auto& box = DepositBox<Node>::instance();
  Node* head = nullptr;
  {
    ::std::lock_guard<::std::mutex> lock {_mutex};
    // Move entire list to tmp head
    head = _awaiter_head.next;
    _awaiter_head.next = nullptr;
    auto tail = &head;
    for (auto node = head; node != nullptr; node = node->next) {
      // Clear prev is needed to mark node unconnected. The next pointer is safe
      // to keep valid, because prev == nullptr is enough for the concurrent
      // canceler
      node->prev = nullptr;

      // Try take ownership, and skip failed node.
      // It is safe to just leave nodes linger as it is when fail, because
      // the owner will handle it.
      if (box.take_released(node->id)) {
        tail = &(node->next);
      } else {
        *tail = node->next;
      }
    }
  }
  // Resume when remove nodes and get their ownership successfully.
  int waked = 0;
  for (auto node = head; node != nullptr; node = node->next) {
    node->promise->resume(node->handle);
    box.finish_released(node->id);
    waked++;
  }
  return waked;
}

bool Futex::add_awaiter(Node* node, uint64_t expected_value) noexcept {
  ::std::lock_guard<::std::mutex> lock {_mutex};
  if (expected_value == _value) {
    // Front insert
    node->prev = &_awaiter_head;
    node->next = _awaiter_head.next;
    _awaiter_head.next = node;
    if (node->next) {
      node->next->prev = node;
    }
    return true;
  }
  return false;
}

void Futex::remove_awaiter(Node* node) noexcept {
  ::std::lock_guard<::std::mutex> lock {_mutex};
  // Cleared prev means already unlinked
  if (node->prev) {
    node->prev->next = node->next;
    if (node->next) {
      node->next->prev = node->prev;
    }
  }
}

BABYLON_COROUTINE_NAMESPACE_END

#endif // __cpp_concepts && __cpp_lib_coroutine
