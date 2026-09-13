//  Copyright (c) 2026 The STE||AR Group
//  Copyright (c) 2026 Vansh Dobhal
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <hpx/config.hpp>

#if defined(HPX_HAVE_TRACY)

namespace hpx::threads {

    /// Set the 1-in-N tracing sample rate at runtime. Rates below 1 are
    /// clamped to 1 (emit every task). Called from init_global_data with
    /// the value from hpx.tracing.sample_rate; can be re-invoked from
    /// user code between task-creation waves.
    HPX_CXX_CORE_EXPORT HPX_CORE_EXPORT void set_tracing_sample_rate(
        int rate) noexcept;
}    // namespace hpx::threads

#endif    // HPX_HAVE_TRACY
