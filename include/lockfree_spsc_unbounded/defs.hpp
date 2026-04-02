#ifndef LOCKFREE_SPSC_UNBOUNDED_DEFS
#define LOCKFREE_SPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <type_traits>

namespace tsfqueue::impl {
template <typename T> class lockfree_spsc_unbounded {
  // Works exactly same as the blocking_mpmc_unbounded queue (see this once)
  // with tail pointer pointing to stub node and your head pointer updates as
  // per the pushes. See the Lockless_Node in utils to understand the working.

  //--------------------------------------------------------------------------
  // Note that the next pointers are atomic there. Why ?? [Reason this]
  //
  // if head and tail point to same stub node ,producer is trying to modify next
  // ptr,consumer is trying to read
  // next ptr(to check if queue is empty)->race condition
  //--------------------------------------------------------------------------

  //------------------------------------------------------------------------------
  // Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
  // me for details)
  //
  // cpu update on common cache line,if producer makes some changes,it
  // invalidates other cores(consumers) cache,so this has to keep on updating
  // which is time consuming
  // cache aligning make the pointers sit on different cache lines,so now we can
  // spam modifications
  //--------------------------------------------------------------------------------

  // [Copy of blocking_mpmc_unbounded]
  // For the implementation, we start with a stub node and both head and tail
  // are initialized to it.

  // When we push, we make a new stub node, move the data
  //  into the current tail and then change the tail to the new stub.

  // We have two methods : wait_and_pop() which waits on the queue and returns
  // element &
  //  try_pop() which returns an element if queue is not empty otherwise returns
  //  some neutral element OR a false boolean whichever is applicable. Pop works
  //  by returning the data stored in head node and replacing head to its next
  //  node. We handle the empty queue gracefully as per the pop type.

private:
  using node = tsfqueue::utils::Lockless_Node<T>;

  // Add the private members :
  // 1. node* head;
  // 2. node* tail;

  alignas(tsfq::impl::cache_line_size) node *head;
  alignas(tsfq::impl::cache_line_size) node *tail;

  // Description of private members :
  // 1. node* head -> Pointer to the head node
  // 2. node* tail -> Pointer to tail node
  // 3. Cache align 1-2

  // capacity
  alignas(tsfq::impl::cache_line_size) std::atomic<size_t> capacity{0};

public:
  // Public member functions :

  // static asserts
  static_assert(std::is_default_constructible_v<T>,
                "T must be default constructible");
  static_assert(std::is_nothrow_destructible_v<T>,
                "T must be nothrow destructible");

  // Add relevant constructors and destructors -> Add these here only

  lockfree_spsc_unbounded() {

    head = new node();
    head->next.store(nullptr, std::memory_order_relaxed);

    tail = head;
  }

  ~lockfree_spsc_unbounded() {

    while (head != nullptr) {
      node *current = head;
      head = head->next.load(std::memory_order_relaxed);
      delete current;
    }
  }

  // Delete Copy and Move constructor
  // Copy constructor not required as its spsc,could also cause double free
  // crash move constructor can break the queue if thread is running and we move
  // the queue somewhere else

  lockfree_spsc_unbounded(const lockfree_spsc_unbounded &) = delete;
  lockfree_spsc_unbounded &operator=(const lockfree_spsc_unbounded &) = delete;

  lockfree_spsc_unbounded(lockfree_spsc_unbounded &&) = delete;
  lockfree_spsc_unbounded &operator=(lockfree_spsc_unbounded &&) = delete;

  // 1. void push(value) : Pushes the value inside the queue, copies the value
  void push(T);

  // 2. void wait_and_pop(value ref) : Blocking wait on queue, returns value in
  // the reference passed as parameter
  void wait_and_pop(T &);

  // 3. bool try_pop(value ref) : Returns true and
  // gives the value in reference passed, false otherwise
  bool try_pop(T &);

  // 4. bool empty() : Returns
  // whether the queue is empty or not at that instant
  bool empty();

  // 5. bool peek(value ref) : Returns the front/top element of queue in ref
  // (false if empty queue)
  bool unsafe_peek(T &);

  // 6. Add static asserts

  // 7. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  template <typename... Args> void emplace_back(Args &&...args);

  // 8. Add size() function
  size_t size();

  // 9. Any more suggestions ??

  // 10. Why no shared_ptr ?? [Reason this]
  //
};
} // namespace tsfqueue::impl

#endif