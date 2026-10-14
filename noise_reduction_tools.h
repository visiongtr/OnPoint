#ifndef NOISE_REDUCTION_TOOLS_H
#define NOISE_REDUCTION_TOOLS_H

#include <QImage>

namespace NoiseReductionTools
{

struct NoiseReductionSettings
{
    // 0 = unchanged, 100 = strongest available correction.
    int luminanceNoise = 28;
    int colorNoise = 45;

    // 0 = smooth more freely, 100 = strongly protect edges and texture.
    int detailProtection = 78;

    // Final blend with the untouched source.
    int blend = 85;
};

bool hasVisibleAdjustment(
    const NoiseReductionSettings &settings
    );

QImage applyNoiseReduction(
    const QImage &sourceImage,
    const NoiseReductionSettings &settings
    );

} // namespace NoiseReductionTools

#endif // NOISE_REDUCTION_TOOLS_H
