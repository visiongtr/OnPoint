#include "noisereductiondialog.h"

#include <QApplication>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace
{

QIcon makeInspectionIcon()
{
    QPixmap pixmap(
        24,
        24
        );

    pixmap.fill(
        Qt::transparent
        );

    QPainter painter(
        &pixmap
        );

    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );

    QPen lensPen(
        QColor("#DDE2E4"),
        1.8
        );

    lensPen.setCapStyle(
        Qt::RoundCap
        );

    lensPen.setJoinStyle(
        Qt::RoundJoin
        );

    painter.setPen(
        lensPen
        );

    painter.setBrush(
        Qt::NoBrush
        );

    painter.drawEllipse(
        QRectF(
            4.0,
            4.0,
            11.0,
            11.0
            )
        );

    painter.drawLine(
        QPointF(
            13.0,
            13.0
            ),
        QPointF(
            20.0,
            20.0
            )
        );

    painter.setPen(
        QPen(
            QColor("#B8894F"),
            1.8
            )
        );

    painter.drawPoint(
        QPointF(
            8.0,
            8.0
            )
        );

    painter.drawPoint(
        QPointF(
            11.0,
            10.0
            )
        );

    painter.drawPoint(
        QPointF(
            8.5,
            12.0
            )
        );

    return QIcon(
        pixmap
        );
}

class DoubleClickResetSlider : public QSlider
{
public:
    explicit DoubleClickResetSlider(
        Qt::Orientation orientation,
        int resetValue,
        QWidget *parent = nullptr
        )
        : QSlider(
              orientation,
              parent
              ),
          m_resetValue(
              resetValue
              )
    {
    }

protected:
    void mouseDoubleClickEvent(
        QMouseEvent *event
        ) override
    {
        if (event->button() ==
            Qt::LeftButton) {
            setValue(
                m_resetValue
                );

            event->accept();
            return;
        }

        QSlider::mouseDoubleClickEvent(
            event
            );
    }

private:
    int m_resetValue = 0;
};

class WaitCursorGuard
{
public:
    WaitCursorGuard()
    {
        QApplication::setOverrideCursor(
            Qt::WaitCursor
            );
    }

    ~WaitCursorGuard()
    {
        QApplication::restoreOverrideCursor();
    }
};

} // namespace

NoiseReductionDialog::NoiseReductionDialog(
    const QImage &sourceBaseImage,
    PreviewBuilder previewBuilder,
    QWidget *parent
    )
    : QDialog(parent),
      m_sourceBaseImage(
          sourceBaseImage
          ),
      m_resultBaseImage(
          sourceBaseImage
          ),
      m_previewBuilder(
          std::move(
              previewBuilder
              )
          )
{
    setupUi();
}

QImage NoiseReductionDialog::resultImage() const
{
    return m_resultBaseImage;
}

bool NoiseReductionDialog::hasAdjustment() const
{
    return NoiseReductionTools::
        hasVisibleAdjustment(
            currentSettings()
            );
}

void NoiseReductionDialog::setupUi()
{
    setObjectName(
        "noiseReductionDialog"
        );

    setWindowTitle(
        "Noise Reduction"
        );

    setModal(true);
    resize(940, 770);
    setMinimumSize(760, 620);

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        14,
        14,
        14,
        14
        );

    mainLayout->setSpacing(10);

    QLabel *instructionLabel =
        new QLabel(
            "Quiet grain and colored speckles while preserving real edges, "
            "skin texture, hair, fabric, and photographic depth.",
            this
            );

    instructionLabel->setWordWrap(
        true
        );

    mainLayout->addWidget(
        instructionLabel
        );

    m_previewWidget =
        new ToolPreviewWidget(this);

    m_previewWidget->setMinimumSize(
        640,
        320
        );

    m_previewWidget->setBackgroundColor(
        QColor(
            28,
            28,
            28
            )
        );

    m_previewWidget->setToolModeIcon(
        makeInspectionIcon()
        );

    m_previewWidget->setToolModeToolTip(
        "Inspection mode"
        );

    m_previewWidget->setToolCursor(
        Qt::ArrowCursor
        );

    m_previewWidget->setPanEnabled(
        true
        );

    // Noise correction must be judged at real pixel size.
    // Hand mode starts active so a zoomed photograph is immediately movable.
    m_previewWidget->setLeftButtonPans(
        true
        );

    const QImage originalDisplay =
        buildDisplayImage(
            m_sourceBaseImage
            );

    m_previewWidget->setImage(
        originalDisplay,
        false
        );

    m_previewWidget->setOriginalImage(
        originalDisplay
        );

    m_previewWidget->actualSize();

    mainLayout->addWidget(
        m_previewWidget,
        1
        );

    QHBoxLayout *inspectionLayout =
        new QHBoxLayout();

    QLabel *inspectionLabel =
        new QLabel(
            "Inspect at 100% or greater",
            this
            );

    inspectionLabel->setToolTip(
        "Fit view can hide waxy texture and over-smoothing"
        );

    inspectionLayout->addWidget(
        inspectionLabel
        );

    inspectionLayout->addStretch(1);

    QWidget *zoomBar =
        m_previewWidget
            ->createZoomControlBar(this);

    inspectionLayout->addWidget(
        zoomBar
        );

    inspectionLayout->addStretch(1);

    m_renderStatusLabel =
        new QLabel(
            "Natural starting point",
            this
            );

    m_renderStatusLabel->setMinimumWidth(
        170
        );

    m_renderStatusLabel->setAlignment(
        Qt::AlignRight |
        Qt::AlignVCenter
        );

    inspectionLayout->addWidget(
        m_renderStatusLabel
        );

    mainLayout->addLayout(
        inspectionLayout
        );

    createControlRow(
        "Luminance Noise",
        "Reduces brightness grain. Use restraint on faces, hair, and fine texture.",
        0,
        100,
        28,
        0,
        &m_luminanceSlider,
        &m_luminanceSpinBox
        );

    createControlRow(
        "Color Noise",
        "Reduces red, green, blue, and purple speckles while protecting brightness detail.",
        0,
        100,
        45,
        0,
        &m_colorSlider,
        &m_colorSpinBox
        );

    createControlRow(
        "Detail Protection",
        "Higher values preserve edges and texture more strongly.",
        0,
        100,
        78,
        78,
        &m_detailSlider,
        &m_detailSpinBox
        );

    createControlRow(
        "Blend",
        "Blends the corrected image with the untouched source.",
        0,
        100,
        85,
        100,
        &m_blendSlider,
        &m_blendSpinBox
        );

    connectSliderAndSpinBox(
        m_luminanceSlider,
        m_luminanceSpinBox
        );

    connectSliderAndSpinBox(
        m_colorSlider,
        m_colorSpinBox
        );

    connectSliderAndSpinBox(
        m_detailSlider,
        m_detailSpinBox
        );

    connectSliderAndSpinBox(
        m_blendSlider,
        m_blendSpinBox
        );

    m_previewTimer =
        new QTimer(this);

    m_previewTimer->setSingleShot(
        true
        );

    // Render only after the control pauses briefly. Continuous dragging
    // keeps restarting this timer, and slider release guarantees the final
    // requested value is rendered immediately.
    m_previewTimer->setInterval(
        220
        );

    connect(
        m_previewTimer,
        &QTimer::timeout,
        this,
        &NoiseReductionDialog::
            updateResultAndPreview
        );

    QHBoxLayout *buttonLayout =
        new QHBoxLayout();

    m_resetButton =
        new QPushButton(
            "Reset",
            this
            );

    m_showOriginalButton =
        new QPushButton(
            "Hold to Show Original",
            this
            );

    m_cancelButton =
        new QPushButton(
            "Cancel",
            this
            );

    m_finishButton =
        new QPushButton(
            "Finish",
            this
            );

    buttonLayout->addWidget(
        m_resetButton
        );

    buttonLayout->addStretch(1);

    buttonLayout->addWidget(
        m_showOriginalButton
        );

    buttonLayout->addStretch(1);

    buttonLayout->addWidget(
        m_cancelButton
        );

    buttonLayout->addWidget(
        m_finishButton
        );

    mainLayout->addLayout(
        buttonLayout
        );

    connect(
        m_resetButton,
        &QPushButton::clicked,
        this,
        &NoiseReductionDialog::
            resetControls
        );

    connect(
        m_showOriginalButton,
        &QPushButton::pressed,
        this,
        [this]() {
            m_previewWidget
                ->setShowingOriginal(
                    true
                    );
        });

    connect(
        m_showOriginalButton,
        &QPushButton::released,
        this,
        [this]() {
            m_previewWidget
                ->setShowingOriginal(
                    false
                    );
        });

    connect(
        m_cancelButton,
        &QPushButton::clicked,
        this,
        &QDialog::reject
        );

    connect(
        m_finishButton,
        &QPushButton::clicked,
        this,
        [this]() {
            if (m_previewTimer &&
                m_previewTimer->isActive()) {
                m_previewTimer->stop();
            }

            updateResultAndPreview();
            accept();
        });

    updateResultAndPreview();
}

void NoiseReductionDialog::createControlRow(
    const QString &labelText,
    const QString &toolTip,
    int minimum,
    int maximum,
    int value,
    int doubleClickResetValue,
    QSlider **sliderOut,
    QSpinBox **spinBoxOut
    )
{
    QWidget *rowWidget =
        new QWidget(this);

    rowWidget->setToolTip(
        toolTip
        );

    QHBoxLayout *rowLayout =
        new QHBoxLayout(rowWidget);

    rowLayout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    rowLayout->setSpacing(
        8
        );

    QLabel *label =
        new QLabel(
            labelText,
            rowWidget
            );

    label->setMinimumWidth(
        140
        );

    label->setToolTip(
        toolTip
        );

    QSlider *slider =
        new DoubleClickResetSlider(
            Qt::Horizontal,
            doubleClickResetValue,
            rowWidget
            );

    slider->setRange(
        minimum,
        maximum
        );

    slider->setValue(
        value
        );

    slider->setSingleStep(
        1
        );

    slider->setPageStep(
        10
        );

    slider->setToolTip(
        toolTip
        );

    QSpinBox *spinBox =
        new QSpinBox(rowWidget);

    spinBox->setRange(
        minimum,
        maximum
        );

    spinBox->setValue(
        value
        );

    spinBox->setMinimumWidth(
        70
        );

    spinBox->setToolTip(
        toolTip
        );

    rowLayout->addWidget(
        label
        );

    rowLayout->addWidget(
        slider,
        1
        );

    rowLayout->addWidget(
        spinBox
        );

    QVBoxLayout *mainLayout =
        qobject_cast<QVBoxLayout *>(
            layout()
            );

    if (mainLayout) {
        mainLayout->addWidget(
            rowWidget
            );
    }

    *sliderOut =
        slider;

    *spinBoxOut =
        spinBox;
}

void NoiseReductionDialog::connectSliderAndSpinBox(
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

            spinBox->setValue(
                value
                );

            schedulePreviewUpdate();
        });

    connect(
        slider,
        &QSlider::sliderReleased,
        this,
        [this]() {
            if (!m_previewPending) {
                return;
            }

            if (m_previewTimer &&
                m_previewTimer->isActive()) {
                m_previewTimer->stop();
            }

            updateResultAndPreview();
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

            slider->setValue(
                value
                );

            schedulePreviewUpdate();
        });

    connect(
        spinBox,
        &QSpinBox::editingFinished,
        this,
        [this]() {
            if (!m_previewPending) {
                return;
            }

            if (m_previewTimer &&
                m_previewTimer->isActive()) {
                m_previewTimer->stop();
            }

            updateResultAndPreview();
        });
}

void NoiseReductionDialog::setControls(
    int luminanceNoise,
    int colorNoise,
    int detailProtection,
    int blend
    )
{
    QSignalBlocker luminanceSliderBlocker(
        m_luminanceSlider
        );

    QSignalBlocker luminanceSpinBoxBlocker(
        m_luminanceSpinBox
        );

    QSignalBlocker colorSliderBlocker(
        m_colorSlider
        );

    QSignalBlocker colorSpinBoxBlocker(
        m_colorSpinBox
        );

    QSignalBlocker detailSliderBlocker(
        m_detailSlider
        );

    QSignalBlocker detailSpinBoxBlocker(
        m_detailSpinBox
        );

    QSignalBlocker blendSliderBlocker(
        m_blendSlider
        );

    QSignalBlocker blendSpinBoxBlocker(
        m_blendSpinBox
        );

    m_luminanceSlider->setValue(
        luminanceNoise
        );

    m_luminanceSpinBox->setValue(
        luminanceNoise
        );

    m_colorSlider->setValue(
        colorNoise
        );

    m_colorSpinBox->setValue(
        colorNoise
        );

    m_detailSlider->setValue(
        detailProtection
        );

    m_detailSpinBox->setValue(
        detailProtection
        );

    m_blendSlider->setValue(
        blend
        );

    m_blendSpinBox->setValue(
        blend
        );
}

void NoiseReductionDialog::resetControls()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    // Reset means no noise correction, while retaining sensible neutral
    // values for controls that matter only when correction is active.
    setControls(
        0,
        0,
        78,
        100
        );

    updateResultAndPreview();
}

void NoiseReductionDialog::schedulePreviewUpdate()
{
    m_previewPending = true;

    if (!m_previewTimer) {
        updateResultAndPreview();
        return;
    }

    m_renderStatusLabel->setText(
        "Waiting to render..."
        );

    m_previewTimer->start();
}

void NoiseReductionDialog::updateResultAndPreview()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewPending = false;

    if (!m_previewWidget ||
        m_sourceBaseImage.isNull()) {
        return;
    }

    m_renderStatusLabel->setText(
        "Rendering preview..."
        );

    if (m_finishButton) {
        m_finishButton->setEnabled(
            false
            );
    }

    QApplication::processEvents(
        QEventLoop::ExcludeUserInputEvents
        );

    {
        WaitCursorGuard waitCursor;

        m_resultBaseImage =
            NoiseReductionTools::
                applyNoiseReduction(
                    m_sourceBaseImage,
                    currentSettings()
                    );
    }

    m_previewWidget->setImage(
        buildDisplayImage(
            m_resultBaseImage
            ),
        true
        );

    m_renderStatusLabel->setText(
        "Preview current"
        );

    if (m_finishButton) {
        m_finishButton->setEnabled(
            true
            );
    }
}

NoiseReductionTools::NoiseReductionSettings
NoiseReductionDialog::currentSettings() const
{
    NoiseReductionTools::
        NoiseReductionSettings settings;

    settings.luminanceNoise =
        m_luminanceSlider
            ? m_luminanceSlider->value()
            : 0;

    settings.colorNoise =
        m_colorSlider
            ? m_colorSlider->value()
            : 0;

    settings.detailProtection =
        m_detailSlider
            ? m_detailSlider->value()
            : 78;

    settings.blend =
        m_blendSlider
            ? m_blendSlider->value()
            : 100;

    return settings;
}

QImage NoiseReductionDialog::buildDisplayImage(
    const QImage &baseImage
    ) const
{
    if (m_previewBuilder) {
        return m_previewBuilder(
            baseImage
            );
    }

    return baseImage;
}
