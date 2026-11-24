// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include <utility>
#include <QTimer>
#include <QEventLoop>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <qdesktopservices.h>
#include "common/logging.h"
#include "common/scm_rev.h"
#include "frontend_common/update_checker.h"
#include "frontend_common/update_installer.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/abstract/progress.h"
#ifdef _WIN32
#include "qt_common/util/compress.h"
#endif
#include "ui_update_dialog.h"
#include "update_dialog.h"

#include "common/httplib.h"

#include "common/net/ca_store.h"

#include <QDesktopServices>

#undef GetSaveFileName

namespace fs = std::filesystem;

namespace {

// Never renamed: NVIDIA's per-exe shader cache and Play.Duskbloods.exe's settings name it.
constexpr wchar_t ExeName[] = L"lichii.exe";
// Set for a restarted process to a note it shows once it is up, such as "v5 is installed".
constexpr char RestartNoteVariable[] = "LICHII_UPDATE_NOTE";
// Set by Play.Duskbloods.exe for the process it starts. Its servers stop when that process exits,
// so such a process is not restarted after an update: the player starts the game from it again.
constexpr char LauncherVariable[] = "EDEN_NX2_ONLINE_SERVER";

// Read in PrepareLaunch, before a swap can rename the running exe into update-old.
fs::path& LaunchExe() {
    static fs::path exe;
    return exe;
}

QString& RestartNote() {
    static QString note;
    return note;
}

bool& StartedByLauncher() {
    static bool launcher{};
    return launcher;
}

fs::path InstallDir() {
    if (LaunchExe().empty()) {
        LaunchExe() = UpdateInstaller::RunningExePath();
    }
    return LaunchExe().parent_path();
}

// A message box on top of the main window. The update dialog and the progress dialogs are closed
// by the time these show, and a box parented to them can end up hidden.
QMessageBox::StandardButton Notify(QWidget* window, QMessageBox::Icon icon, const QString& title,
                                   const QString& text,
                                   QMessageBox::StandardButtons buttons = QMessageBox::Ok) {
    QMessageBox box(icon, title, text, buttons, window);
    box.setWindowFlag(Qt::WindowStaysOnTopHint);
    box.show();
    box.raise();
    box.activateWindow();
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

std::string PathText(const fs::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}

QString DuskName(const QString& version) {
    return version.isEmpty() ? QCoreApplication::translate("UpdateDialog", "the previous version")
                             : QStringLiteral("Lichii for The Duskbloods v%1").arg(version);
}

std::string RunningVersionText() {
    const auto version = UpdateChecker::RunningVersion();
    return version ? version->ToString() : std::string{};
}

QString ErrorText(const std::error_code& ec) {
    return QString::fromLocal8Bit(ec.message().c_str());
}

QProcessEnvironment& LaunchEnvironment() {
    static QProcessEnvironment environment;
    return environment;
}

// For a process Play.Duskbloods.exe started: say what happened, then close without restarting.
void CloseForLauncher(QWidget* window, const QString& text) {
    LOG_INFO(Frontend, "Closing so the game is started again from Play.Duskbloods.exe");
    Notify(window, QMessageBox::Information,
           QCoreApplication::translate("Updater", "Lichii for The Duskbloods"), text);
    // The log is written in blocks.
    Common::Log::Stop();
    if (window) {
        QMetaObject::invokeMethod(window, &QWidget::close, Qt::QueuedConnection);
    }
}

} // namespace

UpdateDialog::UpdateDialog(const Common::Net::Release& release,
                           std::optional<Common::Net::Asset> dusk_asset, QWidget* parent)
    : QDialog(parent), ui(new Ui::UpdateDialog), m_release(release),
      m_dusk(dusk_asset.has_value()) {
    ui->setupUi(this);

    ui->version->setText(
        tr("%1 is available for download.").arg(QString::fromStdString(release.title)));
    ui->url->setText(
        tr("<a href=\"%1\">View on GitHub</a>").arg(QString::fromStdString(release.html_url)));
    ui->body->setMarkdown(
        QString::fromStdString(UpdateChecker::CleanReleaseNotes(release.body)));

    ui->buttonBox->button(QDialogButtonBox::No)->setText(tr("Remind Me Later"));
    auto* skip = ui->buttonBox->addButton(tr("Skip This Version"), QDialogButtonBox::ActionRole);
    connect(skip, &QPushButton::clicked, this, [this] { done(SkipVersion); });

    if (m_dusk) {
        m_asset = *dusk_asset;
        ui->groupBox->setHidden(true);
        ui->version->setText(tr("%1 is available. You have Lichii for The Duskbloods %2.")
                                 .arg(Updater::ReleaseName(release),
                                      QString::fromLatin1(Common::g_build_label)));
        auto* install = ui->buttonBox->button(QDialogButtonBox::Yes);
        if (m_asset.sha256.empty()) {
            // Without a published checksum there is nothing to verify the download against.
            install->setText(tr("Open Release Page"));
            ui->label_2->setText(tr("This release has no published checksum, so it can't be "
                                    "installed automatically."));
            connect(this, &QDialog::accepted, this, [this] {
                QDesktopServices::openUrl(QUrl{QString::fromStdString(m_release.html_url)});
            });
            return;
        }
        if (Updater::CanInstallInPlace()) {
            install->setText(tr("Install"));
            ui->label_2->setText(tr("Lichii will download the update and install it. Your "
                                    "settings, saves and keys are not changed."));
        } else {
            install->setText(tr("Download"));
        }
        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
        return;
    }

    // TODO(crueter): Find a way to set default
    const auto assets = release.GetPlatformAssets();

    if (assets.empty()) {
        ui->groupBox->setHidden(true);
        connect(this, &QDialog::accepted, this, [release]() {
            QDesktopServices::openUrl(QUrl{QString::fromStdString(release.html_url)});
        });
    } else if (assets.size() == 1) {
        ui->groupBox->setHidden(true);
        m_asset = assets[0];

        if (m_asset.name == "Standard" &&
            std::string_view{Common::g_build_release_platform}.ends_with("-pgo")) {
            ui->version->setText(
                ui->version->text() +
                tr(" This release has no PGO build, so the standard build will be downloaded."));
        }

        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
    } else {
        u32 i = 0;
        for (const Common::Net::Asset& a : assets) {
            QRadioButton* r = new QRadioButton(tr(a.name.c_str()), this);
            connect(r, &QRadioButton::toggled, this, [a, this](bool checked) {
                if (checked)
                    m_asset = a;
            });

            if (i == 0)
                r->setChecked(true);
            ++i;

            ui->radioButtons->addWidget(r);
        }

        connect(this, &QDialog::accepted, this, &UpdateDialog::Download);
    }
}

UpdateDialog::~UpdateDialog() {
    delete ui;
}

void UpdateDialog::Download() {
#ifdef _WIN32
    if (m_dusk && Updater::CanInstallInPlace()) {
        if (UpdateInstaller::IsWritable(InstallDir())) {
            InstallInPlace();
            return;
        }
        LOG_WARNING(Frontend, "Cannot write to {}, downloading the update instead",
                    PathText(InstallDir()));
        Notify(parentWidget(), QMessageBox::Information, tr("Update Needs to Be Installed by Hand"),
               tr("Lichii can't write to its folder (%1), so it can't update itself there. The "
                  "update will be downloaded instead: unzip it and copy its contents over your "
                  "Lichii folder, or move Lichii to a folder you can write to, such as "
                  "Documents.")
                   .arg(QDir::toNativeSeparators(QString::fromStdWString(InstallDir().wstring()))));
    }
#endif
    SaveToFolder();
}

void UpdateDialog::SaveToFolder() {
    // The application folder is read-only inside an AppImage and is the app bundle on macOS.
    auto default_dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (default_dir.isEmpty()) {
        default_dir = QDir::homePath();
    }

    const auto filename = QtCommon::Frontend::GetSaveFileName(
        tr("New Version Location"),
        default_dir % QStringLiteral("/") % QString::fromStdString(m_asset.filename),
        tr("All Files (*.*)"));

    if (filename.isEmpty() || !DownloadTo(filename))
        return;

    // Download is complete. User may choose to open in the file manager.
    auto button = Notify(parentWidget(), QMessageBox::Question, tr("Download Complete"),
                         tr("Successfully downloaded %1. Would you like to open its "
                            "folder?")
                             .arg(QString::fromStdString(m_asset.filename)),
                         QMessageBox::Yes | QMessageBox::No);

    if (button == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filename).absolutePath()));
    }
}

#ifdef _WIN32
void UpdateDialog::InstallInPlace() {
    const auto install_dir = InstallDir();
    const UpdateInstaller::Lock lock{install_dir / UpdateInstaller::LockName};
    if (!lock.Held()) {
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to install the update"),
               tr("Another Lichii window is already updating this folder."));
        return;
    }
    if (fs::exists(install_dir / UpdateInstaller::JournalName)) {
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to install the update"),
               tr("An earlier update is unfinished. Start Lichii again to finish it first."));
        return;
    }

    const auto staging = install_dir / UpdateInstaller::StagingName;
    const auto archive = staging / m_asset.filename;
    const auto unpacked = staging / "unpacked";
    const auto fail = [&](const QString& text) {
        UpdateInstaller::CleanStaging(install_dir);
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to install the update"), text);
    };

    UpdateInstaller::CleanStaging(install_dir);
    std::error_code ec;
    fs::create_directories(unpacked, ec);
    if (ec) {
        fail(tr("Could not create %1: %2")
                 .arg(QString::fromStdWString(unpacked.wstring()), ErrorText(ec)));
        return;
    }
    if (!DownloadTo(QString::fromStdWString(archive.wstring()))) {
        UpdateInstaller::CleanStaging(install_dir);
        return;
    }

    auto progress = QtCommon::Frontend::newProgressDialog(tr("Unpacking..."), tr("Cancel"), 0, 100);
    progress->show();
    const auto extracted = QtCommon::Compress::extractDir(
        QString::fromStdWString(archive.wstring()), QString::fromStdWString(unpacked.wstring()),
        [&](std::size_t total, std::size_t done) {
            QGuiApplication::processEvents();
            if (total != 0) {
                progress->setValue(static_cast<int>((done * 100) / total));
            }
            return !progress->wasCanceled();
        });
    progress->close();
    if (extracted.isEmpty()) {
        fail(tr("The downloaded update could not be unpacked."));
        return;
    }
    fs::remove(archive, ec);

    // A package has no business carrying links; the swap would skip them, refuse the package.
    for (auto it = fs::recursive_directory_iterator(unpacked, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_symlink(ec)) {
            LOG_ERROR(Frontend, "Update package contains a link: {}", PathText(it->path()));
            fail(tr("The downloaded update contains a link, which an update must not have."));
            return;
        }
    }
    if (ec) {
        fail(tr("Could not read the unpacked update: %1").arg(ErrorText(ec)));
        return;
    }

    const auto package = UpdateInstaller::FindPackageRoot(unpacked, ExeName);
    if (!package) {
        // Fritchii v5.8.4 (2026-11-11): a player got this with a good zip: the exe was in it but
        // gone from disk right after unpacking, as an antivirus quarantining a new unsigned exe
        // does.
        const bool in_archive = std::ranges::any_of(extracted, [](const QString& file) {
            return file.endsWith(QStringLiteral("/lichii.exe"), Qt::CaseInsensitive) ||
                   file.endsWith(QStringLiteral("\\lichii.exe"), Qt::CaseInsensitive);
        });
        LOG_ERROR(Frontend, "Update package without {}: {} files unpacked, the exe {} in the list",
                  PathText(ExeName), extracted.size(), in_archive ? "was" : "was not");
        fail(in_archive
                 ? tr("Your antivirus probably removed lichii.exe from the update right after it "
                      "was unpacked.\n\nAllow it in your antivirus and try again, or download %1 "
                      "from the release page and unzip it over your Lichii folder. Your saves "
                      "and settings are safe either way.")
                       .arg(QString::fromStdString(m_asset.filename))
                 : tr("The downloaded update does not contain lichii.exe."));
        return;
    }

    switch (UpdateInstaller::Install(install_dir, *package, RunningVersionText(), ExeName, ec)) {
    case UpdateInstaller::Outcome::Done: {
        LOG_INFO(Frontend, "Installed {} in {}", m_release.tag, PathText(install_dir));
        UpdateInstaller::CleanStaging(install_dir);
        const auto name = Updater::ReleaseName(m_release);
        if (StartedByLauncher()) {
            CloseForLauncher(parentWidget(),
                             tr("%1 is installed.\n\nStart the game again from "
                                "Play.Duskbloods.exe. Help > Roll Back to the Previous Version "
                                "brings the old one back.")
                                 .arg(name));
            return;
        }
        // Restart at once; the new version says it was installed when it is up.
        Updater::Restart(parentWidget(),
                         tr("%1 is installed.\n\nThe previous version is kept: Help > Roll Back "
                            "to the Previous Version brings it back.")
                             .arg(name));
        return;
    }
    case UpdateInstaller::Outcome::Unchanged:
        LOG_ERROR(Frontend, "Could not install {}: {}", m_release.tag, ec.message());
        fail(tr("Could not replace the files of this version (%1). Lichii was not changed.")
                 .arg(ErrorText(ec)));
        return;
    case UpdateInstaller::Outcome::Interrupted:
        LOG_CRITICAL(Frontend, "Installing {} failed and could not be undone: {}", m_release.tag,
                     ec.message());
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to install the update"),
               tr("The update failed (%1) and some files could not be put back. Close Lichii and "
                  "start it again to restore the previous version. If it does not start, download "
                  "%2 again and unzip it over your Lichii folder; your user data is not affected.")
                   .arg(ErrorText(ec), QString::fromStdString(m_asset.filename)));
        return;
    }
}
#endif

// TODO: migrate to a net.cpp wrapper
bool UpdateDialog::DownloadTo(const QString& filename) {
    QSaveFile file(filename);
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        LOG_WARNING(Frontend, "Could not open file {}", filename.toStdString());
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to save file"),
               tr("Could not open file %1 for writing.").arg(filename));
        return false;
    }

    // TODO(crueter): Move to net.cpp
    constexpr std::size_t timeout_seconds = 15;

    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(m_asset.url);
    client->set_connection_timeout(timeout_seconds);
    client->set_read_timeout(timeout_seconds);
    client->set_write_timeout(timeout_seconds);
    client->set_follow_location(true);

    Common::Net::ConfigureCaStore(*client);

    if (client == nullptr) {
        LOG_ERROR(Frontend, "Invalid URL {}{}", m_asset.url, m_asset.path);
        return false;
    }

    auto progress =
        QtCommon::Frontend::newProgressDialog(tr("Downloading..."), tr("Cancel"), 0, 100);
    progress->show();

    QGuiApplication::processEvents();

    // Progress dialog.
    auto progress_callback = [&](size_t processed_size, size_t total_size) {
        QGuiApplication::processEvents();
        if (total_size != 0) {
            progress->setValue(static_cast<int>((processed_size * 100) / total_size));
        }
        return !progress->wasCanceled();
    };

    QCryptographicHash hash(QCryptographicHash::Sha256);
    u64 received = 0;

    // Write file in chunks.
    auto content_receiver = [this, &file, &hash, &received, filename](const char* t_data,
                                                                      size_t data_length) -> bool {
        hash.addData(QByteArrayView(t_data, static_cast<qsizetype>(data_length)));
        received += data_length;
        if (file.write(t_data, static_cast<qint64>(data_length)) == -1) {
            LOG_WARNING(Frontend, "Could not write {} bytes to file {}", data_length,
                        filename.toStdString());
            Notify(parentWidget(), QMessageBox::Critical, tr("Failed to save file"),
                   tr("Could not write to file %1.").arg(filename));
            return false;
        }

        return true;
    };

    // Now send off request. GitHub's file server sometimes answers 502-504 or drops the
    // connection for a moment (v5.2's release hour): try up to three times, a few seconds apart,
    // starting the file over each time. The size and SHA-256 checks below still decide.
    constexpr int attempts = 3;
    httplib::Result result;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        if (attempt > 1) {
            file.seek(0);
            file.resize(0);
            hash.reset();
            received = 0;
            progress->setValue(0);
            progress->setLabelText(tr("Downloading... (try %1 of %2)").arg(attempt).arg(attempts));
            QEventLoop wait;
            QTimer::singleShot(2000 * (attempt - 1), &wait, &QEventLoop::quit);
            wait.exec();
            if (progress->wasCanceled()) {
                break;
            }
        }
        result = client->Get(m_asset.path, content_receiver, progress_callback);
        const bool canceled = !result && result.error() == httplib::Error::Canceled;
        const bool transient = (!result && !canceled) ||
                               (result && (result->status >= 500 || result->status == 429));
        if (!transient || attempt == attempts) {
            break;
        }
        LOG_WARNING(Frontend, "GET to {}{} failed ({}), trying again", m_asset.url, m_asset.path,
                    result ? std::to_string(result->status) : httplib::to_string(result.error()));
    }
    progress->close();

    if (!result) {
        LOG_ERROR(Frontend, "GET to {}{} failed: {}", m_asset.url, m_asset.path,
                  httplib::to_string(result.error()));
        if (result.error() != httplib::Error::Canceled) {
            Notify(parentWidget(), QMessageBox::Critical, tr("Failed to download file"),
                   tr("Could not download the update. Check your internet "
                      "connection and try again."));
        }
        return false;
    }

    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(Frontend, "GET to {}{} returned error status code: {}", m_asset.url, m_asset.path,
                  response.status);
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to download file"),
               tr("Could not download from %1%2\nError code: %3")
                   .arg(QString::fromStdString(m_asset.url), QString::fromStdString(m_asset.path),
                        QString::number(response.status)));
        return false;
    }
    if (!response.has_header("content-type")) {
        LOG_ERROR(Frontend, "GET to {}{} returned no content", m_asset.url, m_asset.path);
        return false;
    }

    // Duskbloods packages are only kept when they match the digest the release list published.
    const bool wrong_size = m_asset.size != 0 && received != m_asset.size;
    const bool wrong_hash = (m_dusk || !m_asset.sha256.empty()) &&
                            hash.result().toHex() != QByteArray::fromStdString(m_asset.sha256);
    if (wrong_size || wrong_hash) {
        LOG_ERROR(Frontend, "Downloaded {} does not match the published size or SHA-256",
                  m_asset.filename);
        file.cancelWriting();
        Notify(parentWidget(), QMessageBox::Critical, tr("Download failed verification"),
               tr("The downloaded file does not match the checksum GitHub published for it, so it "
                  "was discarded. Try again later."));
        return false;
    }

    if (!file.commit()) {
        LOG_WARNING(Frontend, "Could not commit to file {}", filename.toStdString());
        Notify(parentWidget(), QMessageBox::Critical, tr("Failed to save file"),
               tr("Could not commit to file %1.").arg(filename));
        return false;
    }

    return true;
}

namespace Updater {

void PrepareLaunch() {
    UpdateInstaller::WaitForReplacedProcess();
    LaunchExe() = UpdateInstaller::RunningExePath();
    RestartNote() = qEnvironmentVariable(RestartNoteVariable);
    qunsetenv(RestartNoteVariable);
    // Before nx2_switches.txt is applied, so only the real environment counts.
    StartedByLauncher() = !qEnvironmentVariableIsEmpty(LauncherVariable);
    LaunchEnvironment() = QProcessEnvironment::systemEnvironment();
}

std::optional<QString> TakeRestartNote() {
    if (RestartNote().isEmpty()) {
        return std::nullopt;
    }
    return std::exchange(RestartNote(), QString{});
}

bool CanInstallInPlace() {
#ifdef _WIN32
    const auto install_dir = InstallDir();
    const auto exe = LaunchExe();
    return UpdateChecker::RunningVersion().has_value() && !exe.empty() &&
           !UpdateInstaller::IsUpdaterFolder(install_dir) &&
           QString::fromStdWString(exe.filename().wstring())
                   .compare(QString::fromWCharArray(ExeName), Qt::CaseInsensitive) == 0;
#else
    return false;
#endif
}

bool FinishPendingSwap(QWidget* window) {
    if (!CanInstallInPlace()) {
        return false;
    }
    const auto install_dir = InstallDir();
    std::error_code ec;
    if (!fs::exists(install_dir / UpdateInstaller::JournalName, ec) &&
        !fs::exists(install_dir / UpdateInstaller::StagingName, ec)) {
        return false;
    }
    const UpdateInstaller::Lock lock{install_dir / UpdateInstaller::LockName};
    if (!lock.Held()) {
        // Another Lichii window is installing or rolling back right now.
        return false;
    }
    switch (UpdateInstaller::Recover(install_dir, ec)) {
    case UpdateInstaller::Recovery::None:
    case UpdateInstaller::Recovery::Completed:
        UpdateInstaller::CleanStaging(install_dir);
        return false;
    case UpdateInstaller::Recovery::Undone:
        LOG_WARNING(Frontend, "Undid an interrupted update in {}", PathText(install_dir));
        UpdateInstaller::CleanStaging(install_dir);
        return Restart(window, QCoreApplication::translate(
                                   "Updater", "An update was interrupted, so the version it was "
                                              "replacing has been put back."));
    case UpdateInstaller::Recovery::Failed:
        LOG_CRITICAL(Frontend, "Could not undo an interrupted update in {}: {}",
                     PathText(install_dir), ec.message());
        Notify(window, QMessageBox::Critical,
               QCoreApplication::translate("Updater", "Interrupted Update"),
               QCoreApplication::translate(
                   "Updater",
                   "An update was interrupted and could not be undone (%1). Close every Lichii "
                   "window and start Lichii again. If it keeps failing, download "
                   "Lichii-Duskbloods.zip again and unzip it over your Lichii folder; your user "
                   "data is not affected.")
                   .arg(ErrorText(ec)));
        return false;
    }
    return false;
}

std::optional<QString> RollbackVersion() {
    if (!CanInstallInPlace()) {
        return std::nullopt;
    }
    const auto version = UpdateInstaller::PreviousVersion(InstallDir(), ExeName);
    return version ? std::make_optional(QString::fromStdString(*version)) : std::nullopt;
}

void RollBack(QWidget* window) {
    const auto version = RollbackVersion();
    if (!version) {
        return;
    }
    const auto title = QCoreApplication::translate("Updater", "Roll Back");
    const auto install_dir = InstallDir();
    if (!UpdateInstaller::IsWritable(install_dir)) {
        Notify(window, QMessageBox::Critical, title,
               QCoreApplication::translate("Updater", "Lichii can't write to its folder (%1).")
                   .arg(QDir::toNativeSeparators(QString::fromStdWString(install_dir.wstring()))));
        return;
    }
    const auto answer =
        Notify(window, QMessageBox::Question, title,
               QCoreApplication::translate(
                   "Updater", "Go back to %1? Lichii will %2. Your settings, saves "
                              "and keys are not changed, and you can switch back to this "
                              "version the same way.")
                   .arg(DuskName(*version),
                        StartedByLauncher() ? QCoreApplication::translate("Updater", "close")
                                            : QCoreApplication::translate("Updater", "restart")),
               QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    const UpdateInstaller::Lock lock{install_dir / UpdateInstaller::LockName};
    if (!lock.Held()) {
        Notify(window, QMessageBox::Critical, title,
               QCoreApplication::translate("Updater", "Another Lichii window is already "
                                                      "updating this folder."));
        return;
    }
    std::error_code ec;
    switch (UpdateInstaller::RollBack(install_dir, RunningVersionText(), ExeName, ec)) {
    case UpdateInstaller::Outcome::Done:
        LOG_INFO(Frontend, "Rolled back to {} in {}", version->toStdString(),
                 PathText(install_dir));
        UpdateInstaller::CleanStaging(install_dir);
        Restart(window, QCoreApplication::translate("Updater", "%1 is back. Help > Roll Back to "
                                                               "the Previous Version switches to "
                                                               "the newer version again.")
                            .arg(DuskName(*version)));
        return;
    case UpdateInstaller::Outcome::Unchanged:
        LOG_ERROR(Frontend, "Could not roll back: {}", ec.message());
        UpdateInstaller::CleanStaging(install_dir);
        Notify(window, QMessageBox::Critical, title,
               QCoreApplication::translate("Updater",
                                           "Could not roll back (%1). Lichii was not changed.")
                   .arg(ErrorText(ec)));
        return;
    case UpdateInstaller::Outcome::Interrupted:
        LOG_CRITICAL(Frontend, "Rolling back failed and could not be undone: {}", ec.message());
        Notify(window, QMessageBox::Critical, title,
               QCoreApplication::translate(
                   "Updater", "Rolling back failed (%1) and some files could not be put "
                              "back. Close Lichii and start it again to restore this version.")
                   .arg(ErrorText(ec)));
        return;
    }
}

bool Restart(QWidget* window, const QString& note) {
    if (StartedByLauncher()) {
        // Play.Duskbloods.exe's servers stop with this process; a restarted one could not go
        // online.
        CloseForLauncher(window,
                         note + QCoreApplication::translate(
                                    "Updater", "\n\nStart the game again from Play.Duskbloods.exe."));
        return true;
    }
    // The install folder from startup: Qt's applicationDirPath() now names update-old.
    const auto exe = InstallDir() / ExeName;
    const auto program = QString::fromStdWString(exe.wstring());
    std::error_code ec;
    if (UpdateInstaller::IsUpdaterFolder(exe.parent_path()) || !fs::is_regular_file(exe, ec)) {
        LOG_CRITICAL(Frontend, "Not restarting {}: it is not the installed version", PathText(exe));
        Notify(window, QMessageBox::Critical,
               QCoreApplication::translate("Updater", "Failed to restart"),
               QCoreApplication::translate("Updater", "Please start Lichii again yourself."));
        return false;
    }
    std::vector<std::string> arguments;
    for (const auto& argument : QCoreApplication::arguments().mid(1)) {
        arguments.push_back(argument.toStdString());
    }
    QStringList kept;
    for (const auto& argument : UpdateInstaller::RestartArguments(arguments)) {
        kept.push_back(QString::fromStdString(argument));
    }

    // The environment from before main() added the built-in switches, so the new version applies
    // its own.
    auto environment = LaunchEnvironment().isEmpty() ? QProcessEnvironment::systemEnvironment()
                                                     : LaunchEnvironment();
    environment.insert(QString::fromLatin1(UpdateInstaller::RestartWaitVariable),
                       QString::number(QCoreApplication::applicationPid()));
    if (!note.isEmpty()) {
        environment.insert(QString::fromLatin1(RestartNoteVariable), note);
    }
    QProcess process;
    process.setProgram(program);
    process.setArguments(kept);
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QDir::currentPath());
    if (!process.startDetached()) {
        LOG_ERROR(Frontend, "Could not start {}", program.toStdString());
        Notify(window, QMessageBox::Critical,
               QCoreApplication::translate("Updater", "Failed to restart"),
               QCoreApplication::translate("Updater", "Please start Lichii again yourself."));
        return false;
    }
    LOG_INFO(Frontend, "Restarting {}", PathText(exe));
    // The log is written in blocks, and the new process only starts once this one has exited.
    Common::Log::Stop();
    if (window) {
        QMetaObject::invokeMethod(window, &QWidget::close, Qt::QueuedConnection);
    }
    return true;
}

QString ReleaseName(const Common::Net::Release& release) {
    const auto version = UpdateChecker::ParseDuskTag(release.tag);
    return version ? DuskName(QString::fromStdString(version->ToString()))
                   : QString::fromStdString(release.title);
}

} // namespace Updater
