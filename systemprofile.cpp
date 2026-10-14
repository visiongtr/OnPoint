#include "systemprofile.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QUuid>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <thread>
#include <limits>
#include <vector>

namespace
{

QString readTextFile(
    const QString &filePath
    )
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

QString cleanedValue(
    const QString &value
    )
{
    QString result = value.trimmed();

    if (result.size() >= 2 &&
        result.startsWith('"') &&
        result.endsWith('"')) {
        result = result.mid(1, result.size() - 2);
    }

    return result.trimmed();
}

QString osReleaseValue(
    const QString &key
    )
{
    const QString contents =
        readTextFile("/etc/os-release");

    const QStringList lines =
        contents.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        const int separator = line.indexOf('=');

        if (separator <= 0) {
            continue;
        }

        if (line.left(separator).trimmed() == key) {
            return cleanedValue(
                line.mid(separator + 1)
                );
        }
    }

    return QString();
}

QString runCommand(
    const QString &program,
    const QStringList &arguments,
    int timeoutMilliseconds = 1800
    )
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(
        QProcess::MergedChannels
        );

    process.start();

    if (!process.waitForStarted(500)) {
        return QString();
    }

    if (!process.waitForFinished(timeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(300);
        return QString();
    }

    return QString::fromLocal8Bit(
        process.readAll()
        ).trimmed();
}

QString detectCpuModel()
{
    const QString contents =
        readTextFile("/proc/cpuinfo");

    const QRegularExpression expression(
        R"(^\s*(?:model name|Hardware)\s*:\s*(.+)$)",
        QRegularExpression::MultilineOption
        );

    const QRegularExpressionMatch match =
        expression.match(contents);

    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return QSysInfo::currentCpuArchitecture();
}

int detectPhysicalCoreCount()
{
    const QString contents =
        readTextFile("/proc/cpuinfo");

    if (contents.isEmpty()) {
        return 0;
    }

    const QStringList processorBlocks =
        contents.split(
            QRegularExpression(R"(\n\s*\n)"),
            Qt::SkipEmptyParts
            );

    QSet<QString> corePairs;

    for (const QString &block : processorBlocks) {
        QString physicalId;
        QString coreId;

        const QStringList lines =
            block.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            const int separator = line.indexOf(':');

            if (separator <= 0) {
                continue;
            }

            const QString key =
                line.left(separator).trimmed();

            const QString value =
                line.mid(separator + 1).trimmed();

            if (key == "physical id") {
                physicalId = value;
            } else if (key == "core id") {
                coreId = value;
            }
        }

        if (!coreId.isEmpty()) {
            corePairs.insert(
                physicalId + ':' + coreId
                );
        }
    }

    return corePairs.size();
}

quint64 detectMemoryMiB()
{
    const QString contents =
        readTextFile("/proc/meminfo");

    const QRegularExpression expression(
        R"(^MemTotal:\s+(\d+)\s+kB$)",
        QRegularExpression::MultilineOption
        );

    const QRegularExpressionMatch match =
        expression.match(contents);

    if (!match.hasMatch()) {
        return 0;
    }

    return match.captured(1).toULongLong() / 1024ULL;
}

void detectGraphics(
    SystemSnapshot *snapshot
    )
{
    if (!snapshot) {
        return;
    }

    const QString nvidiaOutput =
        runCommand(
            "nvidia-smi",
            {
                "--query-gpu=name,driver_version,memory.total",
                "--format=csv,noheader,nounits"
            },
            2500
            );

    if (!nvidiaOutput.isEmpty()) {
        const QStringList lines =
            nvidiaOutput.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            const QStringList parts =
                line.split(',');

            if (!parts.isEmpty()) {
                snapshot->gpuDescriptions.append(
                    parts.at(0).trimmed()
                    );
            }

            if (parts.size() >= 2 &&
                snapshot->graphicsDriver.isEmpty()) {
                snapshot->graphicsDriver =
                    "NVIDIA " + parts.at(1).trimmed();
            }

            if (parts.size() >= 3 &&
                snapshot->graphicsMemory.isEmpty()) {
                snapshot->graphicsMemory =
                    parts.at(2).trimmed() + " MiB";
            }
        }
    }

    const QString pciOutput =
        runCommand(
            "lspci",
            {"-nnk"},
            2500
            );

    if (!pciOutput.isEmpty()) {
        const QStringList blocks =
            pciOutput.split(
                QRegularExpression(
                    R"(\n(?=(?:[0-9a-fA-F]{4}:)?[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]\s))"
                    ),
                Qt::SkipEmptyParts
                );

        for (const QString &block : blocks) {
            const QString firstLine =
                block.section('\n', 0, 0).trimmed();

            if (!firstLine.contains("VGA compatible controller", Qt::CaseInsensitive) &&
                !firstLine.contains("3D controller", Qt::CaseInsensitive) &&
                !firstLine.contains("Display controller", Qt::CaseInsensitive)) {
                continue;
            }

            QString description = firstLine;
            const int classSeparator = description.indexOf(": ");

            if (classSeparator >= 0) {
                description =
                    description.mid(classSeparator + 2).trimmed();
            }

            if (!description.isEmpty() &&
                !snapshot->gpuDescriptions.contains(description)) {
                snapshot->gpuDescriptions.append(description);
            }

            if (snapshot->graphicsDriver.isEmpty()) {
                const QRegularExpression driverExpression(
                    R"(Kernel driver in use:\s*(.+))"
                    );

                const QRegularExpressionMatch driverMatch =
                    driverExpression.match(block);

                if (driverMatch.hasMatch()) {
                    snapshot->graphicsDriver =
                        driverMatch.captured(1).trimmed();
                }
            }
        }
    }

    const QString glxOutput =
        runCommand(
            "glxinfo",
            {"-B"},
            2200
            );

    if (!glxOutput.isEmpty()) {
        QString renderer;
        QString version;

        const QStringList lines =
            glxOutput.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();

            if (trimmed.startsWith("OpenGL renderer string:")) {
                renderer =
                    trimmed.section(':', 1).trimmed();
            } else if (trimmed.startsWith("OpenGL core profile version string:")) {
                version =
                    trimmed.section(':', 1).trimmed();
            } else if (version.isEmpty() &&
                       trimmed.startsWith("OpenGL version string:")) {
                version =
                    trimmed.section(':', 1).trimmed();
            }
        }

        snapshot->openGlSummary =
            QStringList({renderer, version})
                .filter(
                    QRegularExpression(".+")
                    )
                .join(" — ");

        // lspci can be absent in a minimal installation. The OpenGL renderer
        // is still a useful fallback for the optimizer display and hardware
        // fingerprint when it identifies a real graphics adapter.
        if (snapshot->gpuDescriptions.isEmpty() &&
            !renderer.isEmpty()) {
            snapshot->gpuDescriptions.append(renderer);
        }
    }

    if (snapshot->openGlSummary.isEmpty()) {
        snapshot->openGlSummary =
            "Runtime probe unavailable";
    }

    const QString vulkanOutput =
        runCommand(
            "vulkaninfo",
            {"--summary"},
            2500
            );

    if (!vulkanOutput.isEmpty()) {
        QString deviceName;
        QString apiVersion;

        const QStringList lines =
            vulkanOutput.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();

            if (deviceName.isEmpty() &&
                trimmed.startsWith("deviceName")) {
                deviceName =
                    trimmed.section('=', 1).trimmed();
            }

            if (apiVersion.isEmpty() &&
                trimmed.startsWith("apiVersion")) {
                apiVersion =
                    trimmed.section('=', 1).trimmed();
            }
        }

        snapshot->vulkanSummary =
            QStringList({deviceName, apiVersion})
                .filter(
                    QRegularExpression(".+")
                    )
                .join(" — ");

        if (snapshot->gpuDescriptions.isEmpty() &&
            !deviceName.isEmpty()) {
            snapshot->gpuDescriptions.append(deviceName);
        }
    }

    if (snapshot->vulkanSummary.isEmpty()) {
        snapshot->vulkanSummary =
            "Runtime probe unavailable";
    }

    if (snapshot->gpuDescriptions.isEmpty()) {
        snapshot->gpuDescriptions.append(
            "No graphics adapter was reported"
            );
    }
}

QString makeProfileId()
{
    QString token =
        QUuid::createUuid()
            .toString(QUuid::WithoutBraces)
            .remove('-')
            .left(8)
            .toUpper();

    return "OP-" + token.left(4) + '-' + token.mid(4, 4);
}

int boundedWorkerCount(
    int logicalThreads
    )
{
    if (logicalThreads <= 1) {
        return 1;
    }

    if (logicalThreads <= 4) {
        return qMax(1, logicalThreads - 1);
    }

    if (logicalThreads <= 8) {
        return qMax(2, logicalThreads - 2);
    }

    return qMax(4, logicalThreads - 4);
}

qint64 runSyntheticWorkload(
    int workerCount
    )
{
    constexpr int width = 2048;
    constexpr int height = 1536;
    constexpr int passes = 3;

    std::vector<quint32> pixels(
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height)
        );

    for (std::size_t index = 0;
         index < pixels.size();
         ++index) {
        const quint32 value =
            static_cast<quint32>(index * 2654435761U);

        pixels[index] =
            0xFF000000U |
            (value & 0x00FFFFFFU);
    }

    QElapsedTimer timer;
    timer.start();

    const int threads =
        qBound(1, workerCount, height);

    for (int pass = 0; pass < passes; ++pass) {
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(threads));

        for (int threadIndex = 0;
             threadIndex < threads;
             ++threadIndex) {
            const int firstRow =
                (height * threadIndex) / threads;

            const int lastRow =
                (height * (threadIndex + 1)) / threads;

            workers.emplace_back(
                [&, firstRow, lastRow, pass]() {
                    for (int y = firstRow;
                         y < lastRow;
                         ++y) {
                        quint32 *row =
                            pixels.data() +
                            static_cast<std::size_t>(y) * width;

                        for (int x = 0;
                             x < width;
                             ++x) {
                            const quint32 pixel = row[x];

                            const int red =
                                static_cast<int>((pixel >> 16) & 0xFFU);

                            const int green =
                                static_cast<int>((pixel >> 8) & 0xFFU);

                            const int blue =
                                static_cast<int>(pixel & 0xFFU);

                            const int adjustedRed =
                                qBound(
                                    0,
                                    (red * 113 + green * 7 + pass * 3) / 120,
                                    255
                                    );

                            const int adjustedGreen =
                                qBound(
                                    0,
                                    (green * 109 + blue * 11 + pass * 2) / 120,
                                    255
                                    );

                            const int adjustedBlue =
                                qBound(
                                    0,
                                    (blue * 111 + red * 9 + pass) / 120,
                                    255
                                    );

                            row[x] =
                                0xFF000000U |
                                (static_cast<quint32>(adjustedRed) << 16) |
                                (static_cast<quint32>(adjustedGreen) << 8) |
                                static_cast<quint32>(adjustedBlue);
                        }
                    }
                });
        }

        for (std::thread &worker : workers) {
            worker.join();
        }
    }

    // Prevent an optimizer from discarding the calculation in release builds.
    static std::atomic<quint32> sink {0};
    sink.store(
        pixels[pixels.size() / 2],
        std::memory_order_relaxed
        );

    return qMax<qint64>(1, timer.elapsed());
}

PerformanceSettings benchmarkCpu(
    const SystemSnapshot &snapshot,
    const SystemProfiler::ProgressCallback &progress
    )
{
    PerformanceSettings settings;

    const int logicalThreads =
        qMax(1, snapshot.logicalThreads);

    QList<int> candidates = {
        1,
        qMax(2, logicalThreads / 4),
        qMax(2, logicalThreads / 2),
        boundedWorkerCount(logicalThreads)
    };

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(
        std::unique(candidates.begin(), candidates.end()),
        candidates.end()
        );

    qint64 bestTime = std::numeric_limits<qint64>::max();
    int bestWorkers = 1;

    for (int index = 0;
         index < candidates.size();
         ++index) {
        const int workers =
            qBound(1, candidates.at(index), logicalThreads);

        if (progress) {
            const int percentage =
                56 +
                ((index + 1) * 26) /
                    qMax(1, candidates.size());

            progress(
                percentage,
                QString("Benchmarking CPU image work with %1 worker%2...")
                    .arg(workers)
                    .arg(workers == 1 ? "" : "s")
                );
        }

        const qint64 elapsed =
            runSyntheticWorkload(workers);

        if (elapsed < bestTime) {
            bestTime = elapsed;
            bestWorkers = workers;
        }
    }

    settings.workerThreads =
        qBound(1, bestWorkers, logicalThreads);

    settings.cpuBenchmarkMilliseconds =
        bestTime == std::numeric_limits<qint64>::max()
        ? 0
        : bestTime;

    settings.cpuFallback = true;
    settings.preferredBackend =
        snapshot.gpuDescriptions.isEmpty() ||
                snapshot.gpuDescriptions.first().startsWith("No graphics")
        ? "cpu"
        : "auto";

    if (snapshot.memoryMiB > 0 &&
        snapshot.memoryMiB <= 8192) {
        settings.interactivePreviewMaxDimension = 1400;
        settings.tileHeight = 128;
    } else if (logicalThreads <= 6) {
        settings.interactivePreviewMaxDimension = 1600;
        settings.tileHeight = 192;
    } else if (logicalThreads >= 16 &&
               snapshot.memoryMiB >= 24576) {
        settings.interactivePreviewMaxDimension = 2200;
        settings.tileHeight = 256;
    } else {
        settings.interactivePreviewMaxDimension = 1800;
        settings.tileHeight = 256;
    }

    return settings;
}

QString normalizedProfileFilePath(
    const QString &filePath
    )
{
    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        return filePath;
    }

    return filePath + ".json";
}

} // namespace

QString SystemSnapshot::fingerprint() const
{
    QStringList stableFields;

    stableFields
        << cpuModel
        << QString::number(physicalCores)
        << QString::number(logicalThreads)
        << QString::number(memoryMiB / 256ULL)
        << gpuDescriptions.join('|')
        << graphicsDriver
        << openGlSummary
        << vulkanSummary
        << qtVersion
        << QString::number(SystemProfiler::EngineSchemaVersion);

    const QByteArray digest =
        QCryptographicHash::hash(
            stableFields.join("\n").toUtf8(),
            QCryptographicHash::Sha256
            );

    return QString::fromLatin1(
        digest.toHex()
        );
}

QString SystemSnapshot::displayText() const
{
    const QString gpuText =
        gpuDescriptions.join("\n           ");

    QString text;
    text += "SYSTEM      " +
        (operatingSystem.isEmpty() ? QString("Unknown") : operatingSystem) +
        '\n';
    text += "KERNEL      " +
        (kernelVersion.isEmpty() ? QString("Unknown") : kernelVersion) +
        '\n';
    text += "SESSION     " +
        (desktopSession.isEmpty() ? QString("Unknown") : desktopSession) +
        '\n';
    text += "QT          " +
        (qtVersion.isEmpty() ? QString("Unknown") : qtVersion) +
        "\n\n";
    text += "CPU         " +
        (cpuModel.isEmpty() ? QString("Unknown") : cpuModel) +
        '\n';
    text += QString("CORES       %1 physical / %2 logical\n")
        .arg(physicalCores)
        .arg(logicalThreads);
    text += QString("MEMORY      %1 MiB\n\n")
        .arg(memoryMiB);
    text += "GPU         " +
        (gpuText.isEmpty() ? QString("Unknown") : gpuText) +
        '\n';
    text += "DRIVER      " +
        (graphicsDriver.isEmpty() ? QString("Unknown") : graphicsDriver) +
        '\n';
    text += "VRAM        " +
        (graphicsMemory.isEmpty() ? QString("Unknown") : graphicsMemory) +
        '\n';
    text += "OPENGL      " +
        (openGlSummary.isEmpty() ? QString("Unknown") : openGlSummary) +
        '\n';
    text += "VULKAN      " +
        (vulkanSummary.isEmpty() ? QString("Unknown") : vulkanSummary);

    return text;
}

QString SystemSnapshot::reportText() const
{
    return displayText() +
        "\n\nSYSTEM FINGERPRINT\n" +
        fingerprint();
}

QJsonObject SystemSnapshot::toJson() const
{
    QJsonArray gpuArray;

    for (const QString &gpu : gpuDescriptions) {
        gpuArray.append(gpu);
    }

    QJsonObject object;
    object.insert("operatingSystem", operatingSystem);
    object.insert("kernelVersion", kernelVersion);
    object.insert("desktopSession", desktopSession);
    object.insert("qtVersion", qtVersion);
    object.insert("cpuModel", cpuModel);
    object.insert("physicalCores", physicalCores);
    object.insert("logicalThreads", logicalThreads);
    object.insert("memoryMiB", static_cast<double>(memoryMiB));
    object.insert("gpus", gpuArray);
    object.insert("graphicsDriver", graphicsDriver);
    object.insert("graphicsMemory", graphicsMemory);
    object.insert("openGlSummary", openGlSummary);
    object.insert("vulkanSummary", vulkanSummary);
    return object;
}

SystemSnapshot SystemSnapshot::fromJson(
    const QJsonObject &object
    )
{
    SystemSnapshot snapshot;
    snapshot.operatingSystem = object.value("operatingSystem").toString();
    snapshot.kernelVersion = object.value("kernelVersion").toString();
    snapshot.desktopSession = object.value("desktopSession").toString();
    snapshot.qtVersion = object.value("qtVersion").toString();
    snapshot.cpuModel = object.value("cpuModel").toString();
    snapshot.physicalCores = object.value("physicalCores").toInt();
    snapshot.logicalThreads = object.value("logicalThreads").toInt();
    snapshot.memoryMiB = static_cast<quint64>(object.value("memoryMiB").toVariant().toULongLong());

    const QJsonArray gpuArray = object.value("gpus").toArray();

    for (const QJsonValue &gpu : gpuArray) {
        snapshot.gpuDescriptions.append(gpu.toString());
    }

    snapshot.graphicsDriver = object.value("graphicsDriver").toString();
    snapshot.graphicsMemory = object.value("graphicsMemory").toString();
    snapshot.openGlSummary = object.value("openGlSummary").toString();
    snapshot.vulkanSummary = object.value("vulkanSummary").toString();
    return snapshot;
}

QJsonObject PerformanceSettings::toJson() const
{
    QJsonObject object;
    object.insert("preferredBackend", preferredBackend);
    object.insert("cpuFallback", cpuFallback);
    object.insert("workerThreads", workerThreads);
    object.insert("tileHeight", tileHeight);
    object.insert("interactivePreviewMaxDimension", interactivePreviewMaxDimension);
    object.insert("manualOverride", manualOverride);
    object.insert("cpuBenchmarkMilliseconds", static_cast<double>(cpuBenchmarkMilliseconds));
    return object;
}

PerformanceSettings PerformanceSettings::fromJson(
    const QJsonObject &object
    )
{
    PerformanceSettings settings;
    settings.preferredBackend = object.value("preferredBackend").toString("auto");
    settings.cpuFallback = object.value("cpuFallback").toBool(true);
    settings.workerThreads = qMax(1, object.value("workerThreads").toInt(1));
    settings.tileHeight = qMax(32, object.value("tileHeight").toInt(256));
    settings.interactivePreviewMaxDimension = qMax(640, object.value("interactivePreviewMaxDimension").toInt(1800));
    settings.manualOverride = object.value("manualOverride").toBool(false);
    settings.cpuBenchmarkMilliseconds = object.value("cpuBenchmarkMilliseconds").toVariant().toLongLong();
    return settings;
}

bool PerformanceProfile::isUsableFor(
    const SystemSnapshot &snapshot,
    int expectedEngineSchemaVersion
    ) const
{
    return !profileId.isEmpty() &&
        engineSchemaVersion == expectedEngineSchemaVersion &&
        systemFingerprint == snapshot.fingerprint();
}

QString PerformanceProfile::summaryText() const
{
    QString backend = settings.preferredBackend;

    if (backend.compare("auto", Qt::CaseInsensitive) == 0) {
        backend = "Automatic GPU/CPU selection";
    } else if (backend.compare("gpu", Qt::CaseInsensitive) == 0) {
        backend = "Prefer GPU";
    } else {
        backend = "CPU";
    }

    return QString(
        "PROFILE     %1\n"
        "STATUS      %2\n"
        "BACKEND     %3\n"
        "CPU WORKERS %4\n"
        "TILE HEIGHT %5 rows\n"
        "PREVIEW     %6 px maximum dimension\n"
        "FALLBACK    %7\n"
        "CPU TEST    %8 ms\n"
        "OPTIMIZED   %9"
        )
        .arg(
            profileId,
            settings.manualOverride ? "Manual override" : "Automatically optimized",
            backend,
            QString::number(settings.workerThreads),
            QString::number(settings.tileHeight),
            QString::number(settings.interactivePreviewMaxDimension),
            settings.cpuFallback ? "Enabled" : "Disabled",
            QString::number(settings.cpuBenchmarkMilliseconds),
            optimizedAt
            );
}

QString PerformanceProfile::reportText() const
{
    return QString(
        "ON POINT PHOTO EDITOR — SYSTEM REPORT\n"
        "Generated: %1\n"
        "Application version: %2\n"
        "Engine schema: %3\n"
        "\n"
        "%4\n"
        "\n"
        "%5\n"
        )
        .arg(
            QDateTime::currentDateTime().toString(Qt::ISODate),
            applicationVersion,
            QString::number(engineSchemaVersion),
            system.reportText(),
            summaryText()
            );
}

QJsonObject PerformanceProfile::toJson() const
{
    QJsonObject object;
    object.insert("profileId", profileId);
    object.insert("systemFingerprint", systemFingerprint);
    object.insert("optimizedAt", optimizedAt);
    object.insert("applicationVersion", applicationVersion);
    object.insert("engineSchemaVersion", engineSchemaVersion);
    object.insert("system", system.toJson());
    object.insert("settings", settings.toJson());
    return object;
}

PerformanceProfile PerformanceProfile::fromJson(
    const QJsonObject &object
    )
{
    PerformanceProfile profile;
    profile.profileId = object.value("profileId").toString();
    profile.systemFingerprint = object.value("systemFingerprint").toString();
    profile.optimizedAt = object.value("optimizedAt").toString();
    profile.applicationVersion = object.value("applicationVersion").toString();
    profile.engineSchemaVersion = object.value("engineSchemaVersion").toInt();
    profile.system = SystemSnapshot::fromJson(object.value("system").toObject());
    profile.settings = PerformanceSettings::fromJson(object.value("settings").toObject());
    return profile;
}

namespace SystemProfiler
{

QString activeProfilePath()
{
    QString directory =
        QStandardPaths::writableLocation(
            QStandardPaths::AppConfigLocation
            );

    if (directory.isEmpty()) {
        directory =
            QDir::homePath() +
            "/.config/OnPointPhotoEditor";
    }

    QDir().mkpath(directory);

    return QDir(directory).filePath(
        "performance-profile.json"
        );
}

SystemSnapshot detectSystem(
    const ProgressCallback &progress
    )
{
    SystemSnapshot snapshot;

    if (progress) {
        progress(5, "Reading operating system information...");
    }

    snapshot.operatingSystem =
        osReleaseValue("PRETTY_NAME");

    if (snapshot.operatingSystem.isEmpty()) {
        snapshot.operatingSystem =
            QSysInfo::prettyProductName();
    }

    snapshot.kernelVersion =
        QSysInfo::kernelType() + ' ' +
        QSysInfo::kernelVersion();

    snapshot.desktopSession =
        QString::fromLocal8Bit(
            qgetenv("XDG_CURRENT_DESKTOP")
            );

    const QString sessionType =
        QString::fromLocal8Bit(
            qgetenv("XDG_SESSION_TYPE")
            );

    if (!sessionType.isEmpty()) {
        if (!snapshot.desktopSession.isEmpty()) {
            snapshot.desktopSession += " / ";
        }

        snapshot.desktopSession += sessionType;
    }

    snapshot.qtVersion =
        QString::fromLatin1(qVersion());

    if (progress) {
        progress(18, "Reading CPU topology...");
    }

    snapshot.cpuModel = detectCpuModel();
    snapshot.logicalThreads =
        qMax(1, QThread::idealThreadCount());
    snapshot.physicalCores =
        detectPhysicalCoreCount();

    if (snapshot.physicalCores <= 0) {
        snapshot.physicalCores =
            snapshot.logicalThreads;
    }

    if (progress) {
        progress(30, "Reading installed memory...");
    }

    snapshot.memoryMiB = detectMemoryMiB();

    if (progress) {
        progress(42, "Detecting graphics hardware and drivers...");
    }

    detectGraphics(&snapshot);

    if (progress) {
        progress(52, "Building the system fingerprint...");
    }

    return snapshot;
}

PerformanceProfile optimizeSystem(
    const SystemSnapshot &snapshot,
    const ProgressCallback &progress
    )
{
    if (progress) {
        progress(55, "Preparing a synthetic image-processing test...");
    }

    PerformanceProfile profile;
    profile.profileId = makeProfileId();
    profile.system = snapshot;
    profile.systemFingerprint = snapshot.fingerprint();
    profile.optimizedAt =
        QDateTime::currentDateTime().toString(Qt::ISODate);
    profile.applicationVersion =
        QCoreApplication::applicationVersion();
    profile.engineSchemaVersion = EngineSchemaVersion;
    profile.settings = benchmarkCpu(snapshot, progress);

    if (progress) {
        progress(90, "Selecting safe preview and fallback settings...");
    }

    if (progress) {
        progress(96, "Writing the On Point performance profile...");
    }

    return profile;
}

bool loadProfile(
    PerformanceProfile *profile,
    QString *errorMessage
    )
{
    if (!profile) {
        if (errorMessage) {
            *errorMessage = "No destination profile was supplied.";
        }

        return false;
    }

    QFile file(activeProfilePath());

    if (!file.exists()) {
        if (errorMessage) {
            *errorMessage = "No saved performance profile exists yet.";
        }

        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            file.readAll(),
            &parseError
            );

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        if (errorMessage) {
            *errorMessage =
                "The saved profile is not valid JSON: " +
                parseError.errorString();
        }

        return false;
    }

    *profile =
        PerformanceProfile::fromJson(
            document.object()
            );

    if (profile->profileId.isEmpty()) {
        if (errorMessage) {
            *errorMessage =
                "The saved profile does not contain a profile ID.";
        }

        return false;
    }

    return true;
}

bool saveProfile(
    const PerformanceProfile &profile,
    QString *errorMessage
    )
{
    return saveProfileCopy(
        profile,
        activeProfilePath(),
        errorMessage
        );
}

bool saveProfileCopy(
    const PerformanceProfile &profile,
    const QString &filePath,
    QString *errorMessage
    )
{
    const QString normalizedPath =
        normalizedProfileFilePath(filePath);

    QDir().mkpath(
        QFileInfo(normalizedPath).absolutePath()
        );

    QSaveFile file(normalizedPath);

    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        return false;
    }

    const QByteArray json =
        QJsonDocument(profile.toJson())
            .toJson(QJsonDocument::Indented);

    if (file.write(json) != json.size()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        return false;
    }

    return true;
}

bool saveSystemReport(
    const PerformanceProfile &profile,
    const QString &filePath,
    QString *errorMessage
    )
{
    QSaveFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        return false;
    }

    const QByteArray report =
        profile.reportText().toUtf8();

    if (file.write(report) != report.size()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }

        return false;
    }

    return true;
}

} // namespace SystemProfiler
