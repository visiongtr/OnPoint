#ifndef IMAGEPROCESSINGRUNTIME_H
#define IMAGEPROCESSINGRUNTIME_H

#include "systemprofile.h"
#include "openclprocessingbackend.h"
#include "openclspatialprocessingbackend.h"

#include <QImage>
#include <QMutex>
#include <QRect>
#include <QThreadPool>

#include <array>
#include <atomic>
#include <functional>
#include <memory>

class ImageProcessingRuntime
{
public:
    static ImageProcessingRuntime &instance();

    void configure(
        const PerformanceSettings &settings
        );

    PerformanceSettings settings() const;

    int workerThreads() const;
    int tileHeight() const;
    int interactivePreviewMaxDimension() const;

    bool gpuBackendAvailable() const;
    bool gpuAdjustmentPipelineAvailable() const;
    bool gpuSpatialPipelineAvailable() const;
    QString gpuBackendName() const;
    QString processingBackendSummary() const;

    bool tryApplyChannelLookups(
        QImage &image,
        const OpenClProcessingBackend::ChannelLookup &redLookup,
        const OpenClProcessingBackend::ChannelLookup &greenLookup,
        const OpenClProcessingBackend::ChannelLookup &blueLookup
        );

    bool tryApplyAdjustmentPipeline(
        QImage &image,
        const OpenClProcessingBackend::ChannelLookup &redLookup,
        const OpenClProcessingBackend::ChannelLookup &greenLookup,
        const OpenClProcessingBackend::ChannelLookup &blueLookup,
        const OpenClProcessingBackend::AdjustmentPipelineSettings &settings
        );

    bool tryApplyNoiseReduction(
        QImage &image,
        const OpenClSpatialProcessingBackend::NoiseSettings &settings
        );

    bool tryApplyFocus(
        QImage &image,
        const OpenClSpatialProcessingBackend::FocusSettings &settings
        );

    bool tryApplyOverexposureRecovery(
        QImage &image,
        const QRect &selection,
        const OpenClSpatialProcessingBackend::OverexposureSettings &settings
        );

    QImage makeInteractivePreview(
        const QImage &source
        ) const;

    void parallelForRows(
        int rowCount,
        const std::function<void(int, int)> &rowFunction
        );

private:
    ImageProcessingRuntime();
    ~ImageProcessingRuntime();

    ImageProcessingRuntime(
        const ImageProcessingRuntime &
        ) = delete;

    ImageProcessingRuntime &operator=(
        const ImageProcessingRuntime &
        ) = delete;

    mutable QMutex m_settingsMutex;
    PerformanceSettings m_settings;
    QThreadPool m_threadPool;
    std::unique_ptr<OpenClProcessingBackend> m_gpuBackend;
    std::unique_ptr<OpenClSpatialProcessingBackend> m_spatialGpuBackend;
    std::atomic_bool m_gpuUseReported {false};
    std::atomic_bool m_gpuAdjustmentUseReported {false};
    std::atomic_bool m_gpuNoiseUseReported {false};
    std::atomic_bool m_gpuFocusUseReported {false};
    std::atomic_bool m_gpuOverexposureUseReported {false};
};

#endif // IMAGEPROCESSINGRUNTIME_H
