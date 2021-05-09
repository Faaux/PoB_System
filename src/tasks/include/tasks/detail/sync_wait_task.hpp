///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_DETAIL_SYNC_WAIT_TASK_HPP_INCLUDED
#define CPPCORO_DETAIL_SYNC_WAIT_TASK_HPP_INCLUDED

#include <cassert>
#include <coroutine>
#include <exception>
#include <tasks/detail/lightweight_manual_reset_event.hpp>
#include <utility>

namespace cppcoro
{
namespace detail
{
struct any
{
    template <typename T>
    any(T&&) noexcept
    {
    }
};

template <typename T>
struct is_coroutine_handle : std::false_type
{
};

template <typename PROMISE>
struct is_coroutine_handle<std::coroutine_handle<PROMISE>> : std::true_type
{
};

// NOTE: We're accepting a return value of coroutine_handle<P> here
// which is an extension supported by Clang which is not yet part of
// the C++ coroutines TS.
template <typename T>
struct is_valid_await_suspend_return_value
    : std::disjunction<std::is_void<T>, std::is_same<T, bool>, is_coroutine_handle<T>>
{
};

template <typename T, typename = std::void_t<>>
struct is_awaiter : std::false_type
{
};

// NOTE: We're testing whether await_suspend() will be callable using an
// arbitrary coroutine_handle here by checking if it supports being passed
// a coroutine_handle<void>. This may result in a false-result for some
// types which are only awaitable within a certain context.
template <typename T>
struct is_awaiter<T, std::void_t<decltype(std::declval<T>().await_ready()),
                                 decltype(std::declval<T>().await_suspend(std::declval<std::coroutine_handle<>>())),
                                 decltype(std::declval<T>().await_resume())>>
    : std::conjunction<std::is_constructible<bool, decltype(std::declval<T>().await_ready())>,
                       detail::is_valid_await_suspend_return_value<decltype(std::declval<T>().await_suspend(
                           std::declval<std::coroutine_handle<>>()))>>
{
};

template <typename T>
auto get_awaiter_impl(T&& value, int) noexcept(noexcept(static_cast<T&&>(value).operator co_await()))
    -> decltype(static_cast<T&&>(value).operator co_await())
{
    return static_cast<T&&>(value).operator co_await();
}

template <typename T>
auto get_awaiter_impl(T&& value, long) noexcept(noexcept(operator co_await(static_cast<T&&>(value))))
    -> decltype(operator co_await(static_cast<T&&>(value)))
{
    return operator co_await(static_cast<T&&>(value));
}

template <typename T, std::enable_if_t<cppcoro::detail::is_awaiter<T&&>::value, int> = 0>
T&& get_awaiter_impl(T&& value, cppcoro::detail::any) noexcept
{
    return static_cast<T&&>(value);
}

template <typename T>
auto get_awaiter(T&& value) noexcept(noexcept(detail::get_awaiter_impl(static_cast<T&&>(value), 123)))
    -> decltype(detail::get_awaiter_impl(static_cast<T&&>(value), 123))
{
    return detail::get_awaiter_impl(static_cast<T&&>(value), 123);
}
}  // namespace detail

template <typename T, typename = void>
struct awaitable_traits
{
};

template <typename T>
struct awaitable_traits<T, std::void_t<decltype(cppcoro::detail::get_awaiter(std::declval<T>()))>>
{
    using awaiter_t = decltype(cppcoro::detail::get_awaiter(std::declval<T>()));

    using await_result_t = decltype(std::declval<awaiter_t>().await_resume());
};

namespace detail
{
template <typename RESULT>
class sync_wait_task;

template <typename RESULT>
class sync_wait_task_promise final
{
    using coroutine_handle_t = std::coroutine_handle<sync_wait_task_promise<RESULT>>;

   public:
    using reference = RESULT&&;

    sync_wait_task_promise() noexcept {}

    void start(detail::lightweight_manual_reset_event& event)
    {
        m_event = &event;
        coroutine_handle_t::from_promise(*this).resume();
    }

    auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        class completion_notifier
        {
           public:
            bool await_ready() const noexcept { return false; }

            void await_suspend(coroutine_handle_t coroutine) const noexcept { coroutine.promise().m_event->set(); }

            void await_resume() noexcept {}
        };

        return completion_notifier{};
    }

    auto yield_value(reference result) noexcept
    {
        m_result = std::addressof(result);
        return final_suspend();
    }

    void return_void() noexcept
    {
        // The coroutine should have either yielded a value or thrown
        // an exception in which case it should have bypassed return_void().
        assert(false);
    }

    void unhandled_exception() { m_exception = std::current_exception(); }

    reference result()
    {
        if (m_exception)
        {
            std::rethrow_exception(m_exception);
        }

        return static_cast<reference>(*m_result);
    }

   private:
    detail::lightweight_manual_reset_event* m_event;
    std::remove_reference_t<RESULT>* m_result;
    std::exception_ptr m_exception;
};

template <>
class sync_wait_task_promise<void>
{
    using coroutine_handle_t = std::coroutine_handle<sync_wait_task_promise<void>>;

   public:
    sync_wait_task_promise() noexcept {}

    void start(detail::lightweight_manual_reset_event& event)
    {
        m_event = &event;
        coroutine_handle_t::from_promise(*this).resume();
    }

    auto get_return_object() noexcept { return coroutine_handle_t::from_promise(*this); }

    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept
    {
        class completion_notifier
        {
           public:
            bool await_ready() const noexcept { return false; }

            void await_suspend(coroutine_handle_t coroutine) const noexcept { coroutine.promise().m_event->set(); }

            void await_resume() noexcept {}
        };

        return completion_notifier{};
    }

    void return_void() {}

    void unhandled_exception() { m_exception = std::current_exception(); }

    void result()
    {
        if (m_exception)
        {
            std::rethrow_exception(m_exception);
        }
    }

   private:
    detail::lightweight_manual_reset_event* m_event;
    std::exception_ptr m_exception;
};

template <typename RESULT>
class sync_wait_task final
{
   public:
    using promise_type = sync_wait_task_promise<RESULT>;

    using coroutine_handle_t = std::coroutine_handle<promise_type>;

    sync_wait_task(coroutine_handle_t coroutine) noexcept : m_coroutine(coroutine) {}

    sync_wait_task(sync_wait_task&& other) noexcept
        : m_coroutine(std::exchange(other.m_coroutine, coroutine_handle_t{}))
    {
    }

    ~sync_wait_task()
    {
        if (m_coroutine)
            m_coroutine.destroy();
    }

    sync_wait_task(const sync_wait_task&) = delete;
    sync_wait_task& operator=(const sync_wait_task&) = delete;

    void start(lightweight_manual_reset_event& event) noexcept { m_coroutine.promise().start(event); }

    decltype(auto) result() { return m_coroutine.promise().result(); }

   private:
    coroutine_handle_t m_coroutine;
};

template <typename AWAITABLE, typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
          std::enable_if_t<!std::is_void_v<RESULT>, int> = 0>
sync_wait_task<RESULT> make_sync_wait_task(AWAITABLE&& awaitable)
{
    co_yield co_await std::forward<AWAITABLE>(awaitable);
}

template <typename AWAITABLE, typename RESULT = typename cppcoro::awaitable_traits<AWAITABLE&&>::await_result_t,
          std::enable_if_t<std::is_void_v<RESULT>, int> = 0>
sync_wait_task<void> make_sync_wait_task(AWAITABLE&& awaitable)
{
    co_await std::forward<AWAITABLE>(awaitable);
}
}  // namespace detail
}  // namespace cppcoro

#endif