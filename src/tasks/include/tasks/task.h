#pragma once
#include <coroutine>
#include <iostream>
#include <semaphore>
#include <tasks/detail/lightweight_manual_reset_event.hpp>
#include <tasks/detail/sync_wait_task.hpp>
#include <thread>

namespace cb
{
namespace detail
{
template <typename P>
struct final_awaitable
{
    bool await_ready() const noexcept { return false; }

    void await_resume() const noexcept {}

    std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) const noexcept
    {
        if (h.promise().hasContinuation.exchange(true, std::memory_order_acquire))
        {
            return h.promise().continuation;
        }
        return std::noop_coroutine();
    }
};

struct promise_base
{
    std::suspend_never initial_suspend() { return {}; }

    void unhandled_exception() {}

    std::coroutine_handle<> continuation;
    std::atomic<bool> hasContinuation = false;
};

template <typename Task, typename T>
struct promise : promise_base
{
    Task get_return_object() { return std::coroutine_handle<promise>::from_promise(*this); }

    final_awaitable<promise> final_suspend() noexcept { return {}; }

    void return_value(T const& value) noexcept(std::is_nothrow_copy_assignable_v<T>)
    {
        data = value;
        printf("");
    }

    void return_value(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>)
    {
        data = std::move(value);
        printf("");
    }

    T& result() &
    {
        printf("");
        return data;
    }

    T&& result() &&
    {
        printf("");
        return std::move(data);
    }

   private:
    T data;
};

template <typename Task>
struct promise<Task, void> : promise_base
{
    Task get_return_object() { return std::coroutine_handle<promise>::from_promise(*this); }

    final_awaitable<promise> final_suspend() noexcept { return {}; }

    void return_void() {}

    void result() {}
};

template <typename Task, typename T>
struct promise<Task, T&> : promise_base
{
    Task get_return_object() { return std::coroutine_handle<promise>::from_promise(*this); }

    final_awaitable<promise> final_suspend() noexcept { return {}; }

    void return_value(T& value) noexcept(std::is_nothrow_move_assignable_v<T>) { data = std::addressof(value); }

    T& result() { return *data; }

   private:
    T* data;
};

}  // namespace detail

template <typename T = void>
class [[nodiscard]] task
{
   public:
    using promise_type = detail::promise<task, T>;

   private:
    struct awaitable_base
    {
        std::coroutine_handle<promise_type> coroutine_;

        awaitable_base(std::coroutine_handle<promise_type> coroutine) noexcept : coroutine_(coroutine) {}

        bool await_ready() const noexcept { return !coroutine_ || coroutine_.done(); }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
        {
            auto& basePromise = coroutine_.promise();
            basePromise.continuation = awaitingCoroutine;
            if (basePromise.hasContinuation.exchange(true, std::memory_order_release))
            {
                return awaitingCoroutine;
            }

            return std::noop_coroutine();
        }
    };

   public:
    task() noexcept = default;
    task(std::coroutine_handle<promise_type> coroutine) noexcept : coroutine_{coroutine} {}

    // Delete Copy constructor to avoid multiple people co_awaiting the same task
    task(task const&) = delete;
    task& operator=(task const&) = delete;

    task(task&& other) noexcept : coroutine_{other.coroutine_} { other.coroutine_ = nullptr; }

    task& operator=(task&& other) noexcept
    {
        if (this != &other)
        {
            if (coroutine_)
            {
                coroutine_.destroy();
            }
            coroutine_ = other.coroutine_;
            other.coroutine_ = nullptr;
        }
        return *this;
    }

    ~task()
    {
        if (coroutine_)
        {
            coroutine_.destroy();
        }
    }

    auto operator co_await() const& noexcept
    {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            decltype(auto) await_resume()
            {
                if (!this->coroutine_)
                {
                    assert(false);
                }

                return this->coroutine_.promise().result();
            }
        };

        return awaitable{coroutine_};
    }

    auto operator co_await() const&& noexcept
    {
        struct awaitable : awaitable_base
        {
            using awaitable_base::awaitable_base;

            decltype(auto) await_resume()
            {
                if (!this->coroutine_)
                {
                    assert(false);
                }

                return std::move(this->coroutine_.promise()).result();
            }
        };

        return awaitable{coroutine_};
    }

    [[nodiscard]] bool await_ready() const noexcept { return !coroutine_ || coroutine_.done(); }

   protected:
    std::coroutine_handle<promise_type> coroutine_ = nullptr;
};

template <typename AWAITABLE>
auto sync_wait(AWAITABLE&& awaitable) -> typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t
{
    auto t = cppcoro::detail::make_sync_wait_task(std::forward<AWAITABLE>(awaitable));
    cppcoro::detail::lightweight_manual_reset_event event;
    t.start(event);
    event.wait();
    return t.result();
}

}  // namespace cb