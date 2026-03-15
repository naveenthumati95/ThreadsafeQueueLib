    #ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
    #define LOCKFREE_SPSC_UNBOUNDED_IMPL

    #include "defs.hpp"


    template <typename T>
    using queue = tsfqueue::__impl::lockfree_spsc_unbounded<T>;

    template <typename T> void queue<T>::push(T value) {
        //std::move casts it as rvalue-no cost of copying
        emplace_back(std::move(value));
    }

    template <typename T> bool queue<T>::try_pop(T &value) {    
        node* new_head = head->next.load(std::memory_order_acquire);

        if(new_head == nullptr){
            return false;
        }

        node* old_head = head;
        value = std::move(head->data);
        head = new_head;
        
        delete old_head;

        capacity.fetch_sub(1,std::memory_order_relaxed);

        return true;
    }   

    template <typename T> void queue<T>::wait_and_pop(T &value) {
        
        head->next.wait(nullptr,std::memory_order_acquire);

        node* new_head = head->next.load(std::memory_order_relaxed);
        node* old_head = head;
        value = std::move(head->data);
        
        head = new_head;

        delete old_head;

        capacity.fetch_sub(1,std::memory_order_relaxed);
        
    }

    template <typename T> bool queue<T>::peek(T &value) {
        if(empty()){
            return false;
        }
        value = head -> data;
        return true;
    }

    template <typename T> bool queue<T>::empty(void) {
        return (head->next.load(std::memory_order_acquire)==nullptr);
    }

    template<typename T>
    template<typename... Args>
    void queue<T>::emplace_back(Args&&... args){
            node* stub = new node();
            stub->next.store(nullptr,std::memory_order_relaxed);

            tail->data = T(std::forward<Args>(args)...);
            capacity.fetch_add(1,std::memory_order_relaxed);
            tail->next.store(stub,std::memory_order_release);

            //wakes up one sleeping thread
            tail->next.notify_one();

            tail = stub;
    }

    template<typename T>
    size_t queue<T>::size(){
        return capacity.load(std::memory_order_relaxed);
    }

    #endif

    // 1. Add static asserts

    // 2. Add emplace_back using perfect forwarding and variadic templates (you
    // can use this in push then)

    // 3. Add size() function

    // 4. Any more suggestions ??