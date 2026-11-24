// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "common/net/net.h"

namespace UpdateChecker {

enum class Failure {
    None,
    // GitHub refused the request because of rate limiting.
    RateLimited,
    // GitHub answered with an HTTP error.
    Http,
    // No usable answer arrived, for example when offline.
    Network,
    // The running build has no Duskbloods version, build-N tag or release platform to compare
    // against.
    NotReleaseBuild,
};

struct CheckResult {
    std::optional<Common::Net::Release> update;
    // The package to download for `update` on Duskbloods builds. Its sha256 is empty when the
    // release published no well-formed digest, and such a package must never be installed.
    std::optional<Common::Net::Asset> asset;
    Failure failure{Failure::None};
    int status{};
};

// Classify a failed release request by its HTTP status, or 0 when no response arrived.
Failure ClassifyFailure(int status);

// Whether a build with this tag and platform can be compared against published releases.
bool IsReleaseBuild(std::string_view build_tag, std::string_view platform);

CheckResult Check();
std::optional<Common::Net::Release> GetUpdate();
std::optional<Common::Net::Release> SelectUpdate(
    std::span<const Common::Net::Release> releases, std::string_view build_tag,
    std::string_view platform, Common::Net::LinuxPackage package = Common::Net::LinuxPackage::Any);

// Lichii for The Duskbloods releases are numbered N, N.M or N.M.P: 4 < 4.1 < 4.1.1 < 4.10 < 5.
struct DuskVersion {
    u32 major{};
    u32 minor{};
    u32 patch{};

    auto operator<=>(const DuskVersion&) const = default;
    std::string ToString() const;
};

// "4", "4.1", "4.10" or "4.1.1", as in dist/dusk_version.txt. N.0 is spelled N.
std::optional<DuskVersion> ParseDuskVersion(std::string_view text);

// The version of the running build, from Common::g_build_dusk_version; nullopt for other builds.
std::optional<DuskVersion> RunningVersion();

// "duskbloods-v4", "duskbloods-v4.1".
std::optional<DuskVersion> ParseDuskTag(std::string_view tag);

// The Windows package every release carries.
inline constexpr std::string_view DuskAssetName = "Lichii-Duskbloods.zip";

// Where releases are listed and downloaded from.
struct Source {
    // Scheme and authority of the release list, such as "https://api.github.com".
    std::string origin;
    std::string api_path;
    // Set by EDEN_UPDATE_URL: packages are then accepted from `origin` instead of GitHub.
    bool test{};
};

Source DefaultSource();

// Test-only release list from EDEN_UPDATE_URL, such as "http://127.0.0.1:8000/releases.json".
// Plain HTTP is accepted only for a loopback host.
std::optional<Source> ParseSourceOverride(std::string_view url);

// The Windows package of a release, when its URL belongs to `source`.
std::optional<Common::Net::Asset> SelectDuskAsset(const Common::Net::Release& release,
                                                  const Source& source);

// Put in a release's notes to keep the updater from ever offering it (a test build, a pulled
// release): an HTML comment, so GitHub shows nothing of it on the release page.
inline constexpr std::string_view HiddenReleaseMarker = "<!-- dusk:no-update -->";

// Whether the release's notes carry HiddenReleaseMarker.
[[nodiscard]] bool HiddenFromUpdater(const Common::Net::Release& release);

// The newest release that is newer than `current`, has a Windows package and is neither a
// prerelease nor hidden from the updater. Drafts never reach here (Release::FromJson drops them).
std::optional<Common::Net::Release> SelectDuskUpdate(
    std::span<const Common::Net::Release> releases, const DuskVersion& current,
    const Source& source);

// The part of a release's notes worth showing in the update offer. HTML comments are dropped.
// Between <!-- dusk:notes-start --> and <!-- dusk:notes-end --> only that part is kept; without
// them the leading language link and "# " warning blocks are dropped, up to the first "**New in"
// or "## " line.
std::string CleanReleaseNotes(std::string_view body);

} // namespace UpdateChecker
