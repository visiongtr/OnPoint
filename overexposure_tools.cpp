#include "overexposure_tools.h"
#include "imageprocessingruntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{

struct RgbDouble
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

struct SampleAccumulator
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double weight = 0.0;

    void add(const QRgb pixel, double sampleWeight)
    {
        if (sampleWeight <= 0.0) {
            return;
        }

        red += qRed(pixel) * sampleWeight;
        green += qGreen(pixel) * sampleWeight;
        blue += qBlue(pixel) * sampleWeight;
        weight += sampleWeight;
    }

    bool hasSamples() const
    {
        return weight > 0.0;
    }

    RgbDouble color() const
    {
        if (!hasSamples()) {
            return {};
        }

        return {
            red / weight,
            green / weight,
            blue / weight
        };
    }
};

int clampChannel(double value)
{
    return std::clamp(static_cast<int>(std::round(value)), 0, 255);
}

double clampUnit(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double smoothStep(double edge0, double edge1, double value)
{
    if (edge0 == edge1) {
        return value >= edge1 ? 1.0 : 0.0;
    }

    const double t = clampUnit((value - edge0) / (edge1 - edge0));
    return t * t * (3.0 - (2.0 * t));
}

double luminance(double red, double green, double blue)
{
    return (0.299 * red) + (0.587 * green) + (0.114 * blue);
}

double luminance(QRgb pixel)
{
    return luminance(qRed(pixel), qGreen(pixel), qBlue(pixel));
}

QImage toArgb32(const QImage &sourceImage)
{
    if (sourceImage.format() == QImage::Format_ARGB32 ||
        sourceImage.format() == QImage::Format_RGB32) {
        return sourceImage;
    }

    return sourceImage.convertToFormat(QImage::Format_ARGB32);
}

RgbDouble mixColor(const RgbDouble &a, const RgbDouble &b, double amount)
{
    const double t = clampUnit(amount);

    return {
        a.red + ((b.red - a.red) * t),
        a.green + ((b.green - a.green) * t),
        a.blue + ((b.blue - a.blue) * t)
    };
}

RgbDouble shiftToLuminance(const RgbDouble &color, double targetLuminance)
{
    const double currentLuminance = luminance(color.red, color.green, color.blue);
    const double delta = targetLuminance - currentLuminance;

    return {
        std::clamp(color.red + delta, 0.0, 255.0),
        std::clamp(color.green + delta, 0.0, 255.0),
        std::clamp(color.blue + delta, 0.0, 255.0)
    };
}

double normalizedRegionRadius(const QRect &rect,
                              double x,
                              double y,
                              OverexposureTools::RegionShape shape)
{
    const double centerX = rect.left() + ((rect.width() - 1) / 2.0);
    const double centerY = rect.top() + ((rect.height() - 1) / 2.0);

    const double radiusX = std::max(1.0, rect.width() / 2.0);
    const double radiusY = std::max(1.0, rect.height() / 2.0);

    const double dx = std::abs(x - centerX) / radiusX;
    const double dy = std::abs(y - centerY) / radiusY;

    if (shape == OverexposureTools::RegionShape::Oval) {
        return std::sqrt((dx * dx) + (dy * dy));
    }

    return std::max(dx, dy);
}

double regionWeight(const QRect &rect,
                    double x,
                    double y,
                    const OverexposureTools::OverexposureSettings &settings)
{
    const double radius = normalizedRegionRadius(rect, x, y, settings.shape);

    if (radius > 1.0) {
        return 0.0;
    }

    const double softEdge = std::clamp(settings.softEdge, 0, 100) / 100.0;

    // Higher Soft Edge means the fade starts farther inward.
    const double fadeSize = 0.08 + (softEdge * 0.46);
    const double fadeStart = std::clamp(1.0 - fadeSize, 0.05, 0.95);

    return 1.0 - smoothStep(fadeStart, 1.0, radius);
}

void addSamplesAroundPoint(const QImage &image,
                           double sampleX,
                           double sampleY,
                           int halfSize,
                           double brightCutoff,
                           SampleAccumulator &accumulator)
{
    const int left = std::max(0, static_cast<int>(std::floor(sampleX - halfSize)));
    const int right = std::min(image.width() - 1, static_cast<int>(std::ceil(sampleX + halfSize)));
    const int top = std::max(0, static_cast<int>(std::floor(sampleY - halfSize)));
    const int bottom = std::min(image.height() - 1, static_cast<int>(std::ceil(sampleY + halfSize)));

    for (int y = top; y <= bottom; ++y) {
        const QRgb *scanLine = reinterpret_cast<const QRgb *>(image.constScanLine(y));

        for (int x = left; x <= right; ++x) {
            const QRgb pixel = scanLine[x];
            const double luma = luminance(pixel);

            // Try not to sample the actual blown area as its own repair color.
            const double tooBrightPenalty =
                1.0 - (smoothStep(brightCutoff, 255.0, luma) * 0.85);

            const double alphaWeight = qAlpha(pixel) / 255.0;
            const double sampleWeight = std::max(0.0, tooBrightPenalty * alphaWeight);

            accumulator.add(pixel, sampleWeight);
        }
    }
}

enum class SampleSector
{
    TopLeft = 0,
    Top = 1,
    TopRight = 2,
    Right = 3,
    BottomRight = 4,
    Bottom = 5,
    BottomLeft = 6,
    Left = 7
};

constexpr int SampleSectorCount = 8;

struct SectorSample
{
    bool valid = false;
    RgbDouble color;
};

using SectorSamples = std::array<SectorSample, SampleSectorCount>;

struct Direction
{
    double x = 0.0;
    double y = 0.0;
};

RgbDouble averageColors(const RgbDouble &a, const RgbDouble &b, double amount = 0.5)
{
    return {
        a.red + ((b.red - a.red) * amount),
        a.green + ((b.green - a.green) * amount),
        a.blue + ((b.blue - a.blue) * amount)
    };
}

double distanceFromCenterToRegionEdge(const QRect &selection,
                                      double directionX,
                                      double directionY,
                                      OverexposureTools::RegionShape shape)
{
    const double radiusX = std::max(1.0, selection.width() / 2.0);
    const double radiusY = std::max(1.0, selection.height() / 2.0);

    if (shape == OverexposureTools::RegionShape::Oval) {
        const double denominator =
            std::sqrt(((directionX * directionX) / (radiusX * radiusX)) +
                      ((directionY * directionY) / (radiusY * radiusY)));

        if (denominator <= 0.0) {
            return 0.0;
        }

        return 1.0 / denominator;
    }

    const double xDistance =
        std::abs(directionX) > 0.0001 ? radiusX / std::abs(directionX) : 999999.0;

    const double yDistance =
        std::abs(directionY) > 0.0001 ? radiusY / std::abs(directionY) : 999999.0;

    return std::min(xDistance, yDistance);
}

SectorSample sampleSector(const QImage &image,
                          const QRect &selection,
                          const Direction &direction,
                          int sampleDistancePixels,
                          int sampleWidthPixels,
                          double brightCutoff,
                          OverexposureTools::RegionShape shape)
{
    const double length = std::sqrt(
        (direction.x * direction.x) +
        (direction.y * direction.y)
        );

    if (length <= 0.0) {
        return {};
    }

    const double directionX = direction.x / length;
    const double directionY = direction.y / length;

    const double centerX = selection.left() + ((selection.width() - 1) / 2.0);
    const double centerY = selection.top() + ((selection.height() - 1) / 2.0);

    const double distanceToEdge = distanceFromCenterToRegionEdge(
        selection,
        directionX,
        directionY,
        shape
        );

    const double sampleDistance =
        distanceToEdge +
        sampleDistancePixels +
        (sampleWidthPixels / 2.0);

    const double sampleX = centerX + (directionX * sampleDistance);
    const double sampleY = centerY + (directionY * sampleDistance);

    // Important edge rule:
    // If the sample center is off the image, mark this sector missing.
    // Do not clamp it to the border, because that creates wedge/pie artifacts.
    if (sampleX < 0.0 ||
        sampleY < 0.0 ||
        sampleX >= image.width() ||
        sampleY >= image.height()) {
        return {};
    }

    SampleAccumulator accumulator;

    addSamplesAroundPoint(
        image,
        sampleX,
        sampleY,
        std::max(1, sampleWidthPixels / 2),
        brightCutoff,
        accumulator
        );

    if (!accumulator.hasSamples()) {
        return {};
    }

    SectorSample sample;
    sample.valid = true;
    sample.color = accumulator.color();

    return sample;
}

SectorSamples collectSectorSamples(const QImage &image,
                                   const QRect &selection,
                                   int sampleDistancePixels,
                                   int sampleWidthPixels,
                                   double brightCutoff,
                                   OverexposureTools::RegionShape shape)
{
    const std::array<Direction, SampleSectorCount> directions = {{
        {-1.0, -1.0}, // TL
        { 0.0, -1.0}, // T
        { 1.0, -1.0}, // TR
        { 1.0,  0.0}, // R
        { 1.0,  1.0}, // BR
        { 0.0,  1.0}, // B
        {-1.0,  1.0}, // BL
        {-1.0,  0.0}  // L
    }};

    SectorSamples samples;

    for (int i = 0; i < SampleSectorCount; ++i) {
        samples[i] = sampleSector(
            image,
            selection,
            directions[i],
            sampleDistancePixels,
            sampleWidthPixels,
            brightCutoff,
            shape
            );
    }

    return samples;
}

bool hasAnyValidSector(const SectorSamples &samples)
{
    for (const SectorSample &sample : samples) {
        if (sample.valid) {
            return true;
        }
    }

    return false;
}

SectorSamples fillMissingSectorsFromNeighbors(SectorSamples samples)
{
    if (!hasAnyValidSector(samples)) {
        return samples;
    }

    SectorSamples filledSamples = samples;

    for (int i = 0; i < SampleSectorCount; ++i) {
        if (samples[i].valid) {
            continue;
        }

        int counterClockwiseIndex = -1;
        int clockwiseIndex = -1;

        int counterClockwiseDistance = 0;
        int clockwiseDistance = 0;

        for (int distance = 1; distance < SampleSectorCount; ++distance) {
            const int testIndex = (i - distance + SampleSectorCount) % SampleSectorCount;

            if (samples[testIndex].valid) {
                counterClockwiseIndex = testIndex;
                counterClockwiseDistance = distance;
                break;
            }
        }

        for (int distance = 1; distance < SampleSectorCount; ++distance) {
            const int testIndex = (i + distance) % SampleSectorCount;

            if (samples[testIndex].valid) {
                clockwiseIndex = testIndex;
                clockwiseDistance = distance;
                break;
            }
        }

        if (counterClockwiseIndex >= 0 && clockwiseIndex >= 0) {
            const double totalDistance =
                static_cast<double>(counterClockwiseDistance + clockwiseDistance);

            const double clockwiseWeight =
                static_cast<double>(counterClockwiseDistance) / totalDistance;

            filledSamples[i].valid = true;
            filledSamples[i].color = averageColors(
                samples[counterClockwiseIndex].color,
                samples[clockwiseIndex].color,
                clockwiseWeight
                );
        } else if (counterClockwiseIndex >= 0) {
            filledSamples[i].valid = true;
            filledSamples[i].color = samples[counterClockwiseIndex].color;
        } else if (clockwiseIndex >= 0) {
            filledSamples[i].valid = true;
            filledSamples[i].color = samples[clockwiseIndex].color;
        }
    }

    return filledSamples;
}

RgbDouble averageValidSectorColors(const SectorSamples &samples,
                                   const RgbDouble &fallbackColor)
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    int count = 0;

    for (const SectorSample &sample : samples) {
        if (!sample.valid) {
            continue;
        }

        red += sample.color.red;
        green += sample.color.green;
        blue += sample.color.blue;
        ++count;
    }

    if (count <= 0) {
        return fallbackColor;
    }

    return {
        red / count,
        green / count,
        blue / count
    };
}

RgbDouble directionalSurroundingColor(const SectorSamples &samples,
                                      const QRect &selection,
                                      int x,
                                      int y,
                                      const RgbDouble &fallbackColor)
{
    // The original implementation selected two samples by angle around the
    // selection center. Although the colors were interpolated, the angular
    // sectors could still reveal pie-slice / pyramid-shaped transitions.
    //
    // Build one continuous spatial color field instead. Every valid sample
    // contributes to every pixel with a smoothly changing distance weight.
    // The center is gently pulled toward the overall sample average while the
    // perimeter follows the nearest surrounding colors more strongly.

    const RgbDouble averageColor =
        averageValidSectorColors(samples, fallbackColor);

    const double centerX = selection.left() + ((selection.width() - 1) / 2.0);
    const double centerY = selection.top() + ((selection.height() - 1) / 2.0);

    const double radiusX = std::max(1.0, selection.width() / 2.0);
    const double radiusY = std::max(1.0, selection.height() / 2.0);

    const double normalizedX = std::clamp((x - centerX) / radiusX, -1.0, 1.0);
    const double normalizedY = std::clamp((y - centerY) / radiusY, -1.0, 1.0);

    // Same order as SectorSamples:
    // TL, T, TR, R, BR, B, BL, L.
    // Anchors sit just outside the region so the field naturally points toward
    // the sampled perimeter without producing hard angular boundaries.
    constexpr double anchorRadius = 1.18;
    const std::array<Direction, SampleSectorCount> anchors = {{
        {-anchorRadius, -anchorRadius},
        { 0.0,          -anchorRadius},
        { anchorRadius, -anchorRadius},
        { anchorRadius,  0.0},
        { anchorRadius,  anchorRadius},
        { 0.0,           anchorRadius},
        {-anchorRadius,  anchorRadius},
        {-anchorRadius,  0.0}
    }};

    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double totalWeight = 0.0;

    for (int i = 0; i < SampleSectorCount; ++i) {
        if (!samples[i].valid) {
            continue;
        }

        const double dx = normalizedX - anchors[i].x;
        const double dy = normalizedY - anchors[i].y;
        const double distanceSquared = (dx * dx) + (dy * dy);

        // The floor prevents a nearby sample from becoming a hard point source.
        // A fractional falloff gives a broad, photographic gradient rather than
        // eight visible colored lobes.
        const double weight =
            1.0 / std::pow(distanceSquared + 0.32, 1.20);

        red += samples[i].color.red * weight;
        green += samples[i].color.green * weight;
        blue += samples[i].color.blue * weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0) {
        return averageColor;
    }

    const RgbDouble localField {
        red / totalWeight,
        green / totalWeight,
        blue / totalWeight
    };

    const double edgeDistance = std::clamp(
        std::max(std::abs(normalizedX), std::abs(normalizedY)),
        0.0,
        1.0
        );

    // Near the center, use mostly the overall surrounding average. Toward the
    // perimeter, smoothly follow the local side/corner samples. This removes the
    // center-point pyramid while retaining real directional color variation.
    const double localInfluence =
        0.20 + (0.80 * smoothStep(0.05, 0.92, edgeDistance));

    return mixColor(averageColor, localField, localInfluence);
}


double localAverageLuminance(const QImage &image, int centerX, int centerY)
{
    SampleAccumulator accumulator;

    constexpr int radius = 2;

    const int left = std::max(0, centerX - radius);
    const int right = std::min(image.width() - 1, centerX + radius);
    const int top = std::max(0, centerY - radius);
    const int bottom = std::min(image.height() - 1, centerY + radius);

    for (int y = top; y <= bottom; ++y) {
        const QRgb *scanLine =
            reinterpret_cast<const QRgb *>(image.constScanLine(y));

        for (int x = left; x <= right; ++x) {
            const double dx = static_cast<double>(x - centerX);
            const double dy = static_cast<double>(y - centerY);
            const double distanceSquared = (dx * dx) + (dy * dy);
            const double weight = 1.0 / (1.0 + distanceSquared);

            accumulator.add(scanLine[x], weight);
        }
    }

    if (!accumulator.hasSamples()) {
        return luminance(
            reinterpret_cast<const QRgb *>(image.constScanLine(centerY))[centerX]
            );
    }

    const RgbDouble average = accumulator.color();
    return luminance(average.red, average.green, average.blue);
}

}

namespace OverexposureTools
{

QImage recoverOverexposure(const QImage &sourceImage,
                           const QRect &selection,
                           const OverexposureSettings &settings)
{
    if (sourceImage.isNull() || selection.isNull() || selection.isEmpty()) {
        return sourceImage;
    }

    QImage result = toArgb32(sourceImage);

    const QRect imageRect(0, 0, result.width(), result.height());
    const QRect workRect = selection.normalized().intersected(imageRect);

    if (workRect.isEmpty()) {
        return sourceImage;
    }

    const int safeThreshold = std::clamp(settings.threshold, 0, 100);
    const int safeSampleDistance = std::clamp(settings.sampleDistance, 0, 100);
    const int safeSampleWidth = std::clamp(settings.sampleWidth, 0, 100);

    const double recoveryAmount =
        std::clamp(settings.recoveryAmount, 0, 100) / 100.0;

    const double colorBleed =
        std::clamp(settings.colorBleed, 0, 100) / 100.0;

    const double transparency =
        std::clamp(settings.transparency, 0, 100) / 100.0;

    const double preserveShine =
        std::clamp(settings.preserveShine, 0, 100) / 100.0;

    // 75 lands around strong blown highlight territory.
    const double brightCutoff = 160.0 + (safeThreshold * 0.90);

    const int smallestSide = std::max(1, std::min(workRect.width(), workRect.height()));

    const int sampleDistancePixels = std::max(
        2,
        static_cast<int>(std::round(smallestSide * (0.05 + (safeSampleDistance / 100.0 * 0.60))))
        );

    const int sampleWidthPixels = std::max(
        2,
        static_cast<int>(std::round(smallestSide * (0.03 + (safeSampleWidth / 100.0 * 0.35))))
        );

    // Keep a clean source copy for sampling.
    // Do not let pixels already repaired during this pass feed back into later pixels.
    const QImage samplingImage = result;

    const SectorSamples rawSectorSamples = collectSectorSamples(
        samplingImage,
        workRect,
        sampleDistancePixels,
        sampleWidthPixels,
        brightCutoff,
        settings.shape
        );

    const SectorSamples sectorSamples = fillMissingSectorsFromNeighbors(rawSectorSamples);

    OpenClSpatialProcessingBackend::OverexposureSettings gpuSettings;
    gpuSettings.shape =
        settings.shape == RegionShape::Oval
        ? 0
        : 1;
    gpuSettings.softEdge = std::clamp(
        settings.softEdge,
        0,
        100
        );
    gpuSettings.brightCutoff = brightCutoff;
    gpuSettings.recoveryAmount = recoveryAmount;
    gpuSettings.colorBleed = colorBleed;
    gpuSettings.transparency = transparency;
    gpuSettings.preserveShine = preserveShine;

    for (int sampleIndex = 0;
         sampleIndex < SampleSectorCount;
         ++sampleIndex) {
        gpuSettings.sectorSamples[
            static_cast<std::size_t>(sampleIndex)
            ].valid = sectorSamples[sampleIndex].valid;
        gpuSettings.sectorSamples[
            static_cast<std::size_t>(sampleIndex)
            ].red = sectorSamples[sampleIndex].color.red;
        gpuSettings.sectorSamples[
            static_cast<std::size_t>(sampleIndex)
            ].green = sectorSamples[sampleIndex].color.green;
        gpuSettings.sectorSamples[
            static_cast<std::size_t>(sampleIndex)
            ].blue = sectorSamples[sampleIndex].color.blue;
    }

    if (ImageProcessingRuntime::instance()
            .tryApplyOverexposureRecovery(
                result,
                workRect,
                gpuSettings
                )) {
        return result;
    }

    for (int y = workRect.top(); y <= workRect.bottom(); ++y) {
        QRgb *scanLine = reinterpret_cast<QRgb *>(result.scanLine(y));

        for (int x = workRect.left(); x <= workRect.right(); ++x) {
            const double maskWeight = regionWeight(workRect, x, y, settings);

            if (maskWeight <= 0.0) {
                continue;
            }

            const QRgb originalPixel =
                reinterpret_cast<const QRgb *>(samplingImage.constScanLine(y))[x];

            const double oldRed = qRed(originalPixel);
            const double oldGreen = qGreen(originalPixel);
            const double oldBlue = qBlue(originalPixel);
            const int alpha = qAlpha(originalPixel);

            const double oldLuminance = luminance(oldRed, oldGreen, oldBlue);

            const double hotness = smoothStep(
                brightCutoff - 12.0,
                255.0,
                oldLuminance
                );

            if (hotness <= 0.0) {
                continue;
            }

            const double radius = normalizedRegionRadius(
                workRect,
                x,
                y,
                settings.shape
                );

            const double centerWeight = clampUnit(1.0 - radius);

            const double shineProtection =
                preserveShine * centerWeight * hotness;

            const double correctionWeight =
                hotness *
                maskWeight *
                recoveryAmount *
                transparency *
                (1.0 - (shineProtection * 0.55));

            if (correctionWeight <= 0.0) {
                continue;
            }

            const RgbDouble originalColor {
                oldRed,
                oldGreen,
                oldBlue
            };

            const RgbDouble surroundingColor = directionalSurroundingColor(
                sectorSamples,
                workRect,
                x,
                y,
                originalColor
                );

            // First tint the glare with nearby color.
            RgbDouble targetColor = mixColor(
                originalColor,
                surroundingColor,
                colorBleed
                );

            const double surroundingLuminance = luminance(
                surroundingColor.red,
                surroundingColor.green,
                surroundingColor.blue
                );

            // Then pull brightness down toward the sampled area,
            // while still preserving some shine.
            double targetLuminance =
                surroundingLuminance +
                ((oldLuminance - surroundingLuminance) *
                 (0.20 + (shineProtection * 0.50)));

            // Put back a restrained amount of the source's fine luminance detail.
            // This prevents a large repaired glare area from becoming a perfectly
            // flat painted gradient, while clamping extreme blown-pixel noise.
            const double neighborhoodLuminance =
                localAverageLuminance(samplingImage, x, y);

            const double fineDetail = std::clamp(
                oldLuminance - neighborhoodLuminance,
                -18.0,
                18.0
                );

            targetLuminance = std::clamp(
                targetLuminance + (fineDetail * 0.18),
                0.0,
                255.0
                );

            targetColor = shiftToLuminance(targetColor, targetLuminance);

            const RgbDouble finalColor = mixColor(
                originalColor,
                targetColor,
                correctionWeight
                );

            scanLine[x] = qRgba(
                clampChannel(finalColor.red),
                clampChannel(finalColor.green),
                clampChannel(finalColor.blue),
                alpha
                );
        }
    }

    return result;
}

}
