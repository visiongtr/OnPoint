#ifndef OVEREXPOSURERECOVERYWORKSPACE_H
#define OVEREXPOSURERECOVERYWORKSPACE_H

#include "bright_area_tools.h"
#include "overexposure_tools.h"

#include <QImage>
#include <QPoint>
#include <QVector>
#include <QWidget>

class BrightAreaSelectionWidget;
class OverexposureSelectionWidget;

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QTabWidget;
class QTimer;
class QVBoxLayout;

class OverexposureRecoveryWorkspace : public QWidget
{
    Q_OBJECT

public:
    explicit OverexposureRecoveryWorkspace(
        const QImage &sourceImage,
        OverexposureSelectionWidget *glareCanvas,
        BrightAreaSelectionWidget *brightCanvas,
        QWidget *parent = nullptr
        );

    QImage resultImage() const;
    bool hasAppliedChanges() const;
    int activeTabIndex() const;

    bool applyCurrentRegion();
    bool finish(QImage &finishedImage);

    void resetActiveControls();
    void setShowOriginal(bool showOriginal);

signals:
    void canvasModeChanged(int tabIndex);
    void applyButtonTextChanged(const QString &text);
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class BrightAreaMode
    {
        None,
        Add,
        Subtract
    };

    struct AreaSeed
    {
        QPoint point;
        BrightAreaMode mode = BrightAreaMode::Add;
    };

    void setupUi();
    QWidget *createScrollableTab(QWidget *tab);
    void setupGlareTab(QWidget *tab);
    void setupBrightAreaTab(QWidget *tab);

    QWidget *createControlRow(
        QVBoxLayout *layout,
        const QString &labelText,
        const QString &toolTip,
        int minimum,
        int maximum,
        int value,
        QSlider **sliderOut,
        QSpinBox **spinBoxOut
        );

    void connectSliderSpinBox(QSlider *slider, QSpinBox *spinBox);
    void installNoWheelFilter(QWidget *widget);

    void setGlareRegionShape(OverexposureTools::RegionShape shape);
    OverexposureTools::OverexposureSettings currentGlareSettings() const;
    BrightAreaTools::BrightAreaSettings currentBrightAreaSettings() const;

    void scheduleActivePreview();
    void updateActivePreview();
    void updateGlareOverlay();
    void updateGlarePreview();
    void updateBrightAreaPreview();

    void selectBrightArea(const QPoint &imagePoint);
    void rebuildBrightMaskFromSeeds();
    void combineBrightMask(const QImage &candidateMask, BrightAreaMode mode);
    void clearBrightSelection();

    void setBrightAreaMode(BrightAreaMode mode);
    void setManualRefineEnabled(bool enabled);
    void setOutlineEditEnabled(bool enabled);

    void sampleTintColor(const QPoint &imagePoint);
    void addTintChoice(const QString &name, const QColor &color);
    void updateTintUi();
    void updateTintSliderStyle(const QColor &color);

    void resetGlareControls();
    void resetBrightAreaControls();

    bool applyCurrentGlareRegion();
    bool applyCurrentBrightArea();
    void setCanvasImagesToSource();
    bool activeSelectionExists() const;

    QImage m_originalImage;
    QImage m_sourceImage;
    QImage m_resultImage;

    bool m_hasAppliedRegion = false;
    bool m_showingOriginal = false;

    OverexposureSelectionWidget *m_glareCanvas = nullptr;
    BrightAreaSelectionWidget *m_brightCanvas = nullptr;

    QTabWidget *m_tabWidget = nullptr;
    QPushButton *m_resetTabButton = nullptr;

    // Glare Recovery
    OverexposureTools::RegionShape m_glareRegionShape =
        OverexposureTools::RegionShape::Oval;

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

    // Bright Area Fade
    QImage m_brightMask;
    QImage m_brightPreviewImage;
    QVector<AreaSeed> m_brightAreaSeeds;
    bool m_brightMaskEditedByUser = false;
    BrightAreaMode m_brightAreaMode = BrightAreaMode::None;

    QButtonGroup *m_areaModeButtonGroup = nullptr;
    QPushButton *m_addAreaButton = nullptr;
    QPushButton *m_subtractAreaButton = nullptr;

    QSlider *m_brightThresholdSlider = nullptr;
    QSpinBox *m_brightThresholdSpinBox = nullptr;
    QSlider *m_growShrinkSlider = nullptr;
    QSpinBox *m_growShrinkSpinBox = nullptr;

    QCheckBox *m_manualRefineCheckBox = nullptr;
    QWidget *m_manualRefineControls = nullptr;
    QButtonGroup *m_brushModeButtonGroup = nullptr;
    QPushButton *m_addBrushButton = nullptr;
    QPushButton *m_removeBrushButton = nullptr;
    QCheckBox *m_showMaskCheckBox = nullptr;
    QSlider *m_brushSizeSlider = nullptr;
    QSpinBox *m_brushSizeSpinBox = nullptr;
    QSlider *m_brushSoftnessSlider = nullptr;
    QSpinBox *m_brushSoftnessSpinBox = nullptr;
    QSlider *m_brushStrengthSlider = nullptr;
    QSpinBox *m_brushStrengthSpinBox = nullptr;

    QCheckBox *m_outlineEditCheckBox = nullptr;
    QWidget *m_outlineEditControls = nullptr;
    QPushButton *m_addNodeButton = nullptr;
    QPushButton *m_removeNodeButton = nullptr;

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
    QLabel *m_tintColorSwatch = nullptr;
    QPushButton *m_tintEyedropperButton = nullptr;
    int m_sampledTintIndex = -1;

    QTimer *m_previewTimer = nullptr;
    bool m_previewPending = false;
};

#endif // OVEREXPOSURERECOVERYWORKSPACE_H
