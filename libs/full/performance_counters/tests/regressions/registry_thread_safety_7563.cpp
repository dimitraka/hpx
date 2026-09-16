//  Copyright (c) 2026 RohanOnKeys
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This test exercises the fix for
//      #7563: Data race in performance_counters::registry:
//             countertypes_ has no synchronization
//
// One group of HPX threads concurrently registers new, uniquely named
// counter types via install_counter_type() (which calls add_counter_type(),
// which calls registry::add_counter_type()), while a second group
// concurrently discovers counter types via discover_counter_type() (exact
// name lookup) and discover_counter_types() (full enumeration). Before the
// fix, both paths read and wrote registry::countertypes_, a plain
// std::map, without any synchronization. Running this test under
// ThreadSanitizer reproduces the data race described in the issue.
//
// Independently of ThreadSanitizer, the test also checks a functional
// invariant once every racing registration has completed: every
// registered counter type must be discoverable exactly once, i.e. the
// concurrent access must not have corrupted the registry.

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/include/performance_counters.hpp>
#include <hpx/include/runtime.hpp>
#include <hpx/modules/testing.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace {

    // Pre-registered before the race starts; used to exercise the exact
    // name lookup branch of discover_counter_type() while other threads
    // concurrently register unrelated counter types.
    char const* const anchor_counter_name = "/tests/registry-race/anchor";

    // Base name shared by every counter type registered concurrently; the
    // name used by registrar task \a i is this string followed by \a i.
    std::string const racing_counter_name_base =
        "/tests/registry-race/counter-";

    /// \brief Trivial counter value function shared by every counter type
    ///        registered by this test.
    ///
    /// The value itself is irrelevant here; only the registration and
    /// discovery of the counter *type* is being exercised.
    std::int64_t counter_value(bool /* reset */) noexcept
    {
        return 0;
    }

    /// \brief Return whether \a fullname ends with \a suffix.
    ///
    /// Used instead of an exact match against the discovered fullname,
    /// since the registry's discoverer expands the plain, un-instantiated
    /// type name into an instance name such as
    /// "/tests/registry-race{locality#*/total}/counter-3" before invoking
    /// the caller-supplied callback.
    bool ends_with(std::string const& fullname, std::string const& suffix)
    {
        if (suffix.size() > fullname.size())
            return false;
        return fullname.compare(
                   fullname.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    /// \brief Register one uniquely named counter type.
    ///
    /// Called concurrently, once per registrar task, from independent HPX
    /// worker threads while \a discover_racing_counter_types() runs on
    /// other worker threads. registry::add_counter_type() takes its lock
    /// only around the map insertion itself, so this call may interleave
    /// arbitrarily with any of the concurrent discovery calls.
    ///
    /// \param index Used to build a unique counter type name for this task.
    void register_racing_counter_type(std::size_t index)
    {
        std::string const name =
            racing_counter_name_base + std::to_string(index);
        HPX_TEST_EQ(hpx::performance_counters::install_counter_type(name,
                        &counter_value,
                        "counter type registered concurrently with "
                        "discovery, regression test for #7563"),
            hpx::performance_counters::counter_status::valid_data);
    }

    /// \brief Repeatedly discover counter types until told to stop.
    ///
    /// Called concurrently, once per discoverer task, from independent HPX
    /// worker threads, while \a register_racing_counter_type() runs for
    /// every index in [0, num_registrars) on other worker threads.
    /// Exercises both the exact name lookup branch and the full
    /// enumeration branch of the registry while it is being mutated
    /// concurrently.
    ///
    /// \param keep_going Cleared by the caller once every registrar task
    ///                   has completed; this task keeps discovering
    ///                   counter types until it observes that.
    void discover_racing_counter_types(std::atomic<bool> const& keep_going)
    {
        using hpx::performance_counters::counter_info;

        while (keep_going.load(std::memory_order_relaxed))
        {
            // Exact name lookup: the anchor type is registered before any
            // racing registration starts and must always be found.
            bool found_anchor = false;
            HPX_TEST_EQ(
                hpx::performance_counters::discover_counter_type(
                    anchor_counter_name,
                    [&found_anchor](counter_info const&, hpx::error_code&) {
                        found_anchor = true;
                        return true;
                    }),
                hpx::performance_counters::counter_status::valid_data);
            HPX_TEST(found_anchor);

            // Full enumeration: must see at least the anchor type, and
            // must not crash or corrupt the map while registrations are
            // still in flight on other threads.
            std::size_t seen = 0;
            HPX_TEST_EQ(hpx::performance_counters::discover_counter_types(
                            [&seen](counter_info const&, hpx::error_code&) {
                                ++seen;
                                return true;
                            }),
                hpx::performance_counters::counter_status::valid_data);
            HPX_TEST(seen >= 1);

            hpx::this_thread::yield();
        }
    }
}    // namespace

int main()
{
    HPX_TEST_EQ(hpx::performance_counters::install_counter_type(
                    anchor_counter_name, &counter_value,
                    "anchor counter type used to exercise "
                    "discover_counter_type() concurrently with "
                    "registration, regression test for #7563"),
        hpx::performance_counters::counter_status::valid_data);

    std::size_t const num_workers =
        (std::max) (std::size_t(2), hpx::get_num_worker_threads());
    std::size_t const num_registrars = num_workers * 4;
    std::size_t const num_discoverers = num_workers;

    std::atomic<bool> keep_discovering(true);

    std::vector<hpx::future<void>> discoverers;
    discoverers.reserve(num_discoverers);
    for (std::size_t i = 0; i != num_discoverers; ++i)
    {
        discoverers.push_back(hpx::async([&keep_discovering]() {
            discover_racing_counter_types(keep_discovering);
        }));
    }

    std::vector<hpx::future<void>> registrars;
    registrars.reserve(num_registrars);
    for (std::size_t i = 0; i != num_registrars; ++i)
    {
        registrars.push_back(
            hpx::async([i]() { register_racing_counter_type(i); }));
    }

    hpx::wait_all(registrars);
    keep_discovering.store(false, std::memory_order_relaxed);
    hpx::wait_all(discoverers);

    // With every racing registration now complete, a final, sequential
    // enumeration must find the anchor plus exactly one entry per
    // registrar, with none duplicated and none missing: concurrent
    // access must not have corrupted registry::countertypes_.
    // discover_counter_types() returns every counter type known to this
    // locality, so the callback filters down to the ones registered by
    // this test.
    std::set<std::string> discovered;
    HPX_TEST_EQ(
        hpx::performance_counters::discover_counter_types(
            [&discovered](hpx::performance_counters::counter_info const& info,
                hpx::error_code&) {
                if (info.fullname_.find("registry-race") != std::string::npos)
                {
                    discovered.insert(info.fullname_);
                }
                return true;
            }),
        hpx::performance_counters::counter_status::valid_data);

    HPX_TEST_EQ(discovered.size(), num_registrars + 1);

    bool anchor_found = false;
    for (std::string const& fullname : discovered)
    {
        if (ends_with(fullname, "/anchor"))
            anchor_found = true;
    }
    HPX_TEST(anchor_found);

    for (std::size_t i = 0; i != num_registrars; ++i)
    {
        std::string const suffix = "/counter-" + std::to_string(i);
        bool this_one_found = false;
        for (std::string const& fullname : discovered)
        {
            if (ends_with(fullname, suffix))
                this_one_found = true;
        }
        HPX_TEST(this_one_found);
    }

    return hpx::util::report_errors();
}
#endif
