#ifndef SYSTEMPROFILE_H
#define SYSTEMPROFILE_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>

struct SystemSnapshot
{
    QString operatingSystem;
    QString kernelVersion;
    QString desktopSession;
    QString qtVersion;

    QString cpuModel;
    int physicalCores = 0;
    int logicalThreads = 0;
    quint64 memoryMiB = 0;

    QStringList gpuDescriptions;
    QString graphicsDriver;
    QString graphicsMemory;
    QString openGlSummary;
    QString vulkanSummary;

    QString fingerprint() const;
    QString displayText() const;
    QString reportText() const;

    QJsonObject toJson() const;
    static SystemSnapshot fromJson(
        const QJsonObject &object
        );
};

struct PerformanceSettings
{
    QString preferredBackend = "auto";
    bool cpuFallback = true;
    int workerThreads = 1;
    int tileHeight = 256;
    int interactivePreviewMaxDimension = 1800;
    bool manualOverride = false;
    qint64 cpuBenchmarkMilliseconds = 0;

    QJsonObject toJson() const;
    static PerformanceSettings fromJson(
        const QJsonObject &object
        );
};

struct PerformanceProfile
{
    QString profileId;
    QString systemFingerprint;
    QString optimizedAt;
    QString applicationVersion;
    int engineSchemaVersion = 1;

    SystemSnapshot system;
    PerformanceSettings settings;

    bool isUsableFor(
        const SystemSnapshot &snapshot,
        int expectedEngineSchemaVersion
        ) const;

    QString summaryText() const;
    QString reportText() const;

    QJsonObject toJson() const;
    static PerformanceProfile fromJson(
        const QJsonObject &object
        );
};

namespace SystemProfiler
{

using ProgressCallback =
    std::function<void(int, const QString &)>;

constexpr int EngineSchemaVersion = 2;

QString activeProfilePath();

SystemSnapshot detectSystem(
    const ProgressCallback &progress = ProgressCallback()
    );

PerformanceProfile optimizeSystem(
    const SystemSnapshot &snapshot,
    const ProgressCallback &progress = ProgressCallback()
    );

bool loadProfile(
    PerformanceProfile *profile,
    QString *errorMessage = nullptr
    );

bool saveProfile(
    const PerformanceProfile &profile,
    QString *errorMessage = nullptr
    );

bool saveProfileCopy(
    const PerformanceProfile &profile,
    const QString &filePath,
    QString *errorMessage = nullptr
    );

bool saveSystemReport(
    const PerformanceProfile &profile,
    const QString &filePath,
    QString *errorMessage = nullptr
    );

} // namespace SystemProfiler

#endif // SYSTEMPROFILE_H
