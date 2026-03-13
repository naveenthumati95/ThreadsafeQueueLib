#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T>
using node = tsfqueue::__utils::Node<T>;

template <typename T> void queue<T>::push(T value) {
    // Block Producer Thread.
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

    // Create a new tail node.
    tail->next = std::make_unique<node>();

    // Created the shared pointer of "value"
    tail->data = std::make_shared<T>(value);

    // Now we move to tail to its next, which is actual tail
    tail = tail->next.get();

    // Notify any thread (if any) waiting in "wait_and_pop" to wake up and pop.
    cond.notify_one();

    // Increment size
    csize.fetch_add(1);
}

template <typename T> node<T> * queue<T>::get_tail() {
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);
    return tail;
}

template <typename T>
std::unique_ptr< node<T> > queue<T>::wait_and_get() {
    std::unique_lock<std::mutex> head_unique_lock(head_mutex);
    cond.wait(head_unique_lock, [this](){
        return (csize.load() > 0);
    });
    std::unique_ptr<node> removing_node = std::move(head);
    head = std::move(removing_node->next);
    csize.fetch_sub(1);
    return removing_node;
}

template <typename T> 
std::unique_ptr< node<T> > queue<T>::try_get() {
    std::lock_guard<std::mutex> guard_head_mutex(head_mutex);
    if (size() > 0){
        std::unique_ptr<node> removing_node = std::move(head);
        head = std::move(removing_node->next);
        csize.fetch_sub(1);
        return removing_node;
    }
    return nullptr;
}

template <typename T> 
void queue<T>::wait_and_pop(T &value) {
    value = *(wait_and_get()->data);
}

template <typename T> 
std::shared_ptr<T> queue<T>::wait_and_pop() {
    return wait_and_get()->data;
}

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

template <typename T> 
bool queue<T>::empty() {
    return (size() == 0);
}

template <typename T>
template <typename... Args>
void queue<T>::emplace_back(Args&&... args){
    // Get exclusive excess
    std::lock_guard<std::mutex> guard_tail_mutex(tail_mutex);

    // Create a new tail node.
    tail->next = std::make_unique<node>();

    // Emplace the data directly at the memory address of shared_ptr<T>. (Perfect forwarding)
    tail->data = std::make_shared<T>(std::forward<Args>(args)...);

    // change the tail.
    tail = tail->next.get();

    // Increment size
    csize.fetch_add(1);
}

template <typename T>
size_t queue<T>::size(){
    return csize.load();
}

#endif

// 1. Add static asserts [?]
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then) [DONE]
// 3. Add size() function [DONE]
// 4. Any more suggestions ??