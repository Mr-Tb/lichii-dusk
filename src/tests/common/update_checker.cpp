// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <string>
#include <thread>
#include <vector>
#include <fmt/format.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "common/httplib.h"
#include "common/net/net.h"
#include "frontend_common/update_checker.h"

namespace {

Common::Net::Release MakeRelease(std::string tag, std::string filename) {
    const nlohmann::json json{
        {"tag_name", tag},
        {"name", "Lichii nx2 " + tag},
        {"prerelease", true},
        {"assets",
         {{{"browser_download_url",
            "https://github.com/Mr-Tb/lichii-dusk/releases/download/" + tag + "/" + filename}}}},
    };
    return Common::Net::Release::FromJson(json, "https://api.github.com", "Mr-Tb/lichii-dusk")
        .value();
}

} // namespace

TEST_CASE("Lichii updates compare build numbers, not tag strings", "[updater]") {
    const auto releases = std::array{
        MakeRelease("build-9", "lichii-windows-x86_64.zip"),
        MakeRelease("build-100", "lichii-windows-x86_64.zip"),
        MakeRelease("build-99", "lichii-windows-x86_64.zip"),
    };
    const auto update = UpdateChecker::SelectUpdate(releases, "build-70", "windows-x86_64");
    REQUIRE(update);
    CHECK(update->tag == "build-100");
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, "build-100", "windows-x86_64"));
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, "build-101", "windows-x86_64"));
    CHECK_FALSE(UpdateChecker::SelectUpdate({}, "build-70", "windows-x86_64"));
}

TEST_CASE("Lichii ignores dev builds and malformed release tags", "[updater]") {
    const std::string tag =
        GENERATE("", "nx2", "v0.2.0", "abc123-nx2", "nx2-build-71", "build-", "build-0", "build--1",
                 "build-+71", "build-71junk", "build- 71", "build-18446744073709551616");
    const auto releases = std::array{MakeRelease("build-71", "lichii-windows-x86_64.zip")};
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, tag, "windows-x86_64"));
    auto invalid = releases;
    invalid[0].tag = tag;
    invalid[0].assets = {"https://github.com/Mr-Tb/lichii-dusk/releases/download/" + tag +
                         "/lichii-windows-x86_64.zip"};
    CHECK_FALSE(UpdateChecker::SelectUpdate(invalid, "build-70", "windows-x86_64"));
}

TEST_CASE("Only builds with a release tag and platform compare against releases", "[updater]") {
    CHECK(UpdateChecker::IsReleaseBuild("build-71", "windows-x86_64"));
    CHECK(UpdateChecker::IsReleaseBuild("build-260", "linux-x86_64-pgo"));
    CHECK_FALSE(UpdateChecker::IsReleaseBuild("build-71", ""));
    const std::string tag =
        GENERATE("", "nx2", "abc1234567-nx2", "build-", "build-0", "build-71junk");
    CHECK_FALSE(UpdateChecker::IsReleaseBuild(tag, "windows-x86_64"));
}

TEST_CASE("A build newer than every release is never offered an older one", "[updater]") {
    const auto releases = std::array{
        MakeRelease("build-250", "lichii-windows-x86_64.zip"),
        MakeRelease("build-249", "lichii-windows-x86_64.zip"),
    };
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, "build-260", "windows-x86_64"));
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, "build-250", "windows-x86_64"));
    const auto update = UpdateChecker::SelectUpdate(releases, "build-249", "windows-x86_64");
    REQUIRE(update);
    CHECK(update->tag == "build-250");
}

TEST_CASE("Lichii selects only the matching platform and PGO variant", "[updater]") {
    const std::string platform =
        GENERATE("windows-x86_64", "windows-x86_64-pgo", "linux-x86_64", "linux-x86_64-pgo",
                 "linux-aarch64", "linux-aarch64-pgo", "macos-arm64");
    auto release = MakeRelease("build-71", "unrelated.zip");
    for (const auto name :
         {"windows-x86_64.zip", "windows-x86_64-pgo.zip", "linux-x86_64.tar.gz",
          "linux-x86_64-pgo.tar.gz", "linux-aarch64.tar.gz", "linux-aarch64-pgo.tar.gz",
          "macos-arm64.zip", "linux-x86_64.AppImage", "linux-x86_64-pgo.AppImage",
          "linux-aarch64.AppImage", "linux-aarch64-pgo.AppImage"}) {
        release.assets.push_back(
            "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/lichii-" +
            std::string{name});
    }
    const auto selected = release.GetPlatformAssets(platform);
    REQUIRE(selected.size() == 1);
    CHECK(selected[0].filename ==
          "lichii-" + platform + (platform.starts_with("linux-") ? ".AppImage" : ".zip"));
    CHECK(selected[0].url == "https://github.com");
    CHECK(selected[0].path ==
          "/Mr-Tb/lichii-dusk/releases/download/build-71/" + selected[0].filename);
    CHECK_FALSE(release.GetPlatformAssets("android-arm64").size());
    CHECK_FALSE(release.GetPlatformAssets("").size());
}

TEST_CASE("Lichii skips releases without its asset and falls back to Linux archives", "[updater]") {
    const auto releases = std::array{
        MakeRelease("build-73", "lichii-linux-x86_64.AppImage"),
        MakeRelease("build-71", "lichii-linux-x86_64-pgo.tar.gz"),
        MakeRelease("build-72", "lichii-linux-aarch64-pgo.AppImage"),
    };
    const auto update = UpdateChecker::SelectUpdate(releases, "build-70", "linux-x86_64");
    REQUIRE(update);
    CHECK(update->tag == "build-73");
    CHECK(update->GetPlatformAssets("linux-x86_64")[0].filename == "lichii-linux-x86_64.AppImage");
    CHECK_FALSE(UpdateChecker::SelectUpdate(releases, "build-70", "windows-x86_64"));

    const auto archive = UpdateChecker::SelectUpdate(releases, "build-70", "linux-x86_64-pgo",
                                                     Common::Net::LinuxPackage::Archive);
    REQUIRE(archive);
    CHECK(archive->tag == "build-71");
    CHECK(archive->GetPlatformAssets("linux-x86_64-pgo", Common::Net::LinuxPackage::Archive)[0]
              .filename == "lichii-linux-x86_64-pgo.tar.gz");
}

TEST_CASE("PGO builds fall back to the standard asset, standard builds never get PGO", "[updater]") {
    auto release = MakeRelease("build-71", "lichii-windows-x86_64.zip");
    const auto pgo = release.GetPlatformAssets("windows-x86_64-pgo");
    REQUIRE(pgo.size() == 1);
    CHECK(pgo[0].name == "Standard");
    CHECK(pgo[0].filename == "lichii-windows-x86_64.zip");

    release.assets.push_back(
        "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/lichii-windows-x86_64-pgo.zip");
    const auto preferred = release.GetPlatformAssets("windows-x86_64-pgo");
    REQUIRE(preferred.size() == 1);
    CHECK(preferred[0].name == "PGO");
    CHECK(preferred[0].filename == "lichii-windows-x86_64-pgo.zip");
    CHECK(release.GetPlatformAssets("windows-x86_64")[0].filename == "lichii-windows-x86_64.zip");

    const auto pgo_only = MakeRelease("build-72", "lichii-windows-x86_64-pgo.zip");
    CHECK(pgo_only.GetPlatformAssets("windows-x86_64").empty());
    CHECK_FALSE(UpdateChecker::SelectUpdate(std::array{pgo_only}, "build-70", "windows-x86_64"));
    CHECK(UpdateChecker::SelectUpdate(std::array{pgo_only}, "build-70", "windows-x86_64-pgo"));
    CHECK(release.GetPlatformAssets("macos-arm64-pgo").empty());
}

TEST_CASE("Linux installs only get the package they were installed from", "[updater]") {
    auto release = MakeRelease("build-71", "lichii-linux-x86_64.AppImage");
    release.assets.push_back(
        "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/lichii-linux-x86_64.tar.gz");
    CHECK(release.GetPlatformAssets("linux-x86_64", Common::Net::LinuxPackage::AppImage)[0]
              .filename == "lichii-linux-x86_64.AppImage");
    CHECK(release.GetPlatformAssets("linux-x86_64", Common::Net::LinuxPackage::Archive)[0]
              .filename == "lichii-linux-x86_64.tar.gz");

    const auto appimage_only = MakeRelease("build-72", "lichii-linux-x86_64.AppImage");
    CHECK(appimage_only.GetPlatformAssets("linux-x86_64", Common::Net::LinuxPackage::Archive)
              .empty());
}

TEST_CASE("Asset digest and size come from the release JSON", "[updater]") {
    const std::string sha(64, 'A');
    const nlohmann::json json{
        {"tag_name", "build-71"},
        {"assets",
         {{{"browser_download_url",
            "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/"
            "lichii-windows-x86_64.zip"},
           {"size", u64{1234}},
           {"digest", "sha256:" + sha}},
          {{"browser_download_url",
            "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/"
            "lichii-linux-x86_64.AppImage"},
           {"size", u64{5}},
           {"digest", "sha256:zz"}},
          {{"browser_download_url",
            "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/"
            "lichii-linux-x86_64.tar.gz"},
           {"digest", "md5:abc"}}}},
    };
    const auto release =
        Common::Net::Release::FromJson(json, "https://api.github.com", "Mr-Tb/lichii-dusk").value();
    const auto windows = release.GetPlatformAssets("windows-x86_64");
    REQUIRE(windows.size() == 1);
    CHECK(windows[0].sha256 == std::string(64, 'a'));
    CHECK(windows[0].size == 1234);
    const auto appimage = release.GetPlatformAssets("linux-x86_64");
    REQUIRE(appimage.size() == 1);
    CHECK(appimage[0].sha256.empty());
    CHECK(appimage[0].size == 5);
    const auto archive =
        release.GetPlatformAssets("linux-x86_64", Common::Net::LinuxPackage::Archive);
    REQUIRE(archive.size() == 1);
    CHECK(archive[0].sha256.empty());
    CHECK(archive[0].size == 0);
}

TEST_CASE("Failed release requests are classified by status", "[updater]") {
    CHECK(UpdateChecker::ClassifyFailure(403) == UpdateChecker::Failure::RateLimited);
    CHECK(UpdateChecker::ClassifyFailure(429) == UpdateChecker::Failure::RateLimited);
    CHECK(UpdateChecker::ClassifyFailure(404) == UpdateChecker::Failure::Http);
    CHECK(UpdateChecker::ClassifyFailure(500) == UpdateChecker::Failure::Http);
    CHECK(UpdateChecker::ClassifyFailure(0) == UpdateChecker::Failure::Network);
    CHECK(UpdateChecker::ClassifyFailure(200) == UpdateChecker::Failure::Network);
}

TEST_CASE("Lichii does not select Eden or lookalike download URLs", "[updater]") {
    const std::string url = GENERATE(
        "https://github.com/eden-emu/eden/releases/download/build-71/lichii-windows-x86_64.zip",
        "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-70/lichii-windows-x86_64.zip",
        "https://github.com/Mr-Tb/lichii-dusk/releases/download/build-71/"
        "not-lichii-windows-x86_64.zip",
        "https://github.com.example.com/Mr-Tb/lichii-dusk/releases/download/build-71/"
        "lichii-windows-x86_64.zip");
    auto release = MakeRelease("build-71", "lichii-windows-x86_64.zip");
    release.assets = {url};
    CHECK(release.GetPlatformAssets("windows-x86_64").empty());
}

TEST_CASE("GitHub failures and invalid JSON produce no releases", "[updater]") {
    const std::string_view json =
        GENERATE("", "not JSON", R"({"message":"API rate limit exceeded"})", "null", "[]",
                 R"([{"tag_name":42}])", R"([{"tag_name":"build-71","assets":null}])",
                 R"([{"tag_name":"build-71","draft":true}])");
    CHECK(Common::Net::Release::ListFromJson(json, "https://api.github.com", "Mr-Tb/lichii-dusk")
              .empty());
}

TEST_CASE("GitHub optional fields can be null or missing", "[updater]") {
    const std::string_view json = R"([{"tag_name":"build-71","name":null,"body":null}])";
    const auto releases =
        Common::Net::Release::ListFromJson(json, "https://api.github.com", "Mr-Tb/lichii-dusk");
    REQUIRE(releases.size() == 1);
    CHECK(releases[0].title == "build-71");
    CHECK(releases[0].assets.empty());
    CHECK(releases[0].html_url == "https://github.com/Mr-Tb/lichii-dusk/releases/tag/build-71");
}

TEST_CASE("Release requests handle HTTP failures without throwing", "[updater]") {
    const int status = GENERATE(200, 403, 429, 500);
    httplib::Server server;
    server.Get("/releases", [status](const httplib::Request& request, httplib::Response& response) {
        response.status =
            request.get_header_value("User-Agent") == "Lichii" &&
                    request.get_header_value("Accept") == "application/vnd.github+json"
                ? status
                : 400;
        response.set_content("[]", "application/json");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    const std::jthread listener([&server](std::stop_token stop) {
        const std::stop_callback stop_server(stop, [&server] { server.stop(); });
        server.listen_after_bind();
    });
    server.wait_until_ready();
    const auto response =
        Common::Net::MakeRequest("http://127.0.0.1:" + std::to_string(port), "/releases");
    CHECK(response.has_value() == (status == 200));
}

namespace {

using UpdateChecker::DuskVersion;

const std::string ValidDigest =
    "sha256:597200A1BFFA3D2ECFB2F3F2A464515D57D7F6DC674060F16719830C790BADA7";

Common::Net::Release MakeDuskRelease(const std::string& tag, const std::vector<std::string>& urls,
                                     const std::string& digest = ValidDigest,
                                     bool prerelease = false) {
    nlohmann::json assets = nlohmann::json::array();
    for (const auto& url : urls) {
        assets.push_back(
            {{"browser_download_url", url}, {"size", u64{68126892}}, {"digest", digest}});
    }
    const nlohmann::json json{{"tag_name", tag},
                              {"name", "Lichii for The Duskbloods " + tag},
                              {"prerelease", prerelease},
                              {"assets", assets}};
    return Common::Net::Release::FromJson(json, "https://api.github.com", "Mr-Tb/lichii-dusk")
        .value();
}

std::string GitHubUrl(std::string_view tag, std::string_view name = "Lichii-Duskbloods.zip") {
    return fmt::format("https://github.com/Mr-Tb/lichii-dusk/releases/download/{}/{}", tag, name);
}

} // namespace

TEST_CASE("Duskbloods versions compare numerically, with a patch number", "[updater]") {
    CHECK(UpdateChecker::ParseDuskVersion("4") == DuskVersion{4, 0});
    CHECK(UpdateChecker::ParseDuskVersion("4.1") == DuskVersion{4, 1});
    CHECK(UpdateChecker::ParseDuskVersion("12") == DuskVersion{12, 0});
    CHECK(UpdateChecker::ParseDuskVersion("4.10") == DuskVersion{4, 10});
    CHECK(UpdateChecker::ParseDuskVersion("4.1.1") == DuskVersion{4, 1, 1});
    CHECK(UpdateChecker::ParseDuskVersion("5.0.1") == DuskVersion{5, 0, 1});
    CHECK(UpdateChecker::ParseDuskVersion("999.999.999") == DuskVersion{999, 999, 999});
    CHECK(DuskVersion{4, 9} < DuskVersion{4, 10});
    CHECK(DuskVersion{4, 1} < DuskVersion{4, 1, 1});
    CHECK(DuskVersion{4, 1, 1} < DuskVersion{4, 2});
    CHECK(DuskVersion{4, 0} < DuskVersion{4, 1});
    CHECK(DuskVersion{4, 9} < DuskVersion{5, 0});
    CHECK(DuskVersion{9, 0} < DuskVersion{10, 0});
    CHECK(DuskVersion{4, 1, 1}.ToString() == "4.1.1");
    CHECK(DuskVersion{4, 1}.ToString() == "4.1");
    CHECK(DuskVersion{4, 0}.ToString() == "4");
    CHECK(DuskVersion{5, 0, 1}.ToString() == "5.0.1");

    const std::string invalid =
        GENERATE("", "0", "04", "4.", "4.0", "4.01", "4.a", "v4", " 4", "4 ", "-1", "+4", "4.1.",
                 "4.1.0", "4.1.02", "4.1.2.3", ".5", "4.1000", "4.1.1000", "99999999999");
    CHECK_FALSE(UpdateChecker::ParseDuskVersion(invalid));
}

TEST_CASE("Duskbloods tags are duskbloods-v and a version", "[updater]") {
    CHECK(UpdateChecker::ParseDuskTag("duskbloods-v4") == DuskVersion{4, 0});
    CHECK(UpdateChecker::ParseDuskTag("duskbloods-v4.1") == DuskVersion{4, 1});
    CHECK(UpdateChecker::ParseDuskTag("duskbloods-v4.1.1") == DuskVersion{4, 1, 1});

    const std::string tag =
        GENERATE("", "duskbloods-v", "duskbloods-4", "Duskbloods-v4", "duskbloods-v4-rc",
                 "duskbloods-v4.0", "build-71", "v4", "fritchii-v4", "lichii-duskbloods-v4");
    CHECK_FALSE(UpdateChecker::ParseDuskTag(tag));
}

TEST_CASE("The running version comes from dist/dusk_version.txt", "[updater]") {
    // The test build is configured from this tree, which carries dist/dusk_version.txt.
    const auto running = UpdateChecker::RunningVersion();
    REQUIRE(running);
    CHECK(running->major >= 4);
}

TEST_CASE("GitHub digests are parsed only when they are SHA-256", "[updater]") {
    CHECK(Common::Net::ParseSha256Digest(ValidDigest) ==
          "597200a1bffa3d2ecfb2f3f2a464515d57d7f6dc674060f16719830c790bada7");
    const std::string digest =
        GENERATE(std::string{}, std::string{"sha256:"}, "sha256:" + std::string(63, 'a'),
                 "sha256:" + std::string(65, 'a'), "sha256:" + std::string(63, 'a') + "g",
                 "SHA256:" + std::string(64, 'a'), "md5:" + std::string(64, 'a'),
                 " sha256:" + std::string(64, 'a'), std::string(64, 'a'));
    CHECK_FALSE(Common::Net::ParseSha256Digest(digest));
}

TEST_CASE("Duskbloods selects only Lichii-Duskbloods.zip from Mr-Tb/lichii-dusk", "[updater]") {
    const auto source = UpdateChecker::DefaultSource();
    CHECK(source.origin == "https://api.github.com");
    CHECK(source.api_path == "/repos/Mr-Tb/lichii-dusk/releases?per_page=100");
    CHECK_FALSE(source.test);

    const std::string tag = "duskbloods-v5";
    const auto release =
        MakeDuskRelease(tag, {GitHubUrl(tag, "Lichii-Duskbloods-debug.zip"), GitHubUrl(tag)});
    const auto asset = UpdateChecker::SelectDuskAsset(release, source);
    REQUIRE(asset);
    CHECK(asset->filename == "Lichii-Duskbloods.zip");
    CHECK(asset->url == "https://github.com");
    CHECK(asset->path ==
          "/Mr-Tb/lichii-dusk/releases/download/duskbloods-v5/Lichii-Duskbloods.zip");
    CHECK(asset->sha256 == "597200a1bffa3d2ecfb2f3f2a464515d57d7f6dc674060f16719830c790bada7");
    CHECK(asset->size == 68126892);

    // Selected, but without a digest the package must not be installed: sha256 stays empty.
    const std::string bad_digest = GENERATE("sha256:short", "md5:abc", "");
    const auto unverified =
        UpdateChecker::SelectDuskAsset(MakeDuskRelease(tag, {GitHubUrl(tag)}, bad_digest), source);
    REQUIRE(unverified);
    CHECK(unverified->sha256.empty());
}

TEST_CASE("Duskbloods ignores other zips and lookalike URLs", "[updater]") {
    const std::string url = GENERATE(
        GitHubUrl("duskbloods-v5", "Lichii-Duskbloods-debug.zip"),
        GitHubUrl("duskbloods-v5", "lichii-duskbloods.zip"),
        GitHubUrl("duskbloods-v5", "Lichii-Duskbloods.zip.sha256"), GitHubUrl("duskbloods-v4"),
        std::string{"https://github.com/Mr-Tb/other/releases/download/duskbloods-v5/"
                    "Lichii-Duskbloods.zip"},
        std::string{"https://github.com/zortos293/Lichii/releases/download/duskbloods-v5/"
                    "Lichii-Duskbloods.zip"},
        std::string{"http://github.com/Mr-Tb/lichii-dusk/releases/download/duskbloods-v5/"
                    "Lichii-Duskbloods.zip"},
        std::string{"https://github.com.evil.com/Mr-Tb/lichii-dusk/releases/download/"
                    "duskbloods-v5/Lichii-Duskbloods.zip"},
        std::string{"http://127.0.0.1:8000/Lichii-Duskbloods.zip"},
        std::string{"https://github.com/Mr-Tb/lichii-dusk/archive/refs/tags/duskbloods-v5.zip"});
    const auto release = MakeDuskRelease("duskbloods-v5", {url});
    CHECK_FALSE(UpdateChecker::SelectDuskAsset(release, UpdateChecker::DefaultSource()));
}

TEST_CASE("Duskbloods picks the newest newer release that has a package", "[updater]") {
    const auto releases = std::array{
        MakeDuskRelease("duskbloods-v3", {GitHubUrl("duskbloods-v3")}),
        MakeDuskRelease("duskbloods-v4", {GitHubUrl("duskbloods-v4")}),
        MakeDuskRelease("duskbloods-v6", {GitHubUrl("duskbloods-v6", "other.zip")}),
        MakeDuskRelease("duskbloods-v4.1", {GitHubUrl("duskbloods-v4.1")}),
        MakeDuskRelease("duskbloods-v4.1.1", {GitHubUrl("duskbloods-v4.1.1")}),
        MakeDuskRelease("build-99", {GitHubUrl("build-99")}),
    };
    const auto source = UpdateChecker::DefaultSource();
    const auto select = [&](DuskVersion current) {
        const auto update = UpdateChecker::SelectDuskUpdate(releases, current, source);
        return update ? update->tag : std::string{};
    };
    CHECK(select({3, 0}) == "duskbloods-v4.1.1");
    CHECK(select({4, 0}) == "duskbloods-v4.1.1");
    CHECK(select({4, 1}) == "duskbloods-v4.1.1");
    CHECK(select({4, 1, 1}).empty());
    CHECK(select({6, 0}).empty());
    CHECK_FALSE(UpdateChecker::SelectDuskUpdate({}, {4, 0}, source));
}

TEST_CASE("Duskbloods never offers drafts, prereleases or releases hidden from it", "[updater]") {
    auto hidden = MakeDuskRelease("duskbloods-v9", {GitHubUrl("duskbloods-v9")});
    hidden.body = "Big news!\n<!-- dusk:no-update -->\nMore text";
    const auto releases = std::array{
        MakeDuskRelease("duskbloods-v4.1", {GitHubUrl("duskbloods-v4.1")}),
        MakeDuskRelease("duskbloods-v5", {GitHubUrl("duskbloods-v5")}, ValidDigest, true),
        hidden,
    };
    const auto source = UpdateChecker::DefaultSource();
    CHECK(UpdateChecker::HiddenFromUpdater(releases[2]));
    CHECK_FALSE(UpdateChecker::HiddenFromUpdater(releases[0]));
    const auto update = UpdateChecker::SelectDuskUpdate(releases, {4, 0}, source);
    REQUIRE(update);
    CHECK(update->tag == "duskbloods-v4.1");
    CHECK_FALSE(UpdateChecker::SelectDuskUpdate(releases, {4, 1}, source));

    const nlohmann::json draft = nlohmann::json::array(
        {{{"tag_name", "duskbloods-v6"},
          {"draft", true},
          {"assets", {{{"browser_download_url", GitHubUrl("duskbloods-v6")}}}}}});
    CHECK(Common::Net::Release::ListFromJson(draft, "https://api.github.com", "Mr-Tb/lichii-dusk")
              .empty());
}

TEST_CASE("EDEN_UPDATE_URL takes https anywhere and plain http only on loopback", "[updater]") {
    const auto local = UpdateChecker::ParseSourceOverride("http://127.0.0.1:8000/releases.json");
    REQUIRE(local);
    CHECK(local->origin == "http://127.0.0.1:8000");
    CHECK(local->api_path == "/releases.json");
    CHECK(local->test);
    CHECK(UpdateChecker::ParseSourceOverride("http://localhost:8000")->api_path == "/");
    CHECK(UpdateChecker::ParseSourceOverride("http://LocalHost/x.json"));
    CHECK(UpdateChecker::ParseSourceOverride("http://[::1]:8000/x.json")->origin ==
          "http://[::1]:8000");
    CHECK(UpdateChecker::ParseSourceOverride("https://example.com/r.json")->origin ==
          "https://example.com");

    const std::string url =
        GENERATE("", "127.0.0.1:8000/r.json", "http://example.com/r.json",
                 "http://127.0.0.1.evil.com/r.json", "http://10.0.0.1/r.json",
                 "http://localhost@evil.com/r.json", "ftp://127.0.0.1/r.json", "file:///C:/r.json",
                 "http://127.0.0.1 /r.json", "http:///r.json", "https://");
    CHECK_FALSE(UpdateChecker::ParseSourceOverride(url));
}

TEST_CASE("A test source accepts packages only from its own origin", "[updater]") {
    const auto source = UpdateChecker::ParseSourceOverride("http://127.0.0.1:8000/releases.json");
    REQUIRE(source);
    const auto local =
        MakeDuskRelease("duskbloods-v5", {"http://127.0.0.1:8000/Lichii-Duskbloods.zip"});
    const auto asset = UpdateChecker::SelectDuskAsset(local, *source);
    REQUIRE(asset);
    CHECK(asset->url == "http://127.0.0.1:8000");
    CHECK(asset->path == "/Lichii-Duskbloods.zip");

    const std::string url =
        GENERATE(std::string{"http://127.0.0.1:9000/Lichii-Duskbloods.zip"},
                 std::string{"http://127.0.0.1:80000/Lichii-Duskbloods.zip"},
                 std::string{"http://127.0.0.1:8000/../Lichii-Duskbloods.zip"},
                 std::string{"http://127.0.0.1:8000/x?/Lichii-Duskbloods.zip"},
                 std::string{"http://127.0.0.1:8000/x#/Lichii-Duskbloods.zip"},
                 std::string{"http://127.0.0.1:8000/x\\/Lichii-Duskbloods.zip"},
                 GitHubUrl("duskbloods-v5"));
    CHECK_FALSE(UpdateChecker::SelectDuskAsset(MakeDuskRelease("duskbloods-v5", {url}), *source));
}

TEST_CASE("Release notes in the update offer start at the changes", "[updater]") {
    const std::string banner = "[العربية](https://example/#arabic)\n\n"
                               "# ⚠️ The launcher is NOT in the zip\n\n"
                               "### Download it\n\n"
                               "# \U0001f6e1️ Defender may call it a virus\n\n"
                               "- why\n\n"
                               "Install steps.\n\n";
    CHECK(UpdateChecker::CleanReleaseNotes(banner + "**New in v4**\n- faster\n") ==
          "**New in v4**\n- faster");
    CHECK(UpdateChecker::CleanReleaseNotes(banner + "## Changes\n- faster\n<!-- dusk:no-update -->") ==
          "## Changes\n- faster");
    // Without a heading to start at, only the language link goes.
    CHECK(UpdateChecker::CleanReleaseNotes("[العربية](x)\n"
                                           "# ⚠️ Warning\nbody") ==
          "# ⚠️ Warning\nbody");
    CHECK(UpdateChecker::CleanReleaseNotes("Plain <!-- hidden -->notes") == "Plain notes");
    CHECK(UpdateChecker::CleanReleaseNotes(banner + "<!-- dusk:notes-start -->\n**New in v5**\n"
                                                    "- x <!-- c -->\n<!-- dusk:notes-end -->\ntail") ==
          "**New in v5**\n- x");
    // An unterminated comment hides the rest.
    CHECK(UpdateChecker::CleanReleaseNotes("notes <!-- open") == "notes");
}
