#include "redeyedialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <cmath>

RedEyeDialog::RedEyeDialog(
    const QImage &sourceImage,
    QWidget *parent
    )
    : QDialog(parent),
      m_originalImage(sourceImage),
      m_sourceImage(sourceImage),
      m_resultImage(sourceImage)
{
    setupUi();
    resetControls();
    updatePreview();
}

QImage RedEyeDialog::resultImage() const
{
    return m_resultImage;
}

void RedEyeDialog::setupUi()
{
    setWindowTitle("Red Eye Repair");
    setModal(true);
    resize(920, 760);

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    QLabel *instructionLabel =
        new QLabel(
            "Draw a small box around one eye. If Auto misses the glow, "
            "sample its color first, then adjust the controls."
            );

    mainLayout->addWidget(
        instructionLabel
        );

    m_previewWidget =
        new RedEyeSelectionWidget(this);

    m_previewWidget->setImage(
        m_sourceImage
        );

    m_previewWidget->setOriginalImage(
        m_originalImage
        );

    mainLayout->addWidget(
        m_previewWidget,
        1
        );

    connect(
        m_previewWidget,
        &RedEyeSelectionWidget::selectionChanged,
        this,
        &RedEyeDialog::updatePreview
        );


    connect(
        m_previewWidget,
        &RedEyeSelectionWidget::colorSampleRequested,
        this,
        [this](const QPoint &imagePoint) {
            const QColor sampledColor =
                sampleGlowColor(
                    imagePoint
                    );

            if (!sampledColor.isValid()) {
                return;
            }

            setSampledTargetColor(
                sampledColor
                );

            m_sampleGlowButton->setChecked(
                false
                );

            updatePreview();
        });

    connect(
        m_previewWidget,
        &RedEyeSelectionWidget::colorPickerActiveChanged,
        this,
        [this](bool active) {
            if (m_sampleGlowButton->isChecked() != active) {
                QSignalBlocker blocker(
                    m_sampleGlowButton
                    );

                m_sampleGlowButton->setChecked(
                    active
                    );
            }

            m_sampleGlowButton->setText(
                active
                    ? "Click Eye Glow..."
                    : (m_hasSampledTargetColor
                           ? "Resample Glow"
                           : "Sample Glow")
                );
        });

    QHBoxLayout *modeLayout =
        new QHBoxLayout();

    QLabel *modeLabel =
        new QLabel("Mode");

    modeLabel->setMinimumWidth(90);

    m_modeCombo =
        new QComboBox(this);

    m_modeCombo->addItem(
        "Auto",
        static_cast<int>(
            RedEyeTools::RedEyeMode::Auto
            )
        );

    m_modeCombo->addItem(
        "Manual",
        static_cast<int>(
            RedEyeTools::RedEyeMode::Manual
            )
        );

    QWidget *zoomControlBar =
        m_previewWidget
            ->createZoomControlBar(this);

    QLabel *irisLabel =
        new QLabel("Iris Color");

    m_irisColorCombo =
        new QComboBox(this);

    m_irisColorCombo->addItem(
        "Auto",
        static_cast<int>(
            RedEyeTools::IrisColor::Auto
            )
        );

    m_irisColorCombo->addItem(
        "Brown",
        static_cast<int>(
            RedEyeTools::IrisColor::Brown
            )
        );

    m_irisColorCombo->addItem(
        "Hazel",
        static_cast<int>(
            RedEyeTools::IrisColor::Hazel
            )
        );

    m_irisColorCombo->addItem(
        "Green",
        static_cast<int>(
            RedEyeTools::IrisColor::Green
            )
        );

    m_irisColorCombo->addItem(
        "Blue",
        static_cast<int>(
            RedEyeTools::IrisColor::Blue
            )
        );

    m_irisColorCombo->addItem(
        "Gray",
        static_cast<int>(
            RedEyeTools::IrisColor::Gray
            )
        );

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(m_modeCombo);

    // Keep the shared zoom bar on the same row as Auto / Manual.
    modeLayout->addSpacing(14);
    modeLayout->addWidget(zoomControlBar);

    modeLayout->addSpacing(18);
    modeLayout->addWidget(irisLabel);
    modeLayout->addWidget(m_irisColorCombo);
    modeLayout->addStretch(1);

    mainLayout->addLayout(modeLayout);

    QHBoxLayout *targetColorLayout =
        new QHBoxLayout();

    QLabel *targetColorLabel =
        new QLabel(
            "Target Color",
            this
            );

    targetColorLabel->setMinimumWidth(
        90
        );

    m_sampleGlowButton =
        new QPushButton(
            "Sample Glow",
            this
            );

    m_sampleGlowButton->setCheckable(
        true
        );

    m_sampleGlowButton->setToolTip(
        "Click, then sample the unwanted glow directly from the eye"
        );

    m_targetColorSwatch =
        new QLabel(
            "Auto Red",
            this
            );

    m_targetColorSwatch->setMinimumSize(
        120,
        28
        );

    m_targetColorSwatch->setAlignment(
        Qt::AlignCenter
        );

    m_targetColorSwatch->setFrameShape(
        QFrame::Box
        );

    m_targetColorSwatch->setAutoFillBackground(
        true
        );

    m_useAutoRedButton =
        new QPushButton(
            "Use Auto Red",
            this
            );

    m_useAutoRedButton->setEnabled(
        false
        );

    targetColorLayout->addWidget(
        targetColorLabel
        );

    targetColorLayout->addWidget(
        m_sampleGlowButton
        );

    targetColorLayout->addWidget(
        m_targetColorSwatch
        );

    targetColorLayout->addWidget(
        m_useAutoRedButton
        );

    targetColorLayout->addStretch(
        1
        );

    mainLayout->addLayout(
        targetColorLayout
        );

    connect(
        m_sampleGlowButton,
        &QPushButton::toggled,
        this,
        [this](bool checked) {
            m_previewWidget->setColorPickerActive(
                checked
                );

            m_sampleGlowButton->setText(
                checked
                    ? "Click Eye Glow..."
                    : (m_hasSampledTargetColor
                           ? "Resample Glow"
                           : "Sample Glow")
                );
        });

    connect(
        m_useAutoRedButton,
        &QPushButton::clicked,
        this,
        [this]() {
            clearSampledTargetColor();
            updatePreview();
        });

    createControlRow(
        "Threshold",
        0,
        100,
        50,
        &m_toleranceSlider,
        &m_toleranceSpinBox
        );

    createControlRow(
        "Repair Strength",
        0,
        100,
        85,
        &m_strengthSlider,
        &m_strengthSpinBox
        );

    createControlRow(
        "Pupil Darken",
        0,
        100,
        55,
        &m_pupilDarkenSlider,
        &m_pupilDarkenSpinBox
        );

    createControlRow(
        "Iris Amount",
        0,
        100,
        20,
        &m_irisAmountSlider,
        &m_irisAmountSpinBox
        );

    createControlRow(
        "Blend",
        0,
        100,
        85,
        &m_blendSlider,
        &m_blendSpinBox
        );

    createControlRow(
        "Soft Edge",
        0,
        100,
        55,
        &m_softEdgeSlider,
        &m_softEdgeSpinBox
        );

    connectSliderAndSpinBox(
        m_toleranceSlider,
        m_toleranceSpinBox
        );

    connectSliderAndSpinBox(
        m_strengthSlider,
        m_strengthSpinBox
        );

    connectSliderAndSpinBox(
        m_pupilDarkenSlider,
        m_pupilDarkenSpinBox
        );

    connectSliderAndSpinBox(
        m_irisAmountSlider,
        m_irisAmountSpinBox
        );

    connectSliderAndSpinBox(
        m_blendSlider,
        m_blendSpinBox
        );

    connectSliderAndSpinBox(
        m_softEdgeSlider,
        m_softEdgeSpinBox
        );

    connect(
        m_modeCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        [this]() {
            const bool manualMode =
                m_modeCombo
                    ->currentData()
                    .toInt() ==
                static_cast<int>(
                    RedEyeTools::RedEyeMode::Manual
                    );

            m_irisColorCombo->setEnabled(
                manualMode
                );

            m_irisAmountSlider->setEnabled(
                manualMode
                );

            m_irisAmountSpinBox->setEnabled(
                manualMode
                );

            updatePreview();
        });

    connect(
        m_irisColorCombo,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        &RedEyeDialog::updatePreview
        );

    m_buttonBox =
        new QDialogButtonBox(
            QDialogButtonBox::Reset |
                QDialogButtonBox::Cancel |
                QDialogButtonBox::Apply,
            this
            );

    m_applyEyeButton =
        m_buttonBox->addButton(
            "Apply Eye",
            QDialogButtonBox::ActionRole
            );

    mainLayout->addWidget(
        m_buttonBox
        );

    connect(
        m_buttonBox->button(
            QDialogButtonBox::Reset
            ),
        &QPushButton::clicked,
        this,
        [this]() {
            resetControls();
            updatePreview();
        });

    connect(
        m_applyEyeButton,
        &QPushButton::clicked,
        this,
        [this]() {
            const QRect selection =
                m_previewWidget
                    ->selectionRect();

            if (selection.isNull() ||
                selection.isEmpty()) {
                QMessageBox::information(
                    this,
                    "Red Eye Repair",
                    "Draw a small box around one red eye first."
                    );

                return;
            }

            m_resultImage =
                RedEyeTools::repairRedEye(
                    m_sourceImage,
                    selection,
                    currentSettings()
                    );

            m_sourceImage = m_resultImage;
            m_hasAppliedEye = true;

            m_previewWidget->setImage(
                m_sourceImage
                );

            m_previewWidget
                ->clearSelection();
        });

    connect(
        m_buttonBox->button(
            QDialogButtonBox::Cancel
            ),
        &QPushButton::clicked,
        this,
        &RedEyeDialog::reject
        );

    connect(
        m_buttonBox->button(
            QDialogButtonBox::Apply
            ),
        &QPushButton::clicked,
        this,
        [this]() {
            const QRect selection =
                m_previewWidget
                    ->selectionRect();

            if (!selection.isNull() &&
                !selection.isEmpty()) {
                m_resultImage =
                    RedEyeTools::repairRedEye(
                        m_sourceImage,
                        selection,
                        currentSettings()
                        );

                accept();
                return;
            }

            if (m_hasAppliedEye) {
                m_resultImage =
                    m_sourceImage;

                accept();
                return;
            }

            QMessageBox::information(
                this,
                "Red Eye Repair",
                "Draw a small box around one red eye first."
                );
        });
}

void RedEyeDialog::createControlRow(
    const QString &labelText,
    int minimum,
    int maximum,
    int value,
    QSlider **sliderOut,
    QSpinBox **spinBoxOut
    )
{
    QWidget *rowWidget =
        new QWidget(this);

    QHBoxLayout *rowLayout =
        new QHBoxLayout(rowWidget);

    rowLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    rowLayout->setSpacing(8);

    QLabel *label =
        new QLabel(
            labelText,
            rowWidget
            );

    label->setMinimumWidth(120);

    QSlider *slider =
        new QSlider(
            Qt::Horizontal,
            rowWidget
            );

    slider->setMinimum(minimum);
    slider->setMaximum(maximum);
    slider->setValue(value);

    QSpinBox *spinBox =
        new QSpinBox(rowWidget);

    spinBox->setMinimum(minimum);
    spinBox->setMaximum(maximum);
    spinBox->setValue(value);
    spinBox->setMinimumWidth(70);

    rowLayout->addWidget(label);
    rowLayout->addWidget(slider, 1);
    rowLayout->addWidget(spinBox);

    QVBoxLayout *mainLayout =
        qobject_cast<QVBoxLayout *>(
            layout()
            );

    if (mainLayout) {
        mainLayout->addWidget(
            rowWidget
            );
    }

    *sliderOut = slider;
    *spinBoxOut = spinBox;
}

void RedEyeDialog::connectSliderAndSpinBox(
    QSlider *slider,
    QSpinBox *spinBox
    )
{
    connect(
        slider,
        &QSlider::valueChanged,
        this,
        [this, spinBox](int value) {
            QSignalBlocker blocker(
                spinBox
                );

            spinBox->setValue(value);
            updatePreview();
        });

    connect(
        spinBox,
        QOverload<int>::of(
            &QSpinBox::valueChanged
            ),
        this,
        [this, slider](int value) {
            QSignalBlocker blocker(
                slider
                );

            slider->setValue(value);
            updatePreview();
        });
}

void RedEyeDialog::resetControls()
{
    m_sourceImage = m_originalImage;
    m_resultImage = m_originalImage;
    m_hasAppliedEye = false;

    clearSampledTargetColor();

    m_previewWidget
        ->clearSelection();

    {
        QSignalBlocker modeBlocker(
            m_modeCombo
            );

        QSignalBlocker irisColorBlocker(
            m_irisColorCombo
            );

        m_modeCombo->setCurrentIndex(0);
        m_irisColorCombo->setCurrentIndex(0);
    }

    {
        QSignalBlocker toleranceSliderBlocker(
            m_toleranceSlider
            );

        QSignalBlocker toleranceSpinBoxBlocker(
            m_toleranceSpinBox
            );

        QSignalBlocker strengthSliderBlocker(
            m_strengthSlider
            );

        QSignalBlocker strengthSpinBoxBlocker(
            m_strengthSpinBox
            );

        QSignalBlocker pupilDarkenSliderBlocker(
            m_pupilDarkenSlider
            );

        QSignalBlocker pupilDarkenSpinBoxBlocker(
            m_pupilDarkenSpinBox
            );

        QSignalBlocker irisAmountSliderBlocker(
            m_irisAmountSlider
            );

        QSignalBlocker irisAmountSpinBoxBlocker(
            m_irisAmountSpinBox
            );

        QSignalBlocker blendSliderBlocker(
            m_blendSlider
            );

        QSignalBlocker blendSpinBoxBlocker(
            m_blendSpinBox
            );

        QSignalBlocker softEdgeSliderBlocker(
            m_softEdgeSlider
            );

        QSignalBlocker softEdgeSpinBoxBlocker(
            m_softEdgeSpinBox
            );

        m_toleranceSlider->setValue(50);
        m_toleranceSpinBox->setValue(50);

        m_strengthSlider->setValue(85);
        m_strengthSpinBox->setValue(85);

        m_pupilDarkenSlider->setValue(55);
        m_pupilDarkenSpinBox->setValue(55);

        m_irisAmountSlider->setValue(20);
        m_irisAmountSpinBox->setValue(20);

        m_blendSlider->setValue(85);
        m_blendSpinBox->setValue(85);

        m_softEdgeSlider->setValue(55);
        m_softEdgeSpinBox->setValue(55);
    }

    m_irisColorCombo->setEnabled(false);
    m_irisAmountSlider->setEnabled(false);
    m_irisAmountSpinBox->setEnabled(false);

    m_resultImage = m_originalImage;

    m_previewWidget->setImage(
        m_originalImage
        );
}

QColor RedEyeDialog::sampleGlowColor(
    const QPoint &imagePoint
    ) const
{
    if (m_sourceImage.isNull() ||
        !QRect(
            QPoint(0, 0),
            m_sourceImage.size()
            ).contains(imagePoint)) {
        return QColor();
    }

    const QColor centerColor =
        m_sourceImage.pixelColor(
            imagePoint
            );

    double redSum = 0.0;
    double greenSum = 0.0;
    double blueSum = 0.0;
    double weightSum = 0.0;

    for (int offsetY = -2;
         offsetY <= 2;
         ++offsetY) {
        for (int offsetX = -2;
             offsetX <= 2;
             ++offsetX) {
            const int sampleX =
                qBound(
                    0,
                    imagePoint.x() + offsetX,
                    m_sourceImage.width() - 1
                    );

            const int sampleY =
                qBound(
                    0,
                    imagePoint.y() + offsetY,
                    m_sourceImage.height() - 1
                    );

            const QColor sampleColor =
                m_sourceImage.pixelColor(
                    sampleX,
                    sampleY
                    );

            const double redDifference =
                sampleColor.red() - centerColor.red();

            const double greenDifference =
                sampleColor.green() - centerColor.green();

            const double blueDifference =
                sampleColor.blue() - centerColor.blue();

            const double colorDistance =
                std::sqrt(
                    (redDifference * redDifference) +
                    (greenDifference * greenDifference) +
                    (blueDifference * blueDifference)
                    );

            // Ignore surrounding eyelid/iris pixels that are far from the
            // exact glow color the user clicked.
            if (colorDistance > 95.0 &&
                (offsetX != 0 || offsetY != 0)) {
                continue;
            }

            const double spatialWeight =
                1.0 /
                (1.0 +
                 (offsetX * offsetX) +
                 (offsetY * offsetY));

            redSum +=
                sampleColor.red() * spatialWeight;

            greenSum +=
                sampleColor.green() * spatialWeight;

            blueSum +=
                sampleColor.blue() * spatialWeight;

            weightSum += spatialWeight;
        }
    }

    if (weightSum <= 0.0) {
        return centerColor;
    }

    return QColor(
        qBound(
            0,
            static_cast<int>(
                std::round(redSum / weightSum)
                ),
            255
            ),
        qBound(
            0,
            static_cast<int>(
                std::round(greenSum / weightSum)
                ),
            255
            ),
        qBound(
            0,
            static_cast<int>(
                std::round(blueSum / weightSum)
                ),
            255
            )
        );
}

void RedEyeDialog::setSampledTargetColor(
    const QColor &color
    )
{
    if (!color.isValid()) {
        return;
    }

    m_sampledTargetColor = color;
    m_hasSampledTargetColor = true;

    m_useAutoRedButton->setEnabled(
        true
        );

    m_sampleGlowButton->setText(
        "Resample Glow"
        );

    updateTargetColorDisplay();
}

void RedEyeDialog::clearSampledTargetColor()
{
    m_hasSampledTargetColor = false;
    m_sampledTargetColor =
        QColor(205, 45, 38);

    if (m_previewWidget) {
        m_previewWidget->setColorPickerActive(
            false
            );
    }

    if (m_sampleGlowButton) {
        QSignalBlocker blocker(
            m_sampleGlowButton
            );

        m_sampleGlowButton->setChecked(
            false
            );

        m_sampleGlowButton->setText(
            "Sample Glow"
            );
    }

    if (m_useAutoRedButton) {
        m_useAutoRedButton->setEnabled(
            false
            );
    }

    updateTargetColorDisplay();
}

void RedEyeDialog::updateTargetColorDisplay()
{
    if (!m_targetColorSwatch) {
        return;
    }

    const QColor displayColor =
        m_hasSampledTargetColor
            ? m_sampledTargetColor
            : QColor(130, 34, 30);

    const double displayLuminance =
        (0.299 * displayColor.red()) +
        (0.587 * displayColor.green()) +
        (0.114 * displayColor.blue());

    const QColor textColor =
        displayLuminance > 145.0
            ? QColor(20, 20, 20)
            : QColor(245, 245, 245);

    // This is deliberately local to the live color swatch; it does not
    // append or override the application theme stylesheet.
    m_targetColorSwatch->setStyleSheet(
        QString(
            "QLabel { "
            "background-color: rgb(%1, %2, %3); "
            "color: rgb(%4, %5, %6); "
            "border: 1px solid #6A6A6A; "
            "border-radius: 3px; "
            "padding: 3px; "
            "}"
            )
            .arg(displayColor.red())
            .arg(displayColor.green())
            .arg(displayColor.blue())
            .arg(textColor.red())
            .arg(textColor.green())
            .arg(textColor.blue())
        );

    m_targetColorSwatch->setText(
        m_hasSampledTargetColor
            ? QString(
                  "RGB %1, %2, %3"
                  )
                  .arg(m_sampledTargetColor.red())
                  .arg(m_sampledTargetColor.green())
                  .arg(m_sampledTargetColor.blue())
            : QString("Auto Red")
        );
}

RedEyeTools::RedEyeSettings
RedEyeDialog::currentSettings() const
{
    RedEyeTools::RedEyeSettings settings;

    settings.mode =
        static_cast<
            RedEyeTools::RedEyeMode
            >(
            m_modeCombo
                ->currentData()
                .toInt()
            );

    settings.irisColor =
        static_cast<
            RedEyeTools::IrisColor
            >(
            m_irisColorCombo
                ->currentData()
                .toInt()
            );

    settings.useSampledTarget =
        m_hasSampledTargetColor;

    settings.sampledTargetColor =
        m_sampledTargetColor;

    settings.tolerance =
        m_toleranceSlider->value();

    settings.strength =
        m_strengthSlider->value();

    settings.pupilDarken =
        m_pupilDarkenSlider->value();

    settings.irisAmount =
        m_irisAmountSlider->value();

    settings.blend =
        m_blendSlider->value();

    settings.softEdge =
        m_softEdgeSlider->value();

    return settings;
}

void RedEyeDialog::updatePreview()
{
    const QRect selection =
        m_previewWidget
            ->selectionRect();

    if (selection.isNull() ||
        selection.isEmpty()) {
        m_resultImage =
            m_sourceImage;

        m_previewWidget->setImage(
            m_sourceImage
            );

        return;
    }

    m_resultImage =
        RedEyeTools::repairRedEye(
            m_sourceImage,
            selection,
            currentSettings()
            );

    m_previewWidget->setImage(
        m_resultImage
        );
}
