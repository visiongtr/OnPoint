#ifndef PRINTSERVICE_H
#define PRINTSERVICE_H

#include <QImage>
#include <QMarginsF>
#include <QRectF>
#include <QString>

class QPainter;
class QPrinter;

namespace OnPointPrint
{

enum class ScaleMode
{
    FitToPrintableArea,
    ActualSize
};

struct LayoutSettings
{
    ScaleMode scaleMode = ScaleMode::FitToPrintableArea;

    bool preserveAspectRatio = true;
    bool centerImage = true;
    bool useMargins = true;
    bool showMarginGuide = true;

    // User margins are measured from the physical paper edge.
    // The printer driver's minimum printable margins are always respected.
    QMarginsF marginsMillimeters = QMarginsF(6.35, 6.35, 6.35, 6.35);

    // Manual placement is stored as a normalized position inside the
    // available travel range. 0 = left/top, 0.5 = center, 1 = right/bottom.
    double horizontalPosition = 0.5;
    double verticalPosition = 0.5;
};

struct LayoutResult
{
    QRectF paperRectPoints;
    QRectF printerPrintableRectPoints;
    QRectF contentRectPoints;
    QRectF imageRectPoints;

    QMarginsF effectiveMarginsMillimeters;

    double actualSizeDpi = 0.0;
    bool usedFallbackDpi = false;
    bool imageWillBeClipped = false;
};

LayoutResult calculateLayout(
    const QPrinter &printer,
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    const LayoutSettings &settings
    );

bool renderImage(
    QPrinter &printer,
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    const LayoutSettings &settings,
    QString *errorMessage = nullptr,
    const LayoutResult *layoutOverride = nullptr
    );

} // namespace OnPointPrint

#endif // PRINTSERVICE_H
