// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string_view>

#include "common/common_types.h"
#include "common/settings.h"

// Query reports written when their host work completes, counted per 16-byte slot hash: how many
// were issued and how many were written (or dropped). A read of report memory can then wait for
// the reports in its range alone instead of for all GPU work sent so far (zortos293/Lichii#559,
// Fritchii EXP30).
namespace VideoCommon::ReportSlots {

inline constexpr size_t Slots = 4096;

struct State {
    std::array<std::atomic<u32>, Slots> issued{};
    std::array<std::atomic<u32>, Slots> completed{};
    std::atomic<u32> waiters{};
    std::mutex mutex;
    std::condition_variable cv;
};

inline State& Get() {
    static State state;
    return state;
}

[[nodiscard]] inline size_t Slot(u64 device_address) {
    return (device_address >> 4) % Slots;
}

/// The query cache's thread issued a report to device_address.
inline void Issue(u64 device_address) {
    Get().issued[Slot(device_address)].fetch_add(1, std::memory_order_release);
}

/// The report's operation wrote it to memory or dropped it (the fencing thread).
inline void Complete(u64 device_address) {
    State& state = Get();
    state.completed[Slot(device_address)].fetch_add(1, std::memory_order_seq_cst);
    if (state.waiters.load(std::memory_order_seq_cst) != 0) {
        std::scoped_lock lock{state.mutex};
        state.cv.notify_all();
    }
}

/// Whether every report issued so far to [device_address, device_address + size) is in memory.
[[nodiscard]] inline bool Settled(u64 device_address, u64 size) {
    if (size == 0) {
        return true;
    }
    State& state = Get();
    const u64 first = device_address >> 4;
    const u64 count = std::min<u64>(((device_address + size - 1) >> 4) - first + 1, Slots);
    for (u64 slot = first; slot < first + count; ++slot) {
        const size_t index = slot % Slots;
        if (state.completed[index].load(std::memory_order_seq_cst) !=
            state.issued[index].load(std::memory_order_acquire)) {
            return false;
        }
    }
    return true;
}

/// Waits up to `timeout` for Settled; false on timeout. Only the thread that issues reports may
/// call it (none is issued meanwhile), after submitting the fences that carry them.
[[nodiscard]] inline bool WaitSettled(u64 device_address, u64 size,
                                      std::chrono::milliseconds timeout) {
    if (Settled(device_address, size)) {
        return true;
    }
    State& state = Get();
    state.waiters.fetch_add(1, std::memory_order_seq_cst);
    std::unique_lock lock{state.mutex};
    const bool settled = state.cv.wait_for(
        lock, timeout, [&] { return Settled(device_address, size); });
    lock.unlock();
    state.waiters.fetch_sub(1, std::memory_order_seq_cst);
    return settled;
}

enum class Mode { Off, On, Verify };

/// EDEN_NX2_LEAN_REPORT_WAIT=1 | 0 | verify. Unset: on for The Duskbloods, off elsewhere.
/// verify waits lean, then as before, and counts report words that changed in between.
[[nodiscard]] inline Mode ReadMode() {
    static const char* const value = std::getenv("EDEN_NX2_LEAN_REPORT_WAIT");
    if (value != nullptr && value[0] != '\0') {
        if (std::string_view{value} == "verify") {
            return Mode::Verify;
        }
        return value[0] == '0' ? Mode::Off : Mode::On;
    }
    return Settings::GetCurrentProgramID() == 0x0400D1D02399A000ULL ? Mode::On : Mode::Off;
}

} // namespace VideoCommon::ReportSlots
