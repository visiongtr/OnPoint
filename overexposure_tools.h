#ifndef OVEREXPOSURE_TOOLS_H
#define OVEREXPOSURE_TOOLS_H

#include <QImage>
#include <QRect>

namespace OverexposureTools
{

enum class RegionShape
{
    Oval,
    Rectangle,
    Square
};

struct OverexposureSettings
{
    RegionShape shape = RegionShape::Oval;

    int threshold = 75;
    int sampleDistance = 30;
    int sampleWidth = 20;

    int recoveryAmount = 45;
    int colorBleed = 55;
    int transparency = 75;
    int softEdge = 55;
    int preserveShine = 35;
};

QImage recoverOverexposure(const QImage &sourceImage,
                           const QRect &selection,
                           const OverexposureSettings &settings);

}

#endif // OVEREXPOSURE_TOOLS_H
