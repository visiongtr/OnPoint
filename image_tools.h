#ifndef IMAGE_TOOLS_H
#define IMAGE_TOOLS_H

#include <QImage>

#include <array>

namespace ImageTools
{
QImage applyMasterShift(const QImage &sourceImage, int amount);

QImage applyRedShift(const QImage &sourceImage, int amount);
QImage applyGreenShift(const QImage &sourceImage, int amount);
QImage applyBlueShift(const QImage &sourceImage, int amount);

QImage applyRgbShift(
    const QImage &sourceImage,
    int redAmount,
    int greenAmount,
    int blueAmount
    );

QImage applyBrightness(const QImage &sourceImage, int amount);
QImage applyContrast(const QImage &sourceImage, int amount);
QImage applyGamma(const QImage &sourceImage, int amount);

QImage applyBasicTone(
    const QImage &sourceImage,
    int brightnessAmount,
    int contrastAmount,
    int gammaAmount
    );

QImage applyMainAdjustments(
    const QImage &sourceImage,
    int brightnessAmount,
    int contrastAmount,
    int gammaAmount,
    int saturationAmount,

    int baseHighlightAmount,
    int baseMidtoneAmount,
    int baseShadowAmount,

    int cyanRedHighlightAmount,
    int cyanRedMidtoneAmount,
    int cyanRedShadowAmount,

    int magentaGreenHighlightAmount,
    int magentaGreenMidtoneAmount,
    int magentaGreenShadowAmount,

    int yellowBlueHighlightAmount,
    int yellowBlueMidtoneAmount,
    int yellowBlueShadowAmount
    );

QImage applySaturation(const QImage &source, int amount);
QImage applyHueTone(
    const QImage &source,
    int colorIndex,
    int amount
    );

QImage applyHueToneMix(
    const QImage &source,
    const std::array<int, 8> &amounts
    );

QImage applyBaseHms(
    const QImage &sourceImage,
    int highlightAmount,
    int midtoneAmount,
    int shadowAmount
    );

QImage applyColorHms(
    const QImage &sourceImage,

    int cyanRedHighlightAmount,
    int cyanRedMidtoneAmount,
    int cyanRedShadowAmount,

    int magentaGreenHighlightAmount,
    int magentaGreenMidtoneAmount,
    int magentaGreenShadowAmount,

    int yellowBlueHighlightAmount,
    int yellowBlueMidtoneAmount,
    int yellowBlueShadowAmount
    );

}

#endif // IMAGE_TOOLS_H
