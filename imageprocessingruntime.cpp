#include "imageprocessingruntime.h"

#include <QDebug>
#include <QString>
#include <QMutexLocker>
#include <QRunnable>
#include <QSemaphore>
#include <QThread>

#include <algorithm>
#include <memory>

namespace
{
class RowRangeTask final : public QRunnable
{
public:
    RowRangeTask(
        const std::shared_ptr<std::function<void(int, int)>> &rowFunction,
        int firstRow,
        int endRow,
        QSemaphore *completionSemaphore
        )
        : m_rowFunction(rowFunction)
        , m_firstRow(firstRow)
        , m_endRow(endRow)
        , m_completionSemaphore(completionSemaphore)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        try {
            if (m_rowFunction && *m_rowFunction) {
                (*m_rowFunction)(
                    m_firstRow,
                    m_endRow
                    );
            }
        } catch (...) {
            // Image-processing row functions are not expected to throw.
            // Always release the waiter so a failed worker cannot deadlock
            // the editor. The CPU fallback remains available for future
            // backend-level error handling.
        }

        if (m_completionSemaphore) {
            m_completionSemaphore->release();
        }
    }

private:
    std::shared_ptr<std::function<void(int, int)>> m_rowFunction;
    int m_firstRow = 0;
    int m_endRow = 0;
    QSemaphore *m_completionSemaphore = nullptr;
};
}

ImageProcessingRuntime &ImageProcessingRuntime::instance()
{
    static ImageProcessingRuntime runtime;
    return runtime;
}

ImageProcessingRuntime::ImageProcessingRuntime()
    : m_gpuBackend(
          std::make_unique<OpenClProcessingBackend>()
          )
    , m_spatialGpuBackend(
          std::make_unique<OpenClSpatialProcessingBackend>()
          )
{
    m_settings.workerThreads =
        std::max(1, QThread::idealThreadCount());

    m_threadPool.setMaxThreadCount(
        m_settings.workerThreads
        );

    // Keep the optimized worker pool warm between slider updates.
    m_threadPool.setExpiryTimeout(-1);
}

ImageProcessingRuntime::~ImageProcessingRuntime()
{
    m_threadPool.waitForDone();
}

void ImageProcessingRuntime::configure(
    const PerformanceSettings &settings
    )
{
    PerformanceSettings safeSettings = settings;

    safeSettings.workerThreads =
        std::max(1, safeSettings.workerThreads);

    safeSettings.tileHeight =
        std::max(32, safeSettings.tileHeight);

    safeSettings.interactivePreviewMaxDimension =
        std::max(
            640,
            safeSettings.interactivePreviewMaxDimension
            );

    {
        QMutexLocker locker(&m_settingsMutex);
        m_settings = safeSettings;
    }

    m_gpuUseReported.store(false);
    m_gpuAdjustmentUseReported.store(false);
    m_gpuNoiseUseReported.store(false);
    m_gpuFocusUseReported.store(false);
    m_gpuOverexposureUseReported.store(false);

    m_threadPool.setMaxThreadCount(
        safeSettings.workerThreads
        );

    const bool cpuOnly =
        safeSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0;

    if (m_gpuBackend) {
        if (cpuOnly) {
            m_gpuBackend->shutdown();
        } else {
            m_gpuBackend->initialize();
        }
    }

    if (m_spatialGpuBackend) {
        if (cpuOnly) {
            m_spatialGpuBackend->shutdown();
        } else {
            m_spatialGpuBackend->initialize();
        }
    }

    qInfo().noquote()
        << QString(
               "On Point processing runtime: %1 CPU workers, "
               "%2-row tiles, %3 px interactive preview, "
               "backend preference %4, CPU fallback %5"
               )
               .arg(safeSettings.workerThreads)
               .arg(safeSettings.tileHeight)
               .arg(
                   safeSettings
                       .interactivePreviewMaxDimension
                   )
               .arg(safeSettings.preferredBackend)
               .arg(
                   safeSettings.cpuFallback
                       ? "enabled"
                       : "disabled"
                   );

    if (cpuOnly) {
        qInfo().noquote()
            << "On Point GPU backend: disabled by CPU-only profile";
        return;
    }

    if (m_gpuBackend &&
        m_gpuBackend->isAvailable()) {
        qInfo().noquote()
            << QString(
                   "On Point GPU backend: OpenCL ready — %1"
                   ).arg(m_gpuBackend->deviceName());
        qInfo().noquote()
            << "On Point GPU stage 1: channel lookup processing is active "
               "for Brightness, Contrast, Gamma, RGB shifts, White Balance, "
               "and Hue/Tone";

        if (m_gpuBackend->adjustmentPipelineAvailable()) {
            qInfo().noquote()
                << "On Point GPU stage 2: fused Basic Tone, Base HMS, "
                   "Color HMS, and Saturation pipeline is ready; "
                   "CPU fallback remains active";
        } else {
            qInfo().noquote()
                << "On Point GPU stage 2: advanced color pipeline unavailable; "
                   "those controls remain on the optimized CPU engine";
        }
    } else if (m_gpuBackend) {
        qInfo().noquote()
            << "On Point GPU color backend:"
            << m_gpuBackend->statusText();
    }

    // Stage 3 is deliberately isolated from the color backend. Report it
    // independently so a spatial-capable device can remain useful even if a
    // color-program initialization problem occurs.
    if (m_spatialGpuBackend &&
        m_spatialGpuBackend->isAvailable()) {
        qInfo().noquote()
            << "On Point GPU stage 3: Noise Reduction, Focus/Sharpen, "
               "and Overexposure Recovery spatial pipelines are ready; "
               "CPU fallback remains active";
    } else if (m_spatialGpuBackend) {
        qInfo().noquote()
            << "On Point GPU stage 3: spatial pipelines unavailable —"
            << m_spatialGpuBackend->statusText();
    }
}

PerformanceSettings ImageProcessingRuntime::settings() const
{
    QMutexLocker locker(&m_settingsMutex);
    return m_settings;
}

int ImageProcessingRuntime::workerThreads() const
{
    return settings().workerThreads;
}

int ImageProcessingRuntime::tileHeight() const
{
    return settings().tileHeight;
}

int ImageProcessingRuntime::interactivePreviewMaxDimension() const
{
    return settings().interactivePreviewMaxDimension;
}

bool ImageProcessingRuntime::gpuBackendAvailable() const
{
    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0) {
        return false;
    }

    return m_gpuBackend &&
        m_gpuBackend->isAvailable();
}

bool ImageProcessingRuntime::gpuAdjustmentPipelineAvailable() const
{
    return gpuBackendAvailable() &&
        m_gpuBackend &&
        m_gpuBackend->adjustmentPipelineAvailable();
}

bool ImageProcessingRuntime::gpuSpatialPipelineAvailable() const
{
    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0) {
        return false;
    }

    return m_spatialGpuBackend &&
        m_spatialGpuBackend->isAvailable();
}

QString ImageProcessingRuntime::gpuBackendName() const
{
    if (gpuBackendAvailable() &&
        m_gpuBackend) {
        return m_gpuBackend->deviceName();
    }

    if (gpuSpatialPipelineAvailable() &&
        m_spatialGpuBackend) {
        return m_spatialGpuBackend->deviceName();
    }

    return QString();
}

QString ImageProcessingRuntime::processingBackendSummary() const
{
    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0) {
        return "CPU thread pool";
    }

    if (gpuBackendAvailable() ||
        gpuSpatialPipelineAvailable()) {
        return QString(
            "OpenCL GPU (%1) with CPU fallback"
            ).arg(gpuBackendName());
    }

    return "CPU thread pool (GPU unavailable)";
}

bool ImageProcessingRuntime::tryApplyChannelLookups(
    QImage &image,
    const OpenClProcessingBackend::ChannelLookup &redLookup,
    const OpenClProcessingBackend::ChannelLookup &greenLookup,
    const OpenClProcessingBackend::ChannelLookup &blueLookup
    )
{
    if (image.isNull() ||
        !m_gpuBackend) {
        return false;
    }

    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0 ||
        !m_gpuBackend->isAvailable()) {
        return false;
    }

    const quint64 pixelCount =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());

    const bool preferGpu =
        currentSettings.preferredBackend.compare(
            "gpu",
            Qt::CaseInsensitive
            ) == 0;

    // Automatic mode avoids transfer overhead on small images. A manual
    // Prefer GPU profile lowers the threshold for systems whose graphics
    // processor is substantially stronger than their CPU.
    const quint64 minimumGpuPixels =
        preferGpu
        ? static_cast<quint64>(256) * 256
        : static_cast<quint64>(1200) * 1200;

    if (pixelCount < minimumGpuPixels) {
        return false;
    }

    QString errorMessage;

    if (m_gpuBackend->applyChannelLookups(
            image,
            redLookup,
            greenLookup,
            blueLookup,
            &errorMessage
            )) {
        if (!m_gpuUseReported.exchange(true)) {
            qInfo().noquote()
                << QString(
                       "On Point GPU processing active: channel lookup kernel "
                       "running on %1"
                       ).arg(m_gpuBackend->deviceName());
        }

        return true;
    }

    qWarning().noquote()
        << "On Point GPU processing fell back to CPU:"
        << errorMessage;

    return false;
}

bool ImageProcessingRuntime::tryApplyAdjustmentPipeline(
    QImage &image,
    const OpenClProcessingBackend::ChannelLookup &redLookup,
    const OpenClProcessingBackend::ChannelLookup &greenLookup,
    const OpenClProcessingBackend::ChannelLookup &blueLookup,
    const OpenClProcessingBackend::AdjustmentPipelineSettings &pipelineSettings
    )
{
    if (image.isNull() ||
        !m_gpuBackend) {
        return false;
    }

    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0 ||
        !m_gpuBackend->isAvailable() ||
        !m_gpuBackend->adjustmentPipelineAvailable()) {
        return false;
    }

    const quint64 pixelCount =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());

    const bool preferGpu =
        currentSettings.preferredBackend.compare(
            "gpu",
            Qt::CaseInsensitive
            ) == 0;

    // The fused color pipeline performs enough arithmetic per pixel to
    // overcome transfer overhead sooner than the simpler lookup kernel.
    const quint64 minimumGpuPixels =
        preferGpu
        ? static_cast<quint64>(256) * 256
        : static_cast<quint64>(800) * 800;

    if (pixelCount < minimumGpuPixels) {
        return false;
    }

    QString errorMessage;

    if (m_gpuBackend->applyAdjustmentPipeline(
            image,
            redLookup,
            greenLookup,
            blueLookup,
            pipelineSettings,
            &errorMessage
            )) {
        if (!m_gpuAdjustmentUseReported.exchange(true)) {
            qInfo().noquote()
                << QString(
                       "On Point GPU processing active: fused adjustment "
                       "pipeline running on %1"
                       ).arg(m_gpuBackend->deviceName());
        }

        return true;
    }

    qWarning().noquote()
        << "On Point GPU adjustment pipeline fell back to CPU:"
        << errorMessage;

    return false;
}

bool ImageProcessingRuntime::tryApplyNoiseReduction(
    QImage &image,
    const OpenClSpatialProcessingBackend::NoiseSettings &noiseSettings
    )
{
    if (image.isNull() ||
        !m_spatialGpuBackend) {
        return false;
    }

    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0 ||
        !m_spatialGpuBackend->isAvailable()) {
        return false;
    }

    const quint64 pixelCount =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());

    const bool preferGpu =
        currentSettings.preferredBackend.compare(
            "gpu",
            Qt::CaseInsensitive
            ) == 0;

    // Spatial filters perform enough neighboring-pixel work that the GPU
    // wins earlier than the simple channel lookup path.
    const quint64 minimumGpuPixels =
        preferGpu
        ? static_cast<quint64>(256) * 256
        : static_cast<quint64>(640) * 640;

    if (pixelCount < minimumGpuPixels) {
        return false;
    }

    QString errorMessage;

    if (m_spatialGpuBackend->applyNoiseReduction(
            image,
            noiseSettings,
            &errorMessage
            )) {
        if (!m_gpuNoiseUseReported.exchange(true)) {
            qInfo().noquote()
                << QString(
                       "On Point GPU processing active: Noise Reduction "
                       "spatial pipeline running on %1"
                       ).arg(m_spatialGpuBackend->deviceName());
        }

        return true;
    }

    qWarning().noquote()
        << "On Point GPU Noise Reduction fell back to CPU:"
        << errorMessage;

    return false;
}

bool ImageProcessingRuntime::tryApplyFocus(
    QImage &image,
    const OpenClSpatialProcessingBackend::FocusSettings &focusSettings
    )
{
    if (image.isNull() ||
        !m_spatialGpuBackend) {
        return false;
    }

    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0 ||
        !m_spatialGpuBackend->isAvailable()) {
        return false;
    }

    const quint64 pixelCount =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());

    const bool preferGpu =
        currentSettings.preferredBackend.compare(
            "gpu",
            Qt::CaseInsensitive
            ) == 0;

    const quint64 minimumGpuPixels =
        preferGpu
        ? static_cast<quint64>(256) * 256
        : static_cast<quint64>(640) * 640;

    if (pixelCount < minimumGpuPixels) {
        return false;
    }

    QString errorMessage;

    if (m_spatialGpuBackend->applyFocus(
            image,
            focusSettings,
            &errorMessage
            )) {
        if (!m_gpuFocusUseReported.exchange(true)) {
            qInfo().noquote()
                << QString(
                       "On Point GPU processing active: Focus/Sharpen "
                       "spatial pipeline running on %1"
                       ).arg(m_spatialGpuBackend->deviceName());
        }

        return true;
    }

    qWarning().noquote()
        << "On Point GPU Focus/Sharpen fell back to CPU:"
        << errorMessage;

    return false;
}

bool ImageProcessingRuntime::tryApplyOverexposureRecovery(
    QImage &image,
    const QRect &selection,
    const OpenClSpatialProcessingBackend::OverexposureSettings &overexposureSettings
    )
{
    if (image.isNull() ||
        selection.isEmpty() ||
        !m_spatialGpuBackend) {
        return false;
    }

    const PerformanceSettings currentSettings =
        settings();

    if (currentSettings.preferredBackend.compare(
            "cpu",
            Qt::CaseInsensitive
            ) == 0 ||
        !m_spatialGpuBackend->isAvailable()) {
        return false;
    }

    const quint64 imagePixelCount =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());
    const quint64 selectionPixelCount =
        static_cast<quint64>(selection.width()) *
        static_cast<quint64>(selection.height());

    const bool preferGpu =
        currentSettings.preferredBackend.compare(
            "gpu",
            Qt::CaseInsensitive
            ) == 0;

    const quint64 minimumImagePixels =
        preferGpu
        ? static_cast<quint64>(256) * 256
        : static_cast<quint64>(800) * 800;
    const quint64 minimumSelectionPixels =
        preferGpu
        ? static_cast<quint64>(64) * 64
        : static_cast<quint64>(128) * 128;

    if (imagePixelCount < minimumImagePixels ||
        selectionPixelCount < minimumSelectionPixels) {
        return false;
    }

    QString errorMessage;

    if (m_spatialGpuBackend->applyOverexposureRecovery(
            image,
            selection,
            overexposureSettings,
            &errorMessage
            )) {
        if (!m_gpuOverexposureUseReported.exchange(true)) {
            qInfo().noquote()
                << QString(
                       "On Point GPU processing active: Overexposure Recovery "
                       "spatial pipeline running on %1"
                       ).arg(m_spatialGpuBackend->deviceName());
        }

        return true;
    }

    qWarning().noquote()
        << "On Point GPU Overexposure Recovery fell back to CPU:"
        << errorMessage;

    return false;
}

QImage ImageProcessingRuntime::makeInteractivePreview(
    const QImage &source
    ) const
{
    if (source.isNull()) {
        return source;
    }

    const int maximumDimension =
        interactivePreviewMaxDimension();

    if (source.width() <= maximumDimension &&
        source.height() <= maximumDimension) {
        return source;
    }

    return source.scaled(
        maximumDimension,
        maximumDimension,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );
}

void ImageProcessingRuntime::parallelForRows(
    int rowCount,
    const std::function<void(int, int)> &rowFunction
    )
{
    if (rowCount <= 0 || !rowFunction) {
        return;
    }

    const PerformanceSettings currentSettings =
        settings();

    const int configuredWorkers =
        std::max(1, currentSettings.workerThreads);

    const int configuredTileHeight =
        std::max(32, currentSettings.tileHeight);

    if (configuredWorkers <= 1 ||
        rowCount <= 64) {
        rowFunction(0, rowCount);
        return;
    }

    // Treat the saved tile height as an upper bound. Smaller previews still
    // receive enough independent tiles to keep the optimized worker pool
    // usefully occupied without creating a task for every scan line.
    const int targetTaskCount =
        std::max(
            1,
            std::min(
                rowCount,
                configuredWorkers * 2
                )
            );

    const int balancingTileHeight =
        std::max(
            32,
            (rowCount + targetTaskCount - 1) /
                targetTaskCount
            );

    const int effectiveTileHeight =
        std::max(
            32,
            std::min(
                configuredTileHeight,
                balancingTileHeight
                )
            );

    const int taskCount =
        std::max(
            1,
            (rowCount + effectiveTileHeight - 1) /
                effectiveTileHeight
            );

    if (taskCount <= 1) {
        rowFunction(0, rowCount);
        return;
    }

    QSemaphore completionSemaphore;

    const auto sharedRowFunction =
        std::make_shared<std::function<void(int, int)>>(
            rowFunction
            );

    for (int taskIndex = 0;
         taskIndex < taskCount;
         ++taskIndex) {
        const int firstRow =
            taskIndex * effectiveTileHeight;

        const int endRow =
            std::min(
                rowCount,
                firstRow + effectiveTileHeight
                );

        m_threadPool.start(
            new RowRangeTask(
                sharedRowFunction,
                firstRow,
                endRow,
                &completionSemaphore
                )
            );
    }

    completionSemaphore.acquire(taskCount);
}
