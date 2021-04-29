#pragma once
#include <coroutine>
#include <thread>
#include <semaphore>
#include <iostream>

namespace cb {
namespace detail {

template < typename P, bool Joinable >
struct final_awaitable
{
	bool await_ready() const noexcept
	{
		return false;
	}

	void await_resume() const noexcept
	{}

	std::coroutine_handle<> await_suspend( std::coroutine_handle< P > h ) const noexcept
	{
		if ( h.promise().hasContinuation.exchange( true, std::memory_order_acquire ) ) {
			return h.promise().continuation;
		}
		return std::noop_coroutine();
	}
};

template < typename P >
struct final_awaitable< P, true >
{
	bool await_ready() const noexcept
	{
		return false;
	}

	void await_resume() const noexcept
	{}

	void await_suspend( std::coroutine_handle< P > h ) const noexcept
	{
		h.promise().sem.release();
	}
};

template < bool Joinable >
struct promise_base
{
	std::suspend_never initial_suspend()
	{
		return {};
	}

	void unhandled_exception()
	{}

	std::coroutine_handle<> continuation;
	std::atomic< bool > hasContinuation = false;
};

template <>
struct promise_base< true > : public promise_base< false >
{
	std::binary_semaphore sem{ 0 };
};

template < typename Task, typename T, bool Joinable >
struct promise : promise_base< Joinable >
{
	Task get_return_object()
	{
		return std::coroutine_handle< promise >::from_promise( *this );
	}

	final_awaitable< promise, Joinable > final_suspend() noexcept
	{
		return {};
	}

	void return_value( T const& value ) noexcept( std::is_nothrow_copy_assignable_v< T > )
	{
		data = value;
	}

	void return_value( T&& value ) noexcept( std::is_nothrow_move_assignable_v< T > )
	{
		data = std::move( value );
	}

	T& result() &
	{
		return data;
	}

	T&& result() &&
	{
		return std::move( data );
	}

private:
	T data;
};

template < typename Task, bool Joinable >
struct promise< Task, void, Joinable > : promise_base< Joinable >
{
	Task get_return_object()
	{
		return std::coroutine_handle< promise >::from_promise( *this );
	}

	final_awaitable< promise, Joinable > final_suspend() noexcept
	{
		return {};
	}

	void return_void()
	{}
};

template < typename Task, typename T, bool Joinable >
struct promise< Task, T&, Joinable > : promise_base< Joinable >
{
	Task get_return_object()
	{
		return std::coroutine_handle< promise >::from_promise( *this );
	}

	final_awaitable< promise, Joinable > final_suspend() noexcept
	{
		return {};
	}

	void return_value( T& value ) noexcept( std::is_nothrow_move_assignable_v< T > )
	{
		data = std::addressof( value );
	}

	T& result()
	{
		return *data;
	}

private:
	T* data;
};

}  // namespace detail

template < typename T = void, bool Joinable = false >
class [[nodiscard]] task
{
public:
	using promise_type = detail::promise< task, T, Joinable >;

	task() noexcept = default;
	task( std::coroutine_handle< promise_type > coroutine ) noexcept : coroutine_{ coroutine }
	{}

	// Delete Copy constructor to avoid multiple people co_awaiting the same task
	task( task const& ) = delete;
	task& operator=( task const& ) = delete;

	task( task&& other ) noexcept : coroutine_{ other.coroutine_ }
	{
		other.coroutine_ = nullptr;
	}

	task& operator=( task&& other ) noexcept
	{
		if ( this != &other ) {
			if ( coroutine_ ) {
				coroutine_.destroy();
			}
			coroutine_ = other.coroutine_;
			other.coroutine_ = nullptr;
		}
		return *this;
	}

	[[nodiscard]] bool await_ready() const noexcept
	{
		return !coroutine_ || coroutine_.done();
	}

	std::coroutine_handle<> await_suspend( std::coroutine_handle<> continuation ) const noexcept
	{
		if constexpr ( Joinable ) {
			// This should never happen because joinable tasks are not continuable. Use .join() instead.
			__debugbreak();
			std::terminate();
		} else {
			auto& basePromise = coroutine_.promise();
			basePromise.continuation = continuation;
			if ( basePromise.hasContinuation.exchange( true, std::memory_order_release ) ) {
				return continuation;
			}
		}
		return std::noop_coroutine();
	}

	decltype( auto ) await_resume() const noexcept
	{
		if constexpr ( std::is_same_v< T, void > ) {
			return;
		} else {
			return std::move( coroutine_.promise() ).result();
		}
	}

	// If the Task is Joinable waits for the semaphore to be set
	// Otherwise starts a new Joinable coroutine and sets it as the continuation of this coroutine
	// Once the newly created Joinable coroutine is resumed its semaphore will be set. The calling thread will
	// wait for that semaphore.
	decltype( auto ) join() const noexcept
	{
		if constexpr ( Joinable ) {
			coroutine_.promise().sem.acquire();
			return await_resume();
		} else {
			// Not joinable, create a new joinable task that co_awaits this coroutine
			// The newly created coroutine will be joinable via the semaphore
			auto joinableTask = [ this ]() -> task< T, true > { co_return co_await* this; }();
			return joinableTask.join();
		}
	}

protected:
	std::coroutine_handle< promise_type > coroutine_ = nullptr;
};

}  // namespace cb