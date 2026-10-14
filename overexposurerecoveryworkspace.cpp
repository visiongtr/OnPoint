#include "overexposurerecoveryworkspace.h"

#include "brightareaselectionwidget.h"
#include "overexposureselectionwidget.h"

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace
{

QIcon colorIcon(const QColor &color)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(color);
    return QIcon(pixmap);
}

QImage emptyMask(const QSize &size)
{
    if (size.isEmpty()) {
        return QImage();
    }

    QImage mask(size, QImage::Format_Grayscale8);
    mask.fill(0);
    return mask;
}

}

OverexposureRecoveryWorkspace::OverexposureRecoveryWorkspace(
    const QImage &sourceImage,
    OverexposureSelectionWidget *glareCanvas,
    BrightAreaSelectionWidget *brightCanvas,
    QWidget *parent
    )
    : QWidget(parent)
    , m_originalImage(sourceImage)
    , m_sourceImage(sourceImage)
    , m_resultImage(sourceImage)
    , m_glareCanvas(glareCanvas)
    , m_brightCanvas(brightCanvas)
{
    setObjectName("overexposureWorkspace");

    setupUi();
    resetGlareControls();
    resetBrightAreaControls();
    updateActivePreview();
}

QImage OverexposureRecoveryWorkspace::resultImage() const
{
    return m_resultImage;
}

bool OverexposureRecoveryWorkspace::hasAppliedChanges() const
{
    return m_hasAppliedRegion;
}

int OverexposureRecoveryWorkspace::activeTabIndex() const
{
    return m_tabWidget ? m_tabWidget->currentIndex() : 0;
}

void OverexposureRecoveryWorkspace::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    QWidget *headingRow = new QWidget(this);
    QHBoxLayout *headingLayout = new QHBoxLayout(headingRow);
    headingLayout->setContentsMargins(2, 0, 2, 0);
    headingLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel("Overexposure Recovery", headingRow);
    titleLabel->setObjectName("overexposureWorkspaceTitle");

    m_resetTabButton = new QPushButton("Reset Tab", headingRow);
    m_resetTabButton->setObjectName("overexposureResetButton");
    m_resetTabButton->setToolTip(
        "Reset the controls and active selection on the current tab. "
        "Areas already committed with Apply Area are not removed."
        );

    headingLayout->addWidget(titleLabel, 1);
    headingLayout->addWidget(m_resetTabButton);
    mainLayout->addWidget(headingRow);

    QLabel *instructionLabel = new QLabel(
        "Work directly on the main image. Zoom and scroll normally while the recovery controls remain here."
        );
    instructionLabel->setObjectName("overexposureWorkspaceInstruction");
    instructionLabel->setWordWrap(true);
    mainLayout->addWidget(instructionLabel);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName("overexposureTabWidget");

    QWidget *glareTab = new QWidget(m_tabWidget);
    QWidget *brightAreaTab = new QWidget(m_tabWidget);

    setupGlareTab(glareTab);
    setupBrightAreaTab(brightAreaTab);

    m_tabWidget->addTab(glareTab, "Glare Recovery");
    m_tabWidget->addTab(brightAreaTab, "Bright Area Fade");

    mainLayout->addWidget(m_tabWidget, 1);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(220);

    connect(
        m_previewTimer,
        &QTimer::timeout,
        this,
        &OverexposureRecoveryWorkspace::updateActivePreview
        );

    connect(
        m_resetTabButton,
        &QPushButton::clicked,
        this,
        &OverexposureRecoveryWorkspace::resetActiveControls
        );

    connect(
        m_tabWidget,
        &QTabWidget::currentChanged,
        this,
        [this](int tabIndex) {
            if (m_tintEyedropperButton && m_tintEyedropperButton->isChecked()) {
                QSignalBlocker blocker(m_tintEyedropperButton);
                m_tintEyedropperButton->setChecked(false);
            }

            if (m_brightCanvas) {
                m_brightCanvas->setColorSampleEnabled(false);
            }

            emit canvasModeChanged(tabIndex);
            emit applyButtonTextChanged(tabIndex == 0 ? "Apply Region" : "Apply Area");
            updateActivePreview();
        }
        );
}

QWidget *OverexposureRecoveryWorkspace::createScrollableTab(QWidget *tab)
{
    QVBoxLayout *tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    QScrollArea *scrollArea = new QScrollArea(tab);
    scrollArea->setObjectName("overexposureControlsScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *content = new QWidget(scrollArea);
    content->setObjectName("overexposureControlsContent");
    scrollArea->setWidget(content);

    tabLayout->addWidget(scrollArea);
    return content;
}

void OverexposureRecoveryWorkspace::setupGlareTab(QWidget *tab)
{
    QWidget *content = createScrollableTab(tab);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 6, 6, 8);
    layout->setSpacing(6);

    QLabel *instruction = new QLabel(
        "Draw around one glare spot on the main image. The blue rings show where surrounding color is sampled."
        );
    instruction->setWordWrap(true);
    instruction->setObjectName("overexposureTabInstruction");
    layout->addWidget(instruction);

    QGroupBox *selectionGroup = new QGroupBox("Selection & Sampling", content);
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionGroup);

    QWidget *shapeRow = new QWidget(selectionGroup);
    QHBoxLayout *shapeLayout = new QHBoxLayout(shapeRow);
    shapeLayout->setContentsMargins(0, 0, 0, 0);
    shapeLayout->setSpacing(4);

    QLabel *shapeLabel = new QLabel("Region Shape", shapeRow);
    shapeLabel->setMinimumWidth(108);

    m_glareShapeButtonGroup = new QButtonGroup(this);
    m_glareShapeButtonGroup->setExclusive(true);

    m_ovalShapeButton = new QPushButton("Oval", shapeRow);
    m_rectangleShapeButton = new QPushButton("Rect", shapeRow);
    m_squareShapeButton = new QPushButton("Square", shapeRow);

    for (QPushButton *button : {m_ovalShapeButton, m_rectangleShapeButton, m_squareShapeButton}) {
        button->setCheckable(true);
        button->setObjectName("overexposureModeButton");
        m_glareShapeButtonGroup->addButton(button);
    }

    shapeLayout->addWidget(shapeLabel);
    shapeLayout->addWidget(m_ovalShapeButton);
    shapeLayout->addWidget(m_rectangleShapeButton);
    shapeLayout->addWidget(m_squareShapeButton);

    selectionLayout->addWidget(shapeRow);

    createControlRow(
        selectionLayout,
        "Threshold",
        "Controls how strongly the selected pixels must read as glare before recovery is applied.",
        0, 100, 75,
        &m_thresholdSlider,
        &m_thresholdSpinBox
        );

    createControlRow(
        selectionLayout,
        "Sample Distance",
        "Moves the surrounding color-sample ring farther from or closer to the selected glare.",
        0, 100, 30,
        &m_sampleDistanceSlider,
        &m_sampleDistanceSpinBox
        );

    createControlRow(
        selectionLayout,
        "Sample Width",
        "Changes the thickness of the surrounding area used to estimate replacement color.",
        0, 100, 20,
        &m_sampleWidthSlider,
        &m_sampleWidthSpinBox
        );

    layout->addWidget(selectionGroup);

    QGroupBox *recoveryGroup = new QGroupBox("Recovery", content);
    QVBoxLayout *recoveryLayout = new QVBoxLayout(recoveryGroup);

    createControlRow(
        recoveryLayout,
        "Recovery Amount",
        "Sets how strongly On Point pulls the glare back toward surrounding detail and tone.",
        0, 100, 45,
        &m_recoveryAmountSlider,
        &m_recoveryAmountSpinBox
        );

    createControlRow(
        recoveryLayout,
        "Color Bleed",
        "Allows more surrounding color to enter the repaired glare region.",
        0, 100, 55,
        &m_colorBleedSlider,
        &m_colorBleedSpinBox
        );

    createControlRow(
        recoveryLayout,
        "Transparency",
        "Blends the recovered glare with the original so the correction does not look pasted over.",
        0, 100, 75,
        &m_transparencySlider,
        &m_transparencySpinBox
        );

    createControlRow(
        recoveryLayout,
        "Soft Edge",
        "Feathers the boundary between the corrected glare and the untouched photograph.",
        0, 100, 55,
        &m_softEdgeSlider,
        &m_softEdgeSpinBox
        );

    createControlRow(
        recoveryLayout,
        "Preserve Shine",
        "Keeps a controlled amount of natural highlight so reflective surfaces still look reflective.",
        0, 100, 35,
        &m_preserveShineSlider,
        &m_preserveShineSpinBox
        );

    layout->addWidget(recoveryGroup);
    layout->addStretch();

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

    if (m_glareCanvas) {
        connect(
            m_glareCanvas,
            &OverexposureSelectionWidget::selectionChanged,
            this,
            &OverexposureRecoveryWorkspace::updateGlarePreview
            );
    }
}

void OverexposureRecoveryWorkspace::setupBrightAreaTab(QWidget *tab)
{
    QWidget *content = createScrollableTab(tab);
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 6, 6, 8);
    layout->setSpacing(6);

    QLabel *instruction = new QLabel(
        "Choose + Add Area or − Subtract Area, then click the main image. Add as many separate bright regions as needed."
        );
    instruction->setWordWrap(true);
    instruction->setObjectName("overexposureTabInstruction");
    layout->addWidget(instruction);

    QGroupBox *selectionGroup = new QGroupBox("Selection", content);
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionGroup);

    QWidget *areaModeRow = new QWidget(selectionGroup);
    QHBoxLayout *areaModeLayout = new QHBoxLayout(areaModeRow);
    areaModeLayout->setContentsMargins(0, 0, 0, 0);
    areaModeLayout->setSpacing(6);

    m_areaModeButtonGroup = new QButtonGroup(this);
    m_areaModeButtonGroup->setExclusive(false);

    m_addAreaButton = new QPushButton("+ Add Area", areaModeRow);
    m_subtractAreaButton = new QPushButton("− Subtract Area", areaModeRow);

    m_addAreaButton->setCheckable(true);
    m_subtractAreaButton->setCheckable(true);
    m_addAreaButton->setObjectName("overexposureAreaModeButton");
    m_subtractAreaButton->setObjectName("overexposureAreaModeButton");

    m_areaModeButtonGroup->addButton(m_addAreaButton);
    m_areaModeButtonGroup->addButton(m_subtractAreaButton);

    areaModeLayout->addWidget(m_addAreaButton);
    areaModeLayout->addWidget(m_subtractAreaButton);
    selectionLayout->addWidget(areaModeRow);

    createControlRow(
        selectionLayout,
        "Threshold",
        "Higher values select only the brightest connected pixels. Lower values allow more surrounding light tones into each clicked area.",
        0, 100, 75,
        &m_brightThresholdSlider,
        &m_brightThresholdSpinBox
        );

    createControlRow(
        selectionLayout,
        "Grow / Shrink",
        "Expands or contracts the completed selection before the soft edge is applied. Positive grows; negative shrinks.",
        -100, 100, 0,
        &m_growShrinkSlider,
        &m_growShrinkSpinBox
        );

    layout->addWidget(selectionGroup);

    QGroupBox *fadeGroup = new QGroupBox("Recovery / Fade / Tint", content);
    QVBoxLayout *fadeLayout = new QVBoxLayout(fadeGroup);

    createControlRow(
        fadeLayout,
        "Recovery Amount",
        "Controls how much brightness is pulled back inside the selected area.",
        0, 100, 45,
        &m_brightRecoveryAmountSlider,
        &m_brightRecoveryAmountSpinBox
        );

    createControlRow(
        fadeLayout,
        "Blend",
        "Controls how strongly the recovered result replaces the original bright pixels.",
        0, 100, 75,
        &m_brightBlendSlider,
        &m_brightBlendSpinBox
        );

    createControlRow(
        fadeLayout,
        "Soft Edge",
        "Feathers the selection boundary so the recovery fades naturally into the photograph.",
        0, 100, 55,
        &m_brightSoftEdgeSlider,
        &m_brightSoftEdgeSpinBox
        );

    createControlRow(
        fadeLayout,
        "Preserve Glow",
        "Protects natural brightness so windows, skies, and light sources do not become flat gray patches.",
        0, 100, 45,
        &m_preserveGlowSlider,
        &m_preserveGlowSpinBox
        );

    createControlRow(
        fadeLayout,
        "Color Tint",
        "Introduces the selected tint while preserving the recovered luminance. The slider track shows the active tint color.",
        0, 100, 0,
        &m_tintAmountSlider,
        &m_tintAmountSpinBox
        );

    m_tintAmountSlider->setObjectName("overexposureTintSlider");

    QWidget *tintRow = new QWidget(fadeGroup);
    QHBoxLayout *tintLayout = new QHBoxLayout(tintRow);
    tintLayout->setContentsMargins(0, 0, 0, 0);
    tintLayout->setSpacing(5);

    QLabel *tintLabel = new QLabel("Tint Color", tintRow);
    tintLabel->setMinimumWidth(108);

    m_tintColorCombo = new QComboBox(tintRow);
    m_tintColorCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_tintColorCombo->setToolTip(
        "Choose a preset tint. Each entry includes a color block, or use the eyedropper to sample a color from the main image."
        );

    addTintChoice("Sky Blue", QColor(135, 190, 255));
    addTintChoice("Warm Daylight", QColor(255, 215, 150));
    addTintChoice("Neutral Gray", QColor(210, 210, 210));
    addTintChoice("Soft Cyan", QColor(150, 230, 235));
    addTintChoice("Pale Rose", QColor(235, 175, 180));
    addTintChoice("Soft Green", QColor(165, 215, 170));

    m_tintColorSwatch = new QLabel(tintRow);
    m_tintColorSwatch->setObjectName("overexposureTintSwatch");
    m_tintColorSwatch->setFixedSize(30, 24);
    m_tintColorSwatch->setToolTip("Current tint color");

    m_tintEyedropperButton = new QPushButton("Pick", tintRow);
    m_tintEyedropperButton->setObjectName("overexposureEyedropperButton");
    m_tintEyedropperButton->setCheckable(true);
    m_tintEyedropperButton->setToolTip(
        "Click Pick, then click a color on the main image to use it as the fade tint."
        );

    tintLayout->addWidget(tintLabel);
    tintLayout->addWidget(m_tintColorCombo, 1);
    tintLayout->addWidget(m_tintColorSwatch);
    tintLayout->addWidget(m_tintEyedropperButton);
    fadeLayout->addWidget(tintRow);

    layout->addWidget(fadeGroup);

    QGroupBox *manualGroup = new QGroupBox("Manual Refine", content);
    QVBoxLayout *manualLayout = new QVBoxLayout(manualGroup);

    m_manualRefineCheckBox = new QCheckBox("Enable Manual Refine", manualGroup);
    m_manualRefineCheckBox->setToolTip(
        "Enable brush editing on the full-size main image. The controls below remain disabled until this is checked."
        );
    manualLayout->addWidget(m_manualRefineCheckBox);

    m_manualRefineControls = new QWidget(manualGroup);
    m_manualRefineControls->setObjectName("overexposureDependentControls");
    QVBoxLayout *manualControlsLayout = new QVBoxLayout(m_manualRefineControls);
    manualControlsLayout->setContentsMargins(0, 2, 0, 0);
    manualControlsLayout->setSpacing(5);

    QWidget *brushModeRow = new QWidget(m_manualRefineControls);
    QHBoxLayout *brushModeLayout = new QHBoxLayout(brushModeRow);
    brushModeLayout->setContentsMargins(0, 0, 0, 0);
    brushModeLayout->setSpacing(5);

    m_brushModeButtonGroup = new QButtonGroup(this);
    m_brushModeButtonGroup->setExclusive(true);

    m_addBrushButton = new QPushButton("Add to Mask", brushModeRow);
    m_removeBrushButton = new QPushButton("Remove from Mask", brushModeRow);
    m_addBrushButton->setCheckable(true);
    m_removeBrushButton->setCheckable(true);
    m_addBrushButton->setObjectName("overexposureModeButton");
    m_removeBrushButton->setObjectName("overexposureModeButton");

    m_brushModeButtonGroup->addButton(m_addBrushButton);
    m_brushModeButtonGroup->addButton(m_removeBrushButton);

    brushModeLayout->addWidget(m_addBrushButton);
    brushModeLayout->addWidget(m_removeBrushButton);
    manualControlsLayout->addWidget(brushModeRow);

    m_showMaskCheckBox = new QCheckBox("Show Mask", m_manualRefineControls);
    m_showMaskCheckBox->setChecked(true);
    m_showMaskCheckBox->setToolTip("Show or hide the blue selection overlay while refining.");
    manualControlsLayout->addWidget(m_showMaskCheckBox);

    createControlRow(
        manualControlsLayout,
        "Brush Size",
        "Changes the brush diameter on the main image.",
        1, 200, 36,
        &m_brushSizeSlider,
        &m_brushSizeSpinBox
        );

    createControlRow(
        manualControlsLayout,
        "Brush Soft Edge",
        "Controls how gradually the brush fades at its edge.",
        0, 100, 65,
        &m_brushSoftnessSlider,
        &m_brushSoftnessSpinBox
        );

    createControlRow(
        manualControlsLayout,
        "Brush Strength",
        "Controls how much mask is added or removed with each brush pass.",
        0, 100, 80,
        &m_brushStrengthSlider,
        &m_brushStrengthSpinBox
        );

    manualLayout->addWidget(m_manualRefineControls);
    layout->addWidget(manualGroup);

    QGroupBox *outlineGroup = new QGroupBox("Edit Outline", content);
    QVBoxLayout *outlineLayout = new QVBoxLayout(outlineGroup);

    m_outlineEditCheckBox = new QCheckBox("Enable Outline Editing", outlineGroup);
    m_outlineEditCheckBox->setToolTip(
        "Convert the current mask boundary into draggable points on the main image."
        );
    outlineLayout->addWidget(m_outlineEditCheckBox);

    m_outlineEditControls = new QWidget(outlineGroup);
    m_outlineEditControls->setObjectName("overexposureDependentControls");
    QHBoxLayout *outlineControlsLayout = new QHBoxLayout(m_outlineEditControls);
    outlineControlsLayout->setContentsMargins(0, 2, 0, 0);
    outlineControlsLayout->setSpacing(5);

    QLabel *outlineHelp = new QLabel(
        "Drag points normally. Arm one of these buttons for the next click:",
        m_outlineEditControls
        );
    outlineHelp->setWordWrap(true);

    m_addNodeButton = new QPushButton("Add Node", m_outlineEditControls);
    m_removeNodeButton = new QPushButton("Remove Node", m_outlineEditControls);
    m_addNodeButton->setToolTip("Click, then click the outline where a new control point is needed.");
    m_removeNodeButton->setToolTip("Click, then click an existing outline point to remove it.");

    QVBoxLayout *outlineButtonLayout = new QVBoxLayout;
    outlineButtonLayout->setContentsMargins(0, 0, 0, 0);
    outlineButtonLayout->setSpacing(4);
    outlineButtonLayout->addWidget(m_addNodeButton);
    outlineButtonLayout->addWidget(m_removeNodeButton);

    outlineControlsLayout->addWidget(outlineHelp, 1);
    outlineControlsLayout->addLayout(outlineButtonLayout);
    outlineLayout->addWidget(m_outlineEditControls);

    layout->addWidget(outlineGroup);
    layout->addStretch();

    m_manualRefineControls->setEnabled(false);
    m_outlineEditControls->setEnabled(false);

    connect(m_addAreaButton, &QPushButton::clicked,
            this, [this](bool checked) {
                setBrightAreaMode(checked
                                      ? BrightAreaMode::Add
                                      : BrightAreaMode::None);
            });

    connect(m_subtractAreaButton, &QPushButton::clicked,
            this, [this](bool checked) {
                setBrightAreaMode(checked
                                      ? BrightAreaMode::Subtract
                                      : BrightAreaMode::None);
            });

    connect(m_manualRefineCheckBox, &QCheckBox::toggled,
            this, &OverexposureRecoveryWorkspace::setManualRefineEnabled);

    connect(m_outlineEditCheckBox, &QCheckBox::toggled,
            this, &OverexposureRecoveryWorkspace::setOutlineEditEnabled);

    connect(m_showMaskCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (m_brightCanvas) {
                    m_brightCanvas->setMaskVisible(checked);
                }
            });

    connect(m_addBrushButton, &QPushButton::clicked,
            this, [this]() {
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushAddsToSelection(true);
                }
            });

    connect(m_removeBrushButton, &QPushButton::clicked,
            this, [this]() {
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushAddsToSelection(false);
                }
            });

    connect(m_addNodeButton, &QPushButton::clicked,
            this, [this]() {
                if (m_brightCanvas) {
                    m_brightCanvas->armAddOutlineNode();
                    emit statusMessage("Click the outline where the new node should be added");
                }
            });

    connect(m_removeNodeButton, &QPushButton::clicked,
            this, [this]() {
                if (m_brightCanvas) {
                    m_brightCanvas->armRemoveOutlineNode();
                    emit statusMessage("Click an outline node to remove it");
                }
            });

    connect(m_tintColorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
                updateTintUi();
                updateBrightAreaPreview();
            });

    connect(m_tintEyedropperButton, &QPushButton::toggled,
            this, [this](bool checked) {
                if (checked) {
                    setManualRefineEnabled(false);
                    setOutlineEditEnabled(false);
                    emit statusMessage("Tint eyedropper active — click a color on the main image");
                }

                if (m_brightCanvas) {
                    m_brightCanvas->setColorSampleEnabled(checked);
                }
            });

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
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushSize(value);
                }
            });

    connect(m_brushSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSizeSlider);
                m_brushSizeSlider->setValue(value);
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushSize(value);
                }
            });

    connect(m_brushSoftnessSlider, &QSlider::valueChanged,
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSoftnessSpinBox);
                m_brushSoftnessSpinBox->setValue(value);
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushSoftness(value);
                }
            });

    connect(m_brushSoftnessSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushSoftnessSlider);
                m_brushSoftnessSlider->setValue(value);
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushSoftness(value);
                }
            });

    connect(m_brushStrengthSlider, &QSlider::valueChanged,
            this, [this](int value) {
                QSignalBlocker blocker(m_brushStrengthSpinBox);
                m_brushStrengthSpinBox->setValue(value);
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushStrength(value);
                }
            });

    connect(m_brushStrengthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                QSignalBlocker blocker(m_brushStrengthSlider);
                m_brushStrengthSlider->setValue(value);
                if (m_brightCanvas) {
                    m_brightCanvas->setBrushStrength(value);
                }
            });

    for (QWidget *widget : {
             static_cast<QWidget *>(m_brushSizeSlider),
             static_cast<QWidget *>(m_brushSizeSpinBox),
             static_cast<QWidget *>(m_brushSoftnessSlider),
             static_cast<QWidget *>(m_brushSoftnessSpinBox),
             static_cast<QWidget *>(m_brushStrengthSlider),
             static_cast<QWidget *>(m_brushStrengthSpinBox)}) {
        installNoWheelFilter(widget);
    }

    if (m_brightCanvas) {
        connect(
            m_brightCanvas,
            &BrightAreaSelectionWidget::autoSelectRequested,
            this,
            &OverexposureRecoveryWorkspace::selectBrightArea
            );

        connect(
            m_brightCanvas,
            &BrightAreaSelectionWidget::colorSampleRequested,
            this,
            &OverexposureRecoveryWorkspace::sampleTintColor
            );

        connect(
            m_brightCanvas,
            &BrightAreaSelectionWidget::maskEdited,
            this,
            [this](const QImage &mask) {
                m_brightMask = mask;
                m_brightMaskEditedByUser = true;
                m_brightAreaSeeds.clear();
                updateBrightAreaPreview();
            }
            );

        connect(
            m_brightCanvas,
            &BrightAreaSelectionWidget::outlineNodeActionCompleted,
            this,
            [this]() {
                emit statusMessage("Outline node edit complete — drag nodes normally or arm another action");
            }
            );
    }

    updateTintUi();
}

QWidget *OverexposureRecoveryWorkspace::createControlRow(
    QVBoxLayout *layout,
    const QString &labelText,
    const QString &toolTip,
    int minimum,
    int maximum,
    int value,
    QSlider **sliderOut,
    QSpinBox **spinBoxOut
    )
{
    QWidget *row = new QWidget(this);
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(5);

    QLabel *label = new QLabel(labelText, row);
    label->setMinimumWidth(108);
    label->setToolTip(toolTip);

    QSlider *slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    slider->setToolTip(toolTip);

    QSpinBox *spinBox = new QSpinBox(row);
    spinBox->setRange(minimum, maximum);
    spinBox->setValue(value);
    spinBox->setMinimumWidth(54);
    spinBox->setToolTip(toolTip);

    rowLayout->addWidget(label);
    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(spinBox);
    layout->addWidget(row);

    installNoWheelFilter(slider);
    installNoWheelFilter(spinBox);

    *sliderOut = slider;
    *spinBoxOut = spinBox;
    return row;
}

void OverexposureRecoveryWorkspace::connectSliderSpinBox(
    QSlider *slider,
    QSpinBox *spinBox
    )
{
    connect(slider, &QSlider::valueChanged,
            this, [this, spinBox](int value) {
                QSignalBlocker blocker(spinBox);
                spinBox->setValue(value);
                scheduleActivePreview();
            });

    connect(slider, &QSlider::sliderReleased,
            this, [this]() {
                if (m_previewPending) {
                    updateActivePreview();
                }
            });

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, slider](int value) {
                QSignalBlocker blocker(slider);
                slider->setValue(value);
                scheduleActivePreview();
            });

    connect(spinBox, &QSpinBox::editingFinished,
            this, [this]() {
                if (m_previewPending) {
                    updateActivePreview();
                }
            });
}

void OverexposureRecoveryWorkspace::installNoWheelFilter(QWidget *widget)
{
    if (widget) {
        widget->installEventFilter(this);
    }
}

bool OverexposureRecoveryWorkspace::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel &&
        (qobject_cast<QSlider *>(watched) ||
         qobject_cast<QAbstractSpinBox *>(watched))) {

        QWidget *control = qobject_cast<QWidget *>(watched);
        QAbstractScrollArea *scrollArea = nullptr;

        for (QWidget *parentWidget = control ? control->parentWidget() : nullptr;
             parentWidget;
             parentWidget = parentWidget->parentWidget()) {
            scrollArea = qobject_cast<QAbstractScrollArea *>(parentWidget);
            if (scrollArea) {
                break;
            }
        }

        if (scrollArea) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
            QScrollBar *bar = scrollArea->verticalScrollBar();
            int amount = 0;

            if (!wheelEvent->pixelDelta().isNull()) {
                amount = wheelEvent->pixelDelta().y();
            } else {
                amount = (wheelEvent->angleDelta().y() / 120) * bar->singleStep() * 3;
            }

            bar->setValue(bar->value() - amount);
        }

        event->accept();
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void OverexposureRecoveryWorkspace::setGlareRegionShape(
    OverexposureTools::RegionShape shape
    )
{
    m_glareRegionShape = shape;

    m_ovalShapeButton->setChecked(shape == OverexposureTools::RegionShape::Oval);
    m_rectangleShapeButton->setChecked(shape == OverexposureTools::RegionShape::Rectangle);
    m_squareShapeButton->setChecked(shape == OverexposureTools::RegionShape::Square);

    updateGlareOverlay();
    updateGlarePreview();
}

OverexposureTools::OverexposureSettings
OverexposureRecoveryWorkspace::currentGlareSettings() const
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

BrightAreaTools::BrightAreaSettings
OverexposureRecoveryWorkspace::currentBrightAreaSettings() const
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

void OverexposureRecoveryWorkspace::scheduleActivePreview()
{
    m_previewPending = true;

    if (!m_previewTimer) {
        updateActivePreview();
        return;
    }

    m_previewTimer->start();
}

void OverexposureRecoveryWorkspace::updateActivePreview()
{
    if (m_previewTimer && m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        updateGlarePreview();
    } else {
        updateBrightAreaPreview();
    }
}

void OverexposureRecoveryWorkspace::updateGlareOverlay()
{
    if (!m_glareCanvas) {
        return;
    }

    m_glareCanvas->setOverlaySettings(
        m_sampleDistanceSlider->value(),
        m_sampleWidthSlider->value(),
        m_glareRegionShape
        );
}

void OverexposureRecoveryWorkspace::updateGlarePreview()
{
    if (!m_glareCanvas) {
        return;
    }

    updateGlareOverlay();

    if (m_showingOriginal) {
        m_glareCanvas->setImage(m_originalImage);
        return;
    }

    const QRect selection = m_glareCanvas->selectionRect();

    if (selection.isNull() || selection.isEmpty()) {
        m_resultImage = m_sourceImage;
        m_glareCanvas->setImage(m_sourceImage);
        return;
    }

    m_resultImage = OverexposureTools::recoverOverexposure(
        m_sourceImage,
        selection,
        currentGlareSettings()
        );

    m_glareCanvas->setImage(m_resultImage);
}

void OverexposureRecoveryWorkspace::updateBrightAreaPreview()
{
    if (!m_brightCanvas) {
        return;
    }

    if (!m_brightMaskEditedByUser && !m_brightAreaSeeds.isEmpty()) {
        rebuildBrightMaskFromSeeds();
    }

    if (m_showingOriginal) {
        m_brightCanvas->setImage(m_originalImage);
        m_brightCanvas->setMask(m_brightMask);
        return;
    }

    if (m_brightMask.isNull() ||
        !BrightAreaTools::maskHasSelection(m_brightMask)) {
        m_brightPreviewImage = m_sourceImage;
        m_resultImage = m_sourceImage;
        m_brightCanvas->setImage(m_sourceImage);
        m_brightCanvas->setMask(m_brightMask);
        return;
    }

    m_brightPreviewImage = BrightAreaTools::applyBrightAreaFade(
        m_sourceImage,
        m_brightMask,
        currentBrightAreaSettings()
        );

    m_resultImage = m_brightPreviewImage;
    m_brightCanvas->setImage(m_brightPreviewImage);
    m_brightCanvas->setMask(m_brightMask);
}

void OverexposureRecoveryWorkspace::selectBrightArea(const QPoint &imagePoint)
{
    if (m_sourceImage.isNull()) {
        return;
    }

    if (m_brightAreaMode == BrightAreaMode::None) {
        emit statusMessage("Choose + Add Area or − Subtract Area before clicking the image");
        return;
    }

    const QImage candidate = BrightAreaTools::createBrightAreaMask(
        m_sourceImage,
        imagePoint,
        m_brightThresholdSlider->value()
        );

    if (!BrightAreaTools::maskHasSelection(candidate)) {
        emit statusMessage("No connected bright area was found at that point — lower Threshold or click closer to the highlight");
        return;
    }

    if (m_brightMaskEditedByUser) {
        combineBrightMask(candidate, m_brightAreaMode);
    } else {
        m_brightAreaSeeds.append({imagePoint, m_brightAreaMode});
        rebuildBrightMaskFromSeeds();
    }

    if (m_brightCanvas) {
        m_brightCanvas->setMask(m_brightMask);
    }

    updateBrightAreaPreview();

    emit statusMessage(
        m_brightAreaMode == BrightAreaMode::Add
            ? "Bright area added to the selection"
            : "Bright area subtracted from the selection"
        );
}

void OverexposureRecoveryWorkspace::rebuildBrightMaskFromSeeds()
{
    m_brightMask = emptyMask(m_sourceImage.size());

    for (const AreaSeed &seed : m_brightAreaSeeds) {
        const QImage candidate = BrightAreaTools::createBrightAreaMask(
            m_sourceImage,
            seed.point,
            m_brightThresholdSlider->value()
            );

        combineBrightMask(candidate, seed.mode);
    }

    if (m_brightCanvas) {
        m_brightCanvas->setMask(m_brightMask);
    }
}

void OverexposureRecoveryWorkspace::combineBrightMask(
    const QImage &candidateMask,
    BrightAreaMode mode
    )
{
    if (candidateMask.isNull() || candidateMask.size() != m_sourceImage.size()) {
        return;
    }

    QImage candidate = candidateMask.convertToFormat(QImage::Format_Grayscale8);

    if (m_brightMask.isNull() || m_brightMask.size() != candidate.size()) {
        m_brightMask = emptyMask(candidate.size());
    } else if (m_brightMask.format() != QImage::Format_Grayscale8) {
        m_brightMask = m_brightMask.convertToFormat(QImage::Format_Grayscale8);
    }

    for (int y = 0; y < m_brightMask.height(); ++y) {
        uchar *targetLine = m_brightMask.scanLine(y);
        const uchar *candidateLine = candidate.constScanLine(y);

        for (int x = 0; x < m_brightMask.width(); ++x) {
            if (mode == BrightAreaMode::Add) {
                targetLine[x] = static_cast<uchar>(
                    std::max(static_cast<int>(targetLine[x]),
                             static_cast<int>(candidateLine[x]))
                    );
            } else if (candidateLine[x] > 0) {
                targetLine[x] = 0;
            }
        }
    }
}

void OverexposureRecoveryWorkspace::clearBrightSelection()
{
    m_brightMask = QImage();
    m_brightPreviewImage = m_sourceImage;
    m_brightAreaSeeds.clear();
    m_brightMaskEditedByUser = false;

    if (m_brightCanvas) {
        m_brightCanvas->clearMask();
        m_brightCanvas->setImage(m_sourceImage);
    }
}

void OverexposureRecoveryWorkspace::setBrightAreaMode(BrightAreaMode mode)
{
    m_brightAreaMode = mode;

    {
        QSignalBlocker addBlocker(m_addAreaButton);
        QSignalBlocker subtractBlocker(m_subtractAreaButton);
        m_addAreaButton->setChecked(mode == BrightAreaMode::Add);
        m_subtractAreaButton->setChecked(mode == BrightAreaMode::Subtract);
    }

    if (mode == BrightAreaMode::Add) {
        emit statusMessage("Add Area active — click one or more bright regions on the main image");
    } else if (mode == BrightAreaMode::Subtract) {
        emit statusMessage("Subtract Area active — click a connected region to remove it from the current selection");
    } else {
        emit statusMessage("Automatic selection mode off — choose + Add Area or − Subtract Area to continue");
    }
}

void OverexposureRecoveryWorkspace::setManualRefineEnabled(bool enabled)
{
    if (!m_manualRefineCheckBox || !m_manualRefineControls) {
        return;
    }

    if (enabled) {
        if (m_outlineEditCheckBox && m_outlineEditCheckBox->isChecked()) {
            QSignalBlocker blocker(m_outlineEditCheckBox);
            m_outlineEditCheckBox->setChecked(false);
            m_outlineEditControls->setEnabled(false);
        }

        if (m_tintEyedropperButton && m_tintEyedropperButton->isChecked()) {
            QSignalBlocker blocker(m_tintEyedropperButton);
            m_tintEyedropperButton->setChecked(false);
        }
    }

    if (m_manualRefineCheckBox->isChecked() != enabled) {
        QSignalBlocker blocker(m_manualRefineCheckBox);
        m_manualRefineCheckBox->setChecked(enabled);
    }

    m_manualRefineControls->setEnabled(enabled);

    if (m_brightCanvas) {
        m_brightCanvas->setColorSampleEnabled(false);
        m_brightCanvas->setOutlineEditEnabled(false);
        m_brightCanvas->setManualRefineEnabled(enabled);
    }

    if (enabled) {
        emit statusMessage("Manual Refine active — paint directly on the main image");
    }
}

void OverexposureRecoveryWorkspace::setOutlineEditEnabled(bool enabled)
{
    if (!m_outlineEditCheckBox || !m_outlineEditControls) {
        return;
    }

    if (enabled) {
        if (m_manualRefineCheckBox && m_manualRefineCheckBox->isChecked()) {
            QSignalBlocker blocker(m_manualRefineCheckBox);
            m_manualRefineCheckBox->setChecked(false);
            m_manualRefineControls->setEnabled(false);
        }

        if (m_tintEyedropperButton && m_tintEyedropperButton->isChecked()) {
            QSignalBlocker blocker(m_tintEyedropperButton);
            m_tintEyedropperButton->setChecked(false);
        }
    }

    if (m_outlineEditCheckBox->isChecked() != enabled) {
        QSignalBlocker blocker(m_outlineEditCheckBox);
        m_outlineEditCheckBox->setChecked(enabled);
    }

    m_outlineEditControls->setEnabled(enabled);

    if (m_brightCanvas) {
        m_brightCanvas->setColorSampleEnabled(false);
        m_brightCanvas->setManualRefineEnabled(false);
        m_brightCanvas->setOutlineEditEnabled(enabled);
    }

    if (enabled) {
        emit statusMessage("Outline editing active — drag the yellow nodes on the main image");
    }
}

void OverexposureRecoveryWorkspace::sampleTintColor(const QPoint &imagePoint)
{
    if (m_sourceImage.isNull() ||
        !QRect(QPoint(0, 0), m_sourceImage.size()).contains(imagePoint)) {
        return;
    }

    const QColor color = QColor::fromRgb(m_sourceImage.pixel(imagePoint));

    if (m_sampledTintIndex < 0) {
        m_sampledTintIndex = m_tintColorCombo->count();
        m_tintColorCombo->addItem(colorIcon(color), "Sampled Color", color);
    } else {
        m_tintColorCombo->setItemIcon(m_sampledTintIndex, colorIcon(color));
        m_tintColorCombo->setItemData(m_sampledTintIndex, color);
    }

    m_tintColorCombo->setCurrentIndex(m_sampledTintIndex);

    {
        QSignalBlocker blocker(m_tintEyedropperButton);
        m_tintEyedropperButton->setChecked(false);
    }

    if (m_brightCanvas) {
        m_brightCanvas->setColorSampleEnabled(false);
    }

    updateTintUi();
    updateBrightAreaPreview();

    emit statusMessage(
        QString("Tint sampled: %1").arg(color.name(QColor::HexRgb).toUpper())
        );
}

void OverexposureRecoveryWorkspace::addTintChoice(
    const QString &name,
    const QColor &color
    )
{
    m_tintColorCombo->addItem(colorIcon(color), name, color);
}

void OverexposureRecoveryWorkspace::updateTintUi()
{
    if (!m_tintColorCombo || !m_tintColorSwatch) {
        return;
    }

    const QColor color = m_tintColorCombo->currentData().value<QColor>();

    m_tintColorSwatch->setStyleSheet(
        QString(
            "QLabel#overexposureTintSwatch {"
            " background-color: %1;"
            " border: 1px solid #72787C;"
            " border-radius: 3px;"
            "}"
            ).arg(color.name(QColor::HexRgb))
        );

    updateTintSliderStyle(color);
}

void OverexposureRecoveryWorkspace::updateTintSliderStyle(const QColor &color)
{
    if (!m_tintAmountSlider) {
        return;
    }

    const QString colorName = color.name(QColor::HexRgb);

    m_tintAmountSlider->setStyleSheet(
        QString(
            "QSlider#overexposureTintSlider { background: transparent; }"
            "QSlider#overexposureTintSlider::groove:horizontal {"
            " height: 4px; border: none; border-radius: 2px;"
            " background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            " stop:0 #303438, stop:1 %1);"
            "}"
            "QSlider#overexposureTintSlider::sub-page:horizontal,"
            "QSlider#overexposureTintSlider::add-page:horizontal {"
            " height: 4px; border: none; border-radius: 2px;"
            " background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            " stop:0 #303438, stop:1 %1);"
            "}"
            "QSlider#overexposureTintSlider::handle:horizontal {"
            " width: 8px; margin: -6px 0; border-radius: 4px;"
            " background: %1; border: 1px solid #DDE2E4;"
            "}"
            ).arg(colorName)
        );
}

void OverexposureRecoveryWorkspace::resetActiveControls()
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        resetGlareControls();
        updateGlarePreview();
        emit statusMessage("Glare Recovery controls reset");
        return;
    }

    resetBrightAreaControls();
    updateBrightAreaPreview();
    emit statusMessage("Bright Area Fade controls and active selection reset");
}

void OverexposureRecoveryWorkspace::resetGlareControls()
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

    if (m_glareCanvas) {
        m_glareCanvas->setImage(m_sourceImage);
        m_glareCanvas->clearSelection();
    }
}

void OverexposureRecoveryWorkspace::resetBrightAreaControls()
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

    // Bright Area Fade should be ready for the most common first action:
    // click a blown region to add it to the selection. The mode can still
    // be toggled off or switched to Subtract Area at any time.
    setBrightAreaMode(BrightAreaMode::Add);

    {
        QSignalBlocker manualBlocker(m_manualRefineCheckBox);
        QSignalBlocker outlineBlocker(m_outlineEditCheckBox);
        m_manualRefineCheckBox->setChecked(false);
        m_outlineEditCheckBox->setChecked(false);
    }

    m_manualRefineControls->setEnabled(false);
    m_outlineEditControls->setEnabled(false);

    m_showMaskCheckBox->setChecked(true);
    m_addBrushButton->setChecked(true);
    m_removeBrushButton->setChecked(false);

    clearBrightSelection();

    if (m_brightCanvas) {
        m_brightCanvas->setBrushAddsToSelection(true);
        m_brightCanvas->setBrushSize(36);
        m_brightCanvas->setBrushSoftness(65);
        m_brightCanvas->setBrushStrength(80);
        m_brightCanvas->setMaskVisible(true);
        m_brightCanvas->setManualRefineEnabled(false);
        m_brightCanvas->setOutlineEditEnabled(false);
        m_brightCanvas->setColorSampleEnabled(false);
    }

    if (m_tintEyedropperButton) {
        QSignalBlocker blocker(m_tintEyedropperButton);
        m_tintEyedropperButton->setChecked(false);
    }

    updateTintUi();
}

bool OverexposureRecoveryWorkspace::applyCurrentRegion()
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        return applyCurrentGlareRegion();
    }

    return applyCurrentBrightArea();
}

bool OverexposureRecoveryWorkspace::applyCurrentGlareRegion()
{
    if (!m_glareCanvas) {
        return false;
    }

    const QRect selection = m_glareCanvas->selectionRect();

    if (selection.isNull() || selection.isEmpty()) {
        QMessageBox::information(
            this,
            "Glare Recovery",
            "Draw around a glare area on the main image first."
            );
        return false;
    }

    updateGlarePreview();
    m_sourceImage = m_resultImage;
    m_hasAppliedRegion = true;

    m_glareCanvas->clearSelection();
    setCanvasImagesToSource();
    m_resultImage = m_sourceImage;

    emit statusMessage("Glare region applied — draw another region or click Finish");
    return true;
}

bool OverexposureRecoveryWorkspace::applyCurrentBrightArea()
{
    if (m_previewTimer && m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }
    m_previewPending = false;

    if (m_brightCanvas && !m_brightCanvas->mask().isNull()) {
        m_brightMask = m_brightCanvas->mask();
    }

    if (m_brightMask.isNull() ||
        !BrightAreaTools::maskHasSelection(m_brightMask)) {
        QMessageBox::information(
            this,
            "Bright Area Fade",
            "Use + Add Area on the main image, or enable Manual Refine to paint a selection first."
            );
        return false;
    }

    const QImage bakedImage = BrightAreaTools::applyBrightAreaFade(
        m_sourceImage,
        m_brightMask,
        currentBrightAreaSettings()
        );

    if (bakedImage.isNull()) {
        QMessageBox::warning(
            this,
            "Bright Area Fade",
            "The bright-area fade could not be applied."
            );
        return false;
    }

    m_sourceImage = bakedImage;
    m_resultImage = bakedImage;
    m_brightPreviewImage = bakedImage;
    m_hasAppliedRegion = true;

    // Applying commits the selected set and clears it so the next click
    // starts a genuinely new area instead of applying the same mask twice.
    clearBrightSelection();
    setCanvasImagesToSource();

    emit statusMessage("Bright area applied — add another area or click Finish");
    return true;
}

void OverexposureRecoveryWorkspace::setCanvasImagesToSource()
{
    if (m_glareCanvas) {
        m_glareCanvas->setImage(m_sourceImage);
    }

    if (m_brightCanvas) {
        m_brightCanvas->setImage(m_sourceImage);
        m_brightCanvas->setMask(m_brightMask);
    }
}

bool OverexposureRecoveryWorkspace::activeSelectionExists() const
{
    if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
        if (!m_glareCanvas) {
            return false;
        }

        const QRect selection = m_glareCanvas->selectionRect();
        return !selection.isNull() && !selection.isEmpty();
    }

    return !m_brightMask.isNull() &&
           BrightAreaTools::maskHasSelection(m_brightMask);
}

bool OverexposureRecoveryWorkspace::finish(QImage &finishedImage)
{
    if (m_previewTimer && m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }
    m_previewPending = false;

    if (activeSelectionExists()) {
        if (!applyCurrentRegion()) {
            return false;
        }
    }

    if (!m_hasAppliedRegion) {
        QMessageBox::information(
            this,
            "Overexposure Recovery",
            "No glare region or bright area has been applied yet."
            );
        return false;
    }

    // Return detached image storage. The workspace and its preview canvases
    // are about to close, so the committed result must not depend on any
    // implicitly shared preview image owned by this tool session.
    m_resultImage = m_sourceImage.copy();
    finishedImage = m_resultImage.copy();
    return !finishedImage.isNull();
}

void OverexposureRecoveryWorkspace::setShowOriginal(bool showOriginal)
{
    m_showingOriginal = showOriginal;

    if (showOriginal) {
        if (!m_tabWidget || m_tabWidget->currentIndex() == 0) {
            if (m_glareCanvas) {
                m_glareCanvas->setImage(m_originalImage);
                updateGlareOverlay();
            }
        } else if (m_brightCanvas) {
            m_brightCanvas->setImage(m_originalImage);
            m_brightCanvas->setMask(m_brightMask);
        }
        return;
    }

    updateActivePreview();
}
