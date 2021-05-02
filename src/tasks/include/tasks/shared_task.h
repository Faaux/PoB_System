#pragma once
#include <coroutine>
#include <iostream>
#include <semaphore>
#include <thread>

namespace cb
{
namespace detail
{
struct shared_task_waiter
{
    std::coroutine_handle<> continuation_;
    shared_task_waiter* next_ = nullptr;
};

template <typename P>
struct shared_final_awaitable
{
    bool await_ready() const noexcept { return false; }

    void await_resume() const noexcept {}

    void await_suspend(std::coroutine_handle<P> h) const noexcept
    {
        auto& promise = h.promise();

        // Exchange operation needs to be 'release' so that subsequent awaiters have
        // visibility of the result. Also needs to be 'acquire' so we have visibility
        // of writes to the waiters list.
        void* const value_ready_value = &promise;
        void* waiters = promise.waiters_.exchange(value_ready_value, std::memory_order_acq_rel);
        if (waiters != nullptr)
        {
            shared_task_waiter* waiter = static_cast<shared_task_waiter*>(waiters);
            while (waiter->next_ != nullptr)
            {
                // Read the m_next pointer before resuming the coroutine
                // since resuming the coroutine may destroy the shared_task_waiter value.
                auto* next = waiter->next_;
                waiter->continuation_.resume();
                waiter = next;
            }

            // Resume last waiter in tail position to allow it to potentially
            // be compiled as a tail-call.
            waiter->continuation_.resume();
        }
    }
};

struct shared_promise_base
{
    std::suspend_never initial_suspend() { return {}; }

    void unhandled_exception() {}

    bool is_ready() const noexcept
    {
        const void* const value_ready_value = this;
        return waiters_.load(std::memory_order_acquire) == value_ready_value;
    }

    void add_ref() noexcept { ref_count_.fetch_add(1, std::memory_order_relaxed); }

    /// Decrement the reference count.
    ///
    /// \return
    /// true if successfully detached, false if this was the last
    /// reference to the coroutine, in which case the caller must
    /// call destroy() on the coroutine handle.
    bool try_detach() noexcept { return ref_count_.fetch_sub(1, std::memory_order_acq_rel) != 1; }

    /// Try to enqueue a waiter to the list of waiters.
    ///
    /// \param waiter
    /// Pointer to the state from the waiter object.
    /// Must have waiter->m_coroutine member populated with the coroutine
    /// handle of the awaiting coroutine.
    ///
    /// \param coroutine
    /// Coroutine handle for this promise object.
    ///
    /// \return
    /// true if the waiter was successfully queued, in which case
    /// waiter->m_coroutine will be resumed when the task completes.
    /// false if the coroutine was already completed and the awaiting
    /// coroutine can continue without suspending.
    bool try_await(shared_task_waiter* waiter, std::coroutine_handle<> coroutine)
    {
        void* const valueReadyValue = this;
        void* const notStartedValue = &this->waiters_;
        constexpr void* startedNoWaitersValue = static_cast<shared_task_waiter*>(nullptr);

        // NOTE: If the coroutine is not yet started then the first waiter
        // will start the coroutine before enqueuing itself up to the list
        // of suspended waiters waiting for completion. We split this into
        // two steps to allow the first awaiter to return without suspending.
        // This avoids recursively resuming the first waiter inside the call to
        // coroutine.resume() in the case that the coroutine completes
        // synchronously, which could otherwise lead to stack-overflow if
        // the awaiting coroutine awaited many synchronously-completing
        // tasks in a row.

        // Start the coroutine if not already started.
        void* oldWaiters = waiters_.load(std::memory_order_acquire);
        if (oldWaiters == notStartedValue &&
            waiters_.compare_exchange_strong(oldWaiters, startedNoWaitersValue, std::memory_order_relaxed))
        {
            // Start the task executing.
            coroutine.resume();
            oldWaiters = waiters_.load(std::memory_order_acquire);
        }

        // Enqueue the waiter into the list of waiting coroutines.
        do
        {
            if (oldWaiters == valueReadyValue)
            {
                // Coroutine already completed, don't suspend.
                return false;
            }

            waiter->next_ = static_cast<shared_task_waiter*>(oldWaiters);
        } while (!waiters_.compare_exchange_weak(oldWaiters, static_cast<void*>(waiter), std::memory_order_release,
                                                 std::memory_order_acquire));

        return true;
    }

    std::atomic<std::uint32_t> ref_count_ = 1;

    // Value is either
    // - nullptr          - indicates started, no waiters
    // - this             - indicates value is ready
    // - &this->m_waiters - indicates coroutine not started
    // - other            - pointer to head item in linked-list of waiters.
    //                      values are of type 'cppcoro::shared_task_waiter'.
    //                      indicates that the coroutine has been started.
    std::atomic<void*> waiters_;
};

template <typename Task, typename T>
struct shared_promise : shared_promise_base
{
    ~shared_promise()
    {
        if (this->is_ready())
        {
            reinterpret_cast<T*>(&m_valueStorage)->~T();
        }
    }

    Task get_return_object() { return std::coroutine_handle<shared_promise>::from_promise(*this); }

    shared_final_awaitable<shared_promise> final_suspend() noexcept { return {}; }

    template <typename VALUE, typename = std::enable_if_t<std::is_convertible_v<VALUE&&, T> > >
    void return_value(VALUE&& value) noexcept(std::is_nothrow_constructible_v<T, VALUE&&>)
    {
        new (&m_valueStorage) T(std::forward<VALUE>(value));
    }

    T& result() & { return *reinterpret_cast<T*>(&m_valueStorage); }

   private:
    alignas(T) char m_valueStorage[sizeof(T)];
};

template <typename Task>
struct shared_promise<Task, void> : shared_promise_base
{
    Task get_return_object() { return std::coroutine_handle<shared_promise>::from_promise(*this); }

    shared_final_awaitable<shared_promise> final_suspend() noexcept { return {}; }

    void return_void() {}
};

}  // namespace detail

template <typename T = void>
class [[nodiscard]] shared_task
{
   public:
    using promise_type = detail::shared_promise<shared_task, T>;

    shared_task() noexcept = default;
    ~shared_task() { destroy(); }

    shared_task(std::coroutine_handle<promise_type> coroutine) noexcept : coroutine_{coroutine} {}

    shared_task(shared_task&& other) noexcept : coroutine_{other.coroutine_} { other.coroutine_ = nullptr; }

    shared_task& operator=(shared_task&& other) noexcept
    {
        if (this != &other)
        {
            destroy();

            coroutine_ = other.coroutine_;
            other.coroutine_ = nullptr;
        }
        return *this;
    }

    shared_task(const shared_task& other) noexcept : coroutine_(other.coroutine_)
    {
        if (coroutine_)
        {
            coroutine_.promise().add_ref();
        }
    }

    shared_task& operator=(const shared_task& other) noexcept
    {
        if (coroutine_ != other.coroutine_)
        {
            destroy();

            coroutine_ = other.coroutine_;

            if (coroutine_)
            {
                coroutine_.promise().add_ref();
            }
        }

        return *this;
    }

    [[nodiscard]] bool await_ready() const noexcept { return !coroutine_ || coroutine_.promise().is_ready(); }

    bool await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        waiter_.continuation_ = continuation;
        return coroutine_.promise().try_await(&waiter_, coroutine_);
    }

    decltype(auto) await_resume() const noexcept
    {
        if constexpr (std::is_same_v<T, void>)
        {
            return;
        }
        else
        {
            return coroutine_.promise().result();
        }
    }

   protected:
    std::coroutine_handle<promise_type> coroutine_ = nullptr;
    detail::shared_task_waiter waiter_;

   private:
    void destroy() noexcept
    {
        if (coroutine_)
        {
            if (!coroutine_.promise().try_detach())
            {
                coroutine_.destroy();
            }
        }
    }
};

}  // namespace cb