#ifndef FOCUSDIALOG_H
#define FOCUSDIALOG_H

#include "focus_tools.h"
#include "toolpreviewwidget.h"

#include <QDialog>
#include <QImage>

#include <functional>

class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

class FocusDialog : public QDialog
{
    Q_OBJECT

public:
    using PreviewBuilder =
        std::function<
            QImage(const QImage &)
            >;

    explicit FocusDialog(
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
        int amount,
        int radius,
        int threshold,
        int edgeProtection,
        int blend
        );

    void resetControls();

    void schedulePreviewUpdate();
    void updateResultAndPreview();

    FocusTools::FocusSettings
    currentSettings() const;

    QImage buildDisplayImage(
        const QImage &baseImage
        ) const;

    QImage m_sourceBaseImage;
    QImage m_resultBaseImage;

    PreviewBuilder m_previewBuilder;

    ToolPreviewWidget *m_previewWidget = nullptr;

    QSlider *m_amountSlider = nullptr;
    QSpinBox *m_amountSpinBox = nullptr;

    QSlider *m_radiusSlider = nullptr;
    QSpinBox *m_radiusSpinBox = nullptr;

    QSlider *m_thresholdSlider = nullptr;
    QSpinBox *m_thresholdSpinBox = nullptr;

    QSlider *m_edgeProtectionSlider = nullptr;
    QSpinBox *m_edgeProtectionSpinBox = nullptr;

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

#endif // FOCUSDIALOG_H
