#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T>
void queue<T>::push(T value)
{
    // Creating a new stub node.
    std::unique_ptr<node> stub = std::make_unique<node>();

    // Creating a std::shared_ptr to store the value.
    std::shared_ptr<T> val = std::make_shared<T>(std::move(value));

    std::lock_guard<std::mutex> lk_tail(tail_mutex);

    tail->data = std::move(val);

    // saving raw pointer before move
    node *new_tail = stub.get();
    tail->next = std::move(stub);

    tail = new_tail;
    size_q++;

    cond.notify_one();

    return;
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get()
{
    // Using unique_lock to lock and unlock on our will.
    std::unique_lock<std::mutex> lk_head(head_mutex);

    // Waiting until there is atleast one element in the queue.
    cond.wait(lk_head, [this]()
              {
        bool flag=empty();
        return !flag; });

    std::unique_ptr<node> temp = std::move(head->next);
    std::unique_ptr<node> ret = std::move(head);

    head = std::move(temp);
    size_q--;

    return std::move(ret);
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::try_get()
{

    std::lock_guard<std::mutex> lk_head(head_mutex);

    if (empty())
    {
        return nullptr;
    }

    std::unique_ptr<node> temp = std::move(head->next);
    std::unique_ptr<node> ret = std::move(head);

    head = std::move(temp);
    size_q--;

    return std::move(ret);
}

template <typename T>
void queue<T>::wait_and_pop(T &value)
{
    std::unique_ptr<queue<T>::node> ret = std::move(wait_and_get());
    value = *(ret->data);
    return;
}

template <typename T>
std::shared_ptr<T> queue<T>::wait_and_pop()
{
    std::unique_ptr<queue<T>::node> ret = std::move(wait_and_get());
    if (ret == nullptr)
    {
        return nullptr;
    }
    return ret->data;
}

template <typename T>
bool queue<T>::try_pop(T &value)
{
    std::unique_ptr<queue<T>::node> ret = std::move(try_get());
    if (ret == nullptr)
    {
        return false;
    }
    value = *(ret->data);
    return true;
}

template <typename T>
std::shared_ptr<T> queue<T>::try_pop()
{
    std::unique_ptr<queue<T>::node> ret = std::move(try_get());
    if (ret == nullptr)
    {
        return nullptr;
    }
    return ret->data;
}

template <typename T>
bool queue<T>::empty()
{
    return (size_q == 0);
}

template <typename T>
int64_t queue<T>::size()
{
    return size_q;
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??