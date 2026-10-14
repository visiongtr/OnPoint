#include "overexposurerecoverydialog.h"

#include "brightareaselectionwidget.h"
#include "overexposureselectionwidget.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>

OverexposureRecoveryDialog::OverexposureRecoveryDialog(const QImage &sourceImage,
                                                       QWidget *parent)
    : QDialog(parent),
    m_originalImage(sourceImage),
    m_sourceImage(sourceImage),
    m_resultImage(sourceImage)
{
    setupUi();
    resetGlareControls();
    resetBrightAreaControls();
    updateActivePreview();
}

QImage OverexposureRecoveryDialog::resultImage() const
{
    return m_resultImage;
}

void OverexposureRecoveryDialog::setupUi()
{
    setWindowTitle("Overexposure Recovery");
   resize(1040, 740);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_previewTimer =
        new QTimer(this);

    m_previewTimer->setSingleShot(
        true
        );

    // Heavy overexposure processing is delayed while a control is moving.
    // Continuous dragging keeps restarting the timer; releasing the slider
    // guarantees the final requested value is rendered immediately.
    m_previewTimer->setInterval(
        220
        );

    connect(
        m_previewTimer,
        &QTimer::timeout,
        this,
        &OverexposureRecoveryDialog::updateActivePreview
        );

    QLabel *instructionLabel = new QLabel(
        "Use Glare Recovery for localized glare. Use Bright Area Fade for blown windows, washed-out skies, and larger bright regions."
        );
    instructionLabel->setWordWrap(true);

    mainLayout->addWidget(instructionLabel);

    m_tabWidget = new QTabWidget(this);

    QWidget *glareTab = new QWidget(m_tabWidget);
    QWidget *brightAreaTab = new QWidget(m_tabWidget);

    setupGlareTab(glareTab);
    setupBrightAreaTab(brightAreaTab);

    m_tabWidget->addTab(glareTab, "Glare Recovery");
    m_tabWidget->addTab(brightAreaTab, "Bright Area Fade");

    mainLayout->addWidget(m_tabWidget, 1);

    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, [this]() {
                updateButtonLabels();
                updateActivePreview();
            });

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Reset |
            QDialogButtonBox::Cancel |
            QDialogButtonBox::Apply,
        this
        );

    m_showOriginalButton = m_buttonBox->addButton(
        "Show Original",
        QDialogButtonBox::ActionRole
        );

    m_applyRegionButton = m_buttonBox->addButton(
        "Apply Region",
        QDialogButtonBox::ActionRole
        );

    m_buttonBox->button(QDialogButtonBox::Apply)->setText("Finish");
    m_buttonBox->button(QDialogButtonBox::Apply)->setToolTip(
        "Finish overexposure recovery and return to the editor"
        );

    connect(m_showOriginalButton, &QPushButton::pressed,
            this, &OverexposureRecoveryDialog::showOriginalPressed);

    connect(m_showOriginalButton, &QPushButton::released,
            this, &OverexposureRecoveryDialog::showOriginalReleased);

    connect(m_applyRegionButton, &QPushButton::clicked,
            this, [this]() {
                if (applyCurrentRegionToWorkingImage()) {
                    m_applyRegionButton->setText(
                        (!m_tabWidget || m_tabWidget->currentIndex() == 0)
                            ? "Applied Region"
                            : "Applied Area"
                        );

                    QTimer::singleShot(
                        900,
                        this,
                        [this]() {
                            updateButtonLabels();
                        }
                        );
                }
            });

    connect(m_buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, &OverexposureRecoveryDialog::resetActiveControls);

    connect(m_buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);

    connect(m_buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, [this]() {
                const bool glareTabActive =
                    !m_tabWidget || m_tabWidget->currentIndex() == 0;

                if (glareTabActive) {
                    const QRect selection =
                        m_glarePreviewWidget ? m_glarePreviewWidget->selectionRect() : QRect();

                    // Active glare selection: apply it, then finish.
                    if (!selection.isNull() && !selection.isEmpty()) {
                        if (applyCurrentGlareRegionToWorkingImage()) {
                            m_resultImage = m_sourceImage;
                            accept();
                        }

                        return;
                    }

                    // No active selection, but user already clicked Apply Region.
                    if (m_hasAppliedRegion) {
                        m_resultImage = m_sourceImage;
                        accept();
                        return;
                    }

                    QMessageBox::information(
                        this,
                        "Glare Recovery",
                        "Draw around a glare area first."
                        );

                    return;
                }

                QImage finishBrightMask = m_brightMask;

                if (m_brightPreviewWidget && !m_brightPreviewWidget->mask().isNull()) {
                    finishBrightMask = m_brightPreviewWidget->mask();
                }

                const bool hasBrightMask =
                    !finishBrightMask.isNull() &&
                    BrightAreaTools::maskHasSelection(finishBrightMask);

                if (hasBrightMask) {
                    m_brightMask = finishBrightMask;
                }

                // Active bright-area mask: apply it, then finish.
                if (hasBrightMask) {
                    if (applyCurrentBrightAreaToWorkingImage()) {
                        m_resultImage = m_sourceImage;
                        accept();
                    }

                    return;
                }

                // No active mask, but user already clicked Apply Area.
                if (m_hasAppliedRegion) {
                    m_resultImage = m_sourceImage;
                    accept();
                    return;
                }

                QMessageBox::information(
                    this,
                    "Bright Area Fade",
                    "Click inside a blown bright area first, or use Manual Refine to paint a selection."
                    );
            });

    mainLayout->addWidget(m_buttonBox);

    updateButtonLabels();
}

void OverexposureRecoveryDialog::setupGlareTab(QWidget *tab)
{
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);

    QLabel *label = new QLabel(
        "Draw around glare, then use the sample ring and recovery controls to tone it down."
        );
    label->setWordWrap(true);

    tabLayout->addWidget(label);

    m_glarePreviewWidget = new OverexposureSelectionWidget(tab);
    m_glarePreviewWidget->setImage(m_sourceImage);

    tabLayout->addWidget(m_glarePreviewWidget, 1);

    QGroupBox *controlsGroup = new QGroupBox("Glare Controls", tab);
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsGroup);

    QWidget *shapeRowWidget = new QWidget(controlsGroup);
    QHBoxLayout *shapeRowLayout = new QHBoxLayout(shapeRowWidget);
    shapeRowLayout->setContentsMargins(0, 0, 0, 0);
    shapeRowLayout->setSpacing(8);

    QLabel *shapeLabel = new QLabel("Region Shape", shapeRowWidget);
    shapeLabel->setMinimumWidth(135);

    m_glareShapeButtonGroup = new QButtonGroup(this);
    m_glareShapeButtonGroup->setExclusive(true);

    m_ovalShapeButton = new QPushButton("◯ Oval", shapeRowWidget);
    m_rectangleShapeButton = new QPushButton("▭ Rect", shapeRowWidget);
    m_squareShapeButton = new QPushButton("□ Square", shapeRowWidget);

    m_ovalShapeButton->setCheckable(true);
    m_rectangleShapeButton->setCheckable(true);
    m_squareShapeButton->setCheckable(true);

    m_glareShapeButtonGroup->addButton(m_ovalShapeButton);
    m_glareShapeButtonGroup->addButton(m_rectangleShapeButton);
    m_glareShapeButtonGroup->addButton(m_squareShapeButton);

    shapeRowLayout->addWidget(shapeLabel);
    shapeRowLayout->addWidget(m_ovalShapeButton);
    shapeRowLayout->addWidget(m_rectangleShapeButton);
    shapeRowLayout->addWidget(m_squareShapeButton);
    shapeRowLayout->addStretch();

    controlsLayout->addWidget(shapeRowWidget);

    createControlRow(controlsLayout, "Threshold", 0, 100, 75,
                     &m_thresholdSlider, &m_thresholdSpinBox);

    createControlRow(controlsLayout, "Sample Distance", 0, 100, 30,
                     &m_sampleDistanceSlider, &m_sampleDistanceSpinBox);

    createControlRow(controlsLayout, "Sample Width", 0, 100, 20,
                     &m_sampleWidthSlider, &m_sampleWidthSpinBox);

    createControlRow(controlsLayout, "Recovery Amount", 0, 100, 45,
                     &m_recoveryAmountSlider, &m_recoveryAmountSpinBox);

    createControlRow(controlsLayout, "Color Bleed", 0, 100, 55,
                     &m_colorBleedSlider, &m_colorBleedSpinBox);

    createControlRow(controlsLayout, "Transparency", 0, 100, 75,
                     &m_transparencySlider, &m_transparencySpinBox);

    createControlRow(controlsLayout, "Soft Edge", 0, 100, 55,
                     &m_softEdgeSlider, &m_softEdgeSpinBox);

    createControlRow(controlsLayout, "Preserve Shine", 0, 100, 35,
                     &m_preserveShineSlider, &m_preserveShineSpinBox);

    tabLayout->addWidget(controlsGroup);

    connect(m_ovalShapeButton, &QPushButton::clicked,
            this, [this]() {
                setGlareRegionShape(OverexposureTools::RegionShape::Oval);
            });

    connect(m_rectangleShapeButton, &QPushButton::clicked,
            this, [this]() {
                setGlareRegionShape(OverexposureTools::RegionShape::Rectangle);
            });

    connect(m_squareShapeButton, &QPushButton::clicked,
            this, [this]() {
                setGlareRegionShape(OverexposureTools::RegionShape::Square);
            });

    connectSliderSpinBox(m_thresholdSlider, m_thresholdSpinBox);
    connectSliderSpinBox(m_sampleDistanceSlider, m_sampleDistanceSpinBox);
    connectSliderSpinBox(m_sampleWidthSlider, m_sampleWidthSpinBox);
    connectSliderSpinBox(m_recoveryAmountSlider, m_recoveryAmountSpinBox);
    connectSliderSpinBox(m_colorBleedSlider, m_colorBleedSpinBox);
    connectSliderSpinBox(m_transparencySlider, m_transparencySpinBox);
    connectSliderSpinBox(m_softEdgeSlider, m_softEdgeSpinBox);
    connectSliderSpinBox(m_preserveShineSlider, m_preserveShineSpinBox);

    connect(m_glarePreviewWidget, &OverexposureSelectionWidget::selectionChanged,
            this, &OverexposureRecoveryDialog::updateGlarePreview);
}

void OverexposureRecoveryDialog::setupBrightAreaTab(QWidget *tab)
{
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);

    QLabel *label = new QLabel(
        "Click inside a blown bright area to auto-select it. Use Manual Refine to add or remove from the selection."
        );
    label->setWordWrap(true);

    tabLayout->addWidget(label);

    m_brightPreviewWidget = new BrightAreaSelectionWidget(tab);
    m_brightPreviewWidget->setImage(m_sourceImage);

    tabLayout->addWidget(m_brightPreviewWidget, 1);

    QGroupBox *selectionGroup = new QGroupBox("Selection", tab);
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionGroup);

    createControlRow(selectionLayout, "Threshold", 0, 100, 75,
                     &m_brightThresholdSlider, &m_brightThresholdSpinBox);

    createControlRow(selectionLayout, "Grow / Shrink", -100, 100, 0,
                     &m_growShrinkSlider, &m_growShrinkSpinBox);

    QWidget *manualRowWidget = new QWidget(selectionGroup);
    QHBoxLayout *manualRowLayout = new QHBoxLayout(manualRowWidget);
    manualRowLayout->setContentsMargins(0, 0, 0, 0);
    manualRowLayout->setSpacing(8);

    QLabel *manualLabel = new QLabel("Manual Refine", manualRowWidget);
    manualLabel->setMinimumWidth(135);

    m_outlineEditCheckBox = new QCheckBox("Edit Outline", manualRowWidget);
    m_manualRefineCheckBox = new QCheckBox("Brush Refine", manualRowWidget);
    m_showMaskCheckBox = new QCheckBox("Show Mask", manualRowWidget);
    m_showMaskCheckBox->setChecked(true);

    m_brushModeButtonGroup = new QButtonGroup(this);
    m_brushModeButtonGroup->setExclusive(true);

    m_addBrushButton = new QPushButton("+ Add", manualRowWidget);
    m_removeBrushButton = new QPushButton("− Remove", manualRowWidget);

    m_addBrushButton->setCheckable(true);
    m_removeBrushButton->setCheckable(true);
    m_addBrushButton->setChecked(true);

    m_brushModeButtonGroup->addButton(m_addBrushButton);
    m_brushModeButtonGroup->addButton(m_removeBrushButton);

    manualRowLayout->addWidget(manualLabel);
    manualRowLayout->addWidget(m_outlineEditCheckBox);
    manualRowLayout->addWidget(m_manualRefineCheckBox);
    manualRowLayout->addWidget(m_showMaskCheckBox);
    manualRowLayout->addWidget(m_addBrushButton);
    manualRowLayout->addWidget(m_removeBrushButton);
    manualRowLayout->addStretch();

    selectionLayout->addWidget(manualRowWidget);

    createControlRow(selectionLayout, "Brush Size", 1, 200, 36,
                     &m_brushSizeSlider, &m_brushSizeSpinBox);

    createControlRow(selectionLayout, "Brush Soft Edge", 0, 100, 65,
                     &m_brushSoftnessSlider, &m_brushSoftnessSpinBox);

    createControlRow(selectionLayout, "Brush Strength", 0, 100, 80,
                     &m_brushStrengthSlider, &m_brushStrengthSpinBox);

    tabLayout->addWidget(selectionGroup);

    QGroupBox *fadeGroup = new QGroupBox("Fade / Tint", tab);
    QVBoxLayout *fadeLayout = new QVBoxLayout(fadeGroup);

    createControlRow(fadeLayout, "Recovery Amount", 0, 100, 45,
                     &m_brightRecoveryAmountSlider, &m_brightRecoveryAmountSpinBox);

    createControlRow(fadeLayout, "Blend", 0, 100, 75,
                     &m_brightBlendSlider, &m_brightBlendSpinBox);

    createControlRow(fadeLayout, "Soft Edge", 0, 100, 55,
                     &m_brightSoftEdgeSlider, &m_brightSoftEdgeSpinBox);

    createControlRow(fadeLayout, "Preserve Glow", 0, 100, 45,
                     &m_preserveGlowSlider, &m_preserveGlowSpinBox);

    createControlRow(fadeLayout, "Color Tint", 0, 100, 0,
                     &m_tintAmountSlider, &m_tintAmountSpinBox);

    QWidget *tintRowWidget = new QWidget(fadeGroup);
    QHBoxLayout *tintRowLayout = new QHBoxLayout(tintRowWidget);
    tintRowLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *tintLabel = new QLabel("Tint Color", tintRowWidget);
    tintLabel->setMinimumWidth(135);

    m_tintColorCombo = new QComboBox(tintRowWidget);
    m_tintColorCombo->addItem("Sky Blue", QColor(135, 190, 255));
    m_tintColorCombo->addItem("Warm Daylight", QColor(255, 215, 150));
    m_tintColorCombo->addItem("Neutral Gray", QColor(210, 210, 210));
    m_tintColorCombo->addItem("Soft Cyan", QColor(150, 230, 235));

    tintRowLayout->addWidget(tintLabel);
    tintRowLayout->addWidget(m_tintColorCombo);
    tintRowLayout->addStretch();

    fadeLayout->addWidget(tintRowWidget);

    tabLayout->addWidget(fadeGroup);

    connect(m_brightPreviewWidget, &BrightAreaSelectionWidget::autoSelectRequested,
            this, &OverexposureRecoveryDialog::autoSelectBrightArea);

    connect(m_brightPreviewWidget, &BrightAreaSelectionWidget::maskEdited,
            this, [this](const QImage &mask) {
                m_brightMask = mask;
                m_brightMaskEditedByUser = true;
                updateBrightAreaPreview();
            });

    connect(m_outlineEditCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (checked) {
                    m_manualRefineCheckBox->blockSignals(true);
                    m_manualRefineCheckBox->setChecked(false);
                    m_manualRefineCheckBox->blockSignals(false);

                    m_brightPreviewWidget->setManualRefineEnabled(false);
                }

                m_brightPreviewWidget->setOutlineEditEnabled(checked);
                updateBrightAreaPreview();
            });

    connect(m_manualRefineCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (checked) {
                    m_outlineEditCheckBox->blockSignals(true);
                    m_outlineEditCheckBox->setChecked(false);
                    m_outlineEditCheckBox->blockSignals(false);

                    m_brightPreviewWidget->setOutlineEditEnabled(false);
                }

                m_brightPreviewWidget->setManualRefineEnabled(checked);
                updateBrightAreaPreview();
            });

    connect(m_showMaskCheckBox, &QCheckBox::toggled,
            m_brightPreviewWidget, &BrightAreaSelectionWidget::setMaskVisible);

    connect(m_addBrushButton, &QPushButton::clicked,
            this, [this]() {
                m_brightPreviewWidget->setBrushAddsToSelection(true);
            });

    connect(m_removeBrushButton, &QPushButton::clicked,
            this, [this]() {
                m_brightPreviewWidget->setBrushAddsToSelection(false);
            });

    connect(m_tintColorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OverexposureRecoveryDialog::updateBrightAreaPreview);

    connectSliderSpinBox(m_brightThresholdSlider, m_brightThresholdSpinBox);
    connectSliderSpinBox(m_growShrinkSlider, m_growShrinkSpinBox);
    connectSliderSpinBox(m_brightRecoveryAmountSlider, m_brightRecoveryAmountSpinBox);
    connectSliderSpinBox(m_brightBlendSlider, m_brightBlendSpinBox);
    connectSliderSpinBox(m_brightSoftEdgeSlider, m_brightSoftEdgeSpinBox);
    connectSliderSpinBox(m_preserveGlowSlider, m_preserveGlowSpinBox);
    connectSliderSpinBox(m_tintAmountSlider, m_tintAmountSpinBox);

    connect(m_brushSizeSlider, &QSlider::valueChanged,
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSizeSpinBox);
                m_brushSizeSpinBox->setValue(value);
                m_brightPreviewWidget->setBrushSize(value);
            });

    connect(m_brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSizeSlider);
                m_brushSizeSlider->setValue(value);
                m_brightPreviewWidget->setBrushSize(value);
            });

    connect(m_brushSoftnessSlider, &QSlider::valueChanged,
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSoftnessSpinBox);
                m_brushSoftnessSpinBox->setValue(value);
                m_brightPreviewWidget->setBrushSoftness(value);
            });

    connect(m_brushSoftnessSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSoftnessSlider);
                m_brushSoftnessSlider->setValue(value);
                m_brightPreviewWidget->setBrushSoftness(value);
            });

    connect(m_brushStrengthSlider, &QSlider::valueChanged,
            this, [this](int value) {
                QSignalBlocker blocker(m_brushStrengthSpinBox);
                m_brushStrengthSpinBox->setValue(value);
                m_brightPreviewWidget->setBrushStrength(value);
            });

    connect(m_brushStrengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushStrengthSlider);
                m_brushStrengthSlider->setValue(value);
                m_brightPreviewWidget->setBrushStrength(value);
            });
}

void OverexposureRecoveryDialog::createControlRow(QVBoxLayout *layout,
                                                  const QString &labelText,
                                                  int minimum,
                                                  int maximum,
                                                  int value,
                                                  QSlider **sliderOut,
                                                  QSpinBox **spinBoxOut)
{
    QWidget *rowWidget = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);

    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    QLabel *label = new QLabel(labelText, rowWidget);
    label->setMinimumWidth(135);

    QSlider *slider = new QSlider(Qt::Horizontal, rowWidget);
    slider->setRange(minimum, maximum);
    slider->setValue(value);

    QSpinBox *spinBox = new QSpinBox(rowWidget);
    spinBox->setRange(minimum, maximum);
    spinBox->setValue(value);
    spinBox->setMinimumWidth(70);

    rowLayout->addWidget(label);
    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(spinBox);

    layout->addWidget(rowWidget);

    *sliderOut = slider;
    *spinBoxOut = spinBox;
}

void OverexposureRecoveryDialog::connectSliderSpinBox(QSlider *slider, QSpinBox *spinBox)
{
    connect(slider, &QSlider::valueChanged,
            this,
            [this, spinBox](int value) {
                QSignalBlocker blocker(spinBox);
                spinBox->setValue(value);
                scheduleActivePreview();
            });

    connect(slider, &QSlider::sliderReleased,
            this,
            [this]() {
                if (!m_previewPending) {
                    return;
                }

                updateActivePreview();
            });

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this, slider](int value) {
                QSignalBlocker blocker(slider);
                slider->setValue(value);
                scheduleActivePreview();
            });

    connect(spinBox, &QSpinBox::editingFinished,
            this,
            [this]() {
                if (!m_previewPending) {
                    return;
                }

                updateActivePreview();
            });
}

void OverexposureRecoveryDialog::setGlareRegionShape(OverexposureTools::RegionShape shape)
{
    m_glareRegionShape = shape;

    m_ovalShapeButton->setChecked(shape == OverexposureTools::RegionShape::Oval);
    m_rectangleShapeButton->setChecked(shape == OverexposureTools::RegionShape::Rectangle);
    m_squareShapeButton->setChecked(shape == OverexposureTools::RegionShape::Square);

    updateGlareOverlay();
    updateGlarePreview();
}

OverexposureTools::OverexposureSettings OverexposureRecoveryDialog::currentGlareSettings() const
{
    OverexposureTools::OverexposureSettings settings;

    settings.shape = m_glareRegionShape;

    settings.threshold = m_thresholdSlider->value();
    settings.sampleDistance = m_sampleDistanceSlider->value();
    settings.sampleWidth = m_sampleWidthSpinBox->value();

    settings.recoveryAmount = m_recoveryAmountSlider->value();
    settings.colorBleed = m_colorBleedSlider->value();
    settings.transparency = m_transparencySlider->value();
    settings.softEdge = m_softEdgeSlider->value();
    settings.preserveShine = m_preserveShineSlider->value();

    return settings;
}

BrightAreaTools::BrightAreaSettings OverexposureRecoveryDialog::currentBrightAreaSettings() const
{
    BrightAreaTools::BrightAreaSettings settings;

    settings.threshold = m_brightThresholdSlider->value();
    settings.growShrink = m_growShrinkSlider->value();

    settings.recoveryAmount = m_brightRecoveryAmountSlider->value();
    settings.blend = m_brightBlendSlider->value();
    settings.softEdge = m_brightSoftEdgeSlider->value();
    settings.preserveGlow = m_preserveGlowSlider->value();

    settings.tintAmount = m_tintAmountSlider->value();
    settings.tintColor = m_tintColorCombo->currentData().value<QColor>();

    return settings;
}

void OverexposureRecoveryDialog::scheduleActivePreview()
{
    m_previewPending = true;

    if (!m_previewTimer) {
        updateActivePreview();
        return;
    }

    m_previewTimer->start();
}

void OverexposureRecoveryDialog::updateActivePreview()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        updateGlarePreview();
        return;
    }

    updateBrightAreaPreview();
}

void OverexposureRecoveryDialog::updateGlareOverlay()
{
    if (!m_glarePreviewWidget) {
        return;
    }

    m_glarePreviewWidget->setOverlaySettings(
        m_sampleDistanceSlider->value(),
        m_sampleWidthSlider->value(),
        m_glareRegionShape
        );
}

void OverexposureRecoveryDialog::updateGlarePreview()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    if (!m_glarePreviewWidget) {
        return;
    }

    updateGlareOverlay();

    const QRect selection = m_glarePreviewWidget->selectionRect();

    if (selection.isNull() || selection.isEmpty()) {
        m_resultImage = m_sourceImage;
        m_glarePreviewWidget->setImage(m_sourceImage);
        return;
    }

    m_resultImage = OverexposureTools::recoverOverexposure(
        m_sourceImage,
        selection,
        currentGlareSettings()
        );

    m_glarePreviewWidget->setImage(m_resultImage);
}

void OverexposureRecoveryDialog::updateBrightMaskFromSeed()
{
    if (!m_hasBrightSeed) {
        return;
    }

    // Once the user edits the mask by outline or brush,
    // do not regenerate it from the original click point.
    if (m_brightMaskEditedByUser ||
        m_manualRefineCheckBox->isChecked() ||
        m_outlineEditCheckBox->isChecked()) {
        return;
    }

    m_brightMask = BrightAreaTools::createBrightAreaMask(
        m_sourceImage,
        m_lastBrightSeedPoint,
        m_brightThresholdSlider->value()
        );

    m_brightPreviewWidget->setMask(m_brightMask);
}

void OverexposureRecoveryDialog::updateBrightAreaPreview()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    if (!m_brightPreviewWidget) {
        return;
    }

    updateBrightMaskFromSeed();

    if (m_brightMask.isNull() || !BrightAreaTools::maskHasSelection(m_brightMask)) {
        m_brightPreviewImage = m_sourceImage;
        m_resultImage = m_sourceImage;

        m_brightPreviewWidget->setImage(m_sourceImage);
        m_brightPreviewWidget->setMask(m_brightMask);

        return;
    }

    // This is the visible Bright Area Fade preview.
    // Keep m_resultImage synced just like Glare Recovery does.
    m_brightPreviewImage = BrightAreaTools::applyBrightAreaFade(
        m_sourceImage,
        m_brightMask,
        currentBrightAreaSettings()
        );

    m_resultImage = m_brightPreviewImage;

    m_brightPreviewWidget->setImage(m_brightPreviewImage);
    m_brightPreviewWidget->setMask(m_brightMask);
}

void OverexposureRecoveryDialog::autoSelectBrightArea(const QPoint &imagePoint)
{
    m_lastBrightSeedPoint = imagePoint;
    m_hasBrightSeed = true;
    m_brightMaskEditedByUser = false;
    m_brightPreviewImage = m_sourceImage;

    m_brightMask = BrightAreaTools::createBrightAreaMask(
        m_sourceImage,
        imagePoint,
        m_brightThresholdSlider->value()
        );

    m_brightPreviewWidget->setMask(m_brightMask);
    updateBrightAreaPreview();
}

void OverexposureRecoveryDialog::resetActiveControls()
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        resetGlareControls();
        updateGlarePreview();
        return;
    }

    resetBrightAreaControls();
    updateBrightAreaPreview();
}

void OverexposureRecoveryDialog::resetGlareControls()
{
    setGlareRegionShape(OverexposureTools::RegionShape::Oval);

    m_thresholdSlider->setValue(75);
    m_sampleDistanceSlider->setValue(30);
    m_sampleWidthSlider->setValue(20);

    m_recoveryAmountSlider->setValue(45);
    m_colorBleedSlider->setValue(55);
    m_transparencySlider->setValue(75);
    m_softEdgeSlider->setValue(55);
    m_preserveShineSlider->setValue(35);

    if (m_glarePreviewWidget) {
        m_glarePreviewWidget->setImage(m_sourceImage);
        m_glarePreviewWidget->clearSelection();
    }
}

void OverexposureRecoveryDialog::resetBrightAreaControls()
{
    m_brightThresholdSlider->setValue(75);
    m_growShrinkSlider->setValue(0);

    m_brightRecoveryAmountSlider->setValue(45);
    m_brightBlendSlider->setValue(75);
    m_brightSoftEdgeSlider->setValue(55);
    m_preserveGlowSlider->setValue(45);

    m_tintAmountSlider->setValue(0);
    m_tintColorCombo->setCurrentIndex(0);

    m_brushSizeSlider->setValue(36);
    m_brushSoftnessSlider->setValue(65);
    m_brushStrengthSlider->setValue(80);

    m_outlineEditCheckBox->setChecked(false);
    m_manualRefineCheckBox->setChecked(false);
    m_showMaskCheckBox->setChecked(true);

    m_addBrushButton->setChecked(true);
    m_brightPreviewWidget->setBrushAddsToSelection(true);

    m_brightMask = QImage();
    m_brightPreviewImage = m_sourceImage;
    m_hasBrightSeed = false;
    m_brightMaskEditedByUser = false;
    m_lastBrightSeedPoint = QPoint();

    if (m_brightPreviewWidget) {
        m_brightPreviewWidget->setImage(m_sourceImage);
        m_brightPreviewWidget->clearMask();
        m_brightPreviewWidget->setOutlineEditEnabled(false);
        m_brightPreviewWidget->setManualRefineEnabled(false);
        m_brightPreviewWidget->setMaskVisible(true);
    }
}

bool OverexposureRecoveryDialog::applyCurrentRegionToWorkingImage()
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        return applyCurrentGlareRegionToWorkingImage();
    }

    return applyCurrentBrightAreaToWorkingImage();
}

bool OverexposureRecoveryDialog::applyCurrentGlareRegionToWorkingImage()
{
    const QRect selection = m_glarePreviewWidget->selectionRect();

    if (selection.isNull() || selection.isEmpty()) {
        QMessageBox::information(
            this,
            "Glare Recovery",
            "Draw around a glare area first."
            );

        return false;
    }

    // Commit exactly what the user is currently seeing.
    updateGlarePreview();

    m_sourceImage = m_resultImage;
    m_hasAppliedRegion = true;

    setPreviewImagesToSource();

    if (m_glarePreviewWidget) {
        m_glarePreviewWidget->clearSelection();
    }

    m_resultImage = m_sourceImage;

    return true;
}

bool OverexposureRecoveryDialog::applyCurrentBrightAreaToWorkingImage()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    QImage maskToApply;

    if (m_brightPreviewWidget) {
        maskToApply = m_brightPreviewWidget->mask();
    }

    if (maskToApply.isNull()) {
        maskToApply = m_brightMask;
    }

    if (maskToApply.isNull() || !BrightAreaTools::maskHasSelection(maskToApply)) {
        QMessageBox::information(
            this,
            "Bright Area Fade",
            "Click inside a blown bright area first, or use Edit Outline / Brush Refine to make a selection."
            );

        return false;
    }

    // Keep the real current mask.
    m_brightMask = maskToApply;

    // Bake the correction directly into the dialog working image.
    const QImage bakedImage = BrightAreaTools::applyBrightAreaFade(
        m_sourceImage,
        m_brightMask,
        currentBrightAreaSettings()
        );

    if (bakedImage.isNull()) {
        QMessageBox::warning(
            this,
            "Bright Area Fade",
            "The bright area fade could not be applied."
            );

        return false;
    }

    m_sourceImage = bakedImage;
    m_resultImage = bakedImage;
    m_brightPreviewImage = bakedImage;
    m_hasAppliedRegion = true;

    // Important:
    // Do NOT clear the mask here.
    // Apply Area should commit the floating image but keep the editable
    // selection alive so the user can keep refining that same area.
    m_hasBrightSeed = false;
    m_brightMaskEditedByUser = true;
    m_lastBrightSeedPoint = QPoint();

    if (m_brightPreviewWidget) {
        m_brightPreviewWidget->setImage(m_sourceImage);
        m_brightPreviewWidget->setMask(m_brightMask);
        m_brightPreviewWidget->setMaskVisible(true);
    }

    if (m_glarePreviewWidget) {
        m_glarePreviewWidget->setImage(m_sourceImage);
    }

    return true;
}

void OverexposureRecoveryDialog::setPreviewImagesToSource()
{
    if (m_glarePreviewWidget) {
        m_glarePreviewWidget->setImage(m_sourceImage);
    }

    if (m_brightPreviewWidget) {
        m_brightPreviewWidget->setImage(m_sourceImage);
    }
}

void OverexposureRecoveryDialog::clearActiveSelection()
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        if (m_glarePreviewWidget) {
            m_glarePreviewWidget->clearSelection();
        }

        return;
    }

    m_brightMask = QImage();
    m_hasBrightSeed = false;

    if (m_brightPreviewWidget) {
        m_brightPreviewWidget->clearMask();
    }
}

void OverexposureRecoveryDialog::showOriginalPressed()
{
    if (m_originalImage.isNull()) {
        return;
    }

    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        m_glarePreviewWidget->setImage(m_originalImage);
        updateGlareOverlay();
        return;
    }

    m_brightPreviewWidget->setImage(m_originalImage);
}

void OverexposureRecoveryDialog::showOriginalReleased()
{
    updateActivePreview();
}

void OverexposureRecoveryDialog::updateButtonLabels()
{
    if (!m_applyRegionButton || !m_tabWidget) {
        return;
    }

    if (m_tabWidget->currentIndex() == 0) {
        m_applyRegionButton->setText("Apply Region");
        m_applyRegionButton->setToolTip(
            "Apply this selected glare region and keep the dialog open"
            );
    } else {
        m_applyRegionButton->setText("Apply Area");
        m_applyRegionButton->setToolTip(
            "Apply this bright-area fade and keep the dialog open"
            );
    }
}
