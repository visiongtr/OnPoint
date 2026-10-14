#ifndef FOCUS_TOOLS_H
#define FOCUS_TOOLS_H

#include <QImage>

namespace FocusTools
{

struct FocusSettings
{
    // 0 = unchanged, 100 = strongest available correction.
    int amount = 18;

    // 1..50 maps from fine-detail sharpening to broader edge recovery.
    int radius = 10;

    // Higher values ignore more grain and tiny texture.
    int threshold = 24;

    // Higher values reduce halos around strong contrast edges.
    int edgeProtection = 72;

    // Final blend with the untouched source.
    int blend = 85;
};

bool hasVisibleAdjustment(
    const FocusSettings &settings
    );

QImage applyFocus(
    const QImage &sourceImage,
    const FocusSettings &settings
    );

} // namespace FocusTools

#endif // FOCUS_TOOLS_H
