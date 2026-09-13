//  Copyright (c) 2026 The STE||AR Group
//  Copyright (c) 2026 Vansh Dobhal
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if defined(HPX_MSVC)

#include <hpx/debugging/detail/dbghelp_lock.hpp>

#include <mutex>

namespace {

    // Process-wide mutex; every DbgHelp call from HPX and (via
    // TRACY_DBGHELP_LOCK below) from Tracy's client goes through it.
    std::mutex g_dbghelp_mtx;

}    // namespace

namespace hpx::util::detail {

    void dbghelp_lock() noexcept
    {
        g_dbghelp_mtx.lock();
    }

    void dbghelp_unlock() noexcept
    {
        g_dbghelp_mtx.unlock();
    }
}    // namespace hpx::util::detail

// Tracy interop entry points. TRACY_DBGHELP_LOCK=HpxDbgHelp in
// HPX_SetupTracy.cmake makes Tracy expand its DbgHelp lock macros
// to these three symbols. Init is empty - the mutex is static-init
// and ready before Tracy's bootstrap runs.
extern "C" HPX_CORE_EXPORT void HpxDbgHelpInit(void) {}

extern "C" HPX_CORE_EXPORT void HpxDbgHelpLock(void)
{
    g_dbghelp_mtx.lock();
}

extern "C" HPX_CORE_EXPORT void HpxDbgHelpUnlock(void)
{
    g_dbghelp_mtx.unlock();
}

#endif    // HPX_MSVC
