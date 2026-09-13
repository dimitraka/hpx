//  Copyright (c) 2026 The STE||AR Group
//  Copyright (c) 2026 Vansh Dobhal
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>

#if defined(HPX_HAVE_TRACY)

#include <hpx/threading_base/detail/task_sampling.hpp>
#include <hpx/threading_base/tracing_sample_rate.hpp>

#include <atomic>

namespace hpx::threads::detail {

    // Compile-time HPX_TRACING_SAMPLE_RATE is the default; the runtime
    // value from hpx.tracing.sample_rate overrides it during startup.
    std::atomic<int> sample_rate{HPX_TRACING_SAMPLE_RATE};

    void set_sample_rate(int rate) noexcept
    {
        if (rate < 1)
            rate = 1;
        sample_rate.store(rate, std::memory_order_relaxed);
    }
}    // namespace hpx::threads::detail

namespace hpx::threads {

    void set_tracing_sample_rate(int rate) noexcept
    {
        detail::set_sample_rate(rate);
    }
}    // namespace hpx::threads

#endif    // HPX_HAVE_TRACY
