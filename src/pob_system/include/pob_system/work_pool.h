#pragma once
#include <concurrent/blockingconcurrentqueue.h>

#include <thread>
class work_pool
{
   public:
    work_pool() : creator_id_(std::this_thread::get_id()) {}
    class schedule_operation
    {
       public:
        schedule_operation(work_pool* wp) noexcept : work_pool_(wp) {}

        bool await_ready() noexcept { return false; }
        void await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept
        {
            awaitingCoroutine_ = awaitingCoroutine;
            work_pool_->schedule_impl(this);
        }
        void await_resume() noexcept {}

       private:
        friend class work_pool;

        work_pool* work_pool_;
        std::coroutine_handle<> awaitingCoroutine_;
        schedule_operation* m_next = nullptr;
    };

    [[nodiscard]] schedule_operation schedule() { return schedule_operation{this}; };
    void run_all_work(cb::task<>& finished)
    {
        if (std::this_thread::get_id() != creator_id_)
        {
            assert(false);
            return;
        }
        schedule_operation* op;
        while (!finished.await_ready())
        {
            while (queue_.wait_dequeue_timed(op, std::chrono::microseconds(100)))
            {
                op->awaitingCoroutine_.resume();
            }
        }
    }

   private:
    void schedule_impl(schedule_operation* operation) { queue_.enqueue(operation); }

    std::thread::id creator_id_;
    moodycamel::BlockingConcurrentQueue<schedule_operation*> queue_;
};