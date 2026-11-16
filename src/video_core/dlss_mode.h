// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>
#include <optional>
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

/// Split scale (The Duskbloods above 1x): the upscaler's inputs stay at 1x and its output is the
/// render size times the mode's factor, at most the scaled target; a spatial stretch covers the
/// rest. Automatic (and FSR 3.1) take a factor just under 2, which DLSS runs as Performance.
constexpr Extent SplitOutput(Quality mode, Extent render, Extent target) {
    float factor = 1.95f;
    switch (mode) {
    case Quality::Quality:
        factor = 1.5f;
        break;
    case Quality::Balanced:
        factor = 1.72f;
        break;
    default:
        break;
    }
    if (render.width == 0 || render.height == 0) {
        return target;
    }
    const float fit_w = static_cast<float>(target.width) / static_cast<float>(render.width);
    const float fit_h = static_cast<float>(target.height) / static_cast<float>(render.height);
    factor = factor < fit_w ? factor : fit_w;
    factor = factor < fit_h ? factor : fit_h;
    const auto scale = [factor](u32 size, u32 limit) {
        const u32 scaled = static_cast<u32>(static_cast<float>(size) * factor + 0.5f);
        return scaled < limit ? scaled : limit;
    };
    const Extent output{scale(render.width, target.width), scale(render.height, target.height)};
    // Within 1% of the target (a region of the target's aspect): the target itself.
    return output.width * 100 >= target.width * 99 && output.height * 100 >= target.height * 99
               ? target
               : output;
}

/// The Duskbloods' split scale is on (perf_flip split_scale, EDEN_NX2_DUSK_SPLIT=1) and the
/// resolution scale is above 1x.
inline bool SplitScaleActive(u64 program_id, bool scaled) {
    return scaled && program_id == DuskbloodsProgramId &&
           Nx2Perf::LiveOr(Nx2Perf::g_split_scale, false);
}

/// The Duskbloods with the default preset M, in every mode, runs NVIDIA's preset for the mode
/// (K at DLAA, Quality and Balanced): M draws thin horizontal lines there at 2x resolution scale
/// (3840x2160 output, as in Fire Emblem) and costs more. keep_m (EDEN_NX2_DUSK_DLSS_M=1) runs M.
constexpr Settings::NvidiaDlssPreset EffectivePreset(u64 program_id,
                                                     Settings::NvidiaDlssPreset preset,
                                                     bool keep_m) {
    return program_id == DuskbloodsProgramId && preset == Settings::NvidiaDlssPreset::M &&
                   !keep_m
               ? Settings::NvidiaDlssPreset::NvidiaDefault
               : preset;
}

inline Settings::NvidiaDlssPreset EffectivePreset(u64 program_id,
                                                  Settings::NvidiaDlssPreset preset) {
    static const bool keep_m = [] {
        const char* const value = std::getenv("EDEN_NX2_DUSK_DLSS_M");
        return value && value[0] == '1';
    }();
    // The debug server's perf_flip dusk_dlss_m overrides the switch live.
    return EffectivePreset(program_id, preset, Nx2Perf::LiveOr(Nx2Perf::g_dusk_dlss_m, keep_m));
}

} // namespace VideoCommon::DlssMode
