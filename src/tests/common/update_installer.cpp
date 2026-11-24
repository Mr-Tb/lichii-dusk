// SPDX-FileCopyrightText: Copyright 2026 Lichii Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frontend_common/update_installer.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

const fs::path Exe{"lichii.exe"};

class TempDir {
public:
    TempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("lichii-update-test-" + std::to_string(stamp));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path path;
};

void Write(const fs::path& file, std::string_view text) {
    fs::create_directories(file.parent_path());
    std::ofstream{file, std::ios::binary | std::ios::trunc} << text;
}

std::string Read(const fs::path& file) {
    std::ifstream stream{file, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream}, {}};
}

// An install of version 4 with user data, and the unpacked version 5 package beside it.
struct Install {
    TempDir dir;
    fs::path root = dir.path / "Lichii-Duskbloods";
    fs::path package = root / UpdateInstaller::StagingName / "unpacked" / "Lichii-Duskbloods";

    Install() {
        Write(root / "lichii.exe", "v4");
        Write(root / "Qt6Core.dll", "v4");
        Write(root / "plugins" / "a.dll", "v4");
        Write(root / "user" / "config" / "qt-config.ini", "settings");
        Write(root / "nx2_switches.txt", "switches");
        Write(root / "notes.txt", "mine");
        Write(root / "Play.Duskbloods.exe", "launcher");
        Write(root / "launcher_settings.json", "{}");

        Write(package / "lichii.exe", "v5");
        Write(package / "Qt6Core.dll", "v5");
        Write(package / "plugins" / "a.dll", "v5");
        Write(package / "plugins" / "new.dll", "v5");
        Write(package / "user" / "config" / "qt-config.ini", "replaced");
        Write(package / "nx2_switches.txt", "replaced");
    }

    void CheckUserData() const {
        CHECK(Read(root / "user" / "config" / "qt-config.ini") == "settings");
        CHECK(Read(root / "nx2_switches.txt") == "switches");
        CHECK(Read(root / "notes.txt") == "mine");
        CHECK(Read(root / "Play.Duskbloods.exe") == "launcher");
        CHECK(Read(root / "launcher_settings.json") == "{}");
    }
};

} // namespace

TEST_CASE("Updates replace only the package's files and keep one rollback copy", "[updater]") {
    Install install;
    std::error_code ec;
    REQUIRE(UpdateInstaller::Install(install.root, install.package, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK_FALSE(ec);

    const auto old_dir = install.root / UpdateInstaller::OldName;
    CHECK(Read(install.root / "lichii.exe") == "v5");
    CHECK(Read(install.root / "Qt6Core.dll") == "v5");
    CHECK(Read(install.root / "plugins" / "new.dll") == "v5");
    CHECK(Read(old_dir / "lichii.exe") == "v4");
    CHECK(Read(old_dir / "plugins" / "a.dll") == "v4");
    CHECK_FALSE(fs::exists(old_dir / "plugins" / "new.dll"));
    CHECK_FALSE(fs::exists(old_dir / "user"));
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::JournalName));
    CHECK(UpdateInstaller::PreviousVersion(install.root, Exe) == "4");
    install.CheckUserData();

    // Rolling back restores version 4, and rolling back again restores version 5.
    REQUIRE(UpdateInstaller::RollBack(install.root, "5", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK(Read(install.root / "lichii.exe") == "v4");
    CHECK(Read(install.root / "plugins" / "a.dll") == "v4");
    CHECK_FALSE(fs::exists(install.root / "plugins" / "new.dll"));
    CHECK(UpdateInstaller::PreviousVersion(install.root, Exe) == "5");
    install.CheckUserData();

    REQUIRE(UpdateInstaller::RollBack(install.root, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK(Read(install.root / "lichii.exe") == "v5");
    CHECK(Read(install.root / "plugins" / "new.dll") == "v5");
    CHECK(UpdateInstaller::PreviousVersion(install.root, Exe) == "4");
    install.CheckUserData();

    UpdateInstaller::CleanStaging(install.root);
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::StagingName));
}

TEST_CASE("Without a manifest an update removes nothing and then writes one", "[updater]") {
    Install install;
    Write(install.root / "avcodec-63.dll", "v4");
    std::error_code ec;
    REQUIRE(UpdateInstaller::Install(install.root, install.package, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK(Read(install.root / "avcodec-63.dll") == "v4");
    CHECK(Read(install.root / UpdateInstaller::ManifestName) ==
          "Qt6Core.dll\nplugins/a.dll\nplugins/new.dll\nlichii.exe\n");
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::OldName / UpdateInstaller::ManifestName));
    install.CheckUserData();

    // Rolling back to the version that had no manifest leaves none, so nothing is pruned later.
    REQUIRE(UpdateInstaller::RollBack(install.root, "5", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::ManifestName));
    CHECK(Read(install.root / "avcodec-63.dll") == "v4");
    REQUIRE(UpdateInstaller::RollBack(install.root, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK(Read(install.root / UpdateInstaller::ManifestName) ==
          "Qt6Core.dll\nplugins/a.dll\nplugins/new.dll\nlichii.exe\n");
    install.CheckUserData();
}

TEST_CASE("Files of the manifest the package dropped move out and come back", "[updater]") {
    Install install;
    const auto& root = install.root;
    const auto old_dir = root / UpdateInstaller::OldName;
    Write(root / "avcodec-63.dll", "v4");
    Write(root / UpdateInstaller::ManifestName,
          "lichii.exe\r\nQT6CORE.DLL\nplugins/a.dll\navcodec-63.dll\nnx2_switches.txt\n"
          "user/config/qt-config.ini\nupdate-old/x\n../outside.dll\nmissing.dll\n");
    Write(install.package / "avcodec-64.dll", "v5");
    // A manifest the package brings is replaced by the list of files placed.
    Write(install.package / UpdateInstaller::ManifestName, "lichii.exe\n");

    std::error_code ec;
    REQUIRE(UpdateInstaller::Install(root, install.package, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK_FALSE(fs::exists(root / "avcodec-63.dll"));
    CHECK(Read(old_dir / "avcodec-63.dll") == "v4");
    CHECK(Read(root / "avcodec-64.dll") == "v5");
    // Listed in another letter case, still the file just placed.
    CHECK(Read(root / "Qt6Core.dll") == "v5");
    CHECK(Read(root / UpdateInstaller::ManifestName) ==
          "Qt6Core.dll\navcodec-64.dll\nplugins/a.dll\nplugins/new.dll\nlichii.exe\n");
    CHECK(Read(old_dir / UpdateInstaller::ManifestName).starts_with("lichii.exe\r\n"));
    install.CheckUserData();

    // The first rollback brings the dropped DLL and the old manifest back.
    REQUIRE(UpdateInstaller::RollBack(root, "5", Exe, ec) == UpdateInstaller::Outcome::Done);
    CHECK(Read(root / "avcodec-63.dll") == "v4");
    CHECK_FALSE(fs::exists(root / "avcodec-64.dll"));
    CHECK(Read(root / "lichii.exe") == "v4");
    CHECK(Read(root / UpdateInstaller::ManifestName).starts_with("lichii.exe\r\n"));
    install.CheckUserData();

    // The second removes it again.
    REQUIRE(UpdateInstaller::RollBack(root, "4", Exe, ec) == UpdateInstaller::Outcome::Done);
    CHECK_FALSE(fs::exists(root / "avcodec-63.dll"));
    CHECK(Read(old_dir / "avcodec-63.dll") == "v4");
    CHECK(Read(root / "avcodec-64.dll") == "v5");
    CHECK(Read(root / "lichii.exe") == "v5");
    CHECK(Read(root / UpdateInstaller::ManifestName) ==
          "Qt6Core.dll\navcodec-64.dll\nplugins/a.dll\nplugins/new.dll\nlichii.exe\n");
    install.CheckUserData();
}

TEST_CASE("A failed swap puts every file and the old rollback copy back", "[updater]") {
    Install install;
    Write(install.root / UpdateInstaller::OldName / "lichii.exe", "v3");
    Write(install.root / UpdateInstaller::OldName / UpdateInstaller::VersionName, "3");
    Write(install.root / UpdateInstaller::ManifestName, "lichii.exe\nnotes-old.dll\n");
    Write(install.root / "notes-old.dll", "v4");
    // A folder where the package has a file: replacing it fails after Qt6Core.dll was swapped.
    fs::create_directories(install.root / "Qt6Gui.dll");
    Write(install.package / "Qt6Gui.dll", "v5");

    std::error_code ec;
    CHECK(UpdateInstaller::Install(install.root, install.package, "4", Exe, ec) ==
          UpdateInstaller::Outcome::Unchanged);
    CHECK(ec);
    CHECK(Read(install.root / "lichii.exe") == "v4");
    CHECK(Read(install.root / "Qt6Core.dll") == "v4");
    CHECK(Read(install.root / "notes-old.dll") == "v4");
    CHECK(Read(install.root / UpdateInstaller::ManifestName) == "lichii.exe\nnotes-old.dll\n");
    CHECK(Read(install.package / "Qt6Core.dll") == "v5");
    CHECK(fs::is_directory(install.root / "Qt6Gui.dll"));
    CHECK(UpdateInstaller::PreviousVersion(install.root, Exe) == "3");
    CHECK(Read(install.root / UpdateInstaller::OldName / "lichii.exe") == "v3");
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::JournalName));
    install.CheckUserData();
}

TEST_CASE("A package without lichii.exe is refused before anything moves", "[updater]") {
    Install install;
    fs::remove(install.package / "lichii.exe");
    std::error_code ec;
    CHECK(UpdateInstaller::Install(install.root, install.package, "4", Exe, ec) ==
          UpdateInstaller::Outcome::Unchanged);
    CHECK(Read(install.root / "Qt6Core.dll") == "v4");
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::JournalName));
    CHECK_FALSE(UpdateInstaller::PreviousVersion(install.root, Exe));
}

TEST_CASE("Symlinks in a package are never installed", "[updater]") {
    Install install;
    Write(install.dir.path / "outside.txt", "outside");
    Write(install.dir.path / "outside" / "x.dll", "outside");
#ifdef _WIN32
    // libstdc++ cannot create links on Windows. They need developer mode or elevation.
    if (!CreateSymbolicLinkW((install.package / "link.txt").c_str(),
                             (install.dir.path / "outside.txt").c_str(),
                             SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) ||
        !CreateSymbolicLinkW((install.package / "linked").c_str(),
                             (install.dir.path / "outside").c_str(),
                             SYMBOLIC_LINK_FLAG_DIRECTORY |
                                 SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        SKIP("symlinks unavailable without developer mode, error " << GetLastError());
    }
#else
    fs::create_symlink(install.dir.path / "outside.txt", install.package / "link.txt");
    fs::create_directory_symlink(install.dir.path / "outside", install.package / "linked");
#endif
    std::error_code ec;
    REQUIRE(UpdateInstaller::Install(install.root, install.package, "4", Exe, ec) ==
            UpdateInstaller::Outcome::Done);
    CHECK_FALSE(fs::exists(install.root / "link.txt"));
    CHECK_FALSE(fs::exists(install.root / "linked"));
    CHECK(Read(install.root / UpdateInstaller::ManifestName).find("link") == std::string::npos);
    CHECK(Read(install.dir.path / "outside.txt") == "outside");
    CHECK(Read(install.dir.path / "outside" / "x.dll") == "outside");
}

TEST_CASE("A swap interrupted by a crash is undone at the next start", "[updater]") {
    Install install;
    const auto& root = install.root;
    const auto old_dir = root / UpdateInstaller::OldName;
    const auto restore = root / UpdateInstaller::StagingName / "previous-old";
    // The state right after lichii.exe was swapped: the earlier rollback copy is aside in
    // `restore`, the old exe is in OldName, and the journal announces a Qt6Core.dll move that
    // never happened.
    Write(restore / "lichii.exe", "v3");
    fs::create_directories(old_dir);
    fs::rename(root / "lichii.exe", old_dir / "lichii.exe");
    fs::rename(install.package / "lichii.exe", root / "lichii.exe");
    Write(root / UpdateInstaller::JournalName,
          "package update-staging/unpacked/Lichii-Duskbloods\nrestore update-staging/previous-old\n"
          "old lichii.exe\nnew lichii.exe\nold Qt6Core.dll\n");

    std::error_code ec;
    CHECK(UpdateInstaller::Recover(root, ec) == UpdateInstaller::Recovery::Undone);
    CHECK(Read(root / "lichii.exe") == "v4");
    CHECK(Read(root / "Qt6Core.dll") == "v4");
    CHECK(Read(install.package / "lichii.exe") == "v5");
    CHECK(Read(old_dir / "lichii.exe") == "v3");
    CHECK_FALSE(fs::exists(restore));
    CHECK_FALSE(fs::exists(root / UpdateInstaller::JournalName));
    CHECK(UpdateInstaller::Recover(root, ec) == UpdateInstaller::Recovery::None);
    install.CheckUserData();
}

TEST_CASE("A swap interrupted while pruning and replacing the manifest is undone", "[updater]") {
    Install install;
    const auto& root = install.root;
    const auto old_dir = root / UpdateInstaller::OldName;
    // Everything was moved, the dropped DLL and the old manifest included, but no commit.
    Write(root / UpdateInstaller::ManifestName, "lichii.exe\navcodec-63.dll\n");
    Write(root / "avcodec-63.dll", "v4");
    fs::create_directories(old_dir);
    fs::rename(root / "lichii.exe", old_dir / "lichii.exe");
    fs::rename(install.package / "lichii.exe", root / "lichii.exe");
    fs::rename(root / "avcodec-63.dll", old_dir / "avcodec-63.dll");
    fs::rename(root / UpdateInstaller::ManifestName, old_dir / UpdateInstaller::ManifestName);
    Write(root / UpdateInstaller::ManifestName, "lichii.exe\n");
    Write(root / UpdateInstaller::JournalName,
          "package update-staging/unpacked/Lichii-Duskbloods\nrestore update-staging/previous-old\n"
          "old lichii.exe\nnew lichii.exe\nold avcodec-63.dll\nold install-manifest.txt\n"
          "new install-manifest.txt\n");

    std::error_code ec;
    CHECK(UpdateInstaller::Recover(root, ec) == UpdateInstaller::Recovery::Undone);
    CHECK(Read(root / "lichii.exe") == "v4");
    CHECK(Read(root / "avcodec-63.dll") == "v4");
    CHECK(Read(root / UpdateInstaller::ManifestName) == "lichii.exe\navcodec-63.dll\n");
    CHECK_FALSE(fs::exists(old_dir));
    CHECK_FALSE(fs::exists(root / UpdateInstaller::JournalName));
    install.CheckUserData();
}

TEST_CASE("A committed swap only has its cleanup finished", "[updater]") {
    Install install;
    const auto restore = install.root / UpdateInstaller::StagingName / "previous-old";
    Write(restore / "lichii.exe", "v3");
    Write(install.root / UpdateInstaller::JournalName,
          "package update-staging/unpacked/Lichii-Duskbloods\nrestore update-staging/previous-old\n"
          "old lichii.exe\nnew lichii.exe\ncommit \n");
    std::error_code ec;
    CHECK(UpdateInstaller::Recover(install.root, ec) == UpdateInstaller::Recovery::Completed);
    CHECK_FALSE(fs::exists(restore));
    CHECK(Read(install.root / "lichii.exe") == "v4");
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::JournalName));
}

TEST_CASE("Journals that point outside the install are not followed", "[updater]") {
    Install install;
    Write(install.root / UpdateInstaller::JournalName,
          "package ../elsewhere\nrestore update-staging/previous-old\nold ../outside.txt\n"
          "new lichii.exe\n");
    std::error_code ec;
    CHECK(UpdateInstaller::Recover(install.root, ec) == UpdateInstaller::Recovery::Failed);
    CHECK(Read(install.root / "lichii.exe") == "v4");
    CHECK(fs::exists(install.root / UpdateInstaller::JournalName));
}

TEST_CASE("A journal cut short in its header is dropped", "[updater]") {
    Install install;
    Write(install.root / UpdateInstaller::JournalName, "package update-staging/unpacked/Lic");
    std::error_code ec;
    CHECK(UpdateInstaller::Recover(install.root, ec) == UpdateInstaller::Recovery::None);
    CHECK_FALSE(fs::exists(install.root / UpdateInstaller::JournalName));
    CHECK(Read(install.root / "lichii.exe") == "v4");
}

TEST_CASE("User data and the updater's own files are protected", "[updater]") {
    for (const auto* path :
         {"user", "user/config/qt-config.ini", "USER/nand/x", "nx2_switches.txt",
          "NX2_Switches.TXT", "update-staging/x", "update-old/lichii.exe", "update-journal.txt",
          "update.lock", "install-manifest.txt", "../lichii.exe", "./x", ""}) {
        CHECK(UpdateInstaller::IsProtected(fs::path{path}));
    }
    for (const auto* path : {"lichii.exe", "plugins/a.dll", "users.txt", "plugins/nx2_switches.txt",
                             "plugins/user/a.dll", "Play.Duskbloods.exe"}) {
        CHECK_FALSE(UpdateInstaller::IsProtected(fs::path{path}));
    }
}

TEST_CASE("Restarts never target the updater's own folders", "[updater]") {
    CHECK(UpdateInstaller::IsUpdaterFolder(fs::path{"C:/Games/Lichii-Duskbloods/update-old"}));
    CHECK(UpdateInstaller::IsUpdaterFolder(fs::path{"C:/Games/Lichii-Duskbloods/Update-Old"}));
    CHECK(UpdateInstaller::IsUpdaterFolder(
        fs::path{"C:/Games/Lichii-Duskbloods/update-staging/unpacked/Lichii-Duskbloods"}));
    CHECK_FALSE(UpdateInstaller::IsUpdaterFolder(fs::path{"C:/Games/Lichii-Duskbloods"}));
    CHECK_FALSE(UpdateInstaller::IsUpdaterFolder(fs::path{"C:/Games/update-older"}));
#if defined(_WIN32) || defined(__linux__)
    CHECK_FALSE(UpdateInstaller::RunningExePath().empty());
#endif
}

TEST_CASE("The package root is the zip's top folder or the unpacked folder", "[updater]") {
    TempDir dir;
    Write(dir.path / "one" / "Lichii-Duskbloods" / "lichii.exe", "v5");
    CHECK(UpdateInstaller::FindPackageRoot(dir.path / "one", Exe) ==
          dir.path / "one" / "Lichii-Duskbloods");
    Write(dir.path / "flat" / "lichii.exe", "v5");
    CHECK(UpdateInstaller::FindPackageRoot(dir.path / "flat", Exe) == dir.path / "flat");
    Write(dir.path / "two" / "a" / "lichii.exe", "v5");
    Write(dir.path / "two" / "b" / "lichii.exe", "v5");
    CHECK_FALSE(UpdateInstaller::FindPackageRoot(dir.path / "two", Exe));
    Write(dir.path / "none" / "a" / "readme.txt", "");
    CHECK_FALSE(UpdateInstaller::FindPackageRoot(dir.path / "none", Exe));
    CHECK(UpdateInstaller::IsWritable(dir.path));
    CHECK_FALSE(UpdateInstaller::IsWritable(dir.path / "missing"));
}

TEST_CASE("Only one updater holds an install's lock", "[updater]") {
    TempDir dir;
    const auto path = dir.path / UpdateInstaller::LockName;
    {
        const UpdateInstaller::Lock first{path};
        const UpdateInstaller::Lock second{path};
        CHECK(first.Held());
        CHECK_FALSE(second.Held());
    }
    const UpdateInstaller::Lock again{path};
    CHECK(again.Held());
}

TEST_CASE("Restarting keeps the frontend options and drops the game", "[updater]") {
    const std::vector<std::string> arguments{"-f",
                                             "-g",
                                             "C:/games/duskbloods.nsp",
                                             "-u",
                                             "1",
                                             "C:/games/other.xci",
                                             "--config=x.ini",
                                             "-qlaunch",
                                             "--pack-nup",
                                             "a",
                                             "-c",
                                             "c.ini",
                                             "--game",
                                             "g.nsp",
                                             "-p",
                                             "args",
                                             "-x",
                                             "*:Info",
                                             "-v",
                                             "--",
                                             "-f"};
    CHECK(
        UpdateInstaller::RestartArguments(arguments) ==
        std::vector<std::string>{"-f", "-u", "1", "--config=x.ini", "-c", "c.ini", "-x", "*:Info"});
    CHECK(UpdateInstaller::RestartArguments({}).empty());
}
