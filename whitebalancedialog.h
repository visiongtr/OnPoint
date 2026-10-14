#ifndef WHITEBALANCEDIALOG_H
#define WHITEBALANCEDIALOG_H

#include "toolpreviewwidget.h"
#include "white_balance_tools.h"

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QPoint>

#include <functional>

class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

class WhiteBalancePreviewWidget : public ToolPreviewWidget
{
public:
    explicit WhiteBalancePreviewWidget(
        QWidget *parent = nullptr
        );

    void setSamplePoint(
        const QPoint &imagePoint
        );

    void clearSamplePoint();

protected:
    void paintImageOverlay(
        QPainter &painter
        ) override;

private:
    bool m_hasSamplePoint = false;
    QPoint m_samplePoint;
};

class WhiteBalanceDialog : public QDialog
{
    Q_OBJECT

public:
    using PreviewBuilder =
        std::function<
            QImage(const QImage &)
            >;

    explicit WhiteBalanceDialog(
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

    void sampleNeutralAt(
        const QPointF &imagePoint
        );

    void useAutomaticBalance();
    void resetControls();

    void schedulePreviewUpdate();
    void updateResultAndPreview();

    void updateSampleDisplay(
        const QColor &color,
        const QString &description
        );

    WhiteBalanceTools::WhiteBalanceSettings
    currentSettings() const;

    QImage buildDisplayImage(
        const QImage &baseImage
        ) const;

    QImage m_sourceBaseImage;
    QImage m_resultBaseImage;

    PreviewBuilder m_previewBuilder;

    WhiteBalanceTools::ChannelGains m_baseGains;

    WhiteBalancePreviewWidget *m_previewWidget = nullptr;

    QPushButton *m_autoButton = nullptr;

    QLabel *m_sampleSwatch = nullptr;
    QLabel *m_sampleDescription = nullptr;

    QSlider *m_temperatureSlider = nullptr;
    QSpinBox *m_temperatureSpinBox = nullptr;

    QSlider *m_tintSlider = nullptr;
    QSpinBox *m_tintSpinBox = nullptr;

    QSlider *m_strengthSlider = nullptr;
    QSpinBox *m_strengthSpinBox = nullptr;

    QPushButton *m_resetButton = nullptr;
    QPushButton *m_showOriginalButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_finishButton = nullptr;

    QTimer *m_previewTimer = nullptr;
};

#endif // WHITEBALANCEDIALOG_H
