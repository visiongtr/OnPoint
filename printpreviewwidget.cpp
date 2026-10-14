#include "printpreviewwidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPrinter>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {

QPointF mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

} // namespace

PrintPreviewWidget::PrintPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    // The paper preview should yield space to the controls on smaller or
    // display-scaled desktops instead of forcing the whole dialog off-screen.
    setMinimumSize(320, 340);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void PrintPreviewWidget::setPrinter(QPrinter *printer)
{
    m_printer = printer;
    update();
}

void PrintPreviewWidget::setImage(
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY
    )
{
    m_image = image;
    m_sourceDotsPerMeterX = sourceDotsPerMeterX;
    m_sourceDotsPerMeterY = sourceDotsPerMeterY;
    update();
}

void PrintPreviewWidget::setLayoutSettings(
    const OnPointPrint::LayoutSettings &settings
    )
{
    m_settings = settings;
    update();
}

OnPointPrint::LayoutSettings PrintPreviewWidget::layoutSettings() const
{
    return m_settings;
}

PrintPreviewWidget::PreviewGeometry PrintPreviewWidget::previewGeometry() const
{
    PreviewGeometry geometry;

    if (!m_printer) {
        return geometry;
    }

    geometry.layout =
        OnPointPrint::calculateLayout(
            *m_printer,
            m_image,
            m_sourceDotsPerMeterX,
            m_sourceDotsPerMeterY,
            m_settings
            );

    if (geometry.layout.paperRectPoints.isEmpty()) {
        return geometry;
    }

    const double availableWidth =
        std::max(1, width() - 48);

    const double availableHeight =
        std::max(1, height() - 48);

    geometry.pointsToWidget =
        std::min(
            availableWidth /
                geometry.layout.paperRectPoints.width(),
            availableHeight /
                geometry.layout.paperRectPoints.height()
            );

    const QSizeF paperWidgetSize(
        geometry.layout.paperRectPoints.width() *
            geometry.pointsToWidget,
        geometry.layout.paperRectPoints.height() *
            geometry.pointsToWidget
        );

    geometry.paperTopLeftWidget =
        QPointF(
            (width() - paperWidgetSize.width()) * 0.5,
            (height() - paperWidgetSize.height()) * 0.5
            );

    auto mapRectToWidget =
        [&geometry](const QRectF &rectPoints) {
            return QRectF(
                geometry.paperTopLeftWidget.x() +
                    rectPoints.x() * geometry.pointsToWidget,
                geometry.paperTopLeftWidget.y() +
                    rectPoints.y() * geometry.pointsToWidget,
                rectPoints.width() * geometry.pointsToWidget,
                rectPoints.height() * geometry.pointsToWidget
                );
        };

    geometry.paperRectWidget =
        mapRectToWidget(
            geometry.layout.paperRectPoints
            );

    geometry.contentRectWidget =
        mapRectToWidget(
            geometry.layout.contentRectPoints
            );

    geometry.imageRectWidget =
        mapRectToWidget(
            geometry.layout.imageRectPoints
            );

    return geometry;
}

void PrintPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(78, 81, 84));

    const PreviewGeometry geometry =
        previewGeometry();

    if (geometry.paperRectWidget.isEmpty()) {
        painter.setPen(Qt::white);
        painter.drawText(
            rect(),
            Qt::AlignCenter,
            "No printer page is available."
            );
        return;
    }

    painter.fillRect(
        geometry.paperRectWidget,
        Qt::white
        );

    painter.save();
    painter.setClipRect(
        geometry.paperRectWidget
        );

    if (!m_image.isNull() &&
        !geometry.imageRectWidget.isEmpty()) {
        painter.setRenderHint(
            QPainter::SmoothPixmapTransform,
            true
            );

        painter.drawImage(
            geometry.imageRectWidget,
            m_image
            );
    }

    painter.restore();

    // The physical sheet edge is always a thin solid black line.
    QPen paperEdgePen(Qt::black);
    paperEdgePen.setWidthF(1.0);
    paperEdgePen.setStyle(Qt::SolidLine);
    painter.setPen(paperEdgePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(
        geometry.paperRectWidget
        );

    // The active margin/printable boundary is a thin dashed black line.
    if (m_settings.showMarginGuide &&
        !geometry.contentRectWidget.isEmpty()) {
        QPen marginPen(Qt::black);
        marginPen.setWidthF(1.0);
        marginPen.setStyle(Qt::DashLine);
        painter.setPen(marginPen);
        painter.drawRect(
            geometry.contentRectWidget
            );
    }
}

void PrintPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const PreviewGeometry geometry =
        previewGeometry();

    if (!geometry.imageRectWidget.contains(mousePosition(event))) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_draggingImage = true;
    m_dragOffset =
        mousePosition(event) -
        geometry.imageRectWidget.topLeft();

    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void PrintPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingImage) {
        applyDraggedImageTopLeft(
            mousePosition(event) - m_dragOffset
            );

        event->accept();
        return;
    }

    updateHoverCursor(
        mousePosition(event)
        );

    QWidget::mouseMoveEvent(event);
}

void PrintPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton &&
        m_draggingImage) {
        m_draggingImage = false;
        updateHoverCursor(mousePosition(event));
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PrintPreviewWidget::leaveEvent(QEvent *event)
{
    if (!m_draggingImage) {
        unsetCursor();
    }

    QWidget::leaveEvent(event);
}

void PrintPreviewWidget::updateHoverCursor(const QPointF &position)
{
    const PreviewGeometry geometry =
        previewGeometry();

    if (geometry.imageRectWidget.contains(position)) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}

void PrintPreviewWidget::applyDraggedImageTopLeft(
    const QPointF &topLeftWidget
    )
{
    const PreviewGeometry geometry =
        previewGeometry();

    if (geometry.pointsToWidget <= 0.0 ||
        geometry.layout.imageRectPoints.isEmpty()) {
        return;
    }

    const QPointF topLeftPoints(
        (topLeftWidget.x() -
         geometry.paperTopLeftWidget.x()) /
            geometry.pointsToWidget,
        (topLeftWidget.y() -
         geometry.paperTopLeftWidget.y()) /
            geometry.pointsToWidget
        );

    const double horizontalTravel =
        geometry.layout.contentRectPoints.width() -
        geometry.layout.imageRectPoints.width();

    const double verticalTravel =
        geometry.layout.contentRectPoints.height() -
        geometry.layout.imageRectPoints.height();

    double horizontalPosition = 0.5;
    double verticalPosition = 0.5;

    if (std::abs(horizontalTravel) > 0.0001) {
        horizontalPosition =
            (topLeftPoints.x() -
             geometry.layout.contentRectPoints.left()) /
            horizontalTravel;
    }

    if (std::abs(verticalTravel) > 0.0001) {
        verticalPosition =
            (topLeftPoints.y() -
             geometry.layout.contentRectPoints.top()) /
            verticalTravel;
    }

    m_settings.centerImage = false;
    m_settings.horizontalPosition =
        std::clamp(horizontalPosition, 0.0, 1.0);
    m_settings.verticalPosition =
        std::clamp(verticalPosition, 0.0, 1.0);

    emit layoutSettingsChanged(
        m_settings
        );

    update();
}
