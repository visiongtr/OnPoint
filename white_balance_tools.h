#ifndef WHITE_BALANCE_TOOLS_H
#define WHITE_BALANCE_TOOLS_H

#include <QColor>
#include <QImage>
#include <QPoint>

namespace WhiteBalanceTools
{

struct ChannelGains
{
    double red = 1.0;
    double green = 1.0;
    double blue = 1.0;
    bool valid = false;
};

struct WhiteBalanceSettings
{
    ChannelGains baseGains;

    // -100 = cooler, +100 = warmer
    int temperature = 0;

    // -100 = greener, +100 = more magenta
    int tint = 0;

    // 0 = unchanged, 100 = full correction
    int strength = 85;
};

QColor sampleAverageColor(
    const QImage &sourceImage,
    const QPoint &centerPoint,
    int radius = 3
    );

ChannelGains gainsFromNeutralColor(
    const QColor &neutralColor
    );

ChannelGains automaticGains(
    const QImage &sourceImage
    );

bool hasVisibleAdjustment(
    const WhiteBalanceSettings &settings
    );

QImage applyWhiteBalance(
    const QImage &sourceImage,
    const WhiteBalanceSettings &settings
    );

} // namespace WhiteBalanceTools

#endif // WHITE_BALANCE_TOOLS_H
