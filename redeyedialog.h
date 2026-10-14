#ifndef REDEYEDIALOG_H
#define REDEYEDIALOG_H

#include "red_eye_tools.h"
#include "redeyeselectionwidget.h"

#include <QColor>
#include <QDialog>
#include <QImage>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

class RedEyeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RedEyeDialog(const QImage &sourceImage, QWidget *parent = nullptr);

    QImage resultImage() const;

private:
    void setupUi();
    void updatePreview();
    void resetControls();

    QColor sampleGlowColor(const QPoint &imagePoint) const;
    void setSampledTargetColor(const QColor &color);
    void clearSampledTargetColor();
    void updateTargetColorDisplay();

    void createControlRow(const QString &labelText,
                          int minimum,
                          int maximum,
                          int value,
                          QSlider **sliderOut,
                          QSpinBox **spinBoxOut);

    void connectSliderAndSpinBox(QSlider *slider, QSpinBox *spinBox);

    RedEyeTools::RedEyeSettings currentSettings() const;

    QImage m_originalImage;
    QImage m_sourceImage;
    QImage m_resultImage;

    bool m_hasAppliedEye = false;
    bool m_hasSampledTargetColor = false;
    QColor m_sampledTargetColor = QColor(205, 45, 38);

    RedEyeSelectionWidget *m_previewWidget = nullptr;

    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_irisColorCombo = nullptr;

    QPushButton *m_sampleGlowButton = nullptr;
    QPushButton *m_useAutoRedButton = nullptr;
    QLabel *m_targetColorSwatch = nullptr;

    QSlider *m_toleranceSlider = nullptr;
    QSpinBox *m_toleranceSpinBox = nullptr;

    QSlider *m_strengthSlider = nullptr;
    QSpinBox *m_strengthSpinBox = nullptr;

    QSlider *m_pupilDarkenSlider = nullptr;
    QSpinBox *m_pupilDarkenSpinBox = nullptr;

    QSlider *m_irisAmountSlider = nullptr;
    QSpinBox *m_irisAmountSpinBox = nullptr;

    QSlider *m_blendSlider = nullptr;
    QSpinBox *m_blendSpinBox = nullptr;

    QSlider *m_softEdgeSlider = nullptr;
    QSpinBox *m_softEdgeSpinBox = nullptr;

    QDialogButtonBox *m_buttonBox = nullptr;
    QPushButton *m_applyEyeButton = nullptr;

};

#endif // REDEYEDIALOG_H
