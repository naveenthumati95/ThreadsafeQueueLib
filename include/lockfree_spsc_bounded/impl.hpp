#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"
#include <utility>

template <typename T, size_t Capacity>
using queue = tsfqueue::__impl::lockfree_spsc_bounded<T, Capacity>;

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_push(T value)
{
  size_t next_tail = tail_cache + 1;
  if (next_tail == capacity)
  {
    next_tail = 0;
  }

  static thread_local int spin_threshold = 100;
  const int min_spin = 10, max_spin = 1000;
  int spin = 0;
  bool spun_success = false;
  bool done = false;
  while (true)
  {
    if (next_tail == head_cache)
    {
      done = true;
      head_cache = head.load(std::memory_order_acquire);
      if (next_tail == head_cache)
      {
        if (spin < spin_threshold)
        {
          // Busy-wait
        }
        else if (spin < spin_threshold + 100)
        {
          std::this_thread::yield();
        }
        else
        {
          head.wait(head_cache, std::memory_order_acquire);
        }
        spin++;
        continue;
      }
    }

    // Refresh head_cache before was_empty calculation to ensure correctness
    if (!done)
    {
      head_cache = head.load(std::memory_order_acquire);
    }
    spun_success = (spin < spin_threshold);
    bool was_empty = (head_cache == tail_cache);
    arr[tail_cache] = std::move(value);
    tail_cache = next_tail;
    tail.store(tail_cache, std::memory_order_release);

    if (was_empty)
    {
      tail.notify_one();
    }
    break;
  }

  // Adapt spin threshold for next time
  int delta = std::max(1, spin / 10);
  if (spun_success)
  {
    spin_threshold = std::min(spin_threshold + delta, max_spin);
  }
  else
  {
    spin_threshold = std::max(spin_threshold - delta, min_spin);
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
  bool was_full = (tail_cache + 1) % capacity == head_cache;
  value = arr[head_cache];
  head_cache = (head_cache + 1) % capacity;
  head.store(head_cache, std::memory_order_release);
  if (was_full)
  {
    head.notify_one();
  }
  return true;
}

template <typename T, size_t Capacity>
void queue<T, Capacity>::wait_and_pop(T &value)
{
  static thread_local int spin_threshold = 100;
  const int min_spin = 10, max_spin = 1000;
  int spin = 0;
  bool spun_success = false;
  bool done = false;
  while (true)
  {
    if (head_cache == tail_cache)
    {
      done = true;
      tail_cache = tail.load(std::memory_order_acquire);
      if (head_cache == tail_cache)
      {
        if (spin < spin_threshold)
        {
          // Busy-wait
        }
        else if (spin < spin_threshold + 100)
        {
          std::this_thread::yield();
        }
        else
        {
          tail.wait(tail_cache, std::memory_order_acquire);
        }
        ++spin;
        continue;
      }
    }

    // Refresh tail_cache before was_full calculation to ensure correctness
    if (!done)
    {
      tail_cache = tail.load(std::memory_order_acquire);
    }
    spun_success = (spin < spin_threshold);
    size_t next_tail = tail_cache + 1;
    if (next_tail == capacity)
    {
      next_tail = 0;
    }
    bool was_full = (next_tail == head_cache);
    value = arr[head_cache];
    head_cache = head_cache + 1;
    if (head_cache == capacity)
    {
      head_cache = 0;
    }
    head.store(head_cache, std::memory_order_release);
    if (was_full)
    {
      head.notify_one();
    }
    break;
  }

  // Adapt spin threshold for next time
  int delta = std::max(1, spin / 10);
  if (spun_success)
  {
    spin_threshold = std::min(spin_threshold + delta, max_spin);
  }
  else
  {
    spin_threshold = std::max(spin_threshold - delta, min_spin);
  }
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::peek(T &value)
{
  if (head_cache == tail_cache)
  {
    tail_cache = tail.load(std::memory_order_acquire);
    if (tail_cache == head_cache) // empty
      return false;
  }
  value = arr[head_cache];
  return true;
}

template <typename T, size_t Capacity>
bool queue<T, Capacity>::empty()
{
  return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
}

template <typename T, size_t Capacity>
template <typename... Args>
bool queue<T, Capacity>::emplace_back(Args &&...args)
{
  if ((tail_cache + 1) % capacity == head_cache)
  {
    head_cache = head.load(std::memory_order_acquire); // refresh cache
    if ((tail_cache + 1) % capacity == head_cache)     // full
    {
      return false;
    }
  }
  bool was_empty = (head_cache == tail_cache);
  arr[tail_cache] = T(std::forward<Args>(args)...);
  tail_cache = (tail_cache + 1) % capacity;
  tail.store(tail_cache, std::memory_order_release);
  if (was_empty)
  {
    tail.notify_one();
  }
  return true;
}

template <typename T, size_t Capacity>
size_t queue<T, Capacity>::size()
{
  size_t t = tail.load(std::memory_order_acquire);
  size_t h = head.load(std::memory_order_acquire);
  return (t - h + capacity) % capacity;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??