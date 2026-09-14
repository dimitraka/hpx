//  Copyright (c) 2026 The STE||AR Group
//  Copyright (c) 2026 Vansh Dobhal
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <hpx/config.hpp>

#if defined(HPX_HAVE_TRACY)

#include <atomic>

namespace hpx::threads::detail {

    // 1-in-N task-sampling rate. Initialized from HPX_TRACING_SAMPLE_RATE
    // (CMake compile-time default); runtime override applied at startup
    // via hpx::threads::set_tracing_sample_rate() in tracing_sample_rate.hpp.
    HPX_CXX_CORE_EXPORT HPX_CORE_EXPORT extern std::atomic<int> sample_rate;

    // Internal setter. The public entry point is
    // hpx::threads::set_tracing_sample_rate().
    HPX_CXX_CORE_EXPORT HPX_CORE_EXPORT void set_sample_rate(int rate) noexcept;

    // Per-worker sampling state. last_rate is the rate value that seeded
    // the current countdown; when it disagrees with the runtime rate, the
    // cycle is abandoned and a fresh one starts at the new rate.
    struct sample_state
    {
        int countdown = 0;
        int last_rate = 0;
    };

    // HPX_NOINLINE so the thread_local address is looked up fresh on each
    // call - a cached address would be wrong once a task migrates workers.
    template <typename State = sample_state>
    HPX_NOINLINE State& sample_state_tls()
    {
        thread_local State state{};
        return state;
    }

    /// 1-in-N countdown with Rate as a template parameter for the unit
    /// test to exercise any rate independently of the build setting.
    /// Production goes through should_sample_next() below.
    template <int Rate>
    inline bool sample_next(int& counter) noexcept
    {
        if constexpr (Rate <= 1)
            return true;
        else
        {
            if (--counter > 0)
                return false;
            counter = Rate;
            return true;
        }
    }

    /// Returns true when the next task should be sampled, based on the
    /// runtime sample_rate.
    inline bool should_sample_next() noexcept
    {
        int const rate = sample_rate.load(std::memory_order_relaxed);
        if (rate <= 1)
            return true;
        sample_state& s = sample_state_tls();
        if (s.last_rate != rate)
        {
            s.last_rate = rate;
            s.countdown = rate;
            return true;
        }
        if (--s.countdown > 0)
            return false;
        s.countdown = rate;
        return true;
    }
}    // namespace hpx::threads::detail

#endif    // HPX_HAVE_TRACY
