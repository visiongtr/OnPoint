#include "overexposureselectionwidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <cmath>

OverexposureSelectionWidget::OverexposureSelectionWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(640, 300);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

void OverexposureSelectionWidget::setImage(const QImage &image)
{
    m_image = image;
    updateScaledPixmap();
    update();
}

void OverexposureSelectionWidget::setOverlaySettings(
    int sampleDistance,
    int sampleWidth,
    OverexposureTools::RegionShape regionShape)
{
    m_sampleDistance = sampleDistance;
    m_sampleWidth = sampleWidth;
    m_regionShape = regionShape;

    update();
}

QRect OverexposureSelectionWidget::selectionRect() const
{
    return effectiveSelectionRect(m_selectionImageRect.normalized());
}

void OverexposureSelectionWidget::clearSelection()
{
    m_selectionImageRect = QRect();
    update();

    emit selectionChanged();
}

void OverexposureSelectionWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScaledPixmap();
}

void OverexposureSelectionWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    if (!m_scaledPixmap.isNull()) {
        painter.drawPixmap(m_pixmapRect.topLeft(), m_scaledPixmap);
    }

    drawSelection(painter);
}

void OverexposureSelectionWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull() || event->button() != Qt::LeftButton) {
        return;
    }

    m_dragging = true;
    m_dragStartImagePoint = widgetPointToImagePoint(event->pos());
    m_dragStartRect = m_selectionImageRect.normalized();

    m_dragMode = hitTest(event->pos());

    if (m_dragMode == DragMode::None) {
        m_dragMode = DragMode::NewSelection;
        m_selectionImageRect = QRect(m_dragStartImagePoint, QSize());
    }

    update();
}

void OverexposureSelectionWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_image.isNull()) {
        return;
    }

    if (!m_dragging) {
        const DragMode mode = hitTest(event->pos());

        switch (mode) {
        case DragMode::Left:
        case DragMode::Right:
            setCursor(Qt::SizeHorCursor);
            break;

        case DragMode::Top:
        case DragMode::Bottom:
            setCursor(Qt::SizeVerCursor);
            break;

        case DragMode::TopLeft:
        case DragMode::BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;

        case DragMode::TopRight:
        case DragMode::BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;

        case DragMode::Move:
            setCursor(Qt::SizeAllCursor);
            break;

        default:
            setCursor(Qt::CrossCursor);
            break;
        }

        return;
    }

    const QPoint currentImagePoint = widgetPointToImagePoint(event->pos());
    m_selectionImageRect = adjustedSelectionForDrag(currentImagePoint);

    update();

    emit selectionChanged();
}

void OverexposureSelectionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || m_image.isNull()) {
        return;
    }

    m_dragging = false;

    const QPoint currentImagePoint = widgetPointToImagePoint(event->pos());
    m_selectionImageRect = adjustedSelectionForDrag(currentImagePoint);

    if (m_selectionImageRect.width() < 4 || m_selectionImageRect.height() < 4) {
        m_selectionImageRect = QRect();
    }

    m_dragMode = DragMode::None;

    update();

    emit selectionChanged();
}

void OverexposureSelectionWidget::updateScaledPixmap()
{
    if (m_image.isNull() || width() <= 0 || height() <= 0) {
        m_scaledPixmap = QPixmap();
        m_pixmapRect = QRect();
        return;
    }

    QPixmap sourcePixmap = QPixmap::fromImage(m_image);

    QSize scaledSize = m_image.size();
    scaledSize.scale(size(), Qt::KeepAspectRatio);

    m_scaledPixmap = sourcePixmap.scaled(
        scaledSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
        );

    const int x = (width() - m_scaledPixmap.width()) / 2;
    const int y = (height() - m_scaledPixmap.height()) / 2;

    m_pixmapRect = QRect(x, y, m_scaledPixmap.width(), m_scaledPixmap.height());
}

QPoint OverexposureSelectionWidget::widgetPointToImagePoint(const QPoint &point) const
{
    if (m_image.isNull() || m_pixmapRect.isEmpty()) {
        return QPoint();
    }

    const int clampedX = std::clamp(point.x(), m_pixmapRect.left(), m_pixmapRect.right());
    const int clampedY = std::clamp(point.y(), m_pixmapRect.top(), m_pixmapRect.bottom());

    const double xRatio =
        static_cast<double>(m_image.width()) /
        static_cast<double>(m_pixmapRect.width());

    const double yRatio =
        static_cast<double>(m_image.height()) /
        static_cast<double>(m_pixmapRect.height());

    const int imageX = std::clamp(
        static_cast<int>(std::round((clampedX - m_pixmapRect.left()) * xRatio)),
        0,
        m_image.width() - 1
        );

    const int imageY = std::clamp(
        static_cast<int>(std::round((clampedY - m_pixmapRect.top()) * yRatio)),
        0,
        m_image.height() - 1
        );

    return QPoint(imageX, imageY);
}

QRect OverexposureSelectionWidget::imageRectToWidgetRect(const QRect &imageRect) const
{
    if (m_image.isNull() || m_pixmapRect.isEmpty()) {
        return QRect();
    }

    const double xRatio =
        static_cast<double>(m_pixmapRect.width()) /
        static_cast<double>(m_image.width());

    const double yRatio =
        static_cast<double>(m_pixmapRect.height()) /
        static_cast<double>(m_image.height());

    const int x = m_pixmapRect.left() + static_cast<int>(std::round(imageRect.left() * xRatio));
    const int y = m_pixmapRect.top() + static_cast<int>(std::round(imageRect.top() * yRatio));
    const int w = static_cast<int>(std::round(imageRect.width() * xRatio));
    const int h = static_cast<int>(std::round(imageRect.height() * yRatio));

    return QRect(x, y, w, h);
}

QRect OverexposureSelectionWidget::effectiveSelectionRect(const QRect &rect) const
{
    if (rect.isNull() || rect.isEmpty()) {
        return QRect();
    }

    QRect normalizedRect = rect.normalized();

    if (m_regionShape != OverexposureTools::RegionShape::Square) {
        return normalizedRect;
    }

    const int side = std::min(normalizedRect.width(), normalizedRect.height());

    const QPoint center = normalizedRect.center();

    QRect squareRect(
        center.x() - (side / 2),
        center.y() - (side / 2),
        side,
        side
        );

    return clampSelectionToImage(squareRect);
}

OverexposureSelectionWidget::DragMode OverexposureSelectionWidget::hitTest(const QPoint &widgetPoint) const
{
    if (m_selectionImageRect.isNull() || m_selectionImageRect.isEmpty()) {
        return DragMode::None;
    }

    const QRect widgetRect = imageRectToWidgetRect(m_selectionImageRect.normalized());

    if (widgetRect.isNull() || widgetRect.isEmpty()) {
        return DragMode::None;
    }

    const int handleRange = 8;
    const QRect hitArea = widgetRect.adjusted(
        -handleRange,
        -handleRange,
        handleRange,
        handleRange
        );

    if (!hitArea.contains(widgetPoint)) {
        return DragMode::None;
    }

    const bool nearLeft = std::abs(widgetPoint.x() - widgetRect.left()) <= handleRange;
    const bool nearRight = std::abs(widgetPoint.x() - widgetRect.right()) <= handleRange;
    const bool nearTop = std::abs(widgetPoint.y() - widgetRect.top()) <= handleRange;
    const bool nearBottom = std::abs(widgetPoint.y() - widgetRect.bottom()) <= handleRange;

    if (nearLeft && nearTop) {
        return DragMode::TopLeft;
    }

    if (nearRight && nearTop) {
        return DragMode::TopRight;
    }

    if (nearLeft && nearBottom) {
        return DragMode::BottomLeft;
    }

    if (nearRight && nearBottom) {
        return DragMode::BottomRight;
    }

    if (nearLeft) {
        return DragMode::Left;
    }

    if (nearRight) {
        return DragMode::Right;
    }

    if (nearTop) {
        return DragMode::Top;
    }

    if (nearBottom) {
        return DragMode::Bottom;
    }

    if (widgetRect.contains(widgetPoint)) {
        return DragMode::Move;
    }

    return DragMode::None;
}

QRect OverexposureSelectionWidget::adjustedSelectionForDrag(const QPoint &currentImagePoint) const
{
    if (m_image.isNull()) {
        return QRect();
    }

    const int minimumSize = 4;

    QRect rect = m_dragStartRect.normalized();

    if (m_dragMode == DragMode::NewSelection) {
        return clampSelectionToImage(QRect(m_dragStartImagePoint, currentImagePoint).normalized());
    }

    if (m_dragMode == DragMode::Move) {
        const QPoint delta = currentImagePoint - m_dragStartImagePoint;
        rect.translate(delta);

        if (rect.left() < 0) {
            rect.moveLeft(0);
        }

        if (rect.top() < 0) {
            rect.moveTop(0);
        }

        if (rect.right() >= m_image.width()) {
            rect.moveRight(m_image.width() - 1);
        }

        if (rect.bottom() >= m_image.height()) {
            rect.moveBottom(m_image.height() - 1);
        }

        return rect;
    }

    switch (m_dragMode) {
    case DragMode::Left:
        rect.setLeft(std::clamp(currentImagePoint.x(), 0, rect.right() - minimumSize));
        break;

    case DragMode::Right:
        rect.setRight(std::clamp(currentImagePoint.x(), rect.left() + minimumSize, m_image.width() - 1));
        break;

    case DragMode::Top:
        rect.setTop(std::clamp(currentImagePoint.y(), 0, rect.bottom() - minimumSize));
        break;

    case DragMode::Bottom:
        rect.setBottom(std::clamp(currentImagePoint.y(), rect.top() + minimumSize, m_image.height() - 1));
        break;

    case DragMode::TopLeft:
        rect.setLeft(std::clamp(currentImagePoint.x(), 0, rect.right() - minimumSize));
        rect.setTop(std::clamp(currentImagePoint.y(), 0, rect.bottom() - minimumSize));
        break;

    case DragMode::TopRight:
        rect.setRight(std::clamp(currentImagePoint.x(), rect.left() + minimumSize, m_image.width() - 1));
        rect.setTop(std::clamp(currentImagePoint.y(), 0, rect.bottom() - minimumSize));
        break;

    case DragMode::BottomLeft:
        rect.setLeft(std::clamp(currentImagePoint.x(), 0, rect.right() - minimumSize));
        rect.setBottom(std::clamp(currentImagePoint.y(), rect.top() + minimumSize, m_image.height() - 1));
        break;

    case DragMode::BottomRight:
        rect.setRight(std::clamp(currentImagePoint.x(), rect.left() + minimumSize, m_image.width() - 1));
        rect.setBottom(std::clamp(currentImagePoint.y(), rect.top() + minimumSize, m_image.height() - 1));
        break;

    default:
        break;
    }

    return clampSelectionToImage(rect.normalized());
}

QRect OverexposureSelectionWidget::clampSelectionToImage(const QRect &rect) const
{
    if (m_image.isNull()) {
        return QRect();
    }

    const QRect imageRect(0, 0, m_image.width(), m_image.height());
    return rect.normalized().intersected(imageRect);
}

void OverexposureSelectionWidget::drawSelection(QPainter &painter)
{
    if (m_image.isNull() ||
        m_selectionImageRect.isNull() ||
        m_selectionImageRect.isEmpty()) {
        return;
    }

    const QRect imageSelection = effectiveSelectionRect(m_selectionImageRect.normalized());

    const int smallestSide = std::max(1, std::min(imageSelection.width(), imageSelection.height()));

    const int sampleDistancePixels = std::max(
        2,
        static_cast<int>(std::round(smallestSide * (0.05 + (m_sampleDistance / 100.0 * 0.60))))
        );

    const int sampleWidthPixels = std::max(
        2,
        static_cast<int>(std::round(smallestSide * (0.03 + (m_sampleWidth / 100.0 * 0.35))))
        );

    const QRect innerSampleRect = imageSelection.adjusted(
        -sampleDistancePixels,
        -sampleDistancePixels,
        sampleDistancePixels,
        sampleDistancePixels
        );

    const QRect outerSampleRect = innerSampleRect.adjusted(
        -sampleWidthPixels,
        -sampleWidthPixels,
        sampleWidthPixels,
        sampleWidthPixels
        );

    const QRect widgetSelection = imageRectToWidgetRect(imageSelection);
    const QRect widgetInnerSample = imageRectToWidgetRect(innerSampleRect);
    const QRect widgetOuterSample = imageRectToWidgetRect(outerSampleRect);

    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QPen(QColor(70, 170, 255), 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);

    if (m_regionShape == OverexposureTools::RegionShape::Oval) {
        painter.drawEllipse(widgetInnerSample);
        painter.drawEllipse(widgetOuterSample);
    } else {
        painter.drawRect(widgetInnerSample);
        painter.drawRect(widgetOuterSample);
    }

    painter.setPen(QPen(QColor(255, 220, 0), 2, Qt::DashLine));

    // No yellow fill here.
    // The fill was tinting the photo and making subtle corrections hard to judge.
    painter.setBrush(Qt::NoBrush);

    if (m_regionShape == OverexposureTools::RegionShape::Oval) {
        painter.drawEllipse(widgetSelection);
    } else {
        painter.drawRect(widgetSelection);
    }

    painter.setBrush(QColor(255, 220, 0));
    painter.setPen(QPen(QColor(30, 30, 30), 1));

    const int handleSize = 7;

    const QPoint points[] = {
        widgetSelection.topLeft(),
        QPoint(widgetSelection.center().x(), widgetSelection.top()),
        widgetSelection.topRight(),
        QPoint(widgetSelection.left(), widgetSelection.center().y()),
        QPoint(widgetSelection.right(), widgetSelection.center().y()),
        widgetSelection.bottomLeft(),
        QPoint(widgetSelection.center().x(), widgetSelection.bottom()),
        widgetSelection.bottomRight()
    };

    for (const QPoint &point : points) {
        painter.drawRect(
            QRect(
                point.x() - (handleSize / 2),
                point.y() - (handleSize / 2),
                handleSize,
                handleSize
                )
            );
    }
}
