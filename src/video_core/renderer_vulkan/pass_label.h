// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Vulkan {

/// What a render pass draws into, for the per-pass GPU timing (Scheduler, pass_timing): the
/// first colour target and the depth target (guest address, guest size, format) and whether it
/// runs at the resolution scale.
struct PassLabel {
    u64 color_addr{};
    u64 depth_addr{};
    u32 width{};
    u32 height{};
    u32 color_format{}; ///< VideoCore::Surface::PixelFormat
    u32 depth_format{};
    u32 colors{};
    bool scaled{};
};

} // namespace Vulkan
