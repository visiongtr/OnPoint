#ifndef OPENCLPROCESSINGBACKEND_H
#define OPENCLPROCESSINGBACKEND_H

#include <QImage>
#include <QString>
#include <QtGlobal>

#include <array>
#include <memory>

class OpenClProcessingBackend
{
public:
    using ChannelLookup = std::array<quint8, 256>;

    struct AdjustmentPipelineSettings
    {
        double baseHighlightShift = 0.0;
        double baseMidtoneShift = 0.0;
        double baseShadowShift = 0.0;

        double cyanRedHighlightShift = 0.0;
        double cyanRedMidtoneShift = 0.0;
        double cyanRedShadowShift = 0.0;

        double magentaGreenHighlightShift = 0.0;
        double magentaGreenMidtoneShift = 0.0;
        double magentaGreenShadowShift = 0.0;

        double yellowBlueHighlightShift = 0.0;
        double yellowBlueMidtoneShift = 0.0;
        double yellowBlueShadowShift = 0.0;

        int saturationAmount = 0;
        bool applyBasicLookups = false;
        bool applyBaseHms = false;
        bool applyColorHms = false;
        bool applySaturation = false;
    };

    OpenClProcessingBackend();
    ~OpenClProcessingBackend();

    OpenClProcessingBackend(
        const OpenClProcessingBackend &
        ) = delete;

    OpenClProcessingBackend &operator=(
        const OpenClProcessingBackend &
        ) = delete;

    void initialize();
    void shutdown();

    bool isAvailable() const;
    bool adjustmentPipelineAvailable() const;
    QString deviceName() const;
    QString statusText() const;

    bool applyChannelLookups(
        QImage &image,
        const ChannelLookup &redLookup,
        const ChannelLookup &greenLookup,
        const ChannelLookup &blueLookup,
        QString *errorMessage = nullptr
        );

    bool applyAdjustmentPipeline(
        QImage &image,
        const ChannelLookup &redLookup,
        const ChannelLookup &greenLookup,
        const ChannelLookup &blueLookup,
        const AdjustmentPipelineSettings &settings,
        QString *errorMessage = nullptr
        );

private:
    class Implementation;
    std::unique_ptr<Implementation> m_implementation;
};

#endif // OPENCLPROCESSINGBACKEND_H
