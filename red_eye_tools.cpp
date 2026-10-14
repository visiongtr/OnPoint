#include "red_eye_tools.h"

#include <QColor>
#include <QtGlobal>

#include <cmath>
#include <vector>

namespace
{

int clampChannel(int value)
{
    return qBound(0, value, 255);
}

QImage toArgb32(const QImage &source)
{
    if (source.format() == QImage::Format_ARGB32 ||
        source.format() == QImage::Format_ARGB32_Premultiplied) {
        return source.convertToFormat(QImage::Format_ARGB32);
    }

    return source.convertToFormat(QImage::Format_ARGB32);
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge0 == edge1) {
        return value < edge0 ? 0.0 : 1.0;
    }

    double t = (value - edge0) / (edge1 - edge0);
    t = qBound(0.0, t, 1.0);

    return t * t * (3.0 - (2.0 * t));
}

struct IrisTone
{
    double red;
    double green;
    double blue;
};

IrisTone irisToneForColor(RedEyeTools::IrisColor color)
{
    switch (color) {
    case RedEyeTools::IrisColor::Brown:
        return { 95.0, 62.0, 34.0 };

    case RedEyeTools::IrisColor::Hazel:
        return { 105.0, 82.0, 42.0 };

    case RedEyeTools::IrisColor::Green:
        return { 55.0, 92.0, 62.0 };

    case RedEyeTools::IrisColor::Blue:
        return { 45.0, 78.0, 120.0 };

    case RedEyeTools::IrisColor::Gray:
        return { 82.0, 88.0, 92.0 };

    case RedEyeTools::IrisColor::Auto:
    default:
        return { 70.0, 62.0, 55.0 };
    }
}

double luminance(double red, double green, double blue)
{
    return (0.299 * red) + (0.587 * green) + (0.114 * blue);
}


double circularHueDistance(int firstHue, int secondHue)
{
    if (firstHue < 0 || secondHue < 0) {
        return 180.0;
    }

    const int directDistance =
        std::abs(firstHue - secondHue);

    return static_cast<double>(
        qMin(directDistance, 360 - directDistance)
        );
}

double sampledTargetScore(int red,
                          int green,
                          int blue,
                          const QColor &targetColor,
                          int tolerance)
{
    const QColor pixelColor(red, green, blue);

    const int targetHue = targetColor.hsvHue();
    const int targetSaturation = targetColor.hsvSaturation();
    const int targetValue = targetColor.value();

    const int pixelHue = pixelColor.hsvHue();
    const int pixelSaturation = pixelColor.hsvSaturation();
    const int pixelValue = pixelColor.value();

    const double redDifference =
        static_cast<double>(red - targetColor.red());

    const double greenDifference =
        static_cast<double>(green - targetColor.green());

    const double blueDifference =
        static_cast<double>(blue - targetColor.blue());

    const double rgbDistance =
        std::sqrt(
            (redDifference * redDifference) +
            (greenDifference * greenDifference) +
            (blueDifference * blueDifference)
            );

    const double rgbTolerance =
        28.0 + (static_cast<double>(tolerance) * 1.75);

    const double rgbScore =
        1.0 - smoothStep(
                  rgbTolerance * 0.40,
                  rgbTolerance,
                  rgbDistance
                  );

    // Near-neutral pet-eye glow can be white, silver, or pale gray.
    // Hue is unstable there, so value and RGB similarity carry the mask.
    if (targetSaturation < 38) {
        const double valueTolerance =
            22.0 + (static_cast<double>(tolerance) * 1.90);

        const double valueScore =
            1.0 - smoothStep(
                      valueTolerance * 0.35,
                      valueTolerance,
                      std::abs(pixelValue - targetValue)
                      );

        const double saturationTolerance =
            28.0 + (static_cast<double>(tolerance) * 1.15);

        const double neutralityScore =
            1.0 - smoothStep(
                      saturationTolerance * 0.45,
                      saturationTolerance,
                      std::abs(pixelSaturation - targetSaturation)
                      );

        return qBound(
            0.0,
            qMax(rgbScore,
                 valueScore * neutralityScore),
            1.0
            );
    }

    const double hueTolerance =
        7.0 + (static_cast<double>(tolerance) * 0.58);

    const double saturationTolerance =
        24.0 + (static_cast<double>(tolerance) * 1.45);

    const double valueTolerance =
        34.0 + (static_cast<double>(tolerance) * 1.65);

    const double hueScore =
        1.0 - smoothStep(
                  hueTolerance * 0.40,
                  hueTolerance,
                  circularHueDistance(pixelHue, targetHue)
                  );

    const double saturationScore =
        1.0 - smoothStep(
                  saturationTolerance * 0.40,
                  saturationTolerance,
                  std::abs(pixelSaturation - targetSaturation)
                  );

    const double valueScore =
        1.0 - smoothStep(
                  valueTolerance * 0.35,
                  valueTolerance,
                  std::abs(pixelValue - targetValue)
                  );

    const double hsvScore =
        hueScore *
        (0.30 + (0.70 * saturationScore)) *
        (0.42 + (0.58 * valueScore));

    return qBound(
        0.0,
        qMax(hsvScore, rgbScore * 0.78),
        1.0
        );
}

} // anonymous namespace

namespace RedEyeTools
{

QImage repairRedEye(const QImage &source,
                    const QRect &selection,
                    const RedEyeSettings &settings)
{
    if (source.isNull() || selection.isNull() || selection.isEmpty()) {
        return source;
    }

    QImage result = toArgb32(source);

    const QRect imageRect(0, 0, result.width(), result.height());
    const QRect workRect = selection.normalized().intersected(imageRect);

    if (workRect.isEmpty()) {
        return source;
    }

    const int safeTolerance = qBound(0, settings.tolerance, 100);
    const int safeStrength = qBound(0, settings.strength, 100);
    const int safePupilDarken = qBound(0, settings.pupilDarken, 100);
    const int safeIrisAmount = qBound(0, settings.irisAmount, 100);
    const int safeBlend = qBound(0, settings.blend, 100);
    const int safeSoftEdge = qBound(0, settings.softEdge, 100);

    const double strengthFactor = static_cast<double>(safeStrength) / 100.0;
    const double blendFactor = static_cast<double>(safeBlend) / 100.0;
    const double softEdgeFactor = static_cast<double>(safeSoftEdge) / 100.0;

    const bool useSampledTarget =
        settings.useSampledTarget &&
        settings.sampledTargetColor.isValid();

    const QColor sampledTargetColor =
        useSampledTarget
            ? settings.sampledTargetColor
            : QColor(205, 45, 38);

    const int sampledTargetSaturation =
        sampledTargetColor.hsvSaturation();

    const int sampledTargetValue =
        sampledTargetColor.value();

    // Higher tolerance catches weaker red-eye pixels.
    const double redDominanceThreshold = 72.0 - (safeTolerance * 0.47);
    const double redRatioThreshold = 1.70 - (safeTolerance * 0.0065);

    const int width = workRect.width();
    const int height = workRect.height();

    std::vector<double> redMaskScore(static_cast<size_t>(width * height), 0.0);

    double weightedXSum = 0.0;
    double weightedYSum = 0.0;
    double weightSum = 0.0;

    QRect redBounds;

    for (int y = workRect.top(); y <= workRect.bottom(); ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(result.constScanLine(y));

        for (int x = workRect.left(); x <= workRect.right(); ++x) {
            const QRgb pixel = scanLine[x];

            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);

            const int average = (red + green + blue) / 3;
            const int maxChannel = qMax(red, qMax(green, blue));
            const int minChannel = qMin(red, qMin(green, blue));

            const bool brightNeutralHighlight =
                average > 220 &&
                (maxChannel - minChannel) < 55;

            double score = 0.0;

            if (useSampledTarget) {
                // Preserve ordinary white catchlights unless the sampled
                // unwanted glow itself is pale/neutral and very bright.
                const bool sampledTargetIsBrightNeutral =
                    sampledTargetSaturation < 38 &&
                    sampledTargetValue > 205;

                if (brightNeutralHighlight &&
                    !sampledTargetIsBrightNeutral) {
                    continue;
                }

                score = sampledTargetScore(
                    red,
                    green,
                    blue,
                    sampledTargetColor,
                    safeTolerance
                    );
            } else {
                const int maxGreenBlue =
                    qMax(green, blue);

                const int redDominance =
                    red - maxGreenBlue;

                const double redRatio =
                    static_cast<double>(red) /
                    static_cast<double>(qMax(1, maxGreenBlue));

                const bool redIsHighEnough =
                    red > 65;

                const bool redLooksLikeEye =
                    redDominance >= redDominanceThreshold ||
                    redRatio >= redRatioThreshold;

                if (!redIsHighEnough ||
                    !redLooksLikeEye ||
                    brightNeutralHighlight) {
                    continue;
                }

                const double dominanceWeight = qBound(
                    0.0,
                    (static_cast<double>(redDominance) -
                     redDominanceThreshold) / 85.0,
                    1.0
                    );

                const double ratioWeight = qBound(
                    0.0,
                    (redRatio - redRatioThreshold) / 0.95,
                    1.0
                    );

                score = qMax(
                    dominanceWeight,
                    ratioWeight
                    );
            }

            if (score <= 0.0) {
                continue;
            }

            const int localX = x - workRect.left();
            const int localY = y - workRect.top();
            const size_t index = static_cast<size_t>((localY * width) + localX);

            redMaskScore[index] = score;

            weightedXSum += static_cast<double>(x) * score;
            weightedYSum += static_cast<double>(y) * score;
            weightSum += score;

            const QRect pixelRect(x, y, 1, 1);

            if (redBounds.isNull()) {
                redBounds = pixelRect;
            } else {
                redBounds = redBounds.united(pixelRect);
            }
        }
    }

    if (weightSum <= 0.0 || redBounds.isNull() || redBounds.isEmpty()) {
        return source;
    }

    const double centerX = weightedXSum / weightSum;
    const double centerY = weightedYSum / weightSum;

    double radius = 2.0;

    for (int y = redBounds.top(); y <= redBounds.bottom(); ++y) {
        for (int x = redBounds.left(); x <= redBounds.right(); ++x) {
            const int localX = x - workRect.left();
            const int localY = y - workRect.top();

            if (localX < 0 || localY < 0 || localX >= width || localY >= height) {
                continue;
            }

            const size_t index = static_cast<size_t>((localY * width) + localX);

            if (redMaskScore[index] <= 0.0) {
                continue;
            }

            const double dx = static_cast<double>(x) - centerX;
            const double dy = static_cast<double>(y) - centerY;
            const double distance = std::sqrt((dx * dx) + (dy * dy));

            radius = qMax(radius, distance);
        }
    }

    const IrisTone irisTone = irisToneForColor(settings.irisColor);
    const bool manualIrisColor =
        settings.mode == RedEyeMode::Manual &&
        settings.irisColor != IrisColor::Auto &&
        safeIrisAmount > 0;

    for (int y = workRect.top(); y <= workRect.bottom(); ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));

        for (int x = workRect.left(); x <= workRect.right(); ++x) {
            const int localX = x - workRect.left();
            const int localY = y - workRect.top();
            const size_t index = static_cast<size_t>((localY * width) + localX);

            const double maskScore = redMaskScore[index];

            if (maskScore <= 0.0) {
                continue;
            }

            const QRgb pixel = scanLine[x];

            const int originalRed = qRed(pixel);
            const int originalGreen = qGreen(pixel);
            const int originalBlue = qBlue(pixel);
            const int alpha = qAlpha(pixel);

            const int average = (originalRed + originalGreen + originalBlue) / 3;
            const int maxChannel = qMax(originalRed, qMax(originalGreen, originalBlue));
            const int minChannel = qMin(originalRed, qMin(originalGreen, originalBlue));

            // Strong catchlight protection. Pale sampled pet-eye glow
            // is allowed through because it may genuinely be white/silver.
            const bool sampledTargetIsBrightNeutral =
                useSampledTarget &&
                sampledTargetSaturation < 38 &&
                sampledTargetValue > 205;

            if (average > 225 &&
                (maxChannel - minChannel) < 60 &&
                !sampledTargetIsBrightNeutral) {
                continue;
            }

            const double dx = static_cast<double>(x) - centerX;
            const double dy = static_cast<double>(y) - centerY;
            const double distance = std::sqrt((dx * dx) + (dy * dy));
            const double normalizedRadius = distance / qMax(1.0, radius);

            // Stronger in the center, softer toward the edge of the red blob.
            const double edgeStart = 0.62 - (softEdgeFactor * 0.20);
            const double edgeEnd = 1.15 + (softEdgeFactor * 0.20);
            const double edgeFade = 1.0 - smoothStep(edgeStart, edgeEnd, normalizedRadius);

            const double centerWeight =
                1.0 - (0.40 * smoothStep(0.0, 1.0, normalizedRadius));

            const double correctionWeight =
                qBound(0.0, maskScore * edgeFade * centerWeight * blendFactor, 1.0);

            if (correctionWeight <= 0.0) {
                continue;
            }

            double correctedRed = originalRed;
            double correctedGreen = originalGreen;
            double correctedBlue = originalBlue;

            if (useSampledTarget) {
                // Remove the sampled glow chroma without inventing replacement
                // detail. This works for red, green, yellow, blue, and pale
                // pet-eye reflections.
                const double neutralLuma =
                    luminance(
                        originalRed,
                        originalGreen,
                        originalBlue
                        );

                correctedRed +=
                    (neutralLuma - correctedRed) * strengthFactor;

                correctedGreen +=
                    (neutralLuma - correctedGreen) * strengthFactor;

                correctedBlue +=
                    (neutralLuma - correctedBlue) * strengthFactor;
            } else {
                const int neutralRed =
                    (originalGreen + originalBlue) / 2;

                correctedRed +=
                    (neutralRed - correctedRed) * strengthFactor;
            }

            // Pupil darkening is strongest near the center.
            const double pupilCenterWeight =
                1.0 - smoothStep(0.20, 1.05, normalizedRadius);

            const double darkenAmount =
                (static_cast<double>(safePupilDarken) / 100.0) *
                (0.18 + (0.62 * pupilCenterWeight)) *
                strengthFactor;

            const double darkenScale = 1.0 - darkenAmount;

            correctedRed *= darkenScale;
            correctedGreen *= darkenScale;
            correctedBlue *= darkenScale;

            // Optional manual iris tint. This is deliberately subtle.
            if (manualIrisColor) {
                const double irisRing =
                    smoothStep(0.22, 0.48, normalizedRadius) *
                    (1.0 - smoothStep(0.78, 1.15, normalizedRadius));

                const double irisBlend =
                    irisRing *
                    (static_cast<double>(safeIrisAmount) / 100.0) *
                    0.45 *
                    correctionWeight;

                if (irisBlend > 0.0) {
                    const double currentLuma =
                        qMax(1.0, luminance(correctedRed, correctedGreen, correctedBlue));

                    const double targetLuma =
                        qMax(1.0, luminance(irisTone.red, irisTone.green, irisTone.blue));

                    const double lumaScale = currentLuma / targetLuma;

                    const double targetRed = irisTone.red * lumaScale;
                    const double targetGreen = irisTone.green * lumaScale;
                    const double targetBlue = irisTone.blue * lumaScale;

                    correctedRed += (targetRed - correctedRed) * irisBlend;
                    correctedGreen += (targetGreen - correctedGreen) * irisBlend;
                    correctedBlue += (targetBlue - correctedBlue) * irisBlend;
                }
            }

            const double finalRed =
                originalRed + ((correctedRed - originalRed) * correctionWeight);

            const double finalGreen =
                originalGreen + ((correctedGreen - originalGreen) * correctionWeight);

            const double finalBlue =
                originalBlue + ((correctedBlue - originalBlue) * correctionWeight);

            scanLine[x] = qRgba(
                clampChannel(static_cast<int>(std::round(finalRed))),
                clampChannel(static_cast<int>(std::round(finalGreen))),
                clampChannel(static_cast<int>(std::round(finalBlue))),
                alpha
                );
        }
    }

    return result;
}

} // namespace RedEyeTools
