// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "video_core/query_cache/report_slots.h"

namespace Slots = VideoCommon::ReportSlots;
using namespace std::chrono_literals;

TEST_CASE("ReportSlots: a read waits for the reports in its range only", "[video_core]") {
    constexpr u64 read = 0x10'0000;
    REQUIRE(Slots::Settled(read, 16));
    Slots::Issue(read + 0x40); // outside the read
    REQUIRE(Slots::Settled(read, 16));
    Slots::Issue(read + 4); // the read's own slot
    REQUIRE_FALSE(Slots::Settled(read, 16));
    REQUIRE_FALSE(Slots::WaitSettled(read, 16, 1ms)); // times out: the caller falls back
    std::thread completer{[] {
        std::this_thread::sleep_for(5ms);
        Slots::Complete(read + 4);
    }};
    REQUIRE(Slots::WaitSettled(read, 16, 2s));
    completer.join();
    REQUIRE_FALSE(Slots::Settled(read + 0x40, 4));
    Slots::Complete(read + 0x40);
    REQUIRE(Slots::Settled(read + 0x40, 4));
    REQUIRE(Slots::Settled(read, 0));
}
