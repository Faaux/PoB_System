///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////
#ifndef CPPCORO_AUTO_RESET_EVENT_HPP_INCLUDED
#define CPPCORO_AUTO_RESET_EVENT_HPP_INCLUDED

#include <tasks/config.h>

#if CORO_WINDOWS
#include <tasks/detail/win32.hpp>
#else
#include <condition_variable>
#include <mutex>
#endif

namespace cppcoro
{
class auto_reset_event
{
   public:
    auto_reset_event(bool initiallySet = false);

    ~auto_reset_event();

    void set();

    void wait();

   private:
#if CORO_WINDOWS
    cppcoro::detail::win32::safe_handle m_event;
#else
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_isSet;
#endif
};
}  // namespace cppcoro

#endif
