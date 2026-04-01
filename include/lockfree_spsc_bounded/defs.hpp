#ifndef LOCKFREE_SPSC_BOUNDED_DEFS
#define LOCKFREE_SPSC_BOUNDED_DEFS
#include "../utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>
namespace tsfqueue::__impl
{
  template <typename T, size_t Capacity>
  class lockfree_spsc_bounded
  {
    // For the implementation, we first take the size of the bounded queue from
    // user inside the templates so that we can do compile time memory allocation.
    // We have two atomic pointer, head and tail, tail for pushing the element and
    // head for popping. We also add check tail == head for empty which means one
    // redundant element during allocation. We keep head_cache and tail_cache as
    // cached copies to have a cache efficient code (discuss with me for details).
    // All the data members are cache aligned to prevent cache-line bouncing.
    // The user is provided with both set of functions : try_pop() and try_push()
    // for a wait-free code And wait_and_pop() and wait_and_push() for a lock-less
    // code but not wait-free variant. Thus, the user is given a choice to choose
    // among the preferred endpoints as per use case.
  private:
    // Add the private members :
    alignas(tsfq::__impl::cache_line_size) std::atomic<size_t> head;
    alignas(tsfq::__impl::cache_line_size) size_t tail_cache;
    alignas(tsfq::__impl::cache_line_size) std::atomic<size_t> tail;
    alignas(tsfq::__impl::cache_line_size) size_t head_cache;
    static constexpr size_t capacity = Capacity + 1;
    alignas(tsfq::__impl::cache_line_size) T arr[capacity];
    // aligned the start of the array too
    // Description of private members :
    // 1. std::atomic<size_t> head is the atomic head pointer
    // 2. std::atomic<size_t> tail is the atomic tail pointer
    // 3. size_t head_cache is the cached head pointer
    // 4. size_t tail_cache is the cached tail pointer
    // 5. T arr[] compile time allocated array
    // Cache align 1-5.
    // 6. static constexpr size_t capcity to store the capcity for operations in
    // functions Why static ?? Why constexpr ?? [Reason this]

    static_assert(Capacity > 0, "queue capacity must be greater than zero");
    
    static_assert(Capacity < std::numeric_limits<size_t>::max() - 1, "prevent overflow");
    
    static_assert(std::is_default_constructible<T>::value,  "type T must be default constructible for array allocation");
    
    static_assert(std::is_move_assignable<T>::value || std::is_copy_assignable<T>::value, "type T must be either move-assignable or copy-assignable");
    
    static_assert(std::is_destructible<T>::value, "type T must be destructible");
    
    static_assert(std::atomic<size_t>::is_always_lock_free, "must be lock-free");
    
    static_assert(alignof(std::atomic<size_t>) <= tsfq::__impl::cache_line_size, "cache line size is inefficient");

  public:
    // Public Member functions :
    // Add appropriate constructors and destructors -> Add here only
    lockfree_spsc_bounded() : head(0), tail(0), head_cache(0), tail_cache(0) {}
    ~lockfree_spsc_bounded() = default;
    // 1. void wait_and_push(value) : Busy wait until element is pushed
    void wait_and_push(T);
    // 2. bool try_push(value) : Try to push if not full else leave (returns false
    // if could not push else true)
    bool try_push(T);
    // 3. void wait_and_pop(value ref) : Busy wait until we have atmost 1 elt and
    void wait_and_pop(T &);
    // then pop it and store in reference
    // 4. bool try_pop(value ref) : Try to pop and return false if failed bool
    bool try_pop(T &);
    // 5. empty(void) : Checks if the queue is empty and return bool
    bool empty();
    // 6. bool peek(value ref) : Peek the top of the queue.
    bool peek(T &);
    template <typename... Args>
    bool emplace_back(Args &&...args);
    size_t size();
    // Will work only in SPSC/MPSC why ?? [Reason this]
    // 8. Add emplace_back using perfect forwarding and variadic templates (you
    // can use this in push then)
    // 9. Add size() function
    // 10. Any more suggestions ??
    // can make the functions peek , empty and size const
    // 11. Why no shared_ptr ?? [Reason this]
  };
} // namespace tsfqueue::__impl
#endif
