#include "printservice.h"

#include <QPageLayout>
#include <QPainter>
#include <QPrinter>

#include <algorithm>
#include <cmath>

namespace {

constexpr double PointsPerInch = 72.0;
constexpr double MillimetersPerInch = 25.4;
constexpr double FallbackActualSizeDpi = 300.0;

QMarginsF millimetersToPoints(const QMarginsF &margins)
{
    const double scale = PointsPerInch / MillimetersPerInch;

    return QMarginsF(
        margins.left() * scale,
        margins.top() * scale,
        margins.right() * scale,
        margins.bottom() * scale
        );
}

QMarginsF minimumPrinterMarginsMillimeters(const QPrinter &printer)
{
    QPageLayout layout = printer.pageLayout();
    layout.setUnits(QPageLayout::Millimeter);
    return layout.minimumMargins();
}

QMarginsF effectiveMarginsMillimeters(
    const QPrinter &printer,
    const OnPointPrint::LayoutSettings &settings
    )
{
    const QMarginsF minimumMargins =
        minimumPrinterMarginsMillimeters(printer);

    if (!settings.useMargins) {
        return minimumMargins;
    }

    return QMarginsF(
        std::max(settings.marginsMillimeters.left(), minimumMargins.left()),
        std::max(settings.marginsMillimeters.top(), minimumMargins.top()),
        std::max(settings.marginsMillimeters.right(), minimumMargins.right()),
        std::max(settings.marginsMillimeters.bottom(), minimumMargins.bottom())
        );
}

double dotsPerMeterToDpi(int dotsPerMeter)
{
    if (dotsPerMeter <= 0) {
        return 0.0;
    }

    return static_cast<double>(dotsPerMeter) * 0.0254;
}

double resolvedActualSizeDpi(
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    bool &usedFallback
    )
{
    const int currentDotsPerMeterX = image.dotsPerMeterX();
    const int currentDotsPerMeterY = image.dotsPerMeterY();

    const double candidates[] = {
        dotsPerMeterToDpi(currentDotsPerMeterX),
        dotsPerMeterToDpi(currentDotsPerMeterY),
        dotsPerMeterToDpi(sourceDotsPerMeterX),
        dotsPerMeterToDpi(sourceDotsPerMeterY)
    };

    for (double candidate : candidates) {
        if (candidate >= 1.0) {
            usedFallback = false;
            return candidate;
        }
    }

    usedFallback = true;
    return FallbackActualSizeDpi;
}

QSizeF fittedSize(
    const QSize &imageSize,
    const QSizeF &targetSize,
    bool preserveAspectRatio
    )
{
    if (imageSize.isEmpty() ||
        targetSize.width() <= 0.0 ||
        targetSize.height() <= 0.0) {
        return QSizeF();
    }

    if (!preserveAspectRatio) {
        return targetSize;
    }

    const double scale = std::min(
        targetSize.width() / static_cast<double>(imageSize.width()),
        targetSize.height() / static_cast<double>(imageSize.height())
        );

    return QSizeF(
        static_cast<double>(imageSize.width()) * scale,
        static_cast<double>(imageSize.height()) * scale
        );
}

double positionedCoordinate(
    double contentStart,
    double contentLength,
    double imageLength,
    double normalizedPosition,
    bool centerImage
    )
{
    const double travel = contentLength - imageLength;

    if (centerImage) {
        return contentStart + travel * 0.5;
    }

    const double clampedPosition =
        std::clamp(normalizedPosition, 0.0, 1.0);

    return contentStart + travel * clampedPosition;
}

} // namespace

namespace OnPointPrint
{

LayoutResult calculateLayout(
    const QPrinter &printer,
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    const LayoutSettings &settings
    )
{
    LayoutResult result;

    const QPageLayout pageLayout = printer.pageLayout();

    result.paperRectPoints =
        pageLayout.fullRect(QPageLayout::Point);

    const QMarginsF minimumMarginsMm =
        minimumPrinterMarginsMillimeters(printer);

    const QMarginsF minimumMarginsPoints =
        millimetersToPoints(minimumMarginsMm);

    result.printerPrintableRectPoints =
        result.paperRectPoints.adjusted(
            minimumMarginsPoints.left(),
            minimumMarginsPoints.top(),
            -minimumMarginsPoints.right(),
            -minimumMarginsPoints.bottom()
            );

    result.effectiveMarginsMillimeters =
        effectiveMarginsMillimeters(
            printer,
            settings
            );

    const QMarginsF effectiveMarginsPoints =
        millimetersToPoints(
            result.effectiveMarginsMillimeters
            );

    result.contentRectPoints =
        result.paperRectPoints.adjusted(
            effectiveMarginsPoints.left(),
            effectiveMarginsPoints.top(),
            -effectiveMarginsPoints.right(),
            -effectiveMarginsPoints.bottom()
            );

    result.contentRectPoints =
        result.contentRectPoints.intersected(
            result.printerPrintableRectPoints
            );

    if (image.isNull() ||
        result.contentRectPoints.width() <= 0.0 ||
        result.contentRectPoints.height() <= 0.0) {
        return result;
    }

    QSizeF imageSizePoints;

    if (settings.scaleMode == ScaleMode::ActualSize) {
        result.actualSizeDpi =
            resolvedActualSizeDpi(
                image,
                sourceDotsPerMeterX,
                sourceDotsPerMeterY,
                result.usedFallbackDpi
                );

        imageSizePoints = QSizeF(
            static_cast<double>(image.width()) /
                result.actualSizeDpi * PointsPerInch,
            static_cast<double>(image.height()) /
                result.actualSizeDpi * PointsPerInch
            );
    } else {
        imageSizePoints =
            fittedSize(
                image.size(),
                result.contentRectPoints.size(),
                settings.preserveAspectRatio
                );
    }

    if (settings.scaleMode == ScaleMode::ActualSize &&
        !settings.preserveAspectRatio) {
        // Actual-size printing has a real physical pixel-to-inch meaning.
        // Keep that meaning and never stretch the image merely because the
        // general Preserve Aspect Ratio control was turned off.
        imageSizePoints = QSizeF(
            static_cast<double>(image.width()) /
                result.actualSizeDpi * PointsPerInch,
            static_cast<double>(image.height()) /
                result.actualSizeDpi * PointsPerInch
            );
    }

    const double imageX =
        positionedCoordinate(
            result.contentRectPoints.left(),
            result.contentRectPoints.width(),
            imageSizePoints.width(),
            settings.horizontalPosition,
            settings.centerImage
            );

    const double imageY =
        positionedCoordinate(
            result.contentRectPoints.top(),
            result.contentRectPoints.height(),
            imageSizePoints.height(),
            settings.verticalPosition,
            settings.centerImage
            );

    result.imageRectPoints =
        QRectF(
            QPointF(imageX, imageY),
            imageSizePoints
            );

    result.imageWillBeClipped =
        !result.printerPrintableRectPoints.contains(
            result.imageRectPoints
            );

    return result;
}

bool renderImage(
    QPrinter &printer,
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    const LayoutSettings &settings,
    QString *errorMessage,
    const LayoutResult *layoutOverride
    )
{
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = "There is no image to print.";
        }

        return false;
    }

    printer.setFullPage(true);

    const LayoutResult layout =
        layoutOverride
            ? *layoutOverride
            : calculateLayout(
                  printer,
                  image,
                  sourceDotsPerMeterX,
                  sourceDotsPerMeterY,
                  settings
                  );

    if (layout.imageRectPoints.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "The selected paper and margin settings leave no printable area.";
        }

        return false;
    }

    QPainter painter;

    if (!painter.begin(&printer)) {
        if (errorMessage) {
            *errorMessage = "The printer could not start a paint session.";
        }

        return false;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const double devicePixelsPerPoint =
        static_cast<double>(printer.resolution()) /
        PointsPerInch;

    const QRectF printableRectPixels(
        layout.printerPrintableRectPoints.x() * devicePixelsPerPoint,
        layout.printerPrintableRectPoints.y() * devicePixelsPerPoint,
        layout.printerPrintableRectPoints.width() * devicePixelsPerPoint,
        layout.printerPrintableRectPoints.height() * devicePixelsPerPoint
        );

    const QRectF imageRectPixels(
        layout.imageRectPoints.x() * devicePixelsPerPoint,
        layout.imageRectPoints.y() * devicePixelsPerPoint,
        layout.imageRectPoints.width() * devicePixelsPerPoint,
        layout.imageRectPoints.height() * devicePixelsPerPoint
        );

    painter.setClipRect(printableRectPixels);
    painter.drawImage(imageRectPixels, image);
    painter.end();

    return true;
}

} // namespace OnPointPrint
