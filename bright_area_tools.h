#ifndef BRIGHT_AREA_TOOLS_H
#define BRIGHT_AREA_TOOLS_H

#include <QColor>
#include <QImage>
#include <QPoint>

namespace BrightAreaTools
{

struct BrightAreaSettings
{
    int threshold = 75;
    int growShrink = 0;

    int recoveryAmount = 45;
    int blend = 75;
    int softEdge = 55;
    int preserveGlow = 45;

    int tintAmount = 0;
    QColor tintColor = QColor(135, 190, 255);
};

QImage createBrightAreaMask(const QImage &sourceImage,
                            const QPoint &seedPoint,
                            int threshold);

QImage applyBrushToMask(const QImage &mask,
                        const QPoint &centerPoint,
                        int brushSize,
                        int brushSoftness,
                        int brushStrength,
                        bool addToSelection);

QImage prepareMask(const QImage &mask,
                   int growShrink,
                   int softEdge);

QImage applyBrightAreaFade(const QImage &sourceImage,
                           const QImage &mask,
                           const BrightAreaSettings &settings);

bool maskHasSelection(const QImage &mask);

}

#endif // BRIGHT_AREA_TOOLS_H
