// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <QDialog>
#include <QString>
#include "common/net/net.h"

class QRadioButton;
namespace Ui {
class UpdateDialog;
}

class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    // exec() returns QDialog::Accepted to install, QDialog::Rejected to be reminded later.
    static constexpr int SkipVersion = 2;

    // `dusk_asset` is the package the Duskbloods update check chose; without it the release's
    // platform assets are offered for download.
    explicit UpdateDialog(const Common::Net::Release& release,
                          std::optional<Common::Net::Asset> dusk_asset = std::nullopt,
                          QWidget* parent = nullptr);
    ~UpdateDialog();

private slots:
    void Download();

private:
    void SaveToFolder();
#ifdef _WIN32
    void InstallInPlace();
#endif
    bool DownloadTo(const QString& filename);

    Ui::UpdateDialog* ui;
    QList<QRadioButton*> m_buttons;
    Common::Net::Asset m_asset;
    Common::Net::Release m_release;
    bool m_dusk{};
};

// In-place installs of the Duskbloods Windows package (see frontend_common/update_installer.h).
namespace Updater {

// Call first in main(): waits for the process this one replaces, and keeps the exe path, the
// environment that a restart uses and whether Play.Duskbloods.exe started this process.
void PrepareLaunch();

// Whether this build can replace its own files: a Duskbloods build on Windows run as lichii.exe.
bool CanInstallInPlace();

// At startup: finish or undo a swap that was interrupted, and remove the staging folder. Returns
// true when Lichii restarted (or closed) to run the restored files.
bool FinishPendingSwap(QWidget* window);

// The version a rollback would restore ("" when unknown), or nullopt when there is none.
std::optional<QString> RollbackVersion();

void RollBack(QWidget* window);

// Start lichii.exe in the install folder again, with the current arguments minus any game, and
// close `window`. The new process shows `note` once it is up. When Play.Duskbloods.exe started
// this process, shows `note` with a reminder to start the game from it, and closes instead.
bool Restart(QWidget* window, const QString& note = {});

// The note the process this one replaced left for it, once.
std::optional<QString> TakeRestartNote();

// "Lichii for The Duskbloods v4.1" for a release tagged duskbloods-v4.1, else its title.
QString ReleaseName(const Common::Net::Release& release);

} // namespace Updater
