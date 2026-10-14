#ifndef RED_EYE_TOOLS_H
#define RED_EYE_TOOLS_H

#include <QColor>
#include <QImage>
#include <QRect>

namespace RedEyeTools
{

enum class RedEyeMode
{
    Auto,
    Manual
};

enum class IrisColor
{
    Auto,
    Brown,
    Hazel,
    Green,
    Blue,
    Gray
};

struct RedEyeSettings
{
    RedEyeMode mode = RedEyeMode::Auto;
    IrisColor irisColor = IrisColor::Auto;

    // Optional target sampled directly from the unwanted eye glow.
    // When false, the original automatic red-eye detector is used.
    bool useSampledTarget = false;
    QColor sampledTargetColor = QColor(205, 45, 38);

    int tolerance = 50;
    int strength = 85;
    int pupilDarken = 55;
    int irisAmount = 20;
    int blend = 85;
    int softEdge = 55;
};

QImage repairRedEye(const QImage &source,
                    const QRect &selection,
                    const RedEyeSettings &settings);

} // namespace RedEyeTools

#endif // RED_EYE_TOOLS_H
