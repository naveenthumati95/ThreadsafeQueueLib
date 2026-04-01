#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T>
using node = tsfqueue::__utils::Node<T>;

template <typename T> void queue<T>::push(T value) {

  static_assert(std::is_copy_constructible_<T> ||
                    std::is_move_constructible_v<T>,
                "T must be copyable or movable to be pushed into the queue.");

  // Create a new tail node.
  std::unique_ptr<node> new_tail_unique_ptr = std::make_unique<node>();

  // Created the shared pointer of "value" [We use std::move here because its
  // efficient for bulky 'T']
  std::shared_ptr<T> shared_ptr_for_value =
      std::make_shared<T>(std::move(value));

  // Block Producer Thread. [Lock mutex only when needed, so we first do the
  // above two operations]
  std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

  tail->data = std::move(shared_ptr_for_value);
  tail->next = std::move(new_tail_unique_ptr);

  // Now we move to tail to its next, which is actual tail
  tail = tail->next.get();

  {
    // Increment size
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    size_q++;
  } // added this scope because if we notify a thread before unlocking
    // size_mutex, in 
    //   the wait_and_pop() function we are checking empty() which requires size_mutex.

    // Notify any thread (if any) waiting in "wait_and_pop" to wake up and pop.
    cond.notify_one();

    return;
}


template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get() {
    //Locking the head mutex
    std::unique_lock<std::mutex> head_lock(head_mutex);

    //Waiting for the queue to not be empty
    cond.wait(head_lock, [this]{
        return !empty();
    });

    //Extracting the head node and updating it
    std::unique_ptr<node> old_head = std::move(head);
    head = std::move(old_head->next);

    //Updating the queue size
    {
        std::lock_guard<std::mutex> size_lock(size_mutex);
        size_q--;
    }

    return std::move(old_head);
}

template <typename T> 
std::unique_ptr<typename queue<T>::node> queue<T>::try_get() {
    std::lock_guard<std::mutex> guard_head_mutex(head_mutex);
    if (size() > 0){
        std::unique_ptr<node> removing_node = std::move(head);
        head = std::move(removing_node->next);

        std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
        size_q--;

        return std::move(removing_node);
    }
    
    return nullptr;
}

template<typename T>
size_t queue<T>::size(){
    std::lock_guard<std::mutex> lock_size(size_mutex);

    return size_q;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {

  static_assert(std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "T must be copy-assignable or move-assignable to be popped "
                "into a reference.");

  // Obtain the popped_node with the help of wait_and_get()
  std::unique_ptr<node> popped_node = wait_and_get();

  // Move the data into value
  value = *popped_node->data; // Removed std::move() from here due to dangling
                              // pointer issue with peek()
}

template <typename T> std::shared_ptr<T> queue<T>::wait_and_pop() {

    //Obtain the popped_node with the help of wait_and_get()
    std::unique_ptr<node> popped_node = wait_and_get();

    //Return the shared pointer of the data
    return popped_node->data;
}

template <typename T>
bool queue<T>::try_pop(T &value) {

  static_assert(std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "T must be copy-assignable or move-assignable to be popped "
                "into a reference.");

  std::unique_ptr<node> removed_node = try_get();
  if (removed_node == nullptr) {
    return false;
  } else {
    value = *(removed_node->data); // Removed std::move() from here due to
                                   // dangling pointer issue with peek()
    return true;
  }
}

template <typename T>
std::shared_ptr<T> queue<T>::try_pop() {
    std::unique_ptr<node> removed_node = try_get();
    if (removed_node == nullptr){
        return nullptr;
    }else{
        return removed_node->data;
    }
}

template <typename T> bool queue<T>::empty() {
    std::lock_guard<std::mutex> lock_size(size_mutex);
    
    return (size_q == 0);
}

template <typename T>
template <typename... Args>
void queue<T>::emplace_back(Args&&... args){
    // Create a new tail node.
    std::unique_ptr<node> new_tail_unique_ptr = std::make_unique<node>();

    // Emplace the data directly at the memory address of shared_ptr<T>. (Perfect forwarding)
    std::shared_ptr<T> shared_ptr_for_value = std::make_shared<T>(std::forward<Args>(args)...);

    // Get exclusive axcess [Do it aftere non-critical tasks]
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

    tail->data = std::move(shared_ptr_for_value);
    tail->next = std::move(new_tail_unique_ptr);

    // change the tail.
    tail = tail->next.get();

    // Increment size [Doing this at the end so that consumer thread does not interfer with this operation.]
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    size_q++;

    return;
}

template <typename T> bool queue<T>::unsafe_peek(T &value) {

  static_assert(std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "T must be copy-assignable or move-assignable to be peeked "
                "into a reference.");

  // Get exclusive axcess of head.
  std::lock_guard<std::mutex> guard_head_mutex(head_mutex);

  // Get current size.
  int csize;
  {
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    csize = size_q;
  }

  if (csize) {
    // We locked head and current size if >0 -> the size is going to be >0 till
    // we release the head_mutex
    value = std::copy(*(head->data));
    return 1;
  }
  return 0;
}

template <typename T> std::shared_ptr<T> queue<T>::unsafe_peek() {
  // Get exclusive axcess of head.
  std::lock_guard<std::mutex> guard_head_mutex(head_mutex);

  // Get current size.
  int csize;
  {
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    csize = size_q;
  }

  if (csize) {
    // We locked head and current size if >0 -> the size is going to be >0 till
    // we release the head_mutex
    std::shared_ptr<T> current_head = head->data;
    return current_head;
  }
  return nullptr;
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_for_and_get(std::chrono::milliseconds timeout)
{
    // Using unique_lock to lock and unlock on our will.
    std::unique_lock<std::mutex> lock_head(head_mutex);

    // Waiting at max until timeout ms of time. 
    if(!cond.wait_for(lock_head,timeout,[this](){
        bool flag=empty();
        return !flag;
    }))
    {
        return nullptr;
    }

    std::unique_ptr<node> new_head = std::move(head->next);
    std::unique_ptr<node> return_node = std::move(head);

    head = std::move(new_head);

    {
        std::lock_guard<std::mutex> lock_size(size_mutex);
        size_q--;
    }

    return std::move(return_node);
}

template <typename T>
std::shared_ptr<T> queue<T>::wait_for_and_pop(std::chrono::milliseconds timeout)
{
    std::unique_ptr<queue<T>::node> return_node = std::move(wait_for_and_get(timeout));
    if (return_node == nullptr)
    {
        return nullptr;
    }

    return return_node->data;
}

template <typename T>
bool queue<T>::wait_for_and_pop(T &value,std::chrono::milliseconds timeout)
{

  static_assert(std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>,
                "T must be copy-assignable or move-assignable to be popped "
                "into a reference.");

  std::unique_ptr<queue<T>::node> return_node =
      std::move(wait_for_and_get(timeout));
  if (return_node == nullptr) {
    return false;
  }

    value = *(return_node->data);

    return true;
}

template <typename T>
void queue<T>::clear()
{
    std::lock_guard<std::mutex> lock_head(head_mutex);
    std::lock_guard<std::mutex> lock_tail(tail_mutex);

    head = std::make_unique<node>();
    tail = head.get();

    std::lock_guard<std::mutex> lock_size(size_mutex);
    size_q = 0;

    return;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??
