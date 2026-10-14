#ifndef NOISEREDUCTIONDIALOG_H
#define NOISEREDUCTIONDIALOG_H

#include "noise_reduction_tools.h"
#include "toolpreviewwidget.h"

#include <QDialog>
#include <QImage>

#include <functional>

class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

class NoiseReductionDialog : public QDialog
{
    Q_OBJECT

public:
    using PreviewBuilder =
        std::function<
            QImage(const QImage &)
            >;

    explicit NoiseReductionDialog(
        const QImage &sourceBaseImage,
        PreviewBuilder previewBuilder,
        QWidget *parent = nullptr
        );

    QImage resultImage() const;
    bool hasAdjustment() const;

private:
    void setupUi();

    void createControlRow(
        const QString &labelText,
        const QString &toolTip,
        int minimum,
        int maximum,
        int value,
        int doubleClickResetValue,
        QSlider **sliderOut,
        QSpinBox **spinBoxOut
        );

    void connectSliderAndSpinBox(
        QSlider *slider,
        QSpinBox *spinBox
        );

    void setControls(
        int luminanceNoise,
        int colorNoise,
        int detailProtection,
        int blend
        );

    void resetControls();

    void schedulePreviewUpdate();
    void updateResultAndPreview();

    NoiseReductionTools::NoiseReductionSettings
    currentSettings() const;

    QImage buildDisplayImage(
        const QImage &baseImage
        ) const;

    QImage m_sourceBaseImage;
    QImage m_resultBaseImage;

    PreviewBuilder m_previewBuilder;

    ToolPreviewWidget *m_previewWidget = nullptr;

    QSlider *m_luminanceSlider = nullptr;
    QSpinBox *m_luminanceSpinBox = nullptr;

    QSlider *m_colorSlider = nullptr;
    QSpinBox *m_colorSpinBox = nullptr;

    QSlider *m_detailSlider = nullptr;
    QSpinBox *m_detailSpinBox = nullptr;

    QSlider *m_blendSlider = nullptr;
    QSpinBox *m_blendSpinBox = nullptr;

    QLabel *m_renderStatusLabel = nullptr;

    QPushButton *m_resetButton = nullptr;
    QPushButton *m_showOriginalButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_finishButton = nullptr;

    QTimer *m_previewTimer = nullptr;
    bool m_previewPending = false;
};

#endif // NOISEREDUCTIONDIALOG_H
