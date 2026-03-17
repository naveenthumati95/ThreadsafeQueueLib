#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"
#include <utility>

template <typename T, size_t Capacity>
using queue = tsfqueue::__impl::lockfree_spsc_bounded<T, Capacity>;

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_push(T value)
{
  while (true)
  {
    if ((tail_cache + 1) % Capacity == head_cache)
    {
      head_cache = head.load();
    }
    else
    {
      arr[tail_cache] = value;
      tail_cache = (tail_cache + 1) % Capacity;
      tail.store(tail_cache);
      break;
    }
  }
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_push(T value)
{
  return emplace_back(std::move(value));
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::try_pop(T &value)
{
  if (tail_cache == head_cache)
  {
    tail_cache = tail.load(std::memory_order_acquire); // refresh cache
    if (tail_cache == head_cache)                      // empty
    {
      return false;
    }
  }
  value = arr[head_cache];
  head_cache = (head_cache + 1) % Capacity;
  head.store(head_cache, std::memory_order_release);
  return true;
}

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_pop(T &value)
{
  while (true)
  {
    if (head_cache == tail_cache)
    {
      tail_cache = tail.load();
    }
    else
    {
      value = arr[head_cache];
      head_cache = (head_cache + 1) % Capacity;
      head.store(head_cache);
      break;
    }
  }
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::peek(T &value)
{
  size_t cur_head = head.load();
  if (cur_head == tail_cache)
  {
    tail_cache = tail.load();
    if (cur_head == tail_cache)
      return false;
  }
  value = arr[cur_head];
  return true;
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::empty()
{
  return head.load() == tail.load();
}

template <typename T, size_t Capacity>
template <typename... Args>
bool queue<T, Capacity>::emplace_back(Args &&...args)
{
  if ((tail_cache + 1) % Capacity == head_cache)
  {
    head_cache = head.load(std::memory_order_acquire); // refresh cache
    if ((tail_cache + 1) % Capacity == head_cache)     // full
    {
      return false;
    }
  }
  arr[tail_cache] = T(std::forward<Args>(args)...);
  tail_cache = (tail_cache + 1) % Capacity;
  tail.store(tail_cache, std::memory_order_release);
  return true;
}

template <typename T, size_t Capacity>
size_t queue<T, Capacity>::size()
{
  size_t t = tail.load(std::memory_order_acquire);
  size_t h = head.load(std::memory_order_acquire);
  return (t - h + Capacity) % Capacity;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??