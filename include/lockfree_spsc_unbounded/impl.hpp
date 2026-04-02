#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

namespace tsfqueue::impl {
template <typename T> void lockfree_spsc_unbounded<T>::push(T value) {

  static_assert(std::is_move_constructible_v<T>,
                "T must be move constructible");

  emplace_back(std::move(value));
}

template <typename T> bool lockfree_spsc_unbounded<T>::try_pop(T &value) {

  static_assert(std::is_nothrow_destructible_v<T>,
                "T must be nothrow destructible");
  static_assert(std::is_move_assignable_v<T>, "T must be move-assignable");

  node *new_head = head->next.load(std::memory_order_acquire);

  if (new_head == nullptr) {
    return false;
  }

  node *old_head = head;
  value = std::move(head->data);
  head = new_head;

  delete old_head;

  capacity.fetch_sub(1, std::memory_order_relaxed);

  return true;
}

template <typename T> void lockfree_spsc_unbounded<T>::wait_and_pop(T &value) {

  static_assert(std::is_nothrow_destructible_v<T>,
                "T must be nothrow destructible");
  static_assert(std::is_move_assignable_v<T>, "T must be move-assignable");

  node *new_head;
  while ((new_head = head->next.load(std::memory_order_acquire)) == nullptr) {
    std::this_thread::yield(); // low latency-high cpu usage
  }

  node *old_head = head;
  value = std::move(head->data);
  head = new_head;

  delete old_head;

  capacity.fetch_sub(1, std::memory_order_relaxed);
}

template <typename T> bool lockfree_spsc_unbounded<T>::unsafe_peek(T &value) {

  static_assert(std::is_copy_assignable_v<T>, "T must be copy-assignable");

  if (empty()) {
    return false;
  }

  value = head->data;
  return true;
}

template <typename T> bool lockfree_spsc_unbounded<T>::empty(void) {
  return (head->next.load(std::memory_order_acquire) == nullptr);
}

template <typename T>
template <typename... Args>
void lockfree_spsc_unbounded<T>::emplace_back(Args &&...args) {
  static_assert(std::is_constructible_v<T, Args &&...>,
                "T must be constructible with Args&&...");
  static_assert(std::is_default_constructible_v<T>,
                "T must be default constructible");
  static_assert(std::is_move_assignable_v<T>, "T must be move-assignable");

  node *stub = new node();
  stub->next.store(nullptr, std::memory_order_relaxed);

  tail->data = T(std::forward<Args>(args)...);
  capacity.fetch_add(1, std::memory_order_relaxed);
  tail->next.store(stub, std::memory_order_release);

  tail = stub;
}

template <typename T> size_t lockfree_spsc_unbounded<T>::size() {
  return capacity.load(std::memory_order_relaxed);
}
} // namespace tsfqueue::impl

#endif

// 1. Add static asserts

// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)

// 3. Add size() function

// 4. Any more suggestions ??