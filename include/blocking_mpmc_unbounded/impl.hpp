#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {
    {
        // Creating a std::unique_ptr for the new node.
        std::unique_ptr<node> new_node = std::make_unique<node>();

        // Creating a std::shared_ptr to store the value.
        std::shared_ptr<T> val = std::make_shared<T>(value);
        new_node->data = val;

        // Getting raw pointer of the new_node coz it is the new tail.
        node* new_tail = new_node.get();

        // Assigning the next pointer of the new_node to nullptr(unique).
        new_node->next = nullptr;

        // Now we are updating the tail pointer, so we should lock the tail_mutex
        std::lock_guard<std::mutex> lk_tail(tail_mutex);

        // std::move() should be used because unique_ptr can not be copied
        tail->next = std::move(new_node);
        tail = new_tail;
        size_q++;
    } // created this scope because notifying while holding tail_mutex lock
    //   will cause issue for the consumer (he may try to lock tail but it is
    //   already locked.)
    
    cond.notify_one();

    return;
}

template <typename T> queue<T>::node *queue<T>::get_tail() {
    std::lock_guard<std::mutex> lk_tail(tail_mutex);
    return tail;
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get() {
    // Using unique_lock to lock and unlock on our will.
    std::unique_lock<std::mutex> lk_head(head_mutex);

    // Waiting until there is atleast one element in the queue.
    cond.wait(lk_head,[](){
        bool flag=empty();
        if(flag)
        {
            std::cout<<"Waiting for an element to be pushed."<<std::endl;
        }
        return !flag;
    });

    if(empty())
    {
        return nullptr;
    }

    // Checking if there is only one element in the queue,
    // because we should modify tail if there is only one element.
    if(head->next->next == nullptr)
    {
        std::lock_guard<std::mutex> lk_tail(tail_mutex);
        tail = head.get();
    }

    std::unique_ptr<node> temp = std::move(head->next->next);

    // retaining the unique pointer of the element that is to be popped.
    std::unique_ptr<node> ret = std::move(head->next);
    head->next = std::move(temp);

    return ret;
}

template <typename T> std::unique_ptr<typename queue<T>::node> queue<T>::try_get() {
    std::lock_guard<std::mutex> lk_head(head_mutex);

    // returning nullptr if the queue is empty.
    if(empty())
    {
        return nullptr;
    }

    // Checking if there is only one element in the queue,
    // because we should modify tail if there is only one element.
    if(head->next->next == nullptr)
    {
        std::lock_guard<std::mutex> lk_tail(tail_mutex);
        tail = head.get();
    }

    std::unique_ptr<node> temp = std::move(head->next->next);

    // retaining the unique pointer of the element that is to be popped.
    std::unique_ptr<node> ret = std::move(head->next);
    head->next = std::move(temp);

    return ret;
}

template <typename T> void queue<T>::wait_and_pop(T &value) {
    std::unique_ptr<queue<T>::node> ret = std::move(wait_and_get());
    if(ret == nullptr)
    {
        return;
    }
    value = *(ret->data);
    return;
}

template <typename T> std::shared_ptr<T> queue<T>::wait_and_pop() {
    std::unique_ptr<queue<T>::node> ret = std::move(wait_and_get());
    if(ret == nullptr)
    {
        return nullptr;
    }
    return ret->data;
}

template <typename T> bool queue<T>::try_pop(T &value) {
    std::unique_ptr<queue<T>::node> ret = std::move(try_get());
    if(ret == nullptr)
    {
        return false;
    }
    value = *(ret->data);
    return true;
}

template <typename T> std::shared_ptr<T> queue<T>::try_pop() {
    std::unique_ptr<queue<T>::node> ret = std::move(try_get());
    if(ret == nullptr)
    {
        return nullptr;
    }
    return ret->data;
}

template <typename T> bool queue<T>::empty() {
    return (size_q==0);
}

template <typename T> int queue<T>::size() {
    return size_q;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??