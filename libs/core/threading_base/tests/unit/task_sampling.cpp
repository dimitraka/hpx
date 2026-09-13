//  Copyright (c) 2026 The STE||AR Group
//  Copyright (c) 2026 Vansh Dobhal
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Unit test for the 1-in-N task-sampling countdown. The template
// sample_next<N> is exercised directly with per-subtest local counters
// so any rate can be checked without touching the per-worker thread_local.
// The runtime path (should_sample_next reading the sample_rate atomic)
// is driven through set_tracing_sample_rate to verify the clamp and the
// end-to-end 1-in-N behavior matches the template.

#include <hpx/modules/testing.hpp>
#include <hpx/modules/threading_base.hpp>
#include <hpx/threading_base/detail/task_sampling.hpp>

int main()
{
    using hpx::threads::set_tracing_sample_rate;
    using hpx::threads::detail::sample_next;
    using hpx::threads::detail::should_sample_next;

    {
        int c = 0;
        for (int i = 0; i != 100; ++i)
            HPX_TEST(sample_next<1>(c));
    }

    {
        int c = 0;
        HPX_TEST(sample_next<3>(c));     // 1
        HPX_TEST(!sample_next<3>(c));    // 2
        HPX_TEST(!sample_next<3>(c));    // 3
        HPX_TEST(sample_next<3>(c));     // 4
        HPX_TEST(!sample_next<3>(c));    // 5
        HPX_TEST(!sample_next<3>(c));    // 6
        HPX_TEST(sample_next<3>(c));     // 7
    }

    {
        int c = 0;
        int emitted = 0;
        for (int i = 0; i != 10000; ++i)
            if (sample_next<10>(c))
                ++emitted;
        HPX_TEST_EQ(emitted, 1000);
    }

    {
        int c = 0;
        int emitted = 0;
        for (int i = 0; i != 100000; ++i)
            if (sample_next<100>(c))
                ++emitted;
        HPX_TEST_EQ(emitted, 1000);
    }

    // Runtime path: rates below 1 must clamp to 1 (emit every task) so
    // a misconfigured hpx.tracing.sample_rate cannot silence tracing.
    {
        set_tracing_sample_rate(0);
        for (int i = 0; i != 100; ++i)
            HPX_TEST(should_sample_next());

        set_tracing_sample_rate(-5);
        for (int i = 0; i != 100; ++i)
            HPX_TEST(should_sample_next());
    }

    // Runtime path: at rate 10 the end-to-end 1-in-N ratio matches the
    // template arithmetic; guards the atomic + NOINLINE tl accessor.
    {
        set_tracing_sample_rate(10);
        int emitted = 0;
        for (int i = 0; i != 10000; ++i)
            if (should_sample_next())
                ++emitted;
        HPX_TEST_EQ(emitted, 1000);
    }

    return hpx::util::report_errors();
}
