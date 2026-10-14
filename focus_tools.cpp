#include "focus_tools.h"
#include "imageprocessingruntime.h"

#include <QtGlobal>

#include <algorithm>
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

double clampUnit(double value)
{
    return std::clamp(
        value,
        0.0,
        1.0
        );
}

double smoothStep(
    double edge0,
    double edge1,
    double value
    )
{
    if (edge1 <= edge0) {
        return value >= edge1
            ? 1.0
            : 0.0;
    }

    const double normalized =
        clampUnit(
            (value - edge0) /
            (edge1 - edge0)
            );

    return
        normalized *
        normalized *
        (3.0 -
         (2.0 * normalized));
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

int pixelLuminance(
    QRgb pixel
    )
{
    return clampByte(
        ((77 * qRed(pixel)) +
         (150 * qGreen(pixel)) +
         (29 * qBlue(pixel)) +
         128) >>
        8
        );
}

void pixelChroma(
    QRgb pixel,
    int &blueDifference,
    int &redDifference
    )
{
    const int red =
        qRed(pixel);

    const int green =
        qGreen(pixel);

    const int blue =
        qBlue(pixel);

    blueDifference =
        clampByte(
            128 +
            (((-43 * red) -
              (85 * green) +
              (128 * blue) +
              128) >>
             8)
            );

    redDifference =
        clampByte(
            128 +
            (((128 * red) -
              (107 * green) -
              (21 * blue) +
              128) >>
             8)
            );
}

QRgb luminanceAndChromaToRgb(
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

std::vector<int> makeGaussianKernel(
    double sigma,
    int &kernelRadius
    )
{
    sigma =
        std::clamp(
            sigma,
            0.35,
            3.20
            );

    kernelRadius =
        std::clamp(
            static_cast<int>(
                std::ceil(
                    sigma * 2.5
                    )
                ),
            1,
            8
            );

    const int kernelSize =
        (kernelRadius * 2) + 1;

    std::vector<double> rawKernel(
        static_cast<std::size_t>(
            kernelSize
            )
        );

    double rawSum = 0.0;

    for (int offset = -kernelRadius;
         offset <= kernelRadius;
         ++offset) {
        const double normalized =
            offset /
            sigma;

        const double value =
            std::exp(
                -0.5 *
                normalized *
                normalized
                );

        rawKernel[
            static_cast<std::size_t>(
                offset +
                kernelRadius
                )
            ] =
            value;

        rawSum += value;
    }

    std::vector<int> kernel(
        static_cast<std::size_t>(
            kernelSize
            )
        );

    int integerSum = 0;

    for (int index = 0;
         index < kernelSize;
         ++index) {
        kernel[
            static_cast<std::size_t>(
                index
                )
            ] =
            std::max(
                1,
                static_cast<int>(
                    std::round(
                        (rawKernel[
                             static_cast<std::size_t>(
                                 index
                                 )
                             ] /
                         rawSum) *
                        65536.0
                        )
                    )
                );

        integerSum +=
            kernel[
                static_cast<std::size_t>(
                    index
                    )
                ];
    }

    // Correct rounding drift at the center so the kernel totals exactly 1.0.
    kernel[
        static_cast<std::size_t>(
            kernelRadius
            )
        ] +=
        65536 -
        integerSum;

    return kernel;
}

void alphaAwareGaussianBlur(
    const std::vector<int> &sourceLuminance,
    const std::vector<int> &sourceAlpha,
    int width,
    int height,
    double sigma,
    std::vector<int> &blurredLuminance
    )
{
    int kernelRadius = 1;

    const std::vector<int> kernel =
        makeGaussianKernel(
            sigma,
            kernelRadius
            );

    const std::size_t pixelCount =
        static_cast<std::size_t>(
            width
            ) *
        static_cast<std::size_t>(
            height
            );

    std::vector<int> horizontalLuminance(
        pixelCount
        );

    std::vector<int> horizontalAlpha(
        pixelCount
        );

    for (int y = 0;
         y < height;
         ++y) {
        for (int x = 0;
             x < width;
             ++x) {
            qint64 luminanceSum = 0;
            qint64 alphaWeightSum = 0;
            qint64 alphaSum = 0;
            qint64 kernelSum = 0;

            for (int offset = -kernelRadius;
                 offset <= kernelRadius;
                 ++offset) {
                const int sampleX =
                    std::clamp(
                        x + offset,
                        0,
                        width - 1
                        );

                const std::size_t sampleIndex =
                    static_cast<std::size_t>(
                        y
                        ) *
                    static_cast<std::size_t>(
                        width
                        ) +
                    static_cast<std::size_t>(
                        sampleX
                        );

                const int kernelWeight =
                    kernel[
                        static_cast<std::size_t>(
                            offset +
                            kernelRadius
                            )
                        ];

                const int alpha =
                    sourceAlpha[
                        sampleIndex
                        ];

                const qint64 visibleWeight =
                    static_cast<qint64>(
                        kernelWeight
                        ) *
                    alpha;

                luminanceSum +=
                    static_cast<qint64>(
                        sourceLuminance[
                            sampleIndex
                            ]
                        ) *
                    visibleWeight;

                alphaWeightSum +=
                    visibleWeight;

                alphaSum +=
                    static_cast<qint64>(
                        alpha
                        ) *
                    kernelWeight;

                kernelSum +=
                    kernelWeight;
            }

            const std::size_t outputIndex =
                static_cast<std::size_t>(
                    y
                    ) *
                static_cast<std::size_t>(
                    width
                    ) +
                static_cast<std::size_t>(
                    x
                    );

            horizontalLuminance[
                outputIndex
                ] =
                alphaWeightSum > 0
                    ? static_cast<int>(
                          (luminanceSum +
                           (alphaWeightSum / 2)) /
                          alphaWeightSum
                          )
                    : sourceLuminance[
                          outputIndex
                          ];

            horizontalAlpha[
                outputIndex
                ] =
                kernelSum > 0
                    ? clampByte(
                          static_cast<int>(
                              (alphaSum +
                               (kernelSum / 2)) /
                              kernelSum
                              )
                          )
                    : sourceAlpha[
                          outputIndex
                          ];
        }
    }

    blurredLuminance.resize(
        pixelCount
        );

    for (int y = 0;
         y < height;
         ++y) {
        for (int x = 0;
             x < width;
             ++x) {
            qint64 luminanceSum = 0;
            qint64 alphaWeightSum = 0;

            for (int offset = -kernelRadius;
                 offset <= kernelRadius;
                 ++offset) {
                const int sampleY =
                    std::clamp(
                        y + offset,
                        0,
                        height - 1
                        );

                const std::size_t sampleIndex =
                    static_cast<std::size_t>(
                        sampleY
                        ) *
                    static_cast<std::size_t>(
                        width
                        ) +
                    static_cast<std::size_t>(
                        x
                        );

                const int kernelWeight =
                    kernel[
                        static_cast<std::size_t>(
                            offset +
                            kernelRadius
                            )
                        ];

                const int alpha =
                    horizontalAlpha[
                        sampleIndex
                        ];

                const qint64 visibleWeight =
                    static_cast<qint64>(
                        kernelWeight
                        ) *
                    alpha;

                luminanceSum +=
                    static_cast<qint64>(
                        horizontalLuminance[
                            sampleIndex
                            ]
                        ) *
                    visibleWeight;

                alphaWeightSum +=
                    visibleWeight;
            }

            const std::size_t outputIndex =
                static_cast<std::size_t>(
                    y
                    ) *
                static_cast<std::size_t>(
                    width
                    ) +
                static_cast<std::size_t>(
                    x
                    );

            blurredLuminance[
                outputIndex
                ] =
                alphaWeightSum > 0
                    ? static_cast<int>(
                          (luminanceSum +
                           (alphaWeightSum / 2)) /
                          alphaWeightSum
                          )
                    : horizontalLuminance[
                          outputIndex
                          ];
        }
    }
}

int localLuminanceGradient(
    const std::vector<int> &luminance,
    int width,
    int height,
    int x,
    int y
    )
{
    const std::size_t centerIndex =
        static_cast<std::size_t>(
            y
            ) *
        static_cast<std::size_t>(
            width
            ) +
        static_cast<std::size_t>(
            x
            );

    const int center =
        luminance[
            centerIndex
            ];

    const int left =
        luminance[
            static_cast<std::size_t>(
                y
                ) *
            static_cast<std::size_t>(
                width
                ) +
            static_cast<std::size_t>(
                std::max(
                    0,
                    x - 1
                    )
                )
            ];

    const int right =
        luminance[
            static_cast<std::size_t>(
                y
                ) *
            static_cast<std::size_t>(
                width
                ) +
            static_cast<std::size_t>(
                std::min(
                    width - 1,
                    x + 1
                    )
                )
            ];

    const int top =
        luminance[
            static_cast<std::size_t>(
                std::max(
                    0,
                    y - 1
                    )
                ) *
            static_cast<std::size_t>(
                width
                ) +
            static_cast<std::size_t>(
                x
                )
            ];

    const int bottom =
        luminance[
            static_cast<std::size_t>(
                std::min(
                    height - 1,
                    y + 1
                    )
                ) *
            static_cast<std::size_t>(
                width
                ) +
            static_cast<std::size_t>(
                x
                )
            ];

    return std::max({
        std::abs(
            center - left
            ),
        std::abs(
            center - right
            ),
        std::abs(
            center - top
            ),
        std::abs(
            center - bottom
            )
    });
}

} // namespace

namespace FocusTools
{

bool hasVisibleAdjustment(
    const FocusSettings &settings
    )
{
    return
        settings.amount > 0 &&
        settings.blend > 0;
}

QImage applyFocus(
    const QImage &sourceImage,
    const FocusSettings &settings
    )
{
    if (sourceImage.isNull() ||
        !hasVisibleAdjustment(settings)) {
        return sourceImage;
    }

    const int amount =
        std::clamp(
            settings.amount,
            0,
            100
            );

    const int radiusControl =
        std::clamp(
            settings.radius,
            1,
            50
            );

    const int thresholdControl =
        std::clamp(
            settings.threshold,
            0,
            100
            );

    const int edgeProtection =
        std::clamp(
            settings.edgeProtection,
            0,
            100
            );

    const double blend =
        std::clamp(
            settings.blend,
            0,
            100
            ) /
        100.0;

    QImage gpuResult =
        toArgb32(sourceImage);

    OpenClSpatialProcessingBackend::FocusSettings gpuSettings;
    gpuSettings.amount = amount;
    gpuSettings.radius = radiusControl;
    gpuSettings.threshold = thresholdControl;
    gpuSettings.edgeProtection = edgeProtection;
    gpuSettings.blend = std::clamp(
        settings.blend,
        0,
        100
        );

    if (ImageProcessingRuntime::instance()
            .tryApplyFocus(
                gpuResult,
                gpuSettings
                )) {
        return gpuResult;
    }

    const QImage source =
        toArgb32(
            sourceImage
            );

    const int width =
        source.width();

    const int height =
        source.height();

    const std::size_t pixelCount =
        static_cast<std::size_t>(
            width
            ) *
        static_cast<std::size_t>(
            height
            );

    std::vector<int> luminance(
        pixelCount
        );

    std::vector<int> alpha(
        pixelCount
        );

    for (int y = 0;
         y < height;
         ++y) {
        const QRgb *scanLine =
            reinterpret_cast<const QRgb *>(
                source.constScanLine(y)
                );

        for (int x = 0;
             x < width;
             ++x) {
            const std::size_t index =
                static_cast<std::size_t>(
                    y
                    ) *
                static_cast<std::size_t>(
                    width
                    ) +
                static_cast<std::size_t>(
                    x
                    );

            luminance[
                index
                ] =
                pixelLuminance(
                    scanLine[x]
                    );

            alpha[
                index
                ] =
                qAlpha(
                    scanLine[x]
                    );
        }
    }

    const double sigma =
        0.42 +
        ((radiusControl - 1) /
         49.0) *
        2.58;

    std::vector<int> blurredLuminance;

    alphaAwareGaussianBlur(
        luminance,
        alpha,
        width,
        height,
        sigma,
        blurredLuminance
        );

    // Recalibrated after real-photo testing:
    // Amount 50 is now approximately the previous Amount 100.
    // The upper half remains available for unusually soft images.
    const double amountScale =
        (amount /
         100.0) *
        3.40;

    const double threshold =
        thresholdControl *
        0.14;

    const double thresholdSoftness =
        1.8 +
        (sigma * 1.4);

    const double edgeProtectionStrength =
        edgeProtection /
        100.0;

    const double maximumDelta =
        4.0 +
        ((100 - edgeProtection) /
         100.0) *
        12.0 +
        (amount /
         100.0) *
        10.0;

    QImage result =
        source;

    for (int y = 0;
         y < height;
         ++y) {
        const QRgb *sourceLine =
            reinterpret_cast<const QRgb *>(
                source.constScanLine(y)
                );

        QRgb *resultLine =
            reinterpret_cast<QRgb *>(
                result.scanLine(y)
                );

        for (int x = 0;
             x < width;
             ++x) {
            const std::size_t index =
                static_cast<std::size_t>(
                    y
                    ) *
                static_cast<std::size_t>(
                    width
                    ) +
                static_cast<std::size_t>(
                    x
                    );

            const int originalLuminance =
                luminance[
                    index
                    ];

            const double highPass =
                originalLuminance -
                blurredLuminance[
                    index
                    ];

            const double detailGate =
                smoothStep(
                    threshold,
                    threshold +
                    thresholdSoftness,
                    std::abs(
                        highPass
                        )
                    );

            const int localGradient =
                localLuminanceGradient(
                    luminance,
                    width,
                    height,
                    x,
                    y
                    );

            const double strongEdge =
                smoothStep(
                    20.0,
                    92.0,
                    localGradient
                    );

            const double edgeGuard =
                1.0 -
                (0.78 *
                 edgeProtectionStrength *
                 strongEdge);

            const double shadowGuard =
                clampUnit(
                    originalLuminance /
                    18.0
                    );

            const double highlightGuard =
                clampUnit(
                    (255 -
                     originalLuminance) /
                    18.0
                    );

            const double toneGuard =
                std::min(
                    shadowGuard,
                    highlightGuard
                    );

            double luminanceDelta =
                highPass *
                amountScale *
                detailGate *
                edgeGuard *
                toneGuard *
                blend;

            luminanceDelta =
                std::clamp(
                    luminanceDelta,
                    -maximumDelta,
                    maximumDelta
                    );

            const int newLuminance =
                clampByte(
                    static_cast<int>(
                        std::round(
                            originalLuminance +
                            luminanceDelta
                            )
                        )
                    );

            int blueDifference = 128;
            int redDifference = 128;

            pixelChroma(
                sourceLine[x],
                blueDifference,
                redDifference
                );

            resultLine[x] =
                luminanceAndChromaToRgb(
                    newLuminance,
                    blueDifference,
                    redDifference,
                    qAlpha(
                        sourceLine[x]
                        )
                    );
        }
    }

    return result;
}

} // namespace FocusTools
