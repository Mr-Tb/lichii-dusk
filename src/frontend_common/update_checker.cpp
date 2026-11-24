// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <fmt/format.h>
#include "common/logging.h"
#include "common/scm_rev.h"
#include "frontend_common/update_checker.h"

namespace UpdateChecker {

namespace {

std::optional<u64> ParseBuildNumber(std::string_view tag) {
    constexpr std::string_view prefix = "build-";
    if (!tag.starts_with(prefix)) {
        return std::nullopt;
    }
    tag.remove_prefix(prefix.size());
    u64 number{};
    const auto [end, error] = std::from_chars(tag.data(), tag.data() + tag.size(), number);
    if (error != std::errc{} || end != tag.data() + tag.size() || number == 0) {
        return std::nullopt;
    }
    return number;
}

constexpr std::string_view ReleaseRepo = "Mr-Tb/lichii-dusk";

std::optional<u32> ParseNumber(std::string_view text) {
    if (text.empty() || (text.size() > 1 && text.front() == '0') ||
        !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; })) {
        return std::nullopt;
    }
    u32 number{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return number;
}

CheckResult CheckDusk([[maybe_unused]] const DuskVersion& current) {
#ifndef _WIN32
    // Only Windows packages are published.
    return {};
#else
    auto source = DefaultSource();
    if (const char* override_url = std::getenv("EDEN_UPDATE_URL");
        override_url != nullptr && *override_url != '\0') {
        const auto parsed = ParseSourceOverride(override_url);
        if (!parsed) {
            LOG_ERROR(Frontend,
                      "Ignoring EDEN_UPDATE_URL {}: it must be https, or http on a loopback host",
                      override_url);
            return {.failure = Failure::Network};
        }
        LOG_WARNING(Frontend, "Checking for updates at {} (EDEN_UPDATE_URL, for testing only)",
                    override_url);
        source = *parsed;
    }

    const auto response = Common::Net::Fetch(source.origin, source.api_path);
    if (!response.body) {
        LOG_WARNING(Frontend, "Update check failed with status {}", response.status);
        return {.failure = ClassifyFailure(response.status), .status = response.status};
    }
    const auto releases = Common::Net::Release::ListFromJson(
        std::string_view{*response.body}, source.origin, std::string{ReleaseRepo});
    auto update = SelectDuskUpdate(releases, current, source);
    if (!update) {
        return {};
    }
    auto asset = SelectDuskAsset(*update, source);
    LOG_INFO(Frontend, "Received update {} ({})", update->title, update->tag);
    return {.update = std::move(update), .asset = std::move(asset)};
#endif
}

} // namespace

Failure ClassifyFailure(int status) {
    if (status == 403 || status == 429) {
        return Failure::RateLimited;
    }
    return status >= 400 ? Failure::Http : Failure::Network;
}

std::optional<Common::Net::Release> SelectUpdate(std::span<const Common::Net::Release> releases,
                                                 std::string_view build_tag,
                                                 std::string_view platform,
                                                 Common::Net::LinuxPackage package) {
    auto newest = ParseBuildNumber(build_tag);
    if (!newest) {
        return std::nullopt;
    }

    const Common::Net::Release* selected = nullptr;
    for (const auto& release : releases) {
        const auto number = ParseBuildNumber(release.tag);
        if (number && *number > *newest && !release.GetPlatformAssets(platform, package).empty()) {
            newest = number;
            selected = &release;
        }
    }
    return selected ? std::make_optional(*selected) : std::nullopt;
}

bool IsReleaseBuild(std::string_view build_tag, std::string_view platform) {
    return ParseBuildNumber(build_tag) && !platform.empty();
}

CheckResult Check() {
    if (const auto running = RunningVersion()) {
        return CheckDusk(*running);
    }
    if (!IsReleaseBuild(Common::g_build_release_tag, Common::g_build_release_platform)) {
        return {.failure = Failure::NotReleaseBuild};
    }

    const auto list = Common::Net::GetReleases();
    if (!list.ok) {
        const auto failure = ClassifyFailure(list.status);
        LOG_WARNING(Frontend, "Update check failed with status {}", list.status);
        return {.failure = failure, .status = list.status};
    }

    auto update = SelectUpdate(list.releases, Common::g_build_release_tag,
                               Common::g_build_release_platform,
                               Common::Net::CurrentLinuxPackage());
    if (update) {
        LOG_INFO(Frontend, "Received update {}", update->title);
    }
    return {.update = std::move(update)};
}

std::optional<Common::Net::Release> GetUpdate() {
    return Check().update;
}

std::string DuskVersion::ToString() const {
    if (patch != 0) {
        return fmt::format("{}.{}.{}", major, minor, patch);
    }
    return minor == 0 ? fmt::format("{}", major) : fmt::format("{}.{}", major, minor);
}

std::optional<DuskVersion> ParseDuskVersion(std::string_view text) {
    const auto dot = text.find('.');
    const auto major = ParseNumber(text.substr(0, dot));
    if (!major || *major == 0) {
        return std::nullopt;
    }
    if (dot == std::string_view::npos) {
        return DuskVersion{.major = *major};
    }
    // N, N.M or N.M.P with 1-3 digits and no leading zeros, and N.0 is spelled N, so each version
    // has one tag (N.0.P is allowed: a fix of N).
    const auto rest = text.substr(dot + 1);
    const auto dot2 = rest.find('.');
    const std::string_view minor_text = rest.substr(0, dot2);
    const auto plain = [](std::string_view digits) {
        return !digits.empty() && digits.size() <= 3 && (digits.size() == 1 || digits[0] != '0');
    };
    const auto minor = ParseNumber(minor_text);
    if (!plain(minor_text) || !minor) {
        return std::nullopt;
    }
    if (dot2 == std::string_view::npos) {
        if (*minor == 0) {
            return std::nullopt;
        }
        return DuskVersion{.major = *major, .minor = *minor};
    }
    const std::string_view patch_text = rest.substr(dot2 + 1);
    const auto patch = ParseNumber(patch_text);
    if (!plain(patch_text) || !patch || *patch == 0) {
        return std::nullopt;
    }
    return DuskVersion{.major = *major, .minor = *minor, .patch = *patch};
}

std::optional<DuskVersion> RunningVersion() {
    // The number is kept apart from the name and the label ("Lichii", "v4.1 test 9").
    return ParseDuskVersion(Common::g_build_dusk_version);
}

std::optional<DuskVersion> ParseDuskTag(std::string_view tag) {
    constexpr std::string_view prefix = "duskbloods-v";
    if (!tag.starts_with(prefix)) {
        return std::nullopt;
    }
    return ParseDuskVersion(tag.substr(prefix.size()));
}

Source DefaultSource() {
    return {.origin = "https://api.github.com",
            .api_path = fmt::format("/repos/{}/releases?per_page=100", ReleaseRepo)};
}

std::optional<Source> ParseSourceOverride(std::string_view url) {
    bool https = false;
    if (url.starts_with("https://")) {
        https = true;
        url.remove_prefix(8);
    } else if (url.starts_with("http://")) {
        url.remove_prefix(7);
    } else {
        return std::nullopt;
    }
    if (std::ranges::any_of(url, [](unsigned char c) { return c <= ' ' || c >= 0x7f; })) {
        return std::nullopt;
    }
    const auto slash = url.find('/');
    const auto authority = url.substr(0, slash);
    if (authority.empty() || authority.find_first_of("@?#\\") != std::string_view::npos) {
        return std::nullopt;
    }
    if (!https) {
        std::string host{authority.starts_with('[') ? authority.substr(0, authority.find(']') + 1)
                                                    : authority.substr(0, authority.find(':'))};
        std::ranges::transform(host, host.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (host != "127.0.0.1" && host != "localhost" && host != "[::1]") {
            return std::nullopt;
        }
    }
    return Source{.origin = fmt::format("{}://{}", https ? "https" : "http", authority),
                  .api_path = slash == std::string_view::npos ? std::string{"/"}
                                                              : std::string{url.substr(slash)},
                  .test = true};
}

std::optional<Common::Net::Asset> SelectDuskAsset(const Common::Net::Release& release,
                                                  const Source& source) {
    if (!ParseDuskTag(release.tag)) {
        return std::nullopt;
    }
    const std::string filename{DuskAssetName};
    for (const auto& url : release.assets) {
        std::string origin;
        if (source.test) {
            if (url.starts_with(source.origin + "/") && url.ends_with("/" + filename) &&
                url.find("..") == std::string::npos &&
                url.find_first_of("?#\\") == std::string::npos) {
                origin = source.origin;
            }
        } else if (url == fmt::format("https://github.com/{}/releases/download/{}/{}", ReleaseRepo,
                                      release.tag, filename)) {
            origin = "https://github.com";
        }
        if (origin.empty()) {
            continue;
        }
        Common::Net::Asset asset{.name = "Duskbloods",
                                 .url = origin,
                                 .path = url.substr(origin.size()),
                                 .filename = filename};
        if (const auto info = release.asset_info.find(url); info != release.asset_info.end()) {
            asset.sha256 = info->second.sha256;
            asset.size = info->second.size;
        }
        return asset;
    }
    return std::nullopt;
}

bool HiddenFromUpdater(const Common::Net::Release& release) {
    return release.body.find(HiddenReleaseMarker) != std::string::npos;
}

std::optional<Common::Net::Release> SelectDuskUpdate(
    std::span<const Common::Net::Release> releases, const DuskVersion& current,
    const Source& source) {
    const Common::Net::Release* selected = nullptr;
    DuskVersion newest = current;
    for (const auto& release : releases) {
        if (release.prerelease) {
            continue;
        }
        if (HiddenFromUpdater(release)) {
            LOG_INFO(Frontend, "Update check skips {}: its notes hide it from the updater",
                     release.tag);
            continue;
        }
        const auto version = ParseDuskTag(release.tag);
        if (version && *version > newest && SelectDuskAsset(release, source)) {
            newest = *version;
            selected = &release;
        }
    }
    return selected ? std::make_optional(*selected) : std::nullopt;
}

std::string CleanReleaseNotes(std::string_view body) {
    constexpr std::string_view start_marker = "<!-- dusk:notes-start -->";
    constexpr std::string_view end_marker = "<!-- dusk:notes-end -->";
    const auto start = body.find(start_marker);
    const auto end = start == std::string_view::npos ? start : body.find(end_marker, start);
    const bool marked = end != std::string_view::npos;
    if (marked) {
        body = body.substr(start + start_marker.size(), end - start - start_marker.size());
    }

    std::string text;
    for (std::size_t pos = 0; pos < body.size();) {
        const auto open = body.find("<!--", pos);
        text.append(body.substr(pos, open - pos));
        if (open == std::string_view::npos) {
            break;
        }
        const auto close = body.find("-->", open + 4);
        pos = close == std::string_view::npos ? body.size() : close + 3;
    }

    const auto trim = [](std::string& s) {
        constexpr const char* blank = " \t\r\n";
        s.erase(0, s.find_first_not_of(blank));
        s.erase(s.find_last_not_of(blank) + 1);
    };
    trim(text);
    if (marked) {
        return text;
    }

    const auto line_end = [&text](std::size_t pos) {
        const auto newline = text.find('\n', pos);
        return newline == std::string::npos ? text.size() : newline + 1;
    };
    if (text.starts_with("[\u0627\u0644\u0639\u0631\u0628\u064a\u0629](")) {
        text.erase(0, line_end(0));
        trim(text);
    }
    if (text.starts_with("# \u26a0") || text.starts_with("# \U0001f6e1")) {
        for (std::size_t pos = 0; pos < text.size(); pos = line_end(pos)) {
            const std::string_view line{text.data() + pos, text.size() - pos};
            if (line.starts_with("**New in") || line.starts_with("## ")) {
                text.erase(0, pos);
                break;
            }
        }
    }
    return text;
}

} // namespace UpdateChecker
