#include "redeyeselectionwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace {

QIcon makeRedEyeSelectorIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );

    QPainterPath eye;
    eye.moveTo(2.5, 12.0);
    eye.cubicTo(6.2, 6.7,
                10.0, 5.1,
                12.0, 5.1);
    eye.cubicTo(16.0, 5.1,
                19.3, 7.4,
                21.5, 12.0);
    eye.cubicTo(18.2, 16.9,
                14.8, 18.7,
                12.0, 18.7);
    eye.cubicTo(8.4, 18.7,
                5.1, 16.5,
                2.5, 12.0);
    eye.closeSubpath();

    painter.setPen(
        QPen(
            QColor(225, 225, 225),
            1.4
            )
        );

    painter.setBrush(
        QColor(80, 80, 80, 90)
        );

    painter.drawPath(eye);

    painter.setPen(
        QPen(
            QColor(80, 10, 10),
            1.0
            )
        );

    painter.setBrush(
        QColor(205, 45, 38)
        );

    painter.drawEllipse(
        QPointF(12.0, 12.0),
        3.2,
        3.2
        );

    painter.setPen(Qt::NoPen);
    painter.setBrush(
        QColor(30, 20, 20)
        );

    painter.drawEllipse(
        QPointF(12.0, 12.0),
        1.15,
        1.15
        );

    return QIcon(pixmap);
}

} // namespace

RedEyeSelectionWidget::RedEyeSelectionWidget(
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
        makeRedEyeSelectorIcon()
        );

    setToolModeToolTip(
        "Red Eye selector: drag a box around one eye"
        );

    // Red Eye selection is the default mode.
    // The hand button switches left drag to panning.
    setPanEnabled(true);
    setLeftButtonPans(false);
}

QRect RedEyeSelectionWidget::selectionRect() const
{
    return m_selectionImageRect.normalized();
}

void RedEyeSelectionWidget::clearSelection()
{
    m_selecting = false;
    m_selectionImageRect = QRect();

    update();

    emit selectionChanged();
}


void RedEyeSelectionWidget::setColorPickerActive(
    bool active
    )
{
    if (m_colorPickerActive == active) {
        return;
    }

    m_colorPickerActive = active;
    m_selecting = false;

    if (active) {
        // Sampling is a tool click, so leave hand/pan mode automatically.
        setLeftButtonPans(
            false
            );
    }

    setToolCursor(
        Qt::CrossCursor
        );

    setToolModeToolTip(
        active
            ? "Click the unwanted eye-glow color"
            : "Red Eye selector: drag a box around one eye"
        );

    update();

    emit colorPickerActiveChanged(
        active
        );
}

bool RedEyeSelectionWidget::colorPickerActive() const
{
    return m_colorPickerActive;
}

void RedEyeSelectionWidget::paintImageOverlay(
    QPainter &painter
    )
{
    if (m_selectionImageRect.isNull() ||
        m_selectionImageRect.isEmpty()) {
        return;
    }

    QPen selectionPen(
        QColor(255, 220, 0),
        2.0,
        Qt::DashLine
        );

    selectionPen.setCosmetic(true);

    painter.setRenderHint(
        QPainter::Antialiasing,
        false
        );

    painter.setPen(selectionPen);

    painter.setBrush(
        QColor(255, 220, 0, 35)
        );

    painter.drawRect(
        QRectF(
            m_selectionImageRect.normalized()
            )
        );
}

void RedEyeSelectionWidget::toolMousePressEvent(
    QMouseEvent *event,
    const QPointF &imagePoint,
    bool insideImage
    )
{
    if (image().isNull() ||
        event->button() != Qt::LeftButton ||
        !insideImage) {
        return;
    }

    if (m_colorPickerActive) {
        const QPoint samplePoint =
            boundedImagePixel(
                imagePoint
                );

        emit colorSampleRequested(
            samplePoint
            );

        setColorPickerActive(
            false
            );

        event->accept();
        return;
    }

    m_selecting = true;

    m_startImagePoint =
        boundedImagePixel(
            imagePoint
            );

    m_selectionImageRect = QRect(
        m_startImagePoint,
        QSize()
        );

    update();
    event->accept();
}

void RedEyeSelectionWidget::toolMouseMoveEvent(
    QMouseEvent *event,
    const QPointF &imagePoint,
    bool
    )
{
    if (!m_selecting ||
        image().isNull()) {
        return;
    }

    const QPoint currentImagePoint =
        boundedImagePixel(
            imagePoint
            );

    m_selectionImageRect =
        QRect(
            m_startImagePoint,
            currentImagePoint
            ).normalized();

    update();
    event->accept();
}

void RedEyeSelectionWidget::toolMouseReleaseEvent(
    QMouseEvent *event,
    const QPointF &imagePoint,
    bool
    )
{
    if (!m_selecting ||
        image().isNull() ||
        event->button() != Qt::LeftButton) {
        return;
    }

    m_selecting = false;

    const QPoint currentImagePoint =
        boundedImagePixel(
            imagePoint
            );

    m_selectionImageRect =
        QRect(
            m_startImagePoint,
            currentImagePoint
            ).normalized();

    if (m_selectionImageRect.width() < 3 ||
        m_selectionImageRect.height() < 3) {
        m_selectionImageRect = QRect();
    }

    update();

    emit selectionChanged();

    event->accept();
}

QPoint RedEyeSelectionWidget::boundedImagePixel(
    const QPointF &imagePoint
    ) const
{
    if (image().isNull()) {
        return QPoint();
    }

    const int imageX = std::clamp(
        static_cast<int>(
            std::round(
                imagePoint.x()
                )
            ),
        0,
        image().width() - 1
        );

    const int imageY = std::clamp(
        static_cast<int>(
            std::round(
                imagePoint.y()
                )
            ),
        0,
        image().height() - 1
        );

    return QPoint(
        imageX,
        imageY
        );
}
