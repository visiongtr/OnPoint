#include "whitebalancedialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{

QIcon makeNeutralEyedropperIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);

    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );

    QPen bodyPen(
        QColor("#DDE2E4"),
        2.0
        );

    bodyPen.setCapStyle(
        Qt::RoundCap
        );

    bodyPen.setJoinStyle(
        Qt::RoundJoin
        );

    painter.setPen(bodyPen);

    painter.drawLine(
        QPointF(7.0, 17.0),
        QPointF(16.5, 7.5)
        );

    painter.drawLine(
        QPointF(14.0, 5.0),
        QPointF(19.0, 10.0)
        );

    painter.drawLine(
        QPointF(16.5, 7.5),
        QPointF(18.3, 5.7)
        );

    painter.drawLine(
        QPointF(18.3, 5.7),
        QPointF(19.8, 7.2)
        );

    QPen tipPen(
        QColor("#B8894F"),
        2.0
        );

    tipPen.setCapStyle(
        Qt::RoundCap
        );

    painter.setPen(tipPen);

    painter.drawLine(
        QPointF(5.0, 19.0),
        QPointF(8.0, 16.0)
        );

    painter.setPen(Qt::NoPen);
    painter.setBrush(
        QColor("#D7D7D2")
        );

    painter.drawEllipse(
        QPointF(4.5, 20.0),
        1.6,
        1.6
        );

    return QIcon(pixmap);
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
          m_resetValue(resetValue)
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

} // namespace

WhiteBalancePreviewWidget::WhiteBalancePreviewWidget(
    QWidget *parent
    )
    : ToolPreviewWidget(parent)
{
    setMinimumSize(640, 300);

    setBackgroundColor(
        QColor(28, 28, 28)
        );

    setToolCursor(
        Qt::CrossCursor
        );

    setToolModeIcon(
        makeNeutralEyedropperIcon()
        );

    setToolModeToolTip(
        "Neutral eyedropper: click something that should be gray or white"
        );

    setPanEnabled(true);
    setLeftButtonPans(false);
}

void WhiteBalancePreviewWidget::setSamplePoint(
    const QPoint &imagePoint
    )
{
    m_samplePoint = imagePoint;
    m_hasSamplePoint = true;
    update();
}

void WhiteBalancePreviewWidget::clearSamplePoint()
{
    m_hasSamplePoint = false;
    m_samplePoint = QPoint();
    update();
}

void WhiteBalancePreviewWidget::paintImageOverlay(
    QPainter &painter
    )
{
    if (!m_hasSamplePoint) {
        return;
    }

    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );

    QPen outerPen(
        QColor(25, 25, 25),
        4.0
        );

    outerPen.setCosmetic(true);

    painter.setPen(outerPen);
    painter.setBrush(Qt::NoBrush);

    painter.drawEllipse(
        QPointF(m_samplePoint),
        8.0,
        8.0
        );

    QPen innerPen(
        QColor(255, 220, 0),
        2.0
        );

    innerPen.setCosmetic(true);

    painter.setPen(innerPen);

    painter.drawEllipse(
        QPointF(m_samplePoint),
        8.0,
        8.0
        );

    painter.drawLine(
        QPointF(
            m_samplePoint.x() - 12.0,
            m_samplePoint.y()
            ),
        QPointF(
            m_samplePoint.x() - 5.0,
            m_samplePoint.y()
            )
        );

    painter.drawLine(
        QPointF(
            m_samplePoint.x() + 5.0,
            m_samplePoint.y()
            ),
        QPointF(
            m_samplePoint.x() + 12.0,
            m_samplePoint.y()
            )
        );

    painter.drawLine(
        QPointF(
            m_samplePoint.x(),
            m_samplePoint.y() - 12.0
            ),
        QPointF(
            m_samplePoint.x(),
            m_samplePoint.y() - 5.0
            )
        );

    painter.drawLine(
        QPointF(
            m_samplePoint.x(),
            m_samplePoint.y() + 5.0
            ),
        QPointF(
            m_samplePoint.x(),
            m_samplePoint.y() + 12.0
            )
        );
}

WhiteBalanceDialog::WhiteBalanceDialog(
    const QImage &sourceBaseImage,
    PreviewBuilder previewBuilder,
    QWidget *parent
    )
    : QDialog(parent),
      m_sourceBaseImage(sourceBaseImage),
      m_resultBaseImage(sourceBaseImage),
      m_previewBuilder(
          std::move(
              previewBuilder
              )
          )
{
    setupUi();
    resetControls();
}

QImage WhiteBalanceDialog::resultImage() const
{
    return m_resultBaseImage;
}

bool WhiteBalanceDialog::hasAdjustment() const
{
    return WhiteBalanceTools::
        hasVisibleAdjustment(
            currentSettings()
            );
}

void WhiteBalanceDialog::setupUi()
{
    setObjectName(
        "whiteBalanceDialog"
        );

    setWindowTitle(
        "White Balance"
        );

    setModal(true);
    resize(920, 760);
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
            "Click something that should be neutral gray or white, "
            "or use Auto as a starting point."
            );

    instructionLabel->setWordWrap(true);

    mainLayout->addWidget(
        instructionLabel
        );

    m_previewWidget =
        new WhiteBalancePreviewWidget(this);

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

    mainLayout->addWidget(
        m_previewWidget,
        1
        );

    connect(
        m_previewWidget,
        &ToolPreviewWidget::imageMousePressed,
        this,
        [this](
            const QPointF &imagePoint,
            Qt::MouseButton button,
            Qt::KeyboardModifiers,
            bool insideImage
            ) {
            if (button != Qt::LeftButton ||
                !insideImage ||
                m_previewWidget
                    ->leftButtonPans()) {
                return;
            }

            sampleNeutralAt(
                imagePoint
                );
        });

    QHBoxLayout *actionLayout =
        new QHBoxLayout();

    m_autoButton =
        new QPushButton(
            "Auto",
            this
            );

    m_autoButton->setCheckable(true);

    m_autoButton->setToolTip(
        "Analyze the photograph and calculate a restrained automatic white balance"
        );

    actionLayout->addWidget(
        m_autoButton
        );

    QWidget *zoomBar =
        m_previewWidget
            ->createZoomControlBar(this);

    actionLayout->addSpacing(10);

    actionLayout->addWidget(
        zoomBar
        );

    actionLayout->addSpacing(16);

    m_sampleSwatch =
        new QLabel(this);

    m_sampleSwatch->setFixedSize(
        28,
        28
        );

    m_sampleSwatch->setToolTip(
        "Sampled neutral color"
        );

    actionLayout->addWidget(
        m_sampleSwatch
        );

    m_sampleDescription =
        new QLabel(
            "No neutral sample",
            this
            );

    actionLayout->addWidget(
        m_sampleDescription
        );

    actionLayout->addStretch(1);

    mainLayout->addLayout(
        actionLayout
        );

    connect(
        m_autoButton,
        &QPushButton::clicked,
        this,
        [this](bool checked) {
            if (checked) {
                useAutomaticBalance();
                return;
            }

            // Return to manual Temperature / Tint control
            // without resetting those sliders.
            m_baseGains =
                WhiteBalanceTools::
                ChannelGains();

            m_previewWidget
                ->clearSamplePoint();

            updateSampleDisplay(
                QColor(),
                "Manual balance"
                );

            updateResultAndPreview();
        });

    createControlRow(
        "Temperature",
        -100,
        100,
        0,
        0,
        &m_temperatureSlider,
        &m_temperatureSpinBox
        );

    createControlRow(
        "Tint",
        -100,
        100,
        0,
        0,
        &m_tintSlider,
        &m_tintSpinBox
        );

    createControlRow(
        "Strength",
        0,
        100,
        85,
        85,
        &m_strengthSlider,
        &m_strengthSpinBox
        );

    // Show the direction of each color correction directly on its track.
    // The center remains neutral gray so zero is visually obvious.
    m_temperatureSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "height: 5px;"
        "border-radius: 2px;"
        "background: qlineargradient("
        "x1:0, y1:0, x2:1, y2:0,"
        "stop:0 #4B78B8,"
        "stop:0.50 #777D80,"
        "stop:1 #D4A64E"
        ");"
        "}"
        "QSlider::sub-page:horizontal {"
        "background: transparent;"
        "}"
        "QSlider::add-page:horizontal {"
        "background: transparent;"
        "}"
        );

    m_temperatureSlider->setToolTip(
        "Cool blue  \u2190  Temperature  \u2192  Warm yellow"
        );

    m_tintSlider->setStyleSheet(
        "QSlider::groove:horizontal {"
        "height: 5px;"
        "border-radius: 2px;"
        "background: qlineargradient("
        "x1:0, y1:0, x2:1, y2:0,"
        "stop:0 #5E9C70,"
        "stop:0.50 #777D80,"
        "stop:1 #B86A9D"
        ");"
        "}"
        "QSlider::sub-page:horizontal {"
        "background: transparent;"
        "}"
        "QSlider::add-page:horizontal {"
        "background: transparent;"
        "}"
        );

    m_tintSlider->setToolTip(
        "Green  \u2190  Tint  \u2192  Magenta"
        );

    connectSliderAndSpinBox(
        m_temperatureSlider,
        m_temperatureSpinBox
        );

    connectSliderAndSpinBox(
        m_tintSlider,
        m_tintSpinBox
        );

    connectSliderAndSpinBox(
        m_strengthSlider,
        m_strengthSpinBox
        );

    m_previewTimer =
        new QTimer(this);

    m_previewTimer->setSingleShot(
        true
        );

    m_previewTimer->setInterval(
        25
        );

    connect(
        m_previewTimer,
        &QTimer::timeout,
        this,
        &WhiteBalanceDialog::
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
        &WhiteBalanceDialog::
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
}

void WhiteBalanceDialog::createControlRow(
    const QString &labelText,
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
        new DoubleClickResetSlider(
            Qt::Horizontal,
            doubleClickResetValue,
            rowWidget
            );

    slider->setRange(
        minimum,
        maximum
        );

    slider->setValue(value);
    slider->setSingleStep(1);
    slider->setPageStep(10);

    QSpinBox *spinBox =
        new QSpinBox(rowWidget);

    spinBox->setRange(
        minimum,
        maximum
        );

    spinBox->setValue(value);
    spinBox->setMinimumWidth(70);

    rowLayout->addWidget(label);

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

    *sliderOut = slider;
    *spinBoxOut = spinBox;
}

void WhiteBalanceDialog::connectSliderAndSpinBox(
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
            schedulePreviewUpdate();
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
            schedulePreviewUpdate();
        });
}

void WhiteBalanceDialog::sampleNeutralAt(
    const QPointF &imagePoint
    )
{
    const QPoint samplePoint(
        std::clamp(
            static_cast<int>(
                std::round(
                    imagePoint.x()
                    )
                ),
            0,
            std::max(
                0,
                m_sourceBaseImage.width() - 1
                )
            ),
        std::clamp(
            static_cast<int>(
                std::round(
                    imagePoint.y()
                    )
                ),
            0,
            std::max(
                0,
                m_sourceBaseImage.height() - 1
                )
            )
        );

    const QColor sampledColor =
        WhiteBalanceTools::
            sampleAverageColor(
                m_sourceBaseImage,
                samplePoint,
                3
                );

    const WhiteBalanceTools::
        ChannelGains sampledGains =
            WhiteBalanceTools::
                gainsFromNeutralColor(
                    sampledColor
                    );

    if (!sampledGains.valid) {
        updateSampleDisplay(
            QColor(),
            "Choose a brighter neutral area"
            );

        return;
    }

    m_autoButton->setChecked(false);

    m_baseGains = sampledGains;

    m_previewWidget->setSamplePoint(
        samplePoint
        );

    updateSampleDisplay(
        sampledColor,
        QString(
            "Neutral sample  R%1  G%2  B%3"
            )
            .arg(
                sampledColor.red()
                )
            .arg(
                sampledColor.green()
                )
            .arg(
                sampledColor.blue()
                )
        );

    updateResultAndPreview();
}

void WhiteBalanceDialog::useAutomaticBalance()
{
    const WhiteBalanceTools::
        ChannelGains autoGains =
            WhiteBalanceTools::
                automaticGains(
                    m_sourceBaseImage
                    );

    if (!autoGains.valid) {
        updateSampleDisplay(
            QColor(),
            "Auto could not find a usable balance"
            );

        return;
    }

    m_baseGains = autoGains;

    m_previewWidget
        ->clearSamplePoint();

    updateSampleDisplay(
        QColor("#D7D7D2"),
        "Automatic balance"
        );

    updateResultAndPreview();
}

void WhiteBalanceDialog::resetControls()
{
    m_baseGains =
        WhiteBalanceTools::
            ChannelGains();

    m_autoButton->setChecked(false);

    {
        QSignalBlocker temperatureSliderBlocker(
            m_temperatureSlider
            );

        QSignalBlocker temperatureSpinBoxBlocker(
            m_temperatureSpinBox
            );

        QSignalBlocker tintSliderBlocker(
            m_tintSlider
            );

        QSignalBlocker tintSpinBoxBlocker(
            m_tintSpinBox
            );

        QSignalBlocker strengthSliderBlocker(
            m_strengthSlider
            );

        QSignalBlocker strengthSpinBoxBlocker(
            m_strengthSpinBox
            );

        m_temperatureSlider->setValue(0);
        m_temperatureSpinBox->setValue(0);

        m_tintSlider->setValue(0);
        m_tintSpinBox->setValue(0);

        m_strengthSlider->setValue(85);
        m_strengthSpinBox->setValue(85);
    }

    if (m_previewTimer &&
        m_previewTimer->isActive()) {
        m_previewTimer->stop();
    }

    m_previewWidget
        ->clearSamplePoint();

    updateSampleDisplay(
        QColor(),
        "No neutral sample"
        );

    updateResultAndPreview();
}

void WhiteBalanceDialog::schedulePreviewUpdate()
{
    if (!m_previewTimer) {
        updateResultAndPreview();
        return;
    }

    m_previewTimer->start();
}

void WhiteBalanceDialog::updateResultAndPreview()
{
    m_resultBaseImage =
        WhiteBalanceTools::
            applyWhiteBalance(
                m_sourceBaseImage,
                currentSettings()
                );

    m_previewWidget->setImage(
        buildDisplayImage(
            m_resultBaseImage
            ),
        true
        );
}

void WhiteBalanceDialog::updateSampleDisplay(
    const QColor &color,
    const QString &description
    )
{
    QColor displayColor = color;

    if (!displayColor.isValid()) {
        displayColor =
            QColor("#44484A");
    }

    m_sampleSwatch->setStyleSheet(
        QString(
            "QLabel {"
            "background-color: rgb(%1, %2, %3);"
            "border: 1px solid #777777;"
            "border-radius: 3px;"
            "}"
            )
            .arg(
                displayColor.red()
                )
            .arg(
                displayColor.green()
                )
            .arg(
                displayColor.blue()
                )
        );

    m_sampleDescription->setText(
        description
        );
}

WhiteBalanceTools::WhiteBalanceSettings
WhiteBalanceDialog::currentSettings() const
{
    WhiteBalanceTools::
        WhiteBalanceSettings settings;

    settings.baseGains =
        m_baseGains;

    settings.temperature =
        m_temperatureSlider
            ? m_temperatureSlider->value()
            : 0;

    settings.tint =
        m_tintSlider
            ? m_tintSlider->value()
            : 0;

    settings.strength =
        m_strengthSlider
            ? m_strengthSlider->value()
            : 85;

    return settings;
}

QImage WhiteBalanceDialog::buildDisplayImage(
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
