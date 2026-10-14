#ifndef OVEREXPOSURERECOVERYDIALOG_H
#define OVEREXPOSURERECOVERYDIALOG_H

#include "bright_area_tools.h"
#include "overexposure_tools.h"

#include <QDialog>
#include <QImage>
#include <QPoint>

class BrightAreaSelectionWidget;
class OverexposureSelectionWidget;

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;
class QVBoxLayout;

class OverexposureRecoveryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OverexposureRecoveryDialog(const QImage &sourceImage, QWidget *parent = nullptr);

    QImage resultImage() const;

private:
    void setupUi();
    void setupGlareTab(QWidget *tab);
    void setupBrightAreaTab(QWidget *tab);

    void createControlRow(QVBoxLayout *layout,
                          const QString &labelText,
                          int minimum,
                          int maximum,
                          int value,
                          QSlider **sliderOut,
                          QSpinBox **spinBoxOut);

    void connectSliderSpinBox(QSlider *slider, QSpinBox *spinBox);

    void setGlareRegionShape(OverexposureTools::RegionShape shape);

    OverexposureTools::OverexposureSettings currentGlareSettings() const;
    BrightAreaTools::BrightAreaSettings currentBrightAreaSettings() const;

    void scheduleActivePreview();
    void updateActivePreview();

    void updateGlareOverlay();
    void updateGlarePreview();

    void updateBrightMaskFromSeed();
    void updateBrightAreaPreview();
    void autoSelectBrightArea(const QPoint &imagePoint);

    void resetActiveControls();
    void resetGlareControls();
    void resetBrightAreaControls();

    bool applyCurrentRegionToWorkingImage();
    bool applyCurrentGlareRegionToWorkingImage();
    bool applyCurrentBrightAreaToWorkingImage();


    void setPreviewImagesToSource();
    void clearActiveSelection();

    void showOriginalPressed();
    void showOriginalReleased();

    void updateButtonLabels();

    QImage m_originalImage;
    QImage m_sourceImage;
    QImage m_resultImage;

    bool m_hasAppliedRegion = false;

    QTabWidget *m_tabWidget = nullptr;

    // Glare Recovery tab
    OverexposureSelectionWidget *m_glarePreviewWidget = nullptr;

    OverexposureTools::RegionShape m_glareRegionShape = OverexposureTools::RegionShape::Oval;

    QButtonGroup *m_glareShapeButtonGroup = nullptr;
    QPushButton *m_ovalShapeButton = nullptr;
    QPushButton *m_rectangleShapeButton = nullptr;
    QPushButton *m_squareShapeButton = nullptr;

    QSlider *m_thresholdSlider = nullptr;
    QSpinBox *m_thresholdSpinBox = nullptr;

    QSlider *m_sampleDistanceSlider = nullptr;
    QSpinBox *m_sampleDistanceSpinBox = nullptr;

    QSlider *m_sampleWidthSlider = nullptr;
    QSpinBox *m_sampleWidthSpinBox = nullptr;

    QSlider *m_recoveryAmountSlider = nullptr;
    QSpinBox *m_recoveryAmountSpinBox = nullptr;

    QSlider *m_colorBleedSlider = nullptr;
    QSpinBox *m_colorBleedSpinBox = nullptr;

    QSlider *m_transparencySlider = nullptr;
    QSpinBox *m_transparencySpinBox = nullptr;

    QSlider *m_softEdgeSlider = nullptr;
    QSpinBox *m_softEdgeSpinBox = nullptr;

    QSlider *m_preserveShineSlider = nullptr;
    QSpinBox *m_preserveShineSpinBox = nullptr;

    // Bright Area Fade tab
    BrightAreaSelectionWidget *m_brightPreviewWidget = nullptr;

    QImage m_brightMask;
    QImage m_brightPreviewImage;

    QPoint m_lastBrightSeedPoint;
    bool m_hasBrightSeed = false;
    bool m_brightMaskEditedByUser = false;

    QCheckBox *m_outlineEditCheckBox = nullptr;
    QCheckBox *m_manualRefineCheckBox = nullptr;
    QCheckBox *m_showMaskCheckBox = nullptr;

    QButtonGroup *m_brushModeButtonGroup = nullptr;
    QPushButton *m_addBrushButton = nullptr;
    QPushButton *m_removeBrushButton = nullptr;

    QSlider *m_brightThresholdSlider = nullptr;
    QSpinBox *m_brightThresholdSpinBox = nullptr;

    QSlider *m_growShrinkSlider = nullptr;
    QSpinBox *m_growShrinkSpinBox = nullptr;

    QSlider *m_brightRecoveryAmountSlider = nullptr;
    QSpinBox *m_brightRecoveryAmountSpinBox = nullptr;

    QSlider *m_brightBlendSlider = nullptr;
    QSpinBox *m_brightBlendSpinBox = nullptr;

    QSlider *m_brightSoftEdgeSlider = nullptr;
    QSpinBox *m_brightSoftEdgeSpinBox = nullptr;

    QSlider *m_preserveGlowSlider = nullptr;
    QSpinBox *m_preserveGlowSpinBox = nullptr;

    QSlider *m_tintAmountSlider = nullptr;
    QSpinBox *m_tintAmountSpinBox = nullptr;

    QComboBox *m_tintColorCombo = nullptr;

    QSlider *m_brushSizeSlider = nullptr;
    QSpinBox *m_brushSizeSpinBox = nullptr;

    QSlider *m_brushSoftnessSlider = nullptr;
    QSpinBox *m_brushSoftnessSpinBox = nullptr;

    QSlider *m_brushStrengthSlider = nullptr;
    QSpinBox *m_brushStrengthSpinBox = nullptr;

    // Dialog buttons
    QDialogButtonBox *m_buttonBox = nullptr;
    QPushButton *m_showOriginalButton = nullptr;
    QPushButton *m_applyRegionButton = nullptr;

    QTimer *m_previewTimer = nullptr;
    bool m_previewPending = false;
};

#endif // OVEREXPOSURERECOVERYDIALOG_H
