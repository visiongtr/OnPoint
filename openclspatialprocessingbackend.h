#ifndef OPENCLSPATIALPROCESSINGBACKEND_H
#define OPENCLSPATIALPROCESSINGBACKEND_H

#include <QImage>
#include <QRect>
#include <QString>

#include <array>
#include <memory>

class OpenClSpatialProcessingBackend
{
public:
    struct NoiseSettings
    {
        int luminanceNoise = 0;
        int colorNoise = 0;
        int detailProtection = 0;
        int blend = 0;
    };

    struct FocusSettings
    {
        int amount = 0;
        int radius = 1;
        int threshold = 0;
        int edgeProtection = 0;
        int blend = 0;
    };

    struct SectorSample
    {
        bool valid = false;
        double red = 0.0;
        double green = 0.0;
        double blue = 0.0;
    };

    struct OverexposureSettings
    {
        // 0 = oval, 1 = rectangle/square.
        int shape = 0;
        int softEdge = 0;
        double brightCutoff = 0.0;
        double recoveryAmount = 0.0;
        double colorBleed = 0.0;
        double transparency = 0.0;
        double preserveShine = 0.0;
        std::array<SectorSample, 8> sectorSamples {};
    };

    OpenClSpatialProcessingBackend();
    ~OpenClSpatialProcessingBackend();

    OpenClSpatialProcessingBackend(
        const OpenClSpatialProcessingBackend &
        ) = delete;

    OpenClSpatialProcessingBackend &operator=(
        const OpenClSpatialProcessingBackend &
        ) = delete;

    void initialize();
    void shutdown();

    bool isAvailable() const;
    QString deviceName() const;
    QString statusText() const;

    bool applyNoiseReduction(
        QImage &image,
        const NoiseSettings &settings,
        QString *errorMessage = nullptr
        );

    bool applyFocus(
        QImage &image,
        const FocusSettings &settings,
        QString *errorMessage = nullptr
        );

    bool applyOverexposureRecovery(
        QImage &image,
        const QRect &selection,
        const OverexposureSettings &settings,
        QString *errorMessage = nullptr
        );

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

#endif // OPENCLSPATIALPROCESSINGBACKEND_H
