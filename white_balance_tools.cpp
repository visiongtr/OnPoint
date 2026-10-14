#include "white_balance_tools.h"
#include "imageprocessingruntime.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace
{

constexpr double kRedLuma = 0.2126;
constexpr double kGreenLuma = 0.7152;
constexpr double kBlueLuma = 0.0722;

double clampUnit(double value)
{
    return std::clamp(
        value,
        0.0,
        1.0
        );
}

double srgbToLinear(double value)
{
    value = clampUnit(value);

    if (value <= 0.04045) {
        return value / 12.92;
    }

    return std::pow(
        (value + 0.055) / 1.055,
        2.4
        );
}

double linearToSrgb(double value)
{
    value = std::max(
        0.0,
        value
        );

    if (value <= 0.0031308) {
        return clampUnit(
            value * 12.92
            );
    }

    return clampUnit(
        (1.055 *
         std::pow(
             value,
             1.0 / 2.4
             )) -
        0.055
        );
}

double linearLuminance(
    double red,
    double green,
    double blue
    )
{
    return
        (kRedLuma * red) +
        (kGreenLuma * green) +
        (kBlueLuma * blue);
}

WhiteBalanceTools::ChannelGains normalizeGains(
    WhiteBalanceTools::ChannelGains gains
    )
{
    if (!gains.valid) {
        return gains;
    }

    gains.red = std::clamp(
        gains.red,
        0.25,
        4.0
        );

    gains.green = std::clamp(
        gains.green,
        0.25,
        4.0
        );

    gains.blue = std::clamp(
        gains.blue,
        0.25,
        4.0
        );

    // Keep a neutral gray close to its original brightness.
    // The color cast changes, but White Balance should not become
    // a disguised exposure control.
    const double neutralLuma =
        (kRedLuma * gains.red) +
        (kGreenLuma * gains.green) +
        (kBlueLuma * gains.blue);

    if (neutralLuma > 0.000001) {
        gains.red /= neutralLuma;
        gains.green /= neutralLuma;
        gains.blue /= neutralLuma;
    }

    gains.red = std::clamp(
        gains.red,
        0.25,
        4.0
        );

    gains.green = std::clamp(
        gains.green,
        0.25,
        4.0
        );

    gains.blue = std::clamp(
        gains.blue,
        0.25,
        4.0
        );

    return gains;
}

WhiteBalanceTools::ChannelGains gainsFromLinearNeutral(
    double red,
    double green,
    double blue
    )
{
    WhiteBalanceTools::ChannelGains gains;

    const double maximumChannel =
        std::max({
            red,
            green,
            blue
        });

    if (maximumChannel <= 0.0001) {
        return gains;
    }

    const double target =
        linearLuminance(
            red,
            green,
            blue
            );

    if (target <= 0.0001) {
        return gains;
    }

    gains.red =
        target /
        std::max(
            red,
            0.0001
            );

    gains.green =
        target /
        std::max(
            green,
            0.0001
            );

    gains.blue =
        target /
        std::max(
            blue,
            0.0001
            );

    gains.valid = true;

    return normalizeGains(gains);
}

QImage toArgb32(
    const QImage &sourceImage
    )
{
    QImage writableImage;

    if (sourceImage.format() ==
            QImage::Format_ARGB32 ||
        sourceImage.format() ==
            QImage::Format_RGB32) {
        writableImage = sourceImage;
    } else {
        writableImage = sourceImage.convertToFormat(
            QImage::Format_ARGB32
            );
    }

    writableImage.detach();
    return writableImage;
}

WhiteBalanceTools::ChannelGains effectiveGains(
    const WhiteBalanceTools::WhiteBalanceSettings &settings
    )
{
    WhiteBalanceTools::ChannelGains gains =
        settings.baseGains;

    if (!gains.valid) {
        gains.red = 1.0;
        gains.green = 1.0;
        gains.blue = 1.0;
        gains.valid = true;
    }

    const double temperature =
        std::clamp(
            settings.temperature,
            -100,
            100
            ) / 100.0;

    const double tint =
        std::clamp(
            settings.tint,
            -100,
            100
            ) / 100.0;

    // Temperature moves red and blue in opposite directions.
    // Tint moves green against the red/blue pair.
    //
    // The ranges are intentionally restrained so the full slider
    // remains useful instead of turning into an effect filter.
    const double warmRed =
        std::exp(
            temperature * 0.32
            );

    const double coolBlue =
        std::exp(
            -temperature * 0.32
            );

    const double magentaPair =
        std::exp(
            tint * 0.135
            );

    const double greenTint =
        std::exp(
            -tint * 0.27
            );

    gains.red *=
        warmRed *
        magentaPair;

    gains.green *=
        greenTint;

    gains.blue *=
        coolBlue *
        magentaPair;

    gains = normalizeGains(gains);

    const double strength =
        std::clamp(
            settings.strength,
            0,
            100
            ) / 100.0;

    // Blend in logarithmic gain space. This produces a more natural
    // halfway correction than a straight channel-value interpolation.
    gains.red =
        std::exp(
            std::log(
                std::max(
                    gains.red,
                    0.0001
                    )
                ) *
            strength
            );

    gains.green =
        std::exp(
            std::log(
                std::max(
                    gains.green,
                    0.0001
                    )
                ) *
            strength
            );

    gains.blue =
        std::exp(
            std::log(
                std::max(
                    gains.blue,
                    0.0001
                    )
                ) *
            strength
            );

    gains.valid = true;

    return gains;
}

} // namespace

namespace WhiteBalanceTools
{

QColor sampleAverageColor(
    const QImage &sourceImage,
    const QPoint &centerPoint,
    int radius
    )
{
    if (sourceImage.isNull()) {
        return QColor();
    }

    const QImage image =
        toArgb32(sourceImage);

    radius = std::max(
        0,
        radius
        );

    const QRect sampleRect =
        QRect(
            centerPoint.x() - radius,
            centerPoint.y() - radius,
            (radius * 2) + 1,
            (radius * 2) + 1
            )
            .intersected(
                image.rect()
                );

    if (sampleRect.isEmpty()) {
        return QColor();
    }

    double redSum = 0.0;
    double greenSum = 0.0;
    double blueSum = 0.0;
    double weightSum = 0.0;

    for (int y = sampleRect.top();
         y <= sampleRect.bottom();
         ++y) {
        const QRgb *scanLine =
            reinterpret_cast<const QRgb *>(
                image.constScanLine(y)
                );

        for (int x = sampleRect.left();
             x <= sampleRect.right();
             ++x) {
            const QRgb pixel =
                scanLine[x];

            const double alpha =
                qAlpha(pixel) /
                255.0;

            if (alpha <= 0.0) {
                continue;
            }

            redSum +=
                srgbToLinear(
                    qRed(pixel) /
                    255.0
                    ) *
                alpha;

            greenSum +=
                srgbToLinear(
                    qGreen(pixel) /
                    255.0
                    ) *
                alpha;

            blueSum +=
                srgbToLinear(
                    qBlue(pixel) /
                    255.0
                    ) *
                alpha;

            weightSum += alpha;
        }
    }

    if (weightSum <= 0.0) {
        return QColor();
    }

    const double red =
        redSum /
        weightSum;

    const double green =
        greenSum /
        weightSum;

    const double blue =
        blueSum /
        weightSum;

    if (std::max({
            red,
            green,
            blue
            }) <= 0.0001) {
        return QColor();
    }

    return QColor::fromRgbF(
        linearToSrgb(red),
        linearToSrgb(green),
        linearToSrgb(blue),
        1.0
        );
}

ChannelGains gainsFromNeutralColor(
    const QColor &neutralColor
    )
{
    if (!neutralColor.isValid()) {
        return ChannelGains();
    }

    return gainsFromLinearNeutral(
        srgbToLinear(
            neutralColor.redF()
            ),
        srgbToLinear(
            neutralColor.greenF()
            ),
        srgbToLinear(
            neutralColor.blueF()
            )
        );
}

ChannelGains automaticGains(
    const QImage &sourceImage
    )
{
    if (sourceImage.isNull()) {
        return ChannelGains();
    }

    const QImage image =
        toArgb32(sourceImage);

    const qint64 pixelCount =
        static_cast<qint64>(
            image.width()
            ) *
        static_cast<qint64>(
            image.height()
            );

    // Keep the analysis fast on very large photographs while still
    // sampling the entire frame evenly.
    const int step =
        std::max(
            1,
            static_cast<int>(
                std::sqrt(
                    std::max<qint64>(
                        1,
                        pixelCount /
                        220000
                        )
                    )
                )
            );

    double redSum = 0.0;
    double greenSum = 0.0;
    double blueSum = 0.0;
    double weightSum = 0.0;

    for (int y = 0;
         y < image.height();
         y += step) {
        const QRgb *scanLine =
            reinterpret_cast<const QRgb *>(
                image.constScanLine(y)
                );

        for (int x = 0;
             x < image.width();
             x += step) {
            const QRgb pixel =
                scanLine[x];

            if (qAlpha(pixel) == 0) {
                continue;
            }

            const double red =
                srgbToLinear(
                    qRed(pixel) /
                    255.0
                    );

            const double green =
                srgbToLinear(
                    qGreen(pixel) /
                    255.0
                    );

            const double blue =
                srgbToLinear(
                    qBlue(pixel) /
                    255.0
                    );

            const double luminance =
                linearLuminance(
                    red,
                    green,
                    blue
                    );

            // Avoid near-black noise and clipped highlights.
            if (luminance < 0.015 ||
                luminance > 0.94) {
                continue;
            }

            const double maximum =
                std::max({
                    red,
                    green,
                    blue
                });

            const double minimum =
                std::min({
                    red,
                    green,
                    blue
                });

            const double saturation =
                maximum > 0.0001
                    ? (maximum - minimum) /
                          maximum
                    : 0.0;

            // Favor neutral and moderately colored regions without
            // pretending strongly colored pixels do not exist.
            const double neutralWeight =
                0.12 +
                (0.88 *
                 std::pow(
                     1.0 - saturation,
                     2.0
                     ));

            const double midtoneWeight =
                std::clamp(
                    1.15 -
                    (std::abs(
                         luminance - 0.45
                         ) *
                     1.65),
                    0.15,
                    1.0
                    );

            const double weight =
                neutralWeight *
                midtoneWeight;

            redSum +=
                red *
                weight;

            greenSum +=
                green *
                weight;

            blueSum +=
                blue *
                weight;

            weightSum += weight;
        }
    }

    if (weightSum <= 0.0) {
        return ChannelGains();
    }

    return gainsFromLinearNeutral(
        redSum /
            weightSum,
        greenSum /
            weightSum,
        blueSum /
            weightSum
        );
}

bool hasVisibleAdjustment(
    const WhiteBalanceSettings &settings
    )
{
    if (settings.strength <= 0) {
        return false;
    }

    const ChannelGains gains =
        effectiveGains(settings);

    constexpr double threshold = 0.0005;

    return
        std::abs(
            gains.red - 1.0
            ) > threshold ||
        std::abs(
            gains.green - 1.0
            ) > threshold ||
        std::abs(
            gains.blue - 1.0
            ) > threshold;
}

QImage applyWhiteBalance(
    const QImage &sourceImage,
    const WhiteBalanceSettings &settings
    )
{
    if (sourceImage.isNull() ||
        !hasVisibleAdjustment(settings)) {
        return sourceImage;
    }

    const ChannelGains gains =
        effectiveGains(settings);

    using ChannelLookup =
        OpenClProcessingBackend::ChannelLookup;

    const auto makeGainLookup = [](double gain) {
        ChannelLookup lookup {};

        for (std::size_t index = 0;
             index < lookup.size();
             ++index) {
            const double linear =
                srgbToLinear(
                    static_cast<double>(index) /
                    255.0
                    ) *
                gain;

            lookup[index] =
                static_cast<quint8>(
                    std::clamp(
                        static_cast<int>(
                            std::round(
                                linearToSrgb(linear) *
                                255.0
                                )
                            ),
                        0,
                        255
                        )
                    );
        }

        return lookup;
    };

    const ChannelLookup redLookup =
        makeGainLookup(gains.red);

    const ChannelLookup greenLookup =
        makeGainLookup(gains.green);

    const ChannelLookup blueLookup =
        makeGainLookup(gains.blue);

    QImage result =
        toArgb32(sourceImage);

    if (ImageProcessingRuntime::instance()
            .tryApplyChannelLookups(
                result,
                redLookup,
                greenLookup,
                blueLookup
                )) {
        return result;
    }

    const int imageWidth = result.width();
    const int bytesPerLine = result.bytesPerLine();
    uchar *imageBits = result.bits();

    ImageProcessingRuntime::instance().parallelForRows(
        result.height(),
        [imageBits,
         bytesPerLine,
         imageWidth,
         redLookup,
         greenLookup,
         blueLookup](
            int firstRow,
            int endRow
            ) {
            for (int y = firstRow;
                 y < endRow;
                 ++y) {
                QRgb *scanLine =
                    reinterpret_cast<QRgb *>(
                        imageBits +
                        (y * bytesPerLine)
                        );

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
        }
        );

    return result;
}

} // namespace WhiteBalanceTools
