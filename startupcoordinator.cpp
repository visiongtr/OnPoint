#include "startupcoordinator.h"

#include "appsettings.h"
#include "mainwindow.h"
#include "imageprocessingruntime.h"
#include "optimizeroptionsdialog.h"
#include "optimizersplashwindow.h"

#include <QApplication>
#include <QFuture>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

StartupCoordinator::StartupCoordinator(
    const QIcon &applicationIcon,
    QObject *parent
    )
    : QObject(parent)
    , m_applicationIcon(applicationIcon)
{
}

void StartupCoordinator::start()
{
    AppSettings::applyTheme();

    const bool forceOptimization =
        AppSettings::optimizeSystemOnNextLaunch();

    m_showSplashAtStartup =
        AppSettings::showSystemOptimizerAtStartup();

    m_splashWindow =
        new OptimizerSplashWindow();

    m_splashWindow->setAttribute(
        Qt::WA_DeleteOnClose,
        false
        );
    m_splashWindow->setWindowIcon(
        m_applicationIcon
        );

    connect(
        m_splashWindow,
        &OptimizerSplashWindow::optionsRequested,
        this,
        &StartupCoordinator::openOptions
        );

    connect(
        m_splashWindow,
        &OptimizerSplashWindow::continueRequested,
        this,
        &StartupCoordinator::launchMainWindow
        );

    if (m_showSplashAtStartup ||
        forceOptimization) {
        showSplashWindow();
    }

    beginSystemCheck(forceOptimization);
}

void StartupCoordinator::beginSystemCheck(
    bool forceOptimization
    )
{
    if (!m_splashWindow ||
        (m_systemWatcher && m_systemWatcher->isRunning())) {
        return;
    }

    m_forceOptimization = forceOptimization;
    m_splashWindow->pauseLaunchCountdown();
    m_splashWindow->setStatus(
        1,
        forceOptimization
            ? "Running the system optimizer again..."
            : "Checking this system against the saved performance profile..."
        );
    m_splashWindow->setSystemText(
        "Reading system information..."
        );

    if (!m_systemWatcher) {
        m_systemWatcher =
            new QFutureWatcher<SystemSnapshot>(this);

        connect(
            m_systemWatcher,
            &QFutureWatcher<SystemSnapshot>::finished,
            this,
            &StartupCoordinator::handleSystemCheckFinished
            );
    }

    QPointer<OptimizerSplashWindow> splashGuard(
        m_splashWindow
        );

    const QFuture<SystemSnapshot> future =
        QtConcurrent::run(
            [splashGuard]() {
                return SystemProfiler::detectSystem(
                    [splashGuard](
                        int progress,
                        const QString &status
                        ) {
                        if (!splashGuard) {
                            return;
                        }

                        QMetaObject::invokeMethod(
                            splashGuard,
                            [splashGuard, progress, status]() {
                                if (splashGuard) {
                                    splashGuard->setStatus(
                                        progress,
                                        status
                                        );
                                }
                            },
                            Qt::QueuedConnection
                            );
                    });
            });

    m_systemWatcher->setFuture(future);
}

void StartupCoordinator::handleSystemCheckFinished()
{
    if (!m_splashWindow ||
        !m_systemWatcher) {
        return;
    }

    m_currentSnapshot =
        m_systemWatcher->result();

    m_splashWindow->setSystemText(
        m_currentSnapshot.displayText()
        );

    PerformanceProfile savedProfile;
    QString loadError;

    const bool profileLoaded =
        SystemProfiler::loadProfile(
            &savedProfile,
            &loadError
            );

    const bool savedProfileUsable =
        profileLoaded &&
        savedProfile.isUsableFor(
            m_currentSnapshot,
            SystemProfiler::EngineSchemaVersion
            );

    if (!m_forceOptimization &&
        savedProfileUsable) {
        m_profile = savedProfile;

        if (!m_showSplashAtStartup &&
            !m_splashWindow->isVisible()) {
            launchMainWindow();
            return;
        }

        m_splashWindow->setProfileReady(
            m_currentSnapshot.displayText() +
            "\n\n" +
            m_profile.summaryText()
            );
        m_splashWindow->startLaunchCountdown(10);
        return;
    }

    // A hidden normal launch only remains hidden while its saved profile is
    // valid. Missing or stale hardware/driver/schema data brings the full
    // optimizer back automatically before recalibration.
    showSplashWindow();

    m_splashWindow->setStatus(
        54,
        profileLoaded
            ? "The saved profile is stale. Re-optimizing for this system..."
            : "No valid profile was found. Optimizing this system..."
        );

    QPointer<OptimizerSplashWindow> splashGuard(
        m_splashWindow
        );

    const SystemSnapshot snapshot =
        m_currentSnapshot;

    const QFuture<PerformanceProfile> optimizationFuture =
        QtConcurrent::run(
            [snapshot, splashGuard]() {
                return SystemProfiler::optimizeSystem(
                    snapshot,
                    [splashGuard](
                        int progress,
                        const QString &status
                        ) {
                        if (!splashGuard) {
                            return;
                        }

                        QMetaObject::invokeMethod(
                            splashGuard,
                            [splashGuard, progress, status]() {
                                if (splashGuard) {
                                    splashGuard->setStatus(
                                        progress,
                                        status
                                        );
                                }
                            },
                            Qt::QueuedConnection
                            );
                    });
            });

    QFutureWatcher<PerformanceProfile> *optimizationWatcher =
        new QFutureWatcher<PerformanceProfile>(this);

    connect(
        optimizationWatcher,
        &QFutureWatcher<PerformanceProfile>::finished,
        this,
        [this, optimizationWatcher]() {
            m_profile = optimizationWatcher->result();
            optimizationWatcher->deleteLater();

            QString saveError;

            if (!SystemProfiler::saveProfile(
                    m_profile,
                    &saveError
                    )) {
                QMessageBox::warning(
                    m_splashWindow,
                    "Performance Profile Was Not Saved",
                    "On Point completed the system optimization but could not "
                    "save its profile:\n\n" + saveError +
                    "\n\nThe editor can still launch, but optimization will run "
                    "again next time."
                    );
            } else {
                AppSettings::setOptimizeSystemOnNextLaunch(false);
            }

            if (!m_splashWindow) {
                return;
            }

            m_splashWindow->setProfileReady(
                m_currentSnapshot.displayText() +
                "\n\n" +
                m_profile.summaryText()
                );
            m_splashWindow->startLaunchCountdown(10);
        });

    optimizationWatcher->setFuture(
        optimizationFuture
        );
}

void StartupCoordinator::showSplashWindow()
{
    if (!m_splashWindow) {
        return;
    }

    m_splashWindow->show();
    m_splashWindow->raise();
    m_splashWindow->activateWindow();
}

void StartupCoordinator::openOptions()
{
    if (!m_splashWindow ||
        m_profile.profileId.isEmpty()) {
        return;
    }

    OptimizerOptionsDialog dialog(
        &m_profile,
        m_splashWindow
        );

    dialog.exec();

    if (dialog.rerunRequested()) {
        beginSystemCheck(true);
        return;
    }

    if (dialog.profileChanged()) {
        m_splashWindow->setProfileReady(
            m_currentSnapshot.displayText() +
            "\n\n" +
            m_profile.summaryText()
            );
    }

    m_splashWindow->startLaunchCountdown(10);
}

void StartupCoordinator::launchMainWindow()
{
    if (m_launchStarted ||
        m_profile.profileId.isEmpty()) {
        return;
    }

    m_launchStarted = true;

    // Apply the tested optimizer profile before any image-processing
    // widgets or tools are created.
    ImageProcessingRuntime::instance().configure(
        m_profile.settings
        );

    m_mainWindow =
        new MainWindow();

    m_mainWindow->setAttribute(
        Qt::WA_DeleteOnClose,
        true
        );
    m_mainWindow->setWindowIcon(
        m_applicationIcon
        );

    connect(
        m_mainWindow,
        &QObject::destroyed,
        qApp,
        &QCoreApplication::quit
        );

    m_mainWindow->showMaximized();

    if (m_splashWindow) {
        m_splashWindow->hide();
        m_splashWindow->deleteLater();
        m_splashWindow = nullptr;
    }
}
