#ifndef STARTUPCOORDINATOR_H
#define STARTUPCOORDINATOR_H

#include "systemprofile.h"

#include <QObject>
#include <QIcon>

class MainWindow;
class OptimizerSplashWindow;
template<typename T>
class QFutureWatcher;

class StartupCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit StartupCoordinator(
        const QIcon &applicationIcon,
        QObject *parent = nullptr
        );

    void start();

private:
    void beginSystemCheck(
        bool forceOptimization
        );

    void handleSystemCheckFinished();
    void showSplashWindow();
    void openOptions();
    void launchMainWindow();

    QIcon m_applicationIcon;
    OptimizerSplashWindow *m_splashWindow = nullptr;
    MainWindow *m_mainWindow = nullptr;

    QFutureWatcher<SystemSnapshot> *m_systemWatcher = nullptr;
    SystemSnapshot m_currentSnapshot;
    PerformanceProfile m_profile;
    bool m_forceOptimization = false;
    bool m_showSplashAtStartup = true;
    bool m_launchStarted = false;
};

#endif // STARTUPCOORDINATOR_H
