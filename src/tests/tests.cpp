#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch.hpp>

#include <tasks/task.h>
#include <tasks/shared_task.h>
#include <tasks/static_thread_pool.h>

#include <string>
#include <concepts>
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

TEST_CASE( "task starts before being awaited" )
{
	bool started = false;
	auto func = [ & ]() -> cb::task<> {
		started = true;
		co_return;
	};

	[ & ]() -> cb::task<> {
		auto t = func();
		CHECK( started );

		co_await t;
	}()
				   .join();
}

TEST_CASE( "task of reference type" )
{
	int value = 3;
	auto f = [ & ]() -> cb::task< int& > { co_return value; };

	[ & ]() -> cb::task<> {
		SECTION( "awaiting rvalue task" )
		{
			decltype( auto ) result = co_await f();
			static_assert( std::is_same< decltype( result ), int& >::value,
						   "co_await r-value reference of task<int&> should result in an int&" );
			CHECK( &result == &value );
		}

		SECTION( "awaiting lvalue task" )
		{
			auto t = f();
			decltype( auto ) result = co_await t;
			static_assert( std::is_same< decltype( result ), int& >::value,
						   "co_await l-value reference of task<int&> should result in an int&" );
			CHECK( &result == &value );
		}
	}()
				   .join();
}

TEST_CASE( "task of reference type with join" )
{
	int value = 3;
	auto f = [ & ]() -> cb::task< int& > { co_return value; };

	SECTION( "awaiting rvalue task" )
	{
		decltype( auto ) result = f().join();
		static_assert( std::is_same< decltype( result ), int& >::value,
					   "co_await r-value reference of task<int&> should result in an int&" );
		CHECK( &result == &value );
	}

	SECTION( "awaiting lvalue task" )
	{
		auto t = f();
		decltype( auto ) result = t.join();
		static_assert( std::is_same< decltype( result ), int& >::value,
					   "co_await l-value reference of task<int&> should result in an int&" );
		CHECK( &result == &value );
	}
}

TEST_CASE( "lots of synchronous completions doesn't result in stack-overflow" )
{
	auto completesSynchronously = []() -> cb::task< int > { co_return 1; };

	auto run = [ & ]() -> cb::task<> {
		int sum = 0;
		for ( int i = 0; i < 1'000'000; ++i ) {
			sum += co_await completesSynchronously();
		}
		CHECK( sum == 1'000'000 );
	};

	run().join();
}

TEST_CASE( "Return value is moved on co_await" )
{
	auto makeTask = []() -> cb::task< std::vector< int > > { co_return { 5 }; };

	[ & ]() -> cb::task<> {
		auto task = makeTask();
		auto result1 = co_await task;
		REQUIRE( result1.size() == 1 );
		auto result2 = co_await task;
		REQUIRE( result2.size() == 0 );
	}()
				   .join();
}

TEST_CASE( "Return value is moved on join" )
{
	auto makeTask = []() -> cb::task< std::vector< int > > { co_return { 5 }; };

	auto task = makeTask();
	auto result = task.join();
	REQUIRE( result.size() == 1 );
	auto result2 = task.join();
	REQUIRE( result2.size() == 0 );
}

TEST_CASE( "task<>.join() test" )
{
	auto makeTask = []() -> cb::task< std::string > { co_return "foo"; };

	auto task = makeTask();
	REQUIRE( task.join() == "foo" );

	REQUIRE( makeTask().join() == "foo" );
}

TEST_CASE( "task<>.join() test: multiple threads" )
{
	// We are creating a new task and starting it inside the sync_wait().
	// The task will reschedule itself for resumption on a thread-pool thread
	// which will sometimes complete before this thread calls event.wait()
	// inside sync_wait(). Thus we're roughly testing the thread-safety of
	// sync_wait().
	cb::static_thread_pool tp{ 1 };

	int value = 0;
	auto createTask = [ & ]() -> cb::task< int > {
		co_await tp.schedule();
		co_return value++;
	};

	for ( int i = 0; i < 10'000; ++i ) {
		CHECK( createTask().join() == i );
	}
}

struct counted
{
	static int default_construction_count;
	static int copy_construction_count;
	static int move_construction_count;
	static int destruction_count;

	int id;

	static void reset_counts()
	{
		default_construction_count = 0;
		copy_construction_count = 0;
		move_construction_count = 0;
		destruction_count = 0;
	}

	static int construction_count()
	{
		return default_construction_count + copy_construction_count + move_construction_count;
	}

	static int active_count()
	{
		return construction_count() - destruction_count;
	}

	counted() : id( default_construction_count++ )
	{}
	counted( const counted& other ) : id( other.id )
	{
		++copy_construction_count;
	}
	counted( counted&& other ) : id( other.id )
	{
		++move_construction_count;
		other.id = -1;
	}
	~counted()
	{
		++destruction_count;
	}
};

int counted::default_construction_count;
int counted::copy_construction_count;
int counted::move_construction_count;
int counted::destruction_count;

TEST_CASE( "result is destroyed when last reference is destroyed" )
{
	counted::reset_counts();

	{
		auto t = []() -> cb::shared_task< counted > { co_return counted{}; }();

		[ & ]() -> cb::task< void > {
			co_await t;

			CHECK( counted::active_count() == 1 );
		}()
					   .join();
	}

	CHECK( counted::active_count() == 0 );
}

TEST_CASE( "multiple awaiters" )
{
	cb::static_thread_pool tp{ 1 };
	std::binary_semaphore sem{ 0 };
	auto produce = [ & ]() -> cb::shared_task< int > {
		co_await tp.schedule();
		sem.acquire();
		co_return 1;
	};

	auto consume = []( cb::shared_task< int > t ) -> cb::task<> {
		int result = co_await t;
		CHECK( result == 1 );
	};

	auto sharedTask = produce();

	[ & ]() -> cb::task< void > {
		auto t1 = consume( sharedTask );
		auto t2 = consume( sharedTask );
		auto t3 = consume( sharedTask );

		sem.release();

		co_await t1;
		co_await t2;
		co_await t3;
	}()
				   .join();

	CHECK( sharedTask.await_ready() );
}

TEST_CASE( "waiting on shared_task in loop doesn't cause stack-overflow" )
{
	// This test checks that awaiting a shared_task that completes
	// synchronously doesn't recursively resume the awaiter inside the
	// call to start executing the task. If it were to do this then we'd
	// expect that this test would result in failure due to stack-overflow.

	auto completesSynchronously = []() -> cb::shared_task< int > { co_return 1; };

	[ & ]() -> cb::task< void > {
		int result = 0;
		for ( int i = 0; i < 1'000'000; ++i ) {
			result += co_await completesSynchronously();
		}
		CHECK( result == 1'000'000 );
	}()
				   .join();
}