#include "image_tools.h"
#include "imageprocessingruntime.h"

#include <algorithm>
#include <cmath>

namespace
{
int clampChannel(int value)
{
    return std::clamp(value, 0, 255);
}

int sliderAmountToChannelShift(int amount)
{
    // Temporary first-pass mapping:
    // -100..100 slider becomes about -128..128 channel shift.
    //
    // We will tune this later against the PhotoImpact reference images.
    return static_cast<int>(std::round(amount * 1.28));
}

QImage toArgb32(const QImage &sourceImage)
{
    QImage writableImage;

    if (sourceImage.format() == QImage::Format_ARGB32 ||
        sourceImage.format() == QImage::Format_RGB32) {
        writableImage = sourceImage;
    } else {
        writableImage =
            sourceImage.convertToFormat(
                QImage::Format_ARGB32
                );
    }

    // Force one safe detach on the calling thread before worker threads
    // begin writing independent scan lines.
    writableImage.detach();
    return writableImage;
}

using ChannelLookup =
    OpenClProcessingBackend::ChannelLookup;

template<typename LookupFunction>
ChannelLookup makeChannelLookup(
    LookupFunction lookupFunction
    )
{
    ChannelLookup lookup {};

    for (std::size_t index = 0;
         index < lookup.size();
         ++index) {
        lookup[index] =
            static_cast<quint8>(
                clampChannel(
                    lookupFunction(
                        static_cast<int>(index)
                        )
                    )
                );
    }

    return lookup;
}

ChannelLookup identityLookup()
{
    return makeChannelLookup(
        [](int value) {
            return value;
        }
        );
}

ChannelLookup brightnessLookup(
    int amount
    )
{
    const int shift =
        sliderAmountToChannelShift(amount);

    return makeChannelLookup(
        [shift](int value) {
            return value + shift;
        }
        );
}

ChannelLookup contrastLookup(
    int amount
    )
{
    const double contrast =
        std::clamp(amount, -100, 100) / 100.0;

    const double factor =
        1.0 + contrast;

    return makeChannelLookup(
        [factor](int value) {
            return static_cast<int>(
                std::round(
                    (value - 128) *
                        factor +
                        128
                    )
                );
        }
        );
}

ChannelLookup gammaLookup(
    int amount
    )
{
    const double slider =
        std::clamp(amount, -100, 100) / 100.0;

    const double gamma =
        std::pow(2.0, -slider);

    return makeChannelLookup(
        [gamma](int value) {
            const double normalized =
                value / 255.0;

            return static_cast<int>(
                std::round(
                    std::pow(normalized, gamma) *
                        255.0
                    )
                );
        }
        );
}

ChannelLookup composeLookups(
    const ChannelLookup &firstLookup,
    const ChannelLookup &secondLookup
    )
{
    ChannelLookup result {};

    for (std::size_t index = 0;
         index < result.size();
         ++index) {
        result[index] =
            secondLookup[
                static_cast<std::size_t>(
                    firstLookup[index]
                    )
                ];
    }

    return result;
}

template<typename RowProcessor>
void processImageRows(
    QImage &image,
    RowProcessor rowProcessor
    )
{
    if (image.isNull()) {
        return;
    }

    const int imageWidth = image.width();
    const int bytesPerLine = image.bytesPerLine();
    uchar *imageBits = image.bits();

    ImageProcessingRuntime::instance().parallelForRows(
        image.height(),
        [imageBits,
         bytesPerLine,
         imageWidth,
         rowProcessor](
            int firstRow,
            int endRow
            ) mutable {
            for (int y = firstRow;
                 y < endRow;
                 ++y) {
                QRgb *scanLine =
                    reinterpret_cast<QRgb *>(
                        imageBits +
                        (y * bytesPerLine)
                        );

                rowProcessor(
                    scanLine,
                    imageWidth
                    );
            }
        }
        );
}

QImage applyChannelLookups(
    const QImage &sourceImage,
    const ChannelLookup &redLookup,
    const ChannelLookup &greenLookup,
    const ChannelLookup &blueLookup
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    QImage outputImage =
        toArgb32(sourceImage);

    if (ImageProcessingRuntime::instance()
            .tryApplyChannelLookups(
                outputImage,
                redLookup,
                greenLookup,
                blueLookup
                )) {
        return outputImage;
    }

    processImageRows(
        outputImage,
        [redLookup,
         greenLookup,
         blueLookup](
            QRgb *scanLine,
            int imageWidth
            ) {
            for (int x = 0;
                 x < imageWidth;
                 ++x) {
                const QRgb pixel =
                    scanLine[x];

                scanLine[x] = qRgba(
                    redLookup[
                        static_cast<std::size_t>(
                            qRed(pixel)
                            )
                    ],
                    greenLookup[
                        static_cast<std::size_t>(
                            qGreen(pixel)
                            )
                    ],
                    blueLookup[
                        static_cast<std::size_t>(
                            qBlue(pixel)
                            )
                    ],
                    qAlpha(pixel)
                    );
            }
        }
        );

    return outputImage;
}
}

namespace ImageTools
{
QImage applyMasterShift(const QImage &sourceImage, int amount)
{
    return applyRgbShift(sourceImage, amount, amount, amount);
}

QImage applyRedShift(const QImage &sourceImage, int amount)
{
    return applyRgbShift(sourceImage, amount, 0, 0);
}

QImage applyGreenShift(const QImage &sourceImage, int amount)
{
    return applyRgbShift(sourceImage, 0, amount, 0);
}

QImage applyBlueShift(const QImage &sourceImage, int amount)
{
    return applyRgbShift(sourceImage, 0, 0, amount);
}

QImage applyRgbShift(
    const QImage &sourceImage,
    int redAmount,
    int greenAmount,
    int blueAmount
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    const ChannelLookup redLookup =
        brightnessLookup(redAmount);
    const ChannelLookup greenLookup =
        brightnessLookup(greenAmount);
    const ChannelLookup blueLookup =
        brightnessLookup(blueAmount);

    return applyChannelLookups(
        sourceImage,
        redLookup,
        greenLookup,
        blueLookup
        );
}

QImage applyBrightness(const QImage &sourceImage, int amount)
{
    // First-pass brightness is the same as master channel lift/lower.
    // Later we can tune this separately if PhotoImpact's brightness curve differs.
    return applyMasterShift(sourceImage, amount);
}

QImage applyContrast(const QImage &sourceImage, int amount)
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    const ChannelLookup lookup =
        contrastLookup(amount);

    return applyChannelLookups(
        sourceImage,
        lookup,
        lookup,
        lookup
        );
}

QImage applyGamma(const QImage &sourceImage, int amount)
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    const ChannelLookup lookup =
        gammaLookup(amount);

    return applyChannelLookups(
        sourceImage,
        lookup,
        lookup,
        lookup
        );
}

QImage applyBasicTone(
    const QImage &sourceImage,
    int brightnessAmount,
    int contrastAmount,
    int gammaAmount
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    if (brightnessAmount == 0 &&
        contrastAmount == 0 &&
        gammaAmount == 0) {
        return sourceImage;
    }

    const ChannelLookup brightness =
        brightnessAmount == 0
        ? identityLookup()
        : brightnessLookup(brightnessAmount);

    const ChannelLookup contrast =
        contrastAmount == 0
        ? identityLookup()
        : contrastLookup(contrastAmount);

    const ChannelLookup gamma =
        gammaAmount == 0
        ? identityLookup()
        : gammaLookup(gammaAmount);

    const ChannelLookup combinedLookup =
        composeLookups(
            composeLookups(
                brightness,
                contrast
                ),
            gamma
            );

    return applyChannelLookups(
        sourceImage,
        combinedLookup,
        combinedLookup,
        combinedLookup
        );
}

QImage applyMainAdjustments(
    const QImage &sourceImage,
    int brightnessAmount,
    int contrastAmount,
    int gammaAmount,
    int saturationAmount,

    int baseHighlightAmount,
    int baseMidtoneAmount,
    int baseShadowAmount,

    int cyanRedHighlightAmount,
    int cyanRedMidtoneAmount,
    int cyanRedShadowAmount,

    int magentaGreenHighlightAmount,
    int magentaGreenMidtoneAmount,
    int magentaGreenShadowAmount,

    int yellowBlueHighlightAmount,
    int yellowBlueMidtoneAmount,
    int yellowBlueShadowAmount
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    const bool hasBasicAdjustment =
        brightnessAmount != 0 ||
        contrastAmount != 0 ||
        gammaAmount != 0;

    const bool hasBaseHmsAdjustment =
        baseHighlightAmount != 0 ||
        baseMidtoneAmount != 0 ||
        baseShadowAmount != 0;

    const bool hasColorHmsAdjustment =
        cyanRedHighlightAmount != 0 ||
        cyanRedMidtoneAmount != 0 ||
        cyanRedShadowAmount != 0 ||
        magentaGreenHighlightAmount != 0 ||
        magentaGreenMidtoneAmount != 0 ||
        magentaGreenShadowAmount != 0 ||
        yellowBlueHighlightAmount != 0 ||
        yellowBlueMidtoneAmount != 0 ||
        yellowBlueShadowAmount != 0;

    const bool hasSaturationAdjustment =
        saturationAmount != 0;

    if (!hasBasicAdjustment &&
        !hasBaseHmsAdjustment &&
        !hasColorHmsAdjustment &&
        !hasSaturationAdjustment) {
        return sourceImage;
    }

    const ChannelLookup brightness =
        brightnessAmount == 0
        ? identityLookup()
        : brightnessLookup(brightnessAmount);

    const ChannelLookup contrast =
        contrastAmount == 0
        ? identityLookup()
        : contrastLookup(contrastAmount);

    const ChannelLookup gamma =
        gammaAmount == 0
        ? identityLookup()
        : gammaLookup(gammaAmount);

    const ChannelLookup combinedLookup =
        composeLookups(
            composeLookups(
                brightness,
                contrast
                ),
            gamma
            );

    OpenClProcessingBackend::AdjustmentPipelineSettings pipelineSettings;

    pipelineSettings.baseHighlightShift =
        sliderAmountToChannelShift(baseHighlightAmount);
    pipelineSettings.baseMidtoneShift =
        sliderAmountToChannelShift(baseMidtoneAmount);
    pipelineSettings.baseShadowShift =
        sliderAmountToChannelShift(baseShadowAmount);

    pipelineSettings.cyanRedHighlightShift =
        sliderAmountToChannelShift(cyanRedHighlightAmount);
    pipelineSettings.cyanRedMidtoneShift =
        sliderAmountToChannelShift(cyanRedMidtoneAmount);
    pipelineSettings.cyanRedShadowShift =
        sliderAmountToChannelShift(cyanRedShadowAmount);

    pipelineSettings.magentaGreenHighlightShift =
        sliderAmountToChannelShift(magentaGreenHighlightAmount);
    pipelineSettings.magentaGreenMidtoneShift =
        sliderAmountToChannelShift(magentaGreenMidtoneAmount);
    pipelineSettings.magentaGreenShadowShift =
        sliderAmountToChannelShift(magentaGreenShadowAmount);

    pipelineSettings.yellowBlueHighlightShift =
        sliderAmountToChannelShift(yellowBlueHighlightAmount);
    pipelineSettings.yellowBlueMidtoneShift =
        sliderAmountToChannelShift(yellowBlueMidtoneAmount);
    pipelineSettings.yellowBlueShadowShift =
        sliderAmountToChannelShift(yellowBlueShadowAmount);

    pipelineSettings.saturationAmount =
        std::clamp(saturationAmount, -100, 100);
    pipelineSettings.applyBasicLookups =
        hasBasicAdjustment;
    pipelineSettings.applyBaseHms =
        hasBaseHmsAdjustment;
    pipelineSettings.applyColorHms =
        hasColorHmsAdjustment;
    pipelineSettings.applySaturation =
        hasSaturationAdjustment;

    ImageProcessingRuntime &processingRuntime =
        ImageProcessingRuntime::instance();

    if (processingRuntime
            .gpuAdjustmentPipelineAvailable()) {
        QImage gpuResult =
            toArgb32(sourceImage);

        if (processingRuntime
                .tryApplyAdjustmentPipeline(
                    gpuResult,
                    combinedLookup,
                    combinedLookup,
                    combinedLookup,
                    pipelineSettings
                    )) {
            return gpuResult;
        }
    }

    // Preserve the exact established CPU operation order when the GPU is
    // unavailable, below the automatic workload threshold, or falls back.
    QImage cpuResult = sourceImage;

    if (hasBasicAdjustment) {
        cpuResult = applyBasicTone(
            cpuResult,
            brightnessAmount,
            contrastAmount,
            gammaAmount
            );
    }

    if (hasBaseHmsAdjustment) {
        cpuResult = applyBaseHms(
            cpuResult,
            baseHighlightAmount,
            baseMidtoneAmount,
            baseShadowAmount
            );
    }

    if (hasColorHmsAdjustment) {
        cpuResult = applyColorHms(
            cpuResult,
            cyanRedHighlightAmount,
            cyanRedMidtoneAmount,
            cyanRedShadowAmount,
            magentaGreenHighlightAmount,
            magentaGreenMidtoneAmount,
            magentaGreenShadowAmount,
            yellowBlueHighlightAmount,
            yellowBlueMidtoneAmount,
            yellowBlueShadowAmount
            );
    }

    if (hasSaturationAdjustment) {
        cpuResult = applySaturation(
            cpuResult,
            saturationAmount
            );
    }

    return cpuResult;
}

QImage applySaturation(const QImage &source, int amount)
{
    if (source.isNull() || amount == 0) {
        return source;
    }

    QImage result = toArgb32(source);

    // -100 = grayscale, 0 = unchanged, +100 = double saturation.
    const double factor =
        1.0 +
        (static_cast<double>(amount) / 100.0);

    processImageRows(
        result,
        [factor](
            QRgb *scanLine,
            int imageWidth
            ) {
            for (int x = 0; x < imageWidth; ++x) {
                const QRgb pixel = scanLine[x];

                const int red = qRed(pixel);
                const int green = qGreen(pixel);
                const int blue = qBlue(pixel);
                const int alpha = qAlpha(pixel);

                const double gray =
                    (0.299 * red) +
                    (0.587 * green) +
                    (0.114 * blue);

                const int newRed = clampChannel(
                    static_cast<int>(
                        std::round(
                            gray +
                            ((red - gray) * factor)
                            )
                        )
                    );

                const int newGreen = clampChannel(
                    static_cast<int>(
                        std::round(
                            gray +
                            ((green - gray) * factor)
                            )
                        )
                    );

                const int newBlue = clampChannel(
                    static_cast<int>(
                        std::round(
                            gray +
                            ((blue - gray) * factor)
                            )
                        )
                    );

                scanLine[x] = qRgba(
                    newRed,
                    newGreen,
                    newBlue,
                    alpha
                    );
            }
        }
        );

    return result;
}

QImage applyBaseHms(
    const QImage &sourceImage,
    int highlightAmount,
    int midtoneAmount,
    int shadowAmount
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    QImage outputImage = toArgb32(sourceImage);

    const double highlightShift =
        sliderAmountToChannelShift(highlightAmount);

    const double midtoneShift =
        sliderAmountToChannelShift(midtoneAmount);

    const double shadowShift =
        sliderAmountToChannelShift(shadowAmount);

    processImageRows(
        outputImage,
        [highlightShift,
         midtoneShift,
         shadowShift](
            QRgb *scanLine,
            int imageWidth
            ) {
            for (int x = 0;
                 x < imageWidth;
                 ++x) {
                const QRgb pixel = scanLine[x];

                const int originalRed = qRed(pixel);
                const int originalGreen = qGreen(pixel);
                const int originalBlue = qBlue(pixel);

                const double luminance =
                    (0.299 * originalRed) +
                    (0.587 * originalGreen) +
                    (0.114 * originalBlue);

                const double tone =
                    luminance / 255.0;

                const double shadowWeight =
                    std::clamp(
                        (0.55 - tone) / 0.55,
                        0.0,
                        1.0
                        );

                const double highlightWeight =
                    std::clamp(
                        (tone - 0.45) / 0.55,
                        0.0,
                        1.0
                        );

                const double midtoneWeight =
                    std::clamp(
                        1.0 -
                        (std::abs(tone - 0.50) / 0.50),
                        0.0,
                        1.0
                        );

                const double totalShift =
                    (shadowShift * shadowWeight) +
                    (midtoneShift * midtoneWeight) +
                    (highlightShift * highlightWeight);

                scanLine[x] = qRgba(
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalRed + totalShift
                                )
                            )
                        ),
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalGreen + totalShift
                                )
                            )
                        ),
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalBlue + totalShift
                                )
                            )
                        ),
                    qAlpha(pixel)
                    );
            }
        }
        );

    return outputImage;
}

QImage applyHueToneMix(
    const QImage &source,
    const std::array<int, 8> &amounts
    )
{
    if (source.isNull()) {
        return source;
    }

    const bool hasAdjustment = std::any_of(
        amounts.begin(),
        amounts.end(),
        [](int amount) {
            return amount != 0;
        }
        );

    if (!hasAdjustment) {
        return source;
    }

    struct ToneVector
    {
        double red;
        double green;
        double blue;
    };

    static const std::array<ToneVector, 8> toneVectors = {{
        {  1.00, -0.50, -0.50 }, // Red
        {  0.85,  0.25, -0.85 }, // Orange / amber
        {  0.65,  0.65, -0.90 }, // Yellow
        { -0.45,  0.90, -0.45 }, // Green
        { -0.85,  0.45,  0.45 }, // Cyan
        { -0.45, -0.45,  0.90 }, // Blue
        {  0.45, -0.85,  0.45 }, // Purple
        {  0.75, -0.85,  0.75 }  // Magenta
    }};

    double totalRedShift = 0.0;
    double totalGreenShift = 0.0;
    double totalBlueShift = 0.0;

    for (std::size_t index = 0;
         index < amounts.size();
         ++index) {
        const int amount =
            std::clamp(
                amounts[index],
                -100,
                100
                );

        const double strength =
            (static_cast<double>(amount) / 100.0) *
            35.0;

        totalRedShift +=
            toneVectors[index].red * strength;

        totalGreenShift +=
            toneVectors[index].green * strength;

        totalBlueShift +=
            toneVectors[index].blue * strength;
    }

    totalRedShift =
        std::clamp(totalRedShift, -96.0, 96.0);

    totalGreenShift =
        std::clamp(totalGreenShift, -96.0, 96.0);

    totalBlueShift =
        std::clamp(totalBlueShift, -96.0, 96.0);

    const double lumaShift =
        (0.299 * totalRedShift) +
        (0.587 * totalGreenShift) +
        (0.114 * totalBlueShift);

    const double correctedRedShift =
        totalRedShift - lumaShift;

    const double correctedGreenShift =
        totalGreenShift - lumaShift;

    const double correctedBlueShift =
        totalBlueShift - lumaShift;

    const ChannelLookup redLookup =
        makeChannelLookup(
            [correctedRedShift](int value) {
                return static_cast<int>(
                    std::round(
                        value + correctedRedShift
                        )
                    );
            }
            );

    const ChannelLookup greenLookup =
        makeChannelLookup(
            [correctedGreenShift](int value) {
                return static_cast<int>(
                    std::round(
                        value + correctedGreenShift
                        )
                    );
            }
            );

    const ChannelLookup blueLookup =
        makeChannelLookup(
            [correctedBlueShift](int value) {
                return static_cast<int>(
                    std::round(
                        value + correctedBlueShift
                        )
                    );
            }
            );

    return applyChannelLookups(
        source,
        redLookup,
        greenLookup,
        blueLookup
        );
}

QImage applyHueTone(
    const QImage &source,
    int colorIndex,
    int amount
    )
{
    if (source.isNull() ||
        colorIndex < 0 ||
        colorIndex >= 8 ||
        amount == 0) {
        return source;
    }

    std::array<int, 8> amounts {};
    amounts[static_cast<std::size_t>(colorIndex)] = amount;

    return applyHueToneMix(source, amounts);
}

QImage applyColorHms(
    const QImage &sourceImage,

    int cyanRedHighlightAmount,
    int cyanRedMidtoneAmount,
    int cyanRedShadowAmount,

    int magentaGreenHighlightAmount,
    int magentaGreenMidtoneAmount,
    int magentaGreenShadowAmount,

    int yellowBlueHighlightAmount,
    int yellowBlueMidtoneAmount,
    int yellowBlueShadowAmount
    )
{
    if (sourceImage.isNull()) {
        return sourceImage;
    }

    QImage outputImage = toArgb32(sourceImage);

    const double cyanRedHighlightShift =
        sliderAmountToChannelShift(
            cyanRedHighlightAmount
            );

    const double cyanRedMidtoneShift =
        sliderAmountToChannelShift(
            cyanRedMidtoneAmount
            );

    const double cyanRedShadowShift =
        sliderAmountToChannelShift(
            cyanRedShadowAmount
            );

    const double magentaGreenHighlightShift =
        sliderAmountToChannelShift(
            magentaGreenHighlightAmount
            );

    const double magentaGreenMidtoneShift =
        sliderAmountToChannelShift(
            magentaGreenMidtoneAmount
            );

    const double magentaGreenShadowShift =
        sliderAmountToChannelShift(
            magentaGreenShadowAmount
            );

    const double yellowBlueHighlightShift =
        sliderAmountToChannelShift(
            yellowBlueHighlightAmount
            );

    const double yellowBlueMidtoneShift =
        sliderAmountToChannelShift(
            yellowBlueMidtoneAmount
            );

    const double yellowBlueShadowShift =
        sliderAmountToChannelShift(
            yellowBlueShadowAmount
            );

    const auto weightedToneShift =
        [](double tone,
           double highlightShift,
           double midtoneShift,
           double shadowShift) {
            const double shadowWeight =
                std::clamp(
                    (0.55 - tone) / 0.55,
                    0.0,
                    1.0
                    );

            const double highlightWeight =
                std::clamp(
                    (tone - 0.45) / 0.55,
                    0.0,
                    1.0
                    );

            const double midtoneWeight =
                std::clamp(
                    1.0 -
                    (std::abs(tone - 0.50) / 0.50),
                    0.0,
                    1.0
                    );

            return
                (shadowShift * shadowWeight) +
                (midtoneShift * midtoneWeight) +
                (highlightShift * highlightWeight);
        };

    processImageRows(
        outputImage,
        [cyanRedHighlightShift,
         cyanRedMidtoneShift,
         cyanRedShadowShift,
         magentaGreenHighlightShift,
         magentaGreenMidtoneShift,
         magentaGreenShadowShift,
         yellowBlueHighlightShift,
         yellowBlueMidtoneShift,
         yellowBlueShadowShift,
         weightedToneShift](
            QRgb *scanLine,
            int imageWidth
            ) {
            for (int x = 0;
                 x < imageWidth;
                 ++x) {
                const QRgb pixel = scanLine[x];

                const int originalRed = qRed(pixel);
                const int originalGreen = qGreen(pixel);
                const int originalBlue = qBlue(pixel);

                const double luminance =
                    (0.299 * originalRed) +
                    (0.587 * originalGreen) +
                    (0.114 * originalBlue);

                const double tone =
                    luminance / 255.0;

                const double redShift =
                    weightedToneShift(
                        tone,
                        cyanRedHighlightShift,
                        cyanRedMidtoneShift,
                        cyanRedShadowShift
                        );

                const double greenShift =
                    weightedToneShift(
                        tone,
                        magentaGreenHighlightShift,
                        magentaGreenMidtoneShift,
                        magentaGreenShadowShift
                        );

                const double blueShift =
                    weightedToneShift(
                        tone,
                        yellowBlueHighlightShift,
                        yellowBlueMidtoneShift,
                        yellowBlueShadowShift
                        );

                scanLine[x] = qRgba(
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalRed + redShift
                                )
                            )
                        ),
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalGreen + greenShift
                                )
                            )
                        ),
                    clampChannel(
                        static_cast<int>(
                            std::round(
                                originalBlue + blueShift
                                )
                            )
                        ),
                    qAlpha(pixel)
                    );
            }
        }
        );

    return outputImage;
}
}
