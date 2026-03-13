#ifndef LOCKFREE_SPSC_UNBOUNDED_DEFS
#define LOCKFREE_SPSC_UNBOUNDED_DEFS

#include "../utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace tsfqueue::__impl {
template <typename T> class lockfree_spsc_unbounded {
  // Works exactly same as the blocking_mpmc_unbounded queue (see this once)
  // with tail pointer pointing to stub node and your head pointer updates as
  // per the pushes. See the Lockless_Node in utils to understand the working.

  // Note that the next pointers are atomic there. Why ?? 
  //[Reason this -> 
  //   Because head read it and tail can write it at same time.
  //   
  //   when the queue is empty, the head thread continously reads the next ptr to check for any item.
  //   assume, when tail thread is adding an element and as address is 64 bit, it first write the upper 20bits, 
  //   At exactly this time head reads the next ptr,
  //   it gets a wrong value of new address.
  //        ]

  // Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
  // me for details)

  // [Copy of blocking_mpmc_unbounded]
  // For the implementation, we start with a stub node and both head and tail
  // are initialized to it. When we push, we make a new stub node, move the data
  // into the current tail and then change the tail to the new stub. We have two
  // methods : wait_and_pop() which waits on the queue and returns element &
  // try_pop() which returns an element if queue is not empty otherwise returns
  // some neutral element OR a false boolean whichever is applicable. Pop works
  // by returning the data stored in head node and replacing head to its next
  // node. We handle the empty queue gracefully as per the pop type.
private:
  using node = tsfqueue::__utils::Lockless_Node<T>;

  // Add the private members :
  node* head;
  node* tail;

  // Description of priavte members :
  // 1. node* head -> Pointer to the head node
  // 2. node* tail -> Pointer to tail node
  // 3. Cache align 1-2

public:
  // Public member functions :
  // Add relevant constructors and destructors -> Add these here only
  lockfree_spsc_unbounded(){
    head = new node();
    head->next.store(nullptr);
    tail = head;
  }

  void push(T);
  void wait_and_pop(T&);
  bool try_pop(T&);
  bool empty();
  bool peek(T&);

  template<typename... Args>
  void emplace_back(Args&&... args);

  size_t size();

  // 1. void push(value) : Pushes the value inside the queue, copies the value
  // 2. void wait_and_pop(value ref) : Blocking wait on queue, returns value in
  // the reference passed as parameter
  // 3. bool try_pop(value ref) : Returns true and
  // gives the value in reference passed, false otherwise
  // 4. bool empty() : Returns
  // whether the queue is empty or not at that instant
  // 5. bool peek(value ref) : Returns the front/top element of queue in ref (false if empty queue)

  // 6. Add static asserts [?]
  
  // 7. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  // 8. Add size() function
  // 9. Any more suggestions ??
  // 10. Why no shared_ptr ?? [Reason this -> 
  //      Lockless_Node<T> is not pointed by a smart pointer, so its not deleted automatically, when we delete it value(type T) is deleted and atomic next ptr. is also deleted....
  //      if we have used shared_pointer<T> the location it points got freed by some external agent, this will break its destructor.
  //     ]
};
} // namespace tsfqueue::__impl

#endif