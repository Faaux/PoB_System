///////////////////////////////////////////////////////////////////////////////
// Copyright (c) Lewis Baker
// Licenced under MIT license. See LICENSE.txt for details.
///////////////////////////////////////////////////////////////////////////////

#include <system_error>
#include <tasks/detail/lightweight_manual_reset_event.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

cppcoro::detail::lightweight_manual_reset_event::lightweight_manual_reset_event(bool initiallySet)
    : m_value(initiallySet ? 1 : 0)
{
}

cppcoro::detail::lightweight_manual_reset_event::~lightweight_manual_reset_event() {}

void cppcoro::detail::lightweight_manual_reset_event::set() noexcept
{
    m_value.store(1, std::memory_order_release);
    ::WakeByAddressAll(&m_value);
}

void cppcoro::detail::lightweight_manual_reset_event::reset() noexcept { m_value.store(0, std::memory_order_relaxed); }

void cppcoro::detail::lightweight_manual_reset_event::wait() noexcept
{
    // Wait in a loop as WaitOnAddress() can have spurious wake-ups.
    int value = m_value.load(std::memory_order_acquire);
    BOOL ok = TRUE;
    while (value == 0)
    {
        if (!ok)
        {
            // Previous call to WaitOnAddress() failed for some reason.
            // Put thread to sleep to avoid sitting in a busy loop if it keeps failing.
            ::Sleep(1);
        }

        ok = ::WaitOnAddress(&m_value, &value, sizeof(m_value), INFINITE);
        value = m_value.load(std::memory_order_acquire);
    }
}