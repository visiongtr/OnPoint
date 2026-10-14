#include "noise_reduction_tools.h"
#include "imageprocessingruntime.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{

int clampByte(int value)
{
    return std::clamp(
        value,
        0,
        255
        );
}

QImage toArgb32(
    const QImage &sourceImage
    )
{
    if (sourceImage.format() ==
            QImage::Format_ARGB32 ||
        sourceImage.format() ==
            QImage::Format_RGB32) {
        return sourceImage;
    }

    return sourceImage.convertToFormat(
        QImage::Format_ARGB32
        );
}

QRgb rgbToEncodedYcbcr(
    QRgb pixel
    )
{
    const int red =
        qRed(pixel);

    const int green =
        qGreen(pixel);

    const int blue =
        qBlue(pixel);

    // Integer BT.601-style conversion.
    const int luminance =
        clampByte(
            ((77 * red) +
             (150 * green) +
             (29 * blue) +
             128) >>
            8
            );

    const int blueDifference =
        clampByte(
            128 +
            (((-43 * red) -
              (85 * green) +
              (128 * blue) +
              128) >>
             8)
            );

    const int redDifference =
        clampByte(
            128 +
            (((128 * red) -
              (107 * green) -
              (21 * blue) +
              128) >>
             8)
            );

    // Encoded storage:
    // red channel   = luminance
    // green channel = blue-difference chroma
    // blue channel  = red-difference chroma
    return qRgba(
        luminance,
        blueDifference,
        redDifference,
        qAlpha(pixel)
        );
}

QRgb encodedYcbcrToRgb(
    int luminance,
    int blueDifference,
    int redDifference,
    int alpha
    )
{
    const double cb =
        blueDifference -
        128.0;

    const double cr =
        redDifference -
        128.0;

    const int red =
        clampByte(
            static_cast<int>(
                std::round(
                    luminance +
                    (1.402 * cr)
                    )
                )
            );

    const int green =
        clampByte(
            static_cast<int>(
                std::round(
                    luminance -
                    (0.344136 * cb) -
                    (0.714136 * cr)
                    )
                )
            );

    const int blue =
        clampByte(
            static_cast<int>(
                std::round(
                    luminance +
                    (1.772 * cb)
                    )
                )
            );

    return qRgba(
        red,
        green,
        blue,
        alpha
        );
}

QImage encodeYcbcr(
    const QImage &sourceImage
    )
{
    const QImage source =
        toArgb32(sourceImage);

    QImage encoded(
        source.size(),
        QImage::Format_ARGB32
        );

    for (int y = 0;
         y < source.height();
         ++y) {
        const QRgb *sourceLine =
            reinterpret_cast<const QRgb *>(
                source.constScanLine(y)
                );

        QRgb *encodedLine =
            reinterpret_cast<QRgb *>(
                encoded.scanLine(y)
                );

        for (int x = 0;
             x < source.width();
             ++x) {
            encodedLine[x] =
                rgbToEncodedYcbcr(
                    sourceLine[x]
                    );
        }
    }

    return encoded;
}

std::array<int, 256> makeRangeWeights(
    double sigma
    )
{
    sigma =
        std::max(
            1.0,
            sigma
            );

    std::array<int, 256> weights {};

    for (int difference = 0;
         difference < 256;
         ++difference) {
        const double normalized =
            difference /
            sigma;

        // A soft rational falloff is cheaper than evaluating an
        // exponential for every neighboring pixel and has a gentler tail.
        const double weight =
            1.0 /
            (1.0 +
             (4.0 *
              normalized *
              normalized));

        weights[
            static_cast<std::size_t>(
                difference
                )
            ] =
            std::max(
                1,
                static_cast<int>(
                    std::round(
                        weight *
                        1024.0
                        )
                    )
                );
    }

    return weights;
}

int spatialWeight(
    int distance
    )
{
    static const std::array<int, 5> weights = {
        1024,
        820,
        610,
        420,
        270
    };

    return weights[
        static_cast<std::size_t>(
            std::clamp(
                distance,
                0,
                4
                )
            )
        ];
}

QImage edgeAwarePass(
    const QImage &encodedInput,
    bool horizontal,
    int radius,
    double rangeSigma,
    bool filterLuminance,
    bool filterChroma
    )
{
    if (encodedInput.isNull() ||
        radius <= 0 ||
        (!filterLuminance &&
         !filterChroma)) {
        return encodedInput;
    }

    radius =
        std::clamp(
            radius,
            1,
            4
            );

    const std::array<int, 256> rangeWeights =
        makeRangeWeights(
            rangeSigma
            );

    QImage output(
        encodedInput.size(),
        QImage::Format_ARGB32
        );

    const int width =
        encodedInput.width();

    const int height =
        encodedInput.height();

    for (int y = 0;
         y < height;
         ++y) {
        QRgb *outputLine =
            reinterpret_cast<QRgb *>(
                output.scanLine(y)
                );

        for (int x = 0;
             x < width;
             ++x) {
            const QRgb centerPixel =
                reinterpret_cast<const QRgb *>(
                    encodedInput.constScanLine(y)
                    )[x];

            const int centerLuminance =
                qRed(centerPixel);

            qint64 luminanceSum = 0;
            qint64 blueDifferenceSum = 0;
            qint64 redDifferenceSum = 0;
            qint64 weightSum = 0;

            for (int offset = -radius;
                 offset <= radius;
                 ++offset) {
                const int sampleX =
                    horizontal
                        ? std::clamp(
                              x + offset,
                              0,
                              width - 1
                              )
                        : x;

                const int sampleY =
                    horizontal
                        ? y
                        : std::clamp(
                              y + offset,
                              0,
                              height - 1
                              );

                const QRgb samplePixel =
                    reinterpret_cast<const QRgb *>(
                        encodedInput.constScanLine(
                            sampleY
                            )
                        )[sampleX];

                const int luminanceDifference =
                    std::abs(
                        qRed(samplePixel) -
                        centerLuminance
                        );

                const int combinedWeight =
                    std::max(
                        1,
                        (rangeWeights[
                             static_cast<std::size_t>(
                                 luminanceDifference
                                 )
                             ] *
                         spatialWeight(
                             std::abs(offset)
                             )) >>
                        10
                        );

                weightSum +=
                    combinedWeight;

                luminanceSum +=
                    static_cast<qint64>(
                        qRed(samplePixel)
                        ) *
                    combinedWeight;

                blueDifferenceSum +=
                    static_cast<qint64>(
                        qGreen(samplePixel)
                        ) *
                    combinedWeight;

                redDifferenceSum +=
                    static_cast<qint64>(
                        qBlue(samplePixel)
                        ) *
                    combinedWeight;
            }

            const int filteredLuminance =
                weightSum > 0
                    ? static_cast<int>(
                          (luminanceSum +
                           (weightSum / 2)) /
                          weightSum
                          )
                    : centerLuminance;

            const int filteredBlueDifference =
                weightSum > 0
                    ? static_cast<int>(
                          (blueDifferenceSum +
                           (weightSum / 2)) /
                          weightSum
                          )
                    : qGreen(centerPixel);

            const int filteredRedDifference =
                weightSum > 0
                    ? static_cast<int>(
                          (redDifferenceSum +
                           (weightSum / 2)) /
                          weightSum
                          )
                    : qBlue(centerPixel);

            outputLine[x] =
                qRgba(
                    filterLuminance
                        ? clampByte(
                              filteredLuminance
                              )
                        : qRed(centerPixel),
                    filterChroma
                        ? clampByte(
                              filteredBlueDifference
                              )
                        : qGreen(centerPixel),
                    filterChroma
                        ? clampByte(
                              filteredRedDifference
                              )
                        : qBlue(centerPixel),
                    qAlpha(centerPixel)
                    );
        }
    }

    return output;
}

QImage edgeAwareSeparableFilter(
    const QImage &encodedInput,
    int radius,
    double rangeSigma,
    bool filterLuminance,
    bool filterChroma
    )
{
    QImage horizontal =
        edgeAwarePass(
            encodedInput,
            true,
            radius,
            rangeSigma,
            filterLuminance,
            filterChroma
            );

    return edgeAwarePass(
        horizontal,
        false,
        radius,
        rangeSigma,
        filterLuminance,
        filterChroma
        );
}

double amountToMix(
    int amount
    )
{
    amount =
        std::clamp(
            amount,
            0,
            100
            );

    if (amount == 0) {
        return 0.0;
    }

    // Low values remain useful, while the top of the slider approaches
    // full correction gradually instead of suddenly becoming destructive.
    return
        1.0 -
        std::exp(
            -amount /
            36.0
            );
}

int localLuminanceGradient(
    const QImage &encodedOriginal,
    int x,
    int y
    )
{
    const int width =
        encodedOriginal.width();

    const int height =
        encodedOriginal.height();

    const QRgb center =
        reinterpret_cast<const QRgb *>(
            encodedOriginal.constScanLine(y)
            )[x];

    const int centerLuminance =
        qRed(center);

    const int left =
        qRed(
            reinterpret_cast<const QRgb *>(
                encodedOriginal.constScanLine(y)
                )[std::max(
                    0,
                    x - 1
                    )]
            );

    const int right =
        qRed(
            reinterpret_cast<const QRgb *>(
                encodedOriginal.constScanLine(y)
                )[std::min(
                    width - 1,
                    x + 1
                    )]
            );

    const int top =
        qRed(
            reinterpret_cast<const QRgb *>(
                encodedOriginal.constScanLine(
                    std::max(
                        0,
                        y - 1
                        )
                    )
                )[x]
            );

    const int bottom =
        qRed(
            reinterpret_cast<const QRgb *>(
                encodedOriginal.constScanLine(
                    std::min(
                        height - 1,
                        y + 1
                        )
                    )
                )[x]
            );

    return std::max({
        std::abs(
            centerLuminance - left
            ),
        std::abs(
            centerLuminance - right
            ),
        std::abs(
            centerLuminance - top
            ),
        std::abs(
            centerLuminance - bottom
            )
    });
}

} // namespace

namespace NoiseReductionTools
{

bool hasVisibleAdjustment(
    const NoiseReductionSettings &settings
    )
{
    return
        settings.blend > 0 &&
        (settings.luminanceNoise > 0 ||
         settings.colorNoise > 0);
}

QImage applyNoiseReduction(
    const QImage &sourceImage,
    const NoiseReductionSettings &settings
    )
{
    if (sourceImage.isNull() ||
        !hasVisibleAdjustment(settings)) {
        return sourceImage;
    }

    const int luminanceAmount =
        std::clamp(
            settings.luminanceNoise,
            0,
            100
            );

    const int colorAmount =
        std::clamp(
            settings.colorNoise,
            0,
            100
            );

    const int detailProtection =
        std::clamp(
            settings.detailProtection,
            0,
            100
            );

    const double overallBlend =
        std::clamp(
            settings.blend,
            0,
            100
            ) /
        100.0;

    QImage gpuResult =
        toArgb32(sourceImage);

    OpenClSpatialProcessingBackend::NoiseSettings gpuSettings;
    gpuSettings.luminanceNoise = luminanceAmount;
    gpuSettings.colorNoise = colorAmount;
    gpuSettings.detailProtection = detailProtection;
    gpuSettings.blend = std::clamp(
        settings.blend,
        0,
        100
        );

    if (ImageProcessingRuntime::instance()
            .tryApplyNoiseReduction(
                gpuResult,
                gpuSettings
                )) {
        return gpuResult;
    }

    const QImage encodedOriginal =
        encodeYcbcr(
            sourceImage
            );

    QImage filtered =
        encodedOriginal;

    if (colorAmount > 0) {
        const int colorRadius =
            std::clamp(
                1 +
                (colorAmount / 26),
                1,
                4
                );

        const double colorRangeSigma =
            14.0 +
            ((100 - detailProtection) *
             0.34) +
            (colorAmount *
             0.12);

        filtered =
            edgeAwareSeparableFilter(
                filtered,
                colorRadius,
                colorRangeSigma,
                false,
                true
                );
    }

    if (luminanceAmount > 0) {
        const int luminanceRadius =
            std::clamp(
                1 +
                (luminanceAmount / 26),
                1,
                4
                );

        const double luminanceRangeSigma =
            4.5 +
            ((100 - detailProtection) *
             0.18) +
            (luminanceAmount *
             0.10);

        filtered =
            edgeAwareSeparableFilter(
                filtered,
                luminanceRadius,
                luminanceRangeSigma,
                true,
                false
                );
    }

    const double luminanceMixBase =
        amountToMix(
            luminanceAmount
            ) *
        overallBlend;

    const double colorMixBase =
        amountToMix(
            colorAmount
            ) *
        overallBlend;

    const double detail =
        detailProtection /
        100.0;

    QImage result =
        toArgb32(
            sourceImage
            );

    for (int y = 0;
         y < result.height();
         ++y) {
        const QRgb *originalLine =
            reinterpret_cast<const QRgb *>(
                encodedOriginal.constScanLine(y)
                );

        const QRgb *filteredLine =
            reinterpret_cast<const QRgb *>(
                filtered.constScanLine(y)
                );

        QRgb *resultLine =
            reinterpret_cast<QRgb *>(
                result.scanLine(y)
                );

        for (int x = 0;
             x < result.width();
             ++x) {
            const QRgb originalPixel =
                originalLine[x];

            const QRgb filteredPixel =
                filteredLine[x];

            const int gradient =
                localLuminanceGradient(
                    encodedOriginal,
                    x,
                    y
                    );

            const double edgeFactor =
                std::clamp(
                    (gradient - 2.0) /
                    26.0,
                    0.0,
                    1.0
                    );

            // Real edges and textured regions retain more of the source.
            // Luminance receives the strongest protection because that is
            // where skin, hair, fabric, and the sense of depth live.
            const double luminanceMix =
                luminanceMixBase *
                (1.0 -
                 (0.86 *
                  detail *
                  edgeFactor));

            const double colorMix =
                colorMixBase *
                (1.0 -
                 (0.58 *
                  detail *
                  edgeFactor));

            const int luminance =
                clampByte(
                    static_cast<int>(
                        std::round(
                            qRed(originalPixel) +
                            ((qRed(filteredPixel) -
                              qRed(originalPixel)) *
                             luminanceMix)
                            )
                        )
                    );

            const int blueDifference =
                clampByte(
                    static_cast<int>(
                        std::round(
                            qGreen(originalPixel) +
                            ((qGreen(filteredPixel) -
                              qGreen(originalPixel)) *
                             colorMix)
                            )
                        )
                    );

            const int redDifference =
                clampByte(
                    static_cast<int>(
                        std::round(
                            qBlue(originalPixel) +
                            ((qBlue(filteredPixel) -
                              qBlue(originalPixel)) *
                             colorMix)
                            )
                        )
                    );

            resultLine[x] =
                encodedYcbcrToRgb(
                    luminance,
                    blueDifference,
                    redDifference,
                    qAlpha(originalPixel)
                    );
        }
    }

    return result;
}

} // namespace NoiseReductionTools
