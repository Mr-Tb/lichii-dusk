// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "common/logging.h"

namespace VideoCommon::Nx2Perf {

[[nodiscard]] inline bool VerifySwitchSet() {
    const char* value = std::getenv("EDEN_NX2_PERF_VERIFY");
    return value != nullptr && value[0] == '1';
}

/// Every decision EDEN_NX2_PERF_VERIFY=1 turns to its slow (reference) answer, one per site, so
/// EDEN_NX2_PERF_SLOW=<name>[,<name>...] (or "all") can force them one at a time.
enum class Site : std::uint32_t {
    WritePages,             // buffer_cache.h WriteMemory: one-lookup early return
    NativeIndirectUniforms, // buffer_cache.h native bank once per frame (pre-check only)
    CleanWords,             // buffer_cache.h SynchronizeBuffer: walk only dirty tracker words
    GeometrySync,           // buffer_cache.h SynchronizeGeometryBuffer: skip a synced binding
    StorageBindingCache,    // buffer_cache.h StorageBufferBinding: thread-local result cache
    UsageTracker,           // usage_tracker.h Track: fill only around the full run
    TrackerRuns,            // word_manager.h ApplyCollectedRanges: in place, deferred caching
    DeviceSeparated,        // device_memory_manager.inc UpdatePagesCachedBatch: no sort/merge
    MethodFast,             // maxwell_3d.cpp CallMethod: state-register and fast method paths
    MethodCbBatch,          // maxwell_3d.cpp constant buffer word batches
    StateBlocks,            // maxwell_3d.cpp front/back state register blocks
    MappingRuns,            // memory_manager.cpp MaxContinuousRange from map-time runs
    LayoutSize,             // memory_manager.cpp GetMemoryLayoutSize from the snapshot
    SubmappedRange,         // memory_manager.cpp GetSubmapped(Device)Range from runs
    IndexRebind,            // vk_buffer_cache.cpp BindIndexBuffer: skip an identical rebind
    Unswizzle,              // texture_cache/util.cpp worker unswizzle (verify: twice, compare)
    AsyncPipelinesSpirv,    // vk_pipeline_cache.cpp worker SPIR-V (verify: GPU-thread reference)
    InlineDefer,            // vk_rasterizer.cpp inline writes: deferred invalidation (not Verify)
    Count,
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(Site::Count)> SiteNames{
    "write_pages",   "native_indirect_uniforms", "clean_words",     "geometry_sync",
    "storage_binding_cache", "usage_tracker",    "tracker_runs",    "device_separated",
    "method_fast",   "method_cb_batch",          "state_blocks",    "mapping_runs",
    "layout_size",   "submapped_range",          "index_rebind",    "unswizzle",
    "async_pipelines_spirv", "inline_defer",
};
static_assert([] {
    for (const std::string_view name : SiteNames) {
        if (name.empty()) {
            return false;
        }
    }
    return true;
}(), "every Nx2Perf::Site needs a name");

struct SlowMask {
    std::uint32_t mask = 0;
    std::uint32_t unknown = 0; // names not recognised
};

/// Parses "a,b,c" (spaces allowed, "all" for every site).
[[nodiscard]] constexpr SlowMask ParseSlow(std::string_view text) {
    SlowMask result;
    while (!text.empty()) {
        const std::size_t comma = text.find(',');
        std::string_view name = text.substr(0, comma);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
        while (!name.empty() && name.front() == ' ') {
            name.remove_prefix(1);
        }
        while (!name.empty() && (name.back() == ' ' || name.back() == '\r')) {
            name.remove_suffix(1);
        }
        if (name.empty()) {
            continue;
        }
        if (name == "all") {
            result.mask = (1u << SiteNames.size()) - 1;
            continue;
        }
        bool found = false;
        for (std::size_t i = 0; i < SiteNames.size(); ++i) {
            if (SiteNames[i] == name) {
                result.mask |= 1u << i;
                found = true;
            }
        }
        result.unknown += found ? 0 : 1;
    }
    return result;
}
static_assert(ParseSlow("").mask == 0);
static_assert(ParseSlow("clean_words").mask == 1u << static_cast<int>(Site::CleanWords));
static_assert(ParseSlow(" write_pages , inline_defer\r").mask ==
              ((1u << static_cast<int>(Site::WritePages)) |
               (1u << static_cast<int>(Site::InlineDefer))));
static_assert(ParseSlow("all").mask == (1u << static_cast<int>(Site::Count)) - 1);
static_assert(ParseSlow("bogus,,mapping_runs").unknown == 1);
static_assert(ParseSlow("bogus,,mapping_runs").mask == 1u << static_cast<int>(Site::MappingRuns));

[[nodiscard]] inline SlowMask SlowSwitch() {
    const char* value = std::getenv("EDEN_NX2_PERF_SLOW");
    return value == nullptr ? SlowMask{} : ParseSlow(value);
}

/// The sites PERF_VERIFY=1 turns slow: all but inline_defer (EDEN_NX2_INLINE_DEFER=0).
inline constexpr std::uint32_t VerifyMask =
    ((1u << SiteNames.size()) - 1) & ~(1u << static_cast<std::uint32_t>(Site::InlineDefer));

/// Texture alias controls for the Duskbloods exposure flicker (live through perf_flip, start
/// switches EDEN_NX2_<NAME upper case>=1; all off is the normal behaviour).
enum class Alias : std::uint32_t {
    SyncAlways,    ///< unused bit: alias_sync_always is the tri-state LiveState::alias_sync
    FullResolve,   ///< fast draw delta draws take the full path when a sampled image has aliases
    ResyncUpload,  ///< an upload of an aliased image copies GPU-written aliases of the same tick
    Diag,          ///< per-second counts of alias work on images of at most 16x16
    Count,
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(Alias::Count)> AliasNames{
    "alias_sync_always", "alias_full_resolve", "alias_resync_upload", "alias_diag",
};

/// The live state. Start: the switches (ReadSwitch). Then the debug server's perf_slow and
/// perf_flip (docs/nx2/debug-mcp.md), applied by RasterizerVulkan::TickFrame at a frame end.
/// Every site reads it with one relaxed atomic load where it decides, and each decision is per
/// call (the fast and the slow path of a site leave the same state), so a flip is safe wherever
/// another thread is; the frame end only gives the caches a reset point (ApplyLive's caller).
struct LiveState {
    bool verify{};              ///< EDEN_NX2_PERF_VERIFY
    std::uint32_t named{};      ///< EDEN_NX2_PERF_SLOW sites (+ inline_defer for INLINE_DEFER=0)
    std::uint32_t dusk_off{};   ///< BackX::DuskPiece bits on their old path
    int uboreuse = -1;          ///< -1: the start switch, else 0/1 (EDEN_NX2_UBOREUSE)
    int frames_ahead = -1;      ///< -1: the start switch, else 0/1 (EDEN_NX2_FRAMES_AHEAD)
    int fence_batch = -1;       ///< -1: the start switch, else 0/1 (EDEN_NX2_FENCE_BATCH)
    int sync_submit = -1;       ///< -1: the start switch, else 0/1 (EDEN_NX2_SYNC_SUBMIT)
    std::uint32_t alias{};      ///< Alias bits
    /// SynchronizeAliases walks the aliases even when no alias tick is newer: -1 (default) for
    /// The Duskbloods' images of at most 16x16, 0 never, 1 always (EDEN_NX2_ALIAS_SYNC_ALWAYS)
    int alias_sync = -1;
    // Upscaler controls (-1: the default described at each use)
    int dlss_tight = -1;         ///< DLSS reads render-sized copies of its inputs at 0,0
    int dlss_hdr = -1;           ///< DLSS colorBuffersHDR (EDEN_NX2_DLSS_HDR)
    int dlss_auto_exposure = -1; ///< DLSS useAutoExposure (EDEN_NX2_DLSS_AUTO_EXPOSURE)
    int dlss_preset = -1;        ///< Settings::NvidiaDlssPreset value over the setting
    int dusk_dlss_m = -1;        ///< The Duskbloods runs preset M as is (EDEN_NX2_DUSK_DLSS_M)
    /// An image that keeps bouncing between 1x and the resolution scale stays at 1x
    /// (EDEN_NX2_RESCALE_PIN; default: The Duskbloods only)
    int rescale_pin = -1;
    /// rescale_pin: the main frame (the upscaler's images and the targets drawn with them) is
    /// never pinned and wins over pinned partners (EDEN_NX2_PIN_MAIN; default on)
    int pin_main = -1;
    /// rescale_pin: a pinned image stays at 1x on every path, not only render targets and the
    /// upscaler (EDEN_NX2_PIN_STRICT; default on)
    int pin_strict = -1;
    /// rescale_pin: a blit from a main frame image leaves a pinned destination at 1x (only a
    /// main frame destination releases the pins) (EDEN_NX2_PIN_BLIT; default on)
    int pin_blit = -1;
    /// A pass that would drag a main frame image to 1x raises partners held back only by their
    /// aliases instead (EDEN_NX2_MAIN_PARTNER; default on)
    int main_partner = -1;
    /// Per-pass GPU timing of one frame every 2 s (EDEN_NX2_PASS_TIMING; default The Duskbloods)
    int pass_timing = -1;

    [[nodiscard]] std::uint32_t Effective() const noexcept {
        return named | (verify ? VerifyMask : 0);
    }
};

/// Sites on their slow path (verify folded in): what Slow() reads.
inline std::atomic<std::uint32_t> g_slow{};
inline std::atomic<bool> g_verify{};
inline std::atomic<std::uint32_t> g_named{};
inline std::atomic<std::uint32_t> g_dusk_off{};
inline std::atomic<int> g_uboreuse{-1};
inline std::atomic<int> g_frames_ahead{-1};
inline std::atomic<int> g_fence_batch{-1};
inline std::atomic<int> g_sync_submit{-1};
inline std::atomic<std::uint32_t> g_alias{};
inline std::atomic<int> g_alias_sync{-1};
inline std::atomic<int> g_dlss_tight{-1};
inline std::atomic<int> g_dlss_hdr{-1};
inline std::atomic<int> g_dlss_auto_exposure{-1};
inline std::atomic<int> g_dlss_preset{-1};
inline std::atomic<int> g_dusk_dlss_m{-1};
inline std::atomic<int> g_rescale_pin{-1};
inline std::atomic<int> g_pin_main{-1};
inline std::atomic<int> g_pin_strict{-1};
inline std::atomic<int> g_pin_blit{-1};
inline std::atomic<int> g_main_partner{-1};
inline std::atomic<int> g_pass_timing{-1};
/// Live changes requested / applied, and the rasterizer frame of the last applied one.
inline std::atomic<std::uint64_t> g_live_requested{};
inline std::atomic<std::uint64_t> g_live_applied{};
inline std::atomic<std::uint64_t> g_live_applied_frame{};
inline std::mutex g_live_mutex;
inline LiveState g_live_pending;
/// The last "DLSS stats:" line (NVIDIA DLSS through Streamline), for the debug server's status;
/// empty until DLSS has run.
inline std::mutex g_dlss_stats_mutex;
inline std::string g_dlss_stats;

inline void StoreLive(const LiveState& state) {
    g_verify.store(state.verify, std::memory_order_relaxed);
    g_named.store(state.named, std::memory_order_relaxed);
    g_slow.store(state.Effective(), std::memory_order_relaxed);
    g_dusk_off.store(state.dusk_off, std::memory_order_relaxed);
    g_uboreuse.store(state.uboreuse, std::memory_order_relaxed);
    g_frames_ahead.store(state.frames_ahead, std::memory_order_relaxed);
    g_fence_batch.store(state.fence_batch, std::memory_order_relaxed);
    g_sync_submit.store(state.sync_submit, std::memory_order_relaxed);
    g_alias.store(state.alias, std::memory_order_relaxed);
    g_alias_sync.store(state.alias_sync, std::memory_order_relaxed);
    g_dlss_tight.store(state.dlss_tight, std::memory_order_relaxed);
    g_dlss_hdr.store(state.dlss_hdr, std::memory_order_relaxed);
    g_dlss_auto_exposure.store(state.dlss_auto_exposure, std::memory_order_relaxed);
    g_dlss_preset.store(state.dlss_preset, std::memory_order_relaxed);
    g_dusk_dlss_m.store(state.dusk_dlss_m, std::memory_order_relaxed);
    g_rescale_pin.store(state.rescale_pin, std::memory_order_relaxed);
    g_pin_main.store(state.pin_main, std::memory_order_relaxed);
    g_pin_strict.store(state.pin_strict, std::memory_order_relaxed);
    g_pin_blit.store(state.pin_blit, std::memory_order_relaxed);
    g_main_partner.store(state.main_partner, std::memory_order_relaxed);
    g_pass_timing.store(state.pass_timing, std::memory_order_relaxed);
}

[[nodiscard]] inline LiveState CurrentLive() {
    return LiveState{
        .verify = g_verify.load(std::memory_order_relaxed),
        .named = g_named.load(std::memory_order_relaxed),
        .dusk_off = g_dusk_off.load(std::memory_order_relaxed),
        .uboreuse = g_uboreuse.load(std::memory_order_relaxed),
        .frames_ahead = g_frames_ahead.load(std::memory_order_relaxed),
        .fence_batch = g_fence_batch.load(std::memory_order_relaxed),
        .sync_submit = g_sync_submit.load(std::memory_order_relaxed),
        .alias = g_alias.load(std::memory_order_relaxed),
        .alias_sync = g_alias_sync.load(std::memory_order_relaxed),
        .dlss_tight = g_dlss_tight.load(std::memory_order_relaxed),
        .dlss_hdr = g_dlss_hdr.load(std::memory_order_relaxed),
        .dlss_auto_exposure = g_dlss_auto_exposure.load(std::memory_order_relaxed),
        .dlss_preset = g_dlss_preset.load(std::memory_order_relaxed),
        .dusk_dlss_m = g_dusk_dlss_m.load(std::memory_order_relaxed),
        .rescale_pin = g_rescale_pin.load(std::memory_order_relaxed),
        .pin_main = g_pin_main.load(std::memory_order_relaxed),
        .pin_strict = g_pin_strict.load(std::memory_order_relaxed),
        .pin_blit = g_pin_blit.load(std::memory_order_relaxed),
        .main_partner = g_main_partner.load(std::memory_order_relaxed),
        .pass_timing = g_pass_timing.load(std::memory_order_relaxed),
    };
}

/// The state the next frame end applies (the pending one, else the live one).
[[nodiscard]] inline LiveState TargetLive() {
    std::scoped_lock lock{g_live_mutex};
    return g_live_requested.load(std::memory_order_relaxed) !=
                   g_live_applied.load(std::memory_order_relaxed)
               ? g_live_pending
               : CurrentLive();
}

/// Debug server thread: the next frame end applies `state`. Returns the request to wait for.
inline std::uint64_t RequestLive(const LiveState& state) {
    std::scoped_lock lock{g_live_mutex};
    g_live_pending = state;
    return g_live_requested.fetch_add(1, std::memory_order_release) + 1;
}

/// "name=slow name=fast ..." of an effective mask.
[[nodiscard]] inline std::string SitesText(std::uint32_t slow) {
    std::string sites;
    for (std::size_t i = 0; i < SiteNames.size(); ++i) {
        sites += sites.empty() ? "" : " ";
        sites += SiteNames[i];
        sites += (slow >> i & 1) != 0 ? "=slow" : "=fast";
    }
    return sites;
}

/// Rasterizer thread, at a frame end: applies a requested change. True when one was applied, so
/// the caller resets the caches the flipped paths fill differently.
inline bool ApplyLive(std::uint64_t frame) {
    const std::uint64_t requested = g_live_requested.load(std::memory_order_acquire);
    if (requested == g_live_applied.load(std::memory_order_relaxed)) {
        return false;
    }
    LiveState state;
    {
        std::scoped_lock lock{g_live_mutex};
        state = g_live_pending;
    }
    StoreLive(state);
    g_live_applied_frame.store(frame, std::memory_order_relaxed);
    g_live_applied.store(requested, std::memory_order_release);
    LOG_INFO(HW_GPU,
             "Nx2Perf live change #{} at frame {}: verify={} dusk_off={:#x} uboreuse={} "
             "frames_ahead={} fence_batch={} sync_submit={} alias={:#x} alias_sync={} "
             "dlss_tight={} dlss_hdr={} dlss_auto_exposure={} dlss_preset={} dusk_dlss_m={} "
             "rescale_pin={} pin_main={} pin_strict={} pin_blit={} "
             "main_partner={} pass_timing={} sites: {}",
             requested, frame, state.verify ? 1 : 0, state.dusk_off, state.uboreuse,
             state.frames_ahead, state.fence_batch, state.sync_submit, state.alias,
             state.alias_sync, state.dlss_tight, state.dlss_hdr, state.dlss_auto_exposure,
             state.dlss_preset, state.dusk_dlss_m, state.rescale_pin,
             state.pin_main, state.pin_strict, state.pin_blit, state.main_partner,
             state.pass_timing, SitesText(state.Effective()));
    return true;
}

/// Called by VideoCore::CreateGPU: static initialization runs before the frontend loads
/// nx2_switches.txt, so a switch set only there takes effect from that point on.
inline void ReadSwitch() {
    const SlowMask slow = SlowSwitch();
    if (slow.unknown != 0) {
        LOG_WARNING(HW_GPU, "EDEN_NX2_PERF_SLOW: {} unknown site name(s) in \"{}\"", slow.unknown,
                    std::getenv("EDEN_NX2_PERF_SLOW"));
    }
    LiveState state = CurrentLive();
    state.verify = VerifySwitchSet();
    state.named = slow.mask;
    if (const char* defer = std::getenv("EDEN_NX2_INLINE_DEFER"); defer && defer[0] == '0') {
        state.named |= 1u << static_cast<std::uint32_t>(Site::InlineDefer);
    }
    for (std::size_t i = 0; i < AliasNames.size(); ++i) {
        std::string name = "EDEN_NX2_";
        for (const char c : AliasNames[i]) {
            name += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        }
        if (const char* value = std::getenv(name.c_str()); value && value[0] == '1') {
            state.alias |= 1u << i;
        }
    }
    if (const char* value = std::getenv("EDEN_NX2_ALIAS_SYNC_ALWAYS"); value && *value) {
        state.alias_sync = value[0] == '1' ? 1 : 0;
    }
    if (const char* value = std::getenv("EDEN_NX2_DLSS_TIGHT"); value && *value) {
        state.dlss_tight = value[0] == '1' ? 1 : 0;
    }
    if (const char* value = std::getenv("EDEN_NX2_RESCALE_PIN"); value && *value) {
        state.rescale_pin = value[0] == '1' ? 1 : 0;
    }
    if (const char* value = std::getenv("EDEN_NX2_PIN_MAIN"); value && *value) {
        state.pin_main = value[0] == '1' ? 1 : 0;
    }
    if (const char* value = std::getenv("EDEN_NX2_PIN_STRICT"); value && *value) {
        state.pin_strict = value[0] == '1' ? 1 : 0;
    }
    for (const auto& [name, field] : {std::pair{"EDEN_NX2_PIN_BLIT", &state.pin_blit},
                                      std::pair{"EDEN_NX2_MAIN_PARTNER", &state.main_partner},
                                      std::pair{"EDEN_NX2_PASS_TIMING", &state.pass_timing}}) {
        if (const char* value = std::getenv(name); value && *value) {
            *field = value[0] == '1' ? 1 : 0;
        }
    }
    StoreLive(state);
    LOG_INFO(HW_GPU, "Nx2Perf sites (PERF_VERIFY={}, alias={:#x}, alias_sync={}): {}",
             state.verify ? 1 : 0, state.alias, state.alias_sync, SitesText(state.Effective()));
}

inline bool Verify() {
    return g_verify.load(std::memory_order_relaxed);
}

/// True when this site takes its slow (reference) path: EDEN_NX2_PERF_VERIFY=1, the site is
/// named in EDEN_NX2_PERF_SLOW, or the debug server's perf_slow turned it slow.
inline bool Slow(Site site) {
    return (g_slow.load(std::memory_order_relaxed) >> static_cast<std::uint32_t>(site) & 1) != 0;
}

/// inline_defer, which PERF_VERIFY does not change (VerifyMask leaves its bit out).
inline bool SlowNamed(Site site) {
    return Slow(site);
}

/// A live override (-1: none) of a start-time boolean switch.
[[nodiscard]] inline bool LiveOr(const std::atomic<int>& live, bool start) {
    const int value = live.load(std::memory_order_relaxed);
    return value < 0 ? start : value != 0;
}

[[nodiscard]] inline int AliasSync() {
    return g_alias_sync.load(std::memory_order_relaxed);
}

[[nodiscard]] inline bool AliasOn(Alias control) {
    return (g_alias.load(std::memory_order_relaxed) >> static_cast<std::uint32_t>(control) & 1) != 0;
}

/// alias_diag counts (texture cache thread), images of at most 16x16 only.
enum class AliasStat : std::uint32_t {
    Skipped,     ///< SynchronizeAliases returned at the newest_alias_tick check
    NoNewer,     ///< walked, no alias newer
    Synced,      ///< walked and copied (one per image sync)
    Uploads,     ///< uploads from guest memory of an image with aliases
    CreatedAlias,///< created with at least one alias
    CreatedBad,  ///< created with a bad overlap and no alias
    Count,
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(AliasStat::Count)>
    AliasStatNames{"skipped", "no_newer", "synced", "uploads", "created_alias", "created_bad"};
inline std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(AliasStat::Count)>
    g_alias_stats{};
/// The last whole second's counts (perf_sites "alias_stats").
inline std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(AliasStat::Count)>
    g_alias_stats_last{};

inline void NoteAlias(AliasStat stat, std::uint32_t width, std::uint32_t height) {
    if (width <= 16 && height <= 16 && AliasOn(Alias::Diag)) {
        g_alias_stats[static_cast<std::size_t>(stat)].fetch_add(1, std::memory_order_relaxed);
    }
}

/// Rasterizer thread, each frame end: at most once a second, logs and keeps the counts.
inline void FlushAliasStats(std::uint64_t now_ms) {
    static std::uint64_t last_ms{};
    if (!AliasOn(Alias::Diag) || now_ms - last_ms < 1000) {
        return;
    }
    last_ms = now_ms;
    std::string text;
    for (std::size_t i = 0; i < g_alias_stats.size(); ++i) {
        const std::uint64_t count = g_alias_stats[i].exchange(0, std::memory_order_relaxed);
        g_alias_stats_last[i].store(count, std::memory_order_relaxed);
        text += (text.empty() ? "" : " ") + std::string{AliasStatNames[i]} + "=" +
                std::to_string(count);
    }
    LOG_INFO(HW_GPU, "Alias sync (images <= 16x16, last second): {}", text);
}

/// Sync submits (nvhost_gpu, EDEN_NX2_SYNC_SUBMIT): time the guest thread waited for its fence.
inline std::atomic<std::uint64_t> g_sync_submit_wait_ns{};
inline std::atomic<std::uint64_t> g_sync_submit_waits{};
/// Temporal upscales (TextureCacheRuntime): evaluations, history resets, and which upscaler ran
/// last (0 none, 1 FSR 3.1, 2 NVIDIA DLSS).
inline std::atomic<std::uint64_t> g_upscales{};
inline std::atomic<std::uint64_t> g_upscale_resets{};
inline std::atomic<int> g_upscaler{};
/// Host commit left (texture cache memory check), for the perf summary.
inline std::atomic<std::uint64_t> g_commit_left_mib{};

/// Resolution scaler changes of images (texture cache thread): scaled up, scaled down, and flips
/// (scaled the other way within 120 frames of the last change, which resamples the contents).
inline std::atomic<std::uint64_t> g_rescale_up{};
inline std::atomic<std::uint64_t> g_rescale_down{};
inline std::atomic<std::uint64_t> g_rescale_flips{};
/// Image copies between a scaled and an unscaled image, brought to one scale first.
inline std::atomic<std::uint64_t> g_rescale_mismatches{};
/// Images pinned at 1x (rescale_pin) in the last second.
inline std::atomic<std::uint64_t> g_rescale_pins{};
/// pin_main, in the last second: pins released (a main frame image, or one drawn with it), and
/// main frame images scaled down.
inline std::atomic<std::uint64_t> g_rescale_releases{};
inline std::atomic<std::uint64_t> g_rescale_main_down{};
/// Pins set since the last pin generation (not reset each second).
inline std::atomic<std::uint64_t> g_rescale_pins_total{};
/// The Duskbloods' upscales tried in the last second, and those whose colour was drawn scaled
/// (not one the upscaler raised from 1x).
inline std::atomic<std::uint64_t> g_upscales_tried{};
inline std::atomic<std::uint64_t> g_upscales_2x{};

/// Why the texture cache changed an image's scale.
enum class RescaleReason : std::uint8_t {
    Target,        ///< RescaleRenderTargets: the bound targets go up together
    TargetPartner, ///< RescaleRenderTargets: a bound target can't scale, so all go down
    Storage,       ///< FillImageViews: written as a storage image (imageStore runs at 1x)
    Alias,         ///< SynchronizeAliases
    Copy,          ///< CopyImage between a scaled and an unscaled image
    Blit,          ///< BlitImage
    Join,          ///< an image joined from overlapping ones
    Upscaler,      ///< the temporal upscaler's inputs and output
    Other,
    Count,
};
inline constexpr std::array<std::string_view, static_cast<std::size_t>(RescaleReason::Count)>
    RescaleReasonNames{"target", "target_partner", "storage",  "alias", "copy",
                       "blit",   "join",           "upscaler", "other"};
inline std::array<std::array<std::atomic<std::uint64_t>, RescaleReasonNames.size()>, 2>
    g_rescale_by{};

/// The images that flipped in the last second (the texture cache notes, the rasterizer thread
/// flushes): address, guest size, format, and the reasons of their last change up and down.
struct FlipImage {
    std::uint64_t gpu_addr{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::string format;
    std::uint32_t flips{};
    RescaleReason last_up{RescaleReason::Other};
    RescaleReason last_down{RescaleReason::Other};
};
inline std::mutex g_flip_images_mutex;
inline std::vector<FlipImage> g_flip_images;

/// `format` is called only for an image not in the table yet.
template <typename Format>
inline void NoteRescale(bool up, bool flip, RescaleReason reason, std::uint64_t gpu_addr,
                        std::uint32_t width, std::uint32_t height, Format&& format) {
    (up ? g_rescale_up : g_rescale_down).fetch_add(1, std::memory_order_relaxed);
    g_rescale_by[up ? 0 : 1][static_cast<std::size_t>(reason)].fetch_add(
        1, std::memory_order_relaxed);
    if (!flip) {
        return;
    }
    g_rescale_flips.fetch_add(1, std::memory_order_relaxed);
    std::scoped_lock lock{g_flip_images_mutex};
    auto it = std::find_if(g_flip_images.begin(), g_flip_images.end(), [&](const FlipImage& i) {
        return i.gpu_addr == gpu_addr && i.width == width && i.height == height;
    });
    if (it == g_flip_images.end()) {
        if (g_flip_images.size() >= 64) {
            return;
        }
        it = g_flip_images.insert(g_flip_images.end(),
                                  FlipImage{gpu_addr, width, height, format()});
    }
    ++it->flips;
    (up ? it->last_up : it->last_down) = reason;
}

/// Render targets that kept a pass with a main frame image at 1x (texture cache thread notes,
/// FlushRescaleStats logs): which image and why it could not scale ("info": not rescalable by
/// its own size or layout, "alias": an image aliasing it is not, "shadow", "pinned"), and the
/// passes main_partner raised instead.
struct PartnerBlock {
    std::uint64_t gpu_addr{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::string format;
    std::string_view why;
    std::uint32_t count{};
};
inline std::mutex g_partner_blocks_mutex;
inline std::vector<PartnerBlock> g_partner_blocks;
inline std::atomic<std::uint64_t> g_main_raised{};

template <typename Format>
inline void NotePartnerBlock(std::uint64_t gpu_addr, std::uint32_t width, std::uint32_t height,
                             std::string_view why, Format&& format) {
    std::scoped_lock lock{g_partner_blocks_mutex};
    auto it = std::find_if(g_partner_blocks.begin(), g_partner_blocks.end(),
                           [&](const PartnerBlock& b) {
                               return b.gpu_addr == gpu_addr && b.width == width &&
                                      b.height == height && b.why == why;
                           });
    if (it == g_partner_blocks.end()) {
        if (g_partner_blocks.size() >= 16) {
            return;
        }
        it = g_partner_blocks.insert(g_partner_blocks.end(),
                                     PartnerBlock{gpu_addr, width, height, format(), why});
    }
    ++it->count;
}

[[nodiscard]] inline std::string RescaleReasonsText(int direction) {
    std::string text;
    for (std::size_t i = 0; i < RescaleReasonNames.size(); ++i) {
        const std::uint64_t count =
            g_rescale_by[direction][i].exchange(0, std::memory_order_relaxed);
        if (count != 0) {
            text += (text.empty() ? "" : ",") + std::string{RescaleReasonNames[i]} + ":" +
                    std::to_string(count);
        }
    }
    return text.empty() ? "-" : text;
}

/// Rasterizer thread, each frame end: at most once a second, logs the changes when there were any.
inline void FlushRescaleStats(std::uint64_t now_ms) {
    static std::uint64_t last_ms{};
    if (now_ms - last_ms < 1000) {
        return;
    }
    last_ms = now_ms;
    const std::uint64_t up = g_rescale_up.exchange(0, std::memory_order_relaxed);
    const std::uint64_t down = g_rescale_down.exchange(0, std::memory_order_relaxed);
    const std::uint64_t flips = g_rescale_flips.exchange(0, std::memory_order_relaxed);
    const std::uint64_t mismatches = g_rescale_mismatches.exchange(0, std::memory_order_relaxed);
    const std::uint64_t pins = g_rescale_pins.exchange(0, std::memory_order_relaxed);
    const std::uint64_t releases = g_rescale_releases.exchange(0, std::memory_order_relaxed);
    const std::uint64_t main_down = g_rescale_main_down.exchange(0, std::memory_order_relaxed);
    const std::uint64_t tried = g_upscales_tried.exchange(0, std::memory_order_relaxed);
    const std::uint64_t upscales_2x = g_upscales_2x.exchange(0, std::memory_order_relaxed);
    const std::string up_by = RescaleReasonsText(0);
    const std::string down_by = RescaleReasonsText(1);
    std::vector<FlipImage> images;
    {
        std::scoped_lock lock{g_flip_images_mutex};
        images.swap(g_flip_images);
    }
    std::vector<PartnerBlock> blocks;
    {
        std::scoped_lock lock{g_partner_blocks_mutex};
        blocks.swap(g_partner_blocks);
    }
    const std::uint64_t main_raised = g_main_raised.exchange(0, std::memory_order_relaxed);
    if (up + down + mismatches + pins + tried + blocks.size() != 0) {
        std::sort(images.begin(), images.end(),
                  [](const FlipImage& a, const FlipImage& b) { return a.flips > b.flips; });
        std::string top;
        for (std::size_t i = 0; i < images.size() && i < 6; ++i) {
            const FlipImage& image = images[i];
            top += fmt::format("{}{:x} {}x{} {} x{} up:{} down:{}", top.empty() ? "" : "; ",
                               image.gpu_addr, image.width, image.height, image.format,
                               image.flips,
                               RescaleReasonNames[static_cast<std::size_t>(image.last_up)],
                               RescaleReasonNames[static_cast<std::size_t>(image.last_down)]);
        }
        std::string blockers;
        for (const PartnerBlock& block : blocks) {
            blockers += fmt::format("{}{:x} {}x{} {} {} x{}", blockers.empty() ? "" : "; ",
                                    block.gpu_addr, block.width, block.height, block.format,
                                    block.why, block.count);
        }
        LOG_INFO(HW_GPU,
                 "Rescale (last second): up={} down={} flips={} mismatched_copies={} pinned={} "
                 "up_by={} down_by={} top_flips=[{}] upscales_2x={}/{} released={} main_down={} "
                 "pins_set_gen={} main_blockers=[{}] main_raised={}",
                 up, down, flips, mismatches, pins, up_by, down_by, top, upscales_2x, tried,
                 releases, main_down, g_rescale_pins_total.load(std::memory_order_relaxed),
                 blockers, main_raised);
    }
}

/// The temporal upscaler's inputs, one record per upscale the texture cache tried (rasterizer
/// thread): what each input was, whether it was written since the previous upscale, and what the
/// upscaler was told. The debug server's upscale_inputs reads the ring.
struct UpscaleInput {
    std::uint64_t gpu_addr{}; ///< 0: none
    std::uint32_t width{};    ///< guest size
    std::uint32_t height{};
    bool scaled{};
    std::uint32_t age{}; ///< upscales since it was last written (0: written for this one)
};
enum class UpscaleOutcome : std::uint8_t { Ran, ScaleMismatch, SizeMismatch, NoView, Declined };
inline constexpr std::array<std::string_view, 5> UpscaleOutcomeNames{
    "ran", "scale_mismatch", "size_mismatch", "no_view", "declined"};
struct UpscaleRecord {
    std::uint64_t frame{};
    UpscaleInput color;
    UpscaleInput depth;
    UpscaleInput motion;
    std::uint32_t region_width{};
    std::uint32_t region_height{};
    std::array<float, 2> jitter{};
    std::array<float, 2> motion_scale{};
    bool reset{};
    UpscaleOutcome outcome{UpscaleOutcome::Ran};
};
/// Per second: records, and the anomalies among them.
struct UpscaleAnomalies {
    std::uint64_t records{};
    std::uint64_t stale{};           ///< an input not written since the previous upscale
    std::uint64_t scale_mismatch{};  ///< inputs at different scales
    std::uint64_t region_mismatch{}; ///< inputs of different sizes, or a region outside them
    std::uint64_t not_run{};         ///< the upscaler did not run on them
    std::uint64_t resets{};
};
inline std::mutex g_upscale_ring_mutex;
inline std::array<UpscaleRecord, 256> g_upscale_ring{};
inline std::uint64_t g_upscale_ring_next{};
inline UpscaleAnomalies g_upscale_anomalies{};

inline void PushUpscale(const UpscaleRecord& record) {
    std::scoped_lock lock{g_upscale_ring_mutex};
    g_upscale_ring[g_upscale_ring_next++ % g_upscale_ring.size()] = record;
    auto& a = g_upscale_anomalies;
    ++a.records;
    const bool has_depth = record.depth.gpu_addr != 0;
    if (record.color.age > 0 || record.motion.age > 0 || (has_depth && record.depth.age > 0)) {
        ++a.stale;
    }
    if (record.motion.scaled != record.color.scaled ||
        (has_depth && record.depth.scaled != record.color.scaled)) {
        ++a.scale_mismatch;
    }
    const auto same_size = [&](const UpscaleInput& input) {
        return input.width == record.color.width && input.height == record.color.height;
    };
    if (!same_size(record.motion) || (has_depth && !same_size(record.depth)) ||
        record.region_width > record.color.width || record.region_height > record.color.height) {
        ++a.region_mismatch;
    }
    if (record.reset) {
        ++a.resets;
    }
    if (record.outcome != UpscaleOutcome::Ran) {
        ++a.not_run;
    }
}

/// The outcome of the record pushed last, decided after the push.
inline void SetUpscaleOutcome(UpscaleOutcome outcome) {
    std::scoped_lock lock{g_upscale_ring_mutex};
    if (g_upscale_ring_next == 0) {
        return;
    }
    UpscaleRecord& record = g_upscale_ring[(g_upscale_ring_next - 1) % g_upscale_ring.size()];
    if (record.outcome == UpscaleOutcome::Ran && outcome != UpscaleOutcome::Ran) {
        ++g_upscale_anomalies.not_run;
    }
    record.outcome = outcome;
}

/// The last `count` records, oldest first.
[[nodiscard]] inline std::vector<UpscaleRecord> UpscaleRing(std::size_t count) {
    std::scoped_lock lock{g_upscale_ring_mutex};
    const std::uint64_t have =
        std::min<std::uint64_t>(g_upscale_ring_next, g_upscale_ring.size());
    const std::uint64_t n = std::min<std::uint64_t>(count, have);
    std::vector<UpscaleRecord> out;
    out.reserve(static_cast<std::size_t>(n));
    for (std::uint64_t i = g_upscale_ring_next - n; i < g_upscale_ring_next; ++i) {
        out.push_back(g_upscale_ring[i % g_upscale_ring.size()]);
    }
    return out;
}

/// Rasterizer thread, each frame end: at most once a second, the upscaler's inputs when there
/// were upscales.
inline void FlushUpscaleInputs(std::uint64_t now_ms) {
    static std::uint64_t last_ms{};
    if (now_ms - last_ms < 1000) {
        return;
    }
    last_ms = now_ms;
    UpscaleAnomalies anomalies;
    UpscaleRecord last;
    {
        std::scoped_lock lock{g_upscale_ring_mutex};
        anomalies = std::exchange(g_upscale_anomalies, UpscaleAnomalies{});
        if (g_upscale_ring_next != 0) {
            last = g_upscale_ring[(g_upscale_ring_next - 1) % g_upscale_ring.size()];
        }
    }
    if (anomalies.records == 0) {
        return;
    }
    const auto input = [](const UpscaleInput& i) {
        return i.gpu_addr == 0 ? std::string{"none"}
                               : fmt::format("{:x} {}x{}{} age {}", i.gpu_addr, i.width,
                                             i.height, i.scaled ? " scaled" : "", i.age);
    };
    const bool clean =
        anomalies.stale + anomalies.scale_mismatch + anomalies.region_mismatch + anomalies.not_run ==
        0;
    LOG_INFO(HW_GPU,
             "Upscale inputs (last second): {} records, {} stale, {} scale_mismatch, {} "
             "region_mismatch, {} not_run, {} resets{}; last: color {}, depth {}, motion {}, "
             "region {}x{}, jitter {:.3f},{:.3f}, mvec scale {:.4f},{:.4f}, {}",
             anomalies.records, anomalies.stale, anomalies.scale_mismatch,
             anomalies.region_mismatch, anomalies.not_run, anomalies.resets,
             clean ? "" : " ANOMALY", input(last.color), input(last.depth), input(last.motion),
             last.region_width, last.region_height, last.jitter[0], last.jitter[1],
             last.motion_scale[0], last.motion_scale[1],
             UpscaleOutcomeNames[static_cast<std::size_t>(last.outcome)]);
}

} // namespace VideoCommon::Nx2Perf
