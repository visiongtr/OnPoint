#include "startupcoordinator.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QStringList>


#if defined(Q_OS_LINUX)
namespace
{
QStringList linuxRusticlDrivers()
{
    QStringList drivers;

    // Inspect DRM cards before Qt or OpenCL is initialized. Mesa Rusticl uses
    // the Gallium driver names below for modern AMD and Intel graphics. This
    // covers AMD integrated/discrete GPUs, Intel Iris Xe integrated graphics,
    // and Intel Arc cards while leaving NVIDIA's vendor runtime untouched.
    for (int cardIndex = 0;
         cardIndex < 32;
         ++cardIndex) {
        QFile vendorFile(
            QString("/sys/class/drm/card%1/device/vendor")
                .arg(cardIndex)
            );

        if (!vendorFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        const QByteArray vendorId =
            vendorFile.readAll().trimmed().toLower();

        if (vendorId == "0x1002" &&
            !drivers.contains("radeonsi")) {
            drivers.append("radeonsi");
        } else if (vendorId == "0x8086" &&
                   !drivers.contains("iris")) {
            drivers.append("iris");
        }
    }

    return drivers;
}

void configureLinuxOpenClEnvironment()
{
    // Mesa deliberately keeps Rusticl devices hidden unless RUSTICL_ENABLE is
    // set. Enable the matching Gallium drivers automatically when rusticl.icd
    // is installed. Respect an explicit user setting so troubleshooting and
    // distribution-specific overrides remain possible.
    if (!qEnvironmentVariableIsEmpty("RUSTICL_ENABLE") ||
        !QFileInfo::exists("/etc/OpenCL/vendors/rusticl.icd")) {
        return;
    }

    const QStringList drivers =
        linuxRusticlDrivers();

    if (drivers.isEmpty()) {
        return;
    }

    qputenv(
        "RUSTICL_ENABLE",
        drivers.join(',').toLatin1()
        );
}
}
#endif

#ifndef ONPOINT_APP_VERSION
#define ONPOINT_APP_VERSION "1.0.0"
#endif

#ifndef ONPOINT_DESKTOP_FILE_ID
#define ONPOINT_DESKTOP_FILE_ID "com.onpoint.OnPointPhotoEditor"
#endif

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX)
    configureLinuxOpenClEnvironment();
#endif

    QApplication application(argc, argv);

    QCoreApplication::setOrganizationName(
        "OnPoint"
        );
    QCoreApplication::setApplicationName(
        "OnPointPhotoEditor"
        );
    QCoreApplication::setApplicationVersion(
        ONPOINT_APP_VERSION
        );

#if defined(Q_OS_LINUX)
    QGuiApplication::setDesktopFileName(
        ONPOINT_DESKTOP_FILE_ID
        );
#endif

    const QIcon appIcon(
        ":/styles/OnPoint1.png"
        );

    application.setWindowIcon(appIcon);

    StartupCoordinator startupCoordinator(
        appIcon,
        &application
        );

    startupCoordinator.start();

    return application.exec();
}
