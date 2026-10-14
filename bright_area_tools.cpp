#include "bright_area_tools.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

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

double cutoffFromThreshold(int threshold)
{
    // Higher threshold = stricter bright-area selection.
    // 75 lands around strong blown-window territory.
    return 120.0 + (std::clamp(threshold, 0, 100) * 1.25);
}

QImage toArgb32(const QImage &sourceImage)
{
    if (sourceImage.format() == QImage::Format_ARGB32 ||
        sourceImage.format() == QImage::Format_RGB32) {
        return sourceImage;
    }

    return sourceImage.convertToFormat(QImage::Format_ARGB32);
}

QImage emptyMaskForImage(const QImage &sourceImage)
{
    QImage mask(sourceImage.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    return mask;
}

QImage ensureGrayMask(const QImage &mask)
{
    if (mask.isNull()) {
        return mask;
    }

    if (mask.format() == QImage::Format_Grayscale8) {
        return mask;
    }

    return mask.convertToFormat(QImage::Format_Grayscale8);
}

int maskValueAt(const QImage &mask, int x, int y)
{
    if (mask.isNull() ||
        x < 0 ||
        y < 0 ||
        x >= mask.width() ||
        y >= mask.height()) {
        return 0;
    }

    return mask.constScanLine(y)[x];
}

QImage dilateMask(const QImage &mask, int radius)
{
    if (mask.isNull() || radius <= 0) {
        return mask;
    }

    QImage sourceMask = ensureGrayMask(mask);
    QImage resultMask(sourceMask.size(), QImage::Format_Grayscale8);
    resultMask.fill(0);

    const int width = sourceMask.width();
    const int height = sourceMask.height();

    for (int y = 0; y < height; ++y) {
        uchar *resultLine = resultMask.scanLine(y);

        for (int x = 0; x < width; ++x) {
            int bestValue = 0;

            const int left = std::max(0, x - radius);
            const int right = std::min(width - 1, x + radius);
            const int top = std::max(0, y - radius);
            const int bottom = std::min(height - 1, y + radius);

            for (int sampleY = top; sampleY <= bottom; ++sampleY) {
                const uchar *sourceLine = sourceMask.constScanLine(sampleY);

                for (int sampleX = left; sampleX <= right; ++sampleX) {
                    bestValue = std::max(bestValue, static_cast<int>(sourceLine[sampleX]));
                }
            }

            resultLine[x] = static_cast<uchar>(bestValue);
        }
    }

    return resultMask;
}

QImage erodeMask(const QImage &mask, int radius)
{
    if (mask.isNull() || radius <= 0) {
        return mask;
    }

    QImage sourceMask = ensureGrayMask(mask);
    QImage resultMask(sourceMask.size(), QImage::Format_Grayscale8);
    resultMask.fill(0);

    const int width = sourceMask.width();
    const int height = sourceMask.height();

    for (int y = 0; y < height; ++y) {
        uchar *resultLine = resultMask.scanLine(y);

        for (int x = 0; x < width; ++x) {
            int bestValue = 255;

            const int left = std::max(0, x - radius);
            const int right = std::min(width - 1, x + radius);
            const int top = std::max(0, y - radius);
            const int bottom = std::min(height - 1, y + radius);

            for (int sampleY = top; sampleY <= bottom; ++sampleY) {
                const uchar *sourceLine = sourceMask.constScanLine(sampleY);

                for (int sampleX = left; sampleX <= right; ++sampleX) {
                    bestValue = std::min(bestValue, static_cast<int>(sourceLine[sampleX]));
                }
            }

            resultLine[x] = static_cast<uchar>(bestValue);
        }
    }

    return resultMask;
}

QImage boxBlurMask(const QImage &mask, int radius)
{
    if (mask.isNull() || radius <= 0) {
        return mask;
    }

    QImage sourceMask = ensureGrayMask(mask);

    const int width = sourceMask.width();
    const int height = sourceMask.height();

    std::vector<int> integral(static_cast<size_t>((width + 1) * (height + 1)), 0);

    auto integralAt = [&](int x, int y) -> int& {
        return integral[static_cast<size_t>(y * (width + 1) + x)];
    };

    for (int y = 1; y <= height; ++y) {
        int rowSum = 0;
        const uchar *sourceLine = sourceMask.constScanLine(y - 1);

        for (int x = 1; x <= width; ++x) {
            rowSum += sourceLine[x - 1];
            integralAt(x, y) = integralAt(x, y - 1) + rowSum;
        }
    }

    QImage resultMask(sourceMask.size(), QImage::Format_Grayscale8);
    resultMask.fill(0);

    for (int y = 0; y < height; ++y) {
        uchar *resultLine = resultMask.scanLine(y);

        for (int x = 0; x < width; ++x) {
            const int left = std::max(0, x - radius);
            const int right = std::min(width - 1, x + radius);
            const int top = std::max(0, y - radius);
            const int bottom = std::min(height - 1, y + radius);

            const int x1 = left;
            const int y1 = top;
            const int x2 = right + 1;
            const int y2 = bottom + 1;

            const int sum =
                integralAt(x2, y2) -
                integralAt(x1, y2) -
                integralAt(x2, y1) +
                integralAt(x1, y1);

            const int area = std::max(1, (x2 - x1) * (y2 - y1));
            resultLine[x] = static_cast<uchar>(std::clamp(sum / area, 0, 255));
        }
    }

    return resultMask;
}

struct MaskBorderContact
{
    bool left = false;
    bool right = false;
    bool top = false;
    bool bottom = false;
};

MaskBorderContact detectMaskBorderContact(const QImage &mask)
{
    MaskBorderContact contact;

    if (mask.isNull()) {
        return contact;
    }

    const QImage grayMask = ensureGrayMask(mask);
    const int width = grayMask.width();
    const int height = grayMask.height();

    if (width <= 0 || height <= 0) {
        return contact;
    }

    for (int y = 0; y < height; ++y) {
        const uchar *line = grayMask.constScanLine(y);

        if (line[0] > 0) {
            contact.left = true;
        }

        if (line[width - 1] > 0) {
            contact.right = true;
        }
    }

    const uchar *topLine = grayMask.constScanLine(0);
    const uchar *bottomLine = grayMask.constScanLine(height - 1);

    for (int x = 0; x < width; ++x) {
        if (topLine[x] > 0) {
            contact.top = true;
        }

        if (bottomLine[x] > 0) {
            contact.bottom = true;
        }
    }

    return contact;
}

QImage padMaskForBorderAwareSoftEdge(const QImage &mask,
                                     int radius,
                                     const MaskBorderContact &contact)
{
    if (mask.isNull() || radius <= 0) {
        return ensureGrayMask(mask);
    }

    const QImage sourceMask = ensureGrayMask(mask);
    const int width = sourceMask.width();
    const int height = sourceMask.height();

    QImage paddedMask(QSize(width + radius * 2, height + radius * 2),
                      QImage::Format_Grayscale8);
    paddedMask.fill(0);

    // Copy original mask into center
    for (int y = 0; y < height; ++y) {
        const uchar *sourceLine = sourceMask.constScanLine(y);
        uchar *destLine = paddedMask.scanLine(y + radius) + radius;
        std::copy(sourceLine, sourceLine + width, destLine);
    }

    // Replicate left edge if touching left border
    if (contact.left) {
        for (int y = 0; y < height; ++y) {
            const uchar edgeValue = sourceMask.constScanLine(y)[0];
            uchar *destLine = paddedMask.scanLine(y + radius);
            std::fill(destLine, destLine + radius, edgeValue);
        }
    }

    // Replicate right edge if touching right border
    if (contact.right) {
        for (int y = 0; y < height; ++y) {
            const uchar edgeValue = sourceMask.constScanLine(y)[width - 1];
            uchar *destLine = paddedMask.scanLine(y + radius) + radius + width;
            std::fill(destLine, destLine + radius, edgeValue);
        }
    }

    // Replicate top edge if touching top border
    if (contact.top) {
        const uchar *sourceTop = sourceMask.constScanLine(0);

        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(y) + radius;
            std::copy(sourceTop, sourceTop + width, destLine);
        }
    }

    // Replicate bottom edge if touching bottom border
    if (contact.bottom) {
        const uchar *sourceBottom = sourceMask.constScanLine(height - 1);

        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(radius + height + y) + radius;
            std::copy(sourceBottom, sourceBottom + width, destLine);
        }
    }

    // Fill corners if needed
    if (contact.top && contact.left) {
        const uchar corner = sourceMask.constScanLine(0)[0];
        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(y);
            std::fill(destLine, destLine + radius, corner);
        }
    }

    if (contact.top && contact.right) {
        const uchar corner = sourceMask.constScanLine(0)[width - 1];
        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(y) + radius + width;
            std::fill(destLine, destLine + radius, corner);
        }
    }

    if (contact.bottom && contact.left) {
        const uchar corner = sourceMask.constScanLine(height - 1)[0];
        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(radius + height + y);
            std::fill(destLine, destLine + radius, corner);
        }
    }

    if (contact.bottom && contact.right) {
        const uchar corner = sourceMask.constScanLine(height - 1)[width - 1];
        for (int y = 0; y < radius; ++y) {
            uchar *destLine = paddedMask.scanLine(radius + height + y) + radius + width;
            std::fill(destLine, destLine + radius, corner);
        }
    }

    return paddedMask;
}

QImage cropPaddedMask(const QImage &mask, int radius, const QSize &targetSize)
{
    if (mask.isNull() || radius <= 0) {
        return mask;
    }

    return mask.copy(radius, radius, targetSize.width(), targetSize.height());
}

struct RgbDouble
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
};

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

RgbDouble mixColor(const RgbDouble &a, const RgbDouble &b, double amount)
{
    const double t = clampUnit(amount);

    return {
        a.red + ((b.red - a.red) * t),
        a.green + ((b.green - a.green) * t),
        a.blue + ((b.blue - a.blue) * t)
    };
}

}

namespace BrightAreaTools
{

bool maskHasSelection(const QImage &mask)
{
    if (mask.isNull()) {
        return false;
    }

    const QImage grayMask = ensureGrayMask(mask);

    for (int y = 0; y < grayMask.height(); ++y) {
        const uchar *line = grayMask.constScanLine(y);

        for (int x = 0; x < grayMask.width(); ++x) {
            if (line[x] > 0) {
                return true;
            }
        }
    }

    return false;
}

QImage createBrightAreaMask(const QImage &sourceImage,
                            const QPoint &seedPoint,
                            int threshold)
{
    if (sourceImage.isNull()) {
        return QImage();
    }

    QImage source = toArgb32(sourceImage);
    QImage mask = emptyMaskForImage(source);

    if (!QRect(0, 0, source.width(), source.height()).contains(seedPoint)) {
        return mask;
    }

    const double cutoff = cutoffFromThreshold(threshold);

    QPoint startPoint = seedPoint;
    double startLuminance =
        luminance(reinterpret_cast<const QRgb *>(source.constScanLine(startPoint.y()))[startPoint.x()]);

    if (startLuminance < cutoff) {
        const int searchRadius = 18;
        double bestLuminance = startLuminance;
        QPoint bestPoint = startPoint;

        for (int y = std::max(0, seedPoint.y() - searchRadius);
             y <= std::min(source.height() - 1, seedPoint.y() + searchRadius);
             ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(source.constScanLine(y));

            for (int x = std::max(0, seedPoint.x() - searchRadius);
                 x <= std::min(source.width() - 1, seedPoint.x() + searchRadius);
                 ++x) {
                const double pixelLuminance = luminance(line[x]);

                if (pixelLuminance > bestLuminance) {
                    bestLuminance = pixelLuminance;
                    bestPoint = QPoint(x, y);
                }
            }
        }

        if (bestLuminance < cutoff) {
            return mask;
        }

        startPoint = bestPoint;
    }

    const int width = source.width();
    const int height = source.height();

    std::vector<uchar> visited(static_cast<size_t>(width * height), 0);
    std::vector<QPoint> stack;
    stack.reserve(4096);

    auto visitedAt = [&](int x, int y) -> uchar& {
        return visited[static_cast<size_t>(y * width + x)];
    };

    stack.push_back(startPoint);
    visitedAt(startPoint.x(), startPoint.y()) = 1;

    while (!stack.empty()) {
        const QPoint point = stack.back();
        stack.pop_back();

        const int x = point.x();
        const int y = point.y();

        const QRgb pixel =
            reinterpret_cast<const QRgb *>(source.constScanLine(y))[x];

        if (luminance(pixel) < cutoff) {
            continue;
        }

        mask.scanLine(y)[x] = 255;

        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                if (offsetX == 0 && offsetY == 0) {
                    continue;
                }

                const int nextX = x + offsetX;
                const int nextY = y + offsetY;

                if (nextX < 0 ||
                    nextY < 0 ||
                    nextX >= width ||
                    nextY >= height) {
                    continue;
                }

                if (visitedAt(nextX, nextY)) {
                    continue;
                }

                visitedAt(nextX, nextY) = 1;
                stack.emplace_back(nextX, nextY);
            }
        }
    }

    return mask;
}

QImage applyBrushToMask(const QImage &mask,
                        const QPoint &centerPoint,
                        int brushSize,
                        int brushSoftness,
                        int brushStrength,
                        bool addToSelection)
{
    if (mask.isNull()) {
        return mask;
    }

    QImage result = ensureGrayMask(mask);

    const int radius = std::max(1, brushSize / 2);
    const double softness = std::clamp(brushSoftness, 0, 100) / 100.0;
    const double strength = std::clamp(brushStrength, 0, 100) / 100.0;
    const double hardRadius = radius * (1.0 - (softness * 0.80));

    const int left = std::max(0, centerPoint.x() - radius);
    const int right = std::min(result.width() - 1, centerPoint.x() + radius);
    const int top = std::max(0, centerPoint.y() - radius);
    const int bottom = std::min(result.height() - 1, centerPoint.y() + radius);

    for (int y = top; y <= bottom; ++y) {
        uchar *line = result.scanLine(y);

        for (int x = left; x <= right; ++x) {
            const double dx = x - centerPoint.x();
            const double dy = y - centerPoint.y();
            const double distance = std::sqrt((dx * dx) + (dy * dy));

            if (distance > radius) {
                continue;
            }

            const double brushWeight =
                (1.0 - smoothStep(hardRadius, radius, distance)) * strength;

            const int currentValue = line[x];

            if (addToSelection) {
                const int addedValue = static_cast<int>(std::round(255.0 * brushWeight));
                line[x] = static_cast<uchar>(std::max(currentValue, addedValue));
            } else {
                const int newValue =
                    static_cast<int>(std::round(currentValue * (1.0 - brushWeight)));

                line[x] = static_cast<uchar>(std::clamp(newValue, 0, 255));
            }
        }
    }

    return result;
}

QImage prepareMask(const QImage &mask,
                   int growShrink,
                   int softEdge)
{
    if (mask.isNull()) {
        return mask;
    }

    QImage preparedMask = ensureGrayMask(mask);

    const int growShrinkRadius =
        static_cast<int>(std::round(std::abs(std::clamp(growShrink, -100, 100)) * 0.14));

    if (growShrink > 0) {
        preparedMask = dilateMask(preparedMask, growShrinkRadius);
    } else if (growShrink < 0) {
        preparedMask = erodeMask(preparedMask, growShrinkRadius);
    }

    const int featherRadius =
        static_cast<int>(std::round(std::clamp(softEdge, 0, 100) * 0.25));

    if (featherRadius > 0) {
        const MaskBorderContact contact = detectMaskBorderContact(preparedMask);

        const QImage paddedMask = padMaskForBorderAwareSoftEdge(preparedMask,
                                                                featherRadius,
                                                                contact);

        const QImage blurredMask = boxBlurMask(paddedMask, featherRadius);

        preparedMask = cropPaddedMask(blurredMask,
                                      featherRadius,
                                      preparedMask.size());
    }

    return preparedMask;
}

QImage applyBrightAreaFade(const QImage &sourceImage,
                           const QImage &mask,
                           const BrightAreaSettings &settings)
{
    if (sourceImage.isNull() || mask.isNull() || !maskHasSelection(mask)) {
        return sourceImage;
    }

    QImage result = toArgb32(sourceImage);
    QImage finalMask = prepareMask(mask, settings.growShrink, settings.softEdge);

    if (finalMask.size() != result.size()) {
        return sourceImage;
    }

    const double recoveryAmount =
        std::clamp(settings.recoveryAmount, 0, 100) / 100.0;

    const double blend =
        std::clamp(settings.blend, 0, 100) / 100.0;

    const double preserveGlow =
        std::clamp(settings.preserveGlow, 0, 100) / 100.0;

    const double tintAmount =
        std::clamp(settings.tintAmount, 0, 100) / 100.0;

    const RgbDouble tintColor {
        static_cast<double>(settings.tintColor.red()),
        static_cast<double>(settings.tintColor.green()),
        static_cast<double>(settings.tintColor.blue())
    };

    for (int y = 0; y < result.height(); ++y) {
        QRgb *resultLine = reinterpret_cast<QRgb *>(result.scanLine(y));
        const uchar *maskLine = finalMask.constScanLine(y);

        for (int x = 0; x < result.width(); ++x) {
            const double maskWeight = maskLine[x] / 255.0;

            if (maskWeight <= 0.0) {
                continue;
            }

            const QRgb originalPixel = resultLine[x];

            const RgbDouble originalColor {
                static_cast<double>(qRed(originalPixel)),
                static_cast<double>(qGreen(originalPixel)),
                static_cast<double>(qBlue(originalPixel))
            };

            const double originalLuminance =
                luminance(originalColor.red, originalColor.green, originalColor.blue);

            // Keep it bright enough to still read as light.
            // The preserve setting protects against gray/muddy blown areas.
            const double targetFloor =
                135.0 + (preserveGlow * 75.0);

            const double pullableRange =
                std::max(0.0, originalLuminance - targetFloor);

            const double targetLuminance =
                originalLuminance - (pullableRange * recoveryAmount);

            RgbDouble targetColor =
                shiftToLuminance(originalColor, targetLuminance);

            if (tintAmount > 0.0) {
                RgbDouble tintedColor = mixColor(targetColor, tintColor, tintAmount);
                tintedColor = shiftToLuminance(tintedColor, targetLuminance);
                targetColor = tintedColor;
            }

            const double correctionWeight = maskWeight * blend;

            const RgbDouble finalColor =
                mixColor(originalColor, targetColor, correctionWeight);

            resultLine[x] = qRgba(
                clampChannel(finalColor.red),
                clampChannel(finalColor.green),
                clampChannel(finalColor.blue),
                qAlpha(originalPixel)
                );
        }
    }

    return result;
}

}
