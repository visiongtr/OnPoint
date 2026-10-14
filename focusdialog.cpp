#include "focusdialog.h"

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

QIcon makeFocusInspectionIcon()
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

    QPen outerPen(
        QColor("#DDE2E4"),
        1.8
        );

    outerPen.setCapStyle(
        Qt::RoundCap
        );

    outerPen.setJoinStyle(
        Qt::RoundJoin
        );

    painter.setPen(
        outerPen
        );

    painter.setBrush(
        Qt::NoBrush
        );

    painter.drawRect(
        QRectF(
            5.0,
            5.0,
            14.0,
            14.0
            )
        );

    painter.setPen(
        QPen(
            QColor("#B8894F"),
            1.8
            )
        );

    painter.drawLine(
        QPointF(8.0, 12.0),
        QPointF(16.0, 12.0)
        );

    painter.drawLine(
        QPointF(12.0, 8.0),
        QPointF(12.0, 16.0)
        );

    painter.drawEllipse(
        QPointF(12.0, 12.0),
        2.0,
        2.0
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

FocusDialog::FocusDialog(
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

QImage FocusDialog::resultImage() const
{
    return m_resultBaseImage;
}

bool FocusDialog::hasAdjustment() const
{
    return FocusTools::
        hasVisibleAdjustment(
            currentSettings()
            );
}

void FocusDialog::setupUi()
{
    setObjectName(
        "focusDialog"
        );

    setWindowTitle(
        "Focus"
        );

    setModal(true);
    resize(940, 800);
    setMinimumSize(760, 650);

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        14,
        14,
        14,
        14
        );

    mainLayout->setSpacing(
        10
        );

    QLabel *instructionLabel =
        new QLabel(
            "Restore edge definition without inventing detail, "
            "sharpening color noise, or outlining the photograph.",
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
        makeFocusInspectionIcon()
        );

    m_previewWidget->setToolModeToolTip(
        "Focus inspection mode"
        );

    m_previewWidget->setToolCursor(
        Qt::ArrowCursor
        );

    m_previewWidget->setPanEnabled(
        true
        );

    // Focus must be judged at actual pixels, not hidden by a fitted preview.
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
        "Fit view hides halos, sharpened grain, and artificial texture"
        );

    inspectionLayout->addWidget(
        inspectionLabel
        );

    inspectionLayout->addStretch(
        1
        );

    QWidget *zoomBar =
        m_previewWidget
            ->createZoomControlBar(this);

    inspectionLayout->addWidget(
        zoomBar
        );

    inspectionLayout->addStretch(
        1
        );

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
        "Amount",
        "Controls how strongly genuine luminance detail is restored.",
        0,
        100,
        18,
        0,
        &m_amountSlider,
        &m_amountSpinBox
        );

    createControlRow(
        "Radius",
        "Lower values target fine detail; higher values affect broader edges.",
        1,
        50,
        10,
        10,
        &m_radiusSlider,
        &m_radiusSpinBox
        );

    createControlRow(
        "Threshold",
        "Higher values avoid sharpening grain, pores, and tiny compression noise.",
        0,
        100,
        24,
        24,
        &m_thresholdSlider,
        &m_thresholdSpinBox
        );

    createControlRow(
        "Edge Protection",
        "Higher values reduce bright and dark halos around strong edges.",
        0,
        100,
        72,
        72,
        &m_edgeProtectionSlider,
        &m_edgeProtectionSpinBox
        );

    createControlRow(
        "Blend",
        "Blends the focused result with the untouched source.",
        0,
        100,
        85,
        100,
        &m_blendSlider,
        &m_blendSpinBox
        );

    connectSliderAndSpinBox(
        m_amountSlider,
        m_amountSpinBox
        );

    connectSliderAndSpinBox(
        m_radiusSlider,
        m_radiusSpinBox
        );

    connectSliderAndSpinBox(
        m_thresholdSlider,
        m_thresholdSpinBox
        );

    connectSliderAndSpinBox(
        m_edgeProtectionSlider,
        m_edgeProtectionSpinBox
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
        &FocusDialog::
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

    buttonLayout->addStretch(
        1
        );

    buttonLayout->addWidget(
        m_showOriginalButton
        );

    buttonLayout->addStretch(
        1
        );

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
        &FocusDialog::
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

void FocusDialog::createControlRow(
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

void FocusDialog::connectSliderAndSpinBox(
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

void FocusDialog::setControls(
    int amount,
    int radius,
    int threshold,
    int edgeProtection,
    int blend
    )
{
    QSignalBlocker amountSliderBlocker(
        m_amountSlider
        );

    QSignalBlocker amountSpinBoxBlocker(
        m_amountSpinBox
        );

    QSignalBlocker radiusSliderBlocker(
        m_radiusSlider
        );

    QSignalBlocker radiusSpinBoxBlocker(
        m_radiusSpinBox
        );

    QSignalBlocker thresholdSliderBlocker(
        m_thresholdSlider
        );

    QSignalBlocker thresholdSpinBoxBlocker(
        m_thresholdSpinBox
        );

    QSignalBlocker edgeSliderBlocker(
        m_edgeProtectionSlider
        );

    QSignalBlocker edgeSpinBoxBlocker(
        m_edgeProtectionSpinBox
        );

    QSignalBlocker blendSliderBlocker(
        m_blendSlider
        );

    QSignalBlocker blendSpinBoxBlocker(
        m_blendSpinBox
        );

    m_amountSlider->setValue(
        amount
        );

    m_amountSpinBox->setValue(
        amount
        );

    m_radiusSlider->setValue(
        radius
        );

    m_radiusSpinBox->setValue(
        radius
        );

    m_thresholdSlider->setValue(
        threshold
        );

    m_thresholdSpinBox->setValue(
        threshold
        );

    m_edgeProtectionSlider->setValue(
        edgeProtection
        );

    m_edgeProtectionSpinBox->setValue(
        edgeProtection
        );

    m_blendSlider->setValue(
        blend
        );

    m_blendSpinBox->setValue(
        blend
        );
}

void FocusDialog::resetControls()
{
    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    // Reset means no focus correction. The supporting controls return
    // to natural values ready for another adjustment.
    setControls(
        0,
        10,
        24,
        72,
        100
        );

    updateResultAndPreview();
}

void FocusDialog::schedulePreviewUpdate()
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

void FocusDialog::updateResultAndPreview()
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
            FocusTools::
                applyFocus(
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

FocusTools::FocusSettings
FocusDialog::currentSettings() const
{
    FocusTools::
        FocusSettings settings;

    settings.amount =
        m_amountSlider
            ? m_amountSlider->value()
            : 0;

    settings.radius =
        m_radiusSlider
            ? m_radiusSlider->value()
            : 10;

    settings.threshold =
        m_thresholdSlider
            ? m_thresholdSlider->value()
            : 24;

    settings.edgeProtection =
        m_edgeProtectionSlider
            ? m_edgeProtectionSlider->value()
            : 72;

    settings.blend =
        m_blendSlider
            ? m_blendSlider->value()
            : 100;

    return settings;
}

QImage FocusDialog::buildDisplayImage(
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
