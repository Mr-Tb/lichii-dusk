// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>
#include <optional>
#include "common/settings.h"
#include "common/settings_enums.h"
#include "video_core/nx2_perf.h"

namespace VideoCommon::DlssMode {

using Quality = Settings::NvidiaDlssQuality;

struct Extent {
    u32 width;
    u32 height;
};

struct Range {
    Extent min;
    Extent max;
};

constexpr bool Fits(Extent render, Extent output, Quality mode, Range range) {
    return render.width != 0 && render.height != 0 && output.width != 0 && output.height != 0 &&
           render.width >= range.min.width && render.width <= range.max.width &&
           render.height >= range.min.height && render.height <= range.max.height &&
           (mode != Quality::Native ||
            (render.width == output.width && render.height == output.height));
}

constexpr Quality Preferred(Extent render, Extent output) {
    const float ratio =
        output.width ? static_cast<float>(render.width) / static_cast<float>(output.width) : 0.0f;
    if (ratio >= 0.99f) {
        return Quality::Native;
    }
    if (ratio >= 0.65f) {
        return Quality::Quality;
    }
    if (ratio >= 0.57f) {
        return Quality::Balanced;
    }
    if (ratio >= 0.49f) {
        return Quality::Performance;
    }
    return Quality::UltraPerformance;
}

struct Selection {
    std::optional<Quality> mode;
    std::optional<Range> requested_range;
    bool fallback{};
    Range range{}; ///< The chosen mode's render size range
};

template <typename Query>
Selection Select(Quality requested, Extent render, Extent output, Query&& query) {
    std::optional<Range> requested_range;
    if (requested != Quality::Automatic) {
        requested_range = query(requested);
        if (requested_range && Fits(render, output, requested, *requested_range)) {
            return {requested, requested_range, false, *requested_range};
        }
    }
    const auto preferred = Preferred(render, output);
    for (const auto mode : {preferred, Quality::Quality, Quality::Balanced, Quality::Performance,
                            Quality::UltraPerformance, Quality::Native}) {
        if (const auto range = query(mode); range && Fits(render, output, mode, *range)) {
            return {mode, requested_range, requested != Quality::Automatic, *range};
        }
    }
    return {std::nullopt, requested_range, requested != Quality::Automatic};
}

inline constexpr u64 DuskbloodsProgramId = 0x0400D1D02399A000ULL;

/// The Duskbloods renders a region of at most about 58% of its output (dynamic resolution): DLAA
/// (render size = output size) and Ultra Performance (a third) never fit it, so they run as
/// Automatic, and the upscaler button does not offer them.
constexpr Quality EffectiveQuality(u64 program_id, Quality mode) {
    return program_id == DuskbloodsProgramId &&
                   (mode == Quality::Native || mode == Quality::UltraPerformance)
               ? Quality::Automatic
               : mode;
}

/// The Duskbloods above 1x runs preset K in every mode: M (the setting's default, and NVIDIA's
/// own preset for Performance) draws pairs of dark rows every 64 output rows at 2x resolution
/// scale (3840x2160 output; 5 of 40 frames at Performance) and costs 14.0 ms there against K's
/// 12.3 on an RTX 3060. At 1x M runs as NVIDIA's preset for the mode. keep (EDEN_NX2_DUSK_DLSS_M=1)
/// runs the chosen preset as is.
constexpr Settings::NvidiaDlssPreset EffectivePreset(u64 program_id,
                                                     Settings::NvidiaDlssPreset preset, bool keep,
                                                     bool scaled) {
    if (program_id != DuskbloodsProgramId || keep) {
        return preset;
    }
    if (scaled) {
        return Settings::NvidiaDlssPreset::K;
    }
    return preset == Settings::NvidiaDlssPreset::M ? Settings::NvidiaDlssPreset::NvidiaDefault
                                                   : preset;
}

inline Settings::NvidiaDlssPreset EffectivePreset(u64 program_id,
                                                  Settings::NvidiaDlssPreset preset) {
    static const bool keep = [] {
        const char* const value = std::getenv("EDEN_NX2_DUSK_DLSS_M");
        return value && value[0] == '1';
    }();
    const auto& resolution = Settings::values.resolution_info;
    // The debug server's perf_flip dusk_dlss_m overrides the switch live.
    return EffectivePreset(program_id, preset, Nx2Perf::LiveOr(Nx2Perf::g_dusk_dlss_m, keep),
                           resolution.up_scale > (1U << resolution.down_shift));
}

} // namespace VideoCommon::DlssMode
