#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"
#include <utility>

namespace tsfqueue::impl
{
  template <typename T, size_t Capacity>
  void lockfree_spsc_bounded<T, Capacity>::wait_and_push(T value)
  {
    size_t tail_load = tail.load(std::memory_order_relaxed);
    size_t next_tail = (tail_load + 1) % capacity;

    while (next_tail == head_cache)
    {
      head_cache = head.load(std::memory_order_acquire); // busy wait
    }

    arr[tail_load] = std::move(value);
    tail_cache = next_tail;
    tail.store(next_tail, std::memory_order_release);
  }

  template <typename T, size_t Capacity>
  bool lockfree_spsc_bounded<T, Capacity>::try_push(T value)
  {
    return emplace_back(std::move(value));
  }

  template <typename T, size_t Capacity>
  template <typename... Args>
  bool lockfree_spsc_bounded<T, Capacity>::emplace_back(Args &&...args)
  {
    size_t tail_load = tail.load(std::memory_order_relaxed);
    if ((tail_load + 1) % capacity == head_cache)
    {
      head_cache = head.load(std::memory_order_acquire);
      if ((tail_load + 1) % capacity == head_cache)
      {
        return false;
      }
    }

    arr[tail_load] = T(std::forward<Args>(args)...);
    tail_load = (tail_load + 1) % capacity;
    tail.store(tail_load, std::memory_order_release);
    return true;
  }

  template <typename T, size_t Capacity>
  bool lockfree_spsc_bounded<T, Capacity>::try_pop(T &value)
  {
    size_t head_load = head.load(std::memory_order_relaxed);
    if (tail_cache == head_load)
    {
      tail_cache = tail.load(std::memory_order_acquire);
      if (tail_cache == head_load)
      {
        return false;
      }
    }

    value = std::move(arr[head_load]);
    head_load = (head_load + 1) % capacity;
    head.store(head_load, std::memory_order_release);
    return true;
  }

  template <typename T, size_t Capacity>
  void lockfree_spsc_bounded<T, Capacity>::wait_and_pop(T &value)
  {
    size_t head_load = head.load(std::memory_order_relaxed);
    while (head_load == tail_cache)
    {
      tail_cache = tail.load(std::memory_order_acquire); // busy wait
    }

    value = std::move(arr[head_load]);
    head_load = (head_load + 1) % capacity;
    head.store(head_load, std::memory_order_release);
  }

  template <typename T, size_t Capacity>
  bool lockfree_spsc_bounded<T, Capacity>::peek(T &value)
  {
    size_t head_load = head.load(std::memory_order_relaxed);
    if (head_load == tail_cache)
    {
      tail_cache = tail.load(std::memory_order_acquire);
      if (head_load == tail_cache)
      {
        return false;
      }
    }
    value = arr[head_load];
    return true;
  }

  template <typename T, size_t Capacity>
  bool lockfree_spsc_bounded<T, Capacity>::empty() const
  {
    return head.load(std::memory_order_relaxed) ==
           tail.load(std::memory_order_relaxed);
    // since queue is very frequently modified
  }

  template <typename T, size_t Capacity>
  size_t lockfree_spsc_bounded<T, Capacity>::size() const
  {
    return (tail.load(std::memory_order_relaxed) -
            head.load(std::memory_order_relaxed) +
            capacity) %
           capacity;
    // again, since size is very frequently changing.
  }
} // namespace tsfqueue::impl

#endif