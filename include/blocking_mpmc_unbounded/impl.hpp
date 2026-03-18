#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T>
using node = tsfqueue::__utils::Node<T>;

template <typename T> void queue<T>::push(T value) {
    // Create a new tail node.
    std::unique_ptr<node> new_tail_unique_ptr = std::make_unique<node>();

    // Created the shared pointer of "value" [We use std::move here because its efficient for bulky 'T']
    std::shared_ptr<T> shared_ptr_for_value = std::make_shared<T>(std::move(value));

    // Block Producer Thread. [Lock mutex only when needed, so we first do the above two operations]
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

    tail->data = std::move(shared_ptr_for_value);
    tail->next = std::move(new_tail_unique_ptr);

    // Now we move to tail to its next, which is actual tail
    tail = tail->next.get();;

    // Increment size
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    size_q++;

    // Notify any thread (if any) waiting in "wait_and_pop" to wake up and pop.
    cond.notify_one();
}

template <typename T> queue<T>::node *queue<T>::get_tail() {}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get() {}

template <typename T> std::unique_ptr<typename queue<T>::node> queue<T>::try_get() {}

template <typename T> void queue<T>::wait_and_pop(T &value) {}

template <typename T> std::shared_ptr<T> queue<T>::wait_and_pop() {}

template <typename T>
bool queue<T>::try_pop(T &value) {
    std::unique_ptr<node> removed_node = try_get();
    if (removed_node == nullptr){
        return 0;
    }else{
        value = *(removed_node->data);
        return 1;
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

    // Get exclusive excess [Do it aftere non-critical tasks]
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

    tail->data = std::move(shared_ptr_for_value);
    tail->next = std::move(new_tail_unique_ptr);

    // change the tail.
    tail = tail->next.get();

    // Increment size [Doing this at the end so that consumer thread does not interfer with this operation.]
    std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
    size_q++;
}

template <typename T>
bool queue<T>::peek(T& value){
    // Get exclusive excess of head.
    std::lock_guard<std::mutex> guard_head_mutex(head_mutex);

    // Get current size.
    int csize;
    {
        std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
        csize = size_q;
    }
    
    if (csize){
        // We locked head and current size if >0 -> the size is going to be >0 till we release the head_mutex
        value = *(head->data);
        return 1;
    }
    return 0;
}

template <typename T>
std::shared_ptr<T> queue<T>::peek(){
    // Get exclusive excess of head.
    std::lock_guard<std::mutex> guard_head_mutex(head_mutex);

    // Get current size.
    int csize;
    {
        std::lock_guard<std::mutex> guard_size_mutex(size_mutex);
        csize = size_q;
    }
    
    if (csize){
        // We locked head and current size if >0 -> the size is going to be >0 till we release the head_mutex
        std::shared_ptr<T> current_head= head->data;
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
    std::unique_ptr<queue<T>::node> return_node = std::move(wait_for_and_get(timeout));
    if(return_node == nullptr)
    {
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
