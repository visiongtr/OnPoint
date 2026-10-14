#include "brightareaselectionwidget.h"
#include "bright_area_tools.h"

#include <QBrush>
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QResizeEvent>
#include <QTimer>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <limits>

BrightAreaSelectionWidget::BrightAreaSelectionWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(640, 300);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    // A compact diagonal black/white tile is clipped to the cached selection
    // boundary. Shifting the tile produces the familiar photo-editor
    // “marching ants” effect without hiding the live recovery preview.
    constexpr int patternSize = 12;
    constexpr int stripeWidth = 3;

    QImage patternImage(
        patternSize,
        patternSize,
        QImage::Format_ARGB32_Premultiplied
        );

    for (int y = 0; y < patternSize; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(patternImage.scanLine(y));

        for (int x = 0; x < patternSize; ++x) {
            const bool whiteStripe =
                (((x + y) / stripeWidth) % 2) == 0;

            line[x] = whiteStripe
                ? qRgba(255, 255, 255, 255)
                : qRgba(0, 0, 0, 255);
        }
    }

    m_marchingAntPattern = QPixmap::fromImage(patternImage);

    m_marchingAntsTimer = new QTimer(this);
    m_marchingAntsTimer->setInterval(90);

    connect(m_marchingAntsTimer, &QTimer::timeout, this, [this]() {
        m_marchingAntsPhase = (m_marchingAntsPhase + 1) % 12;

        if (isVisible() &&
            m_maskVisible &&
            !m_mask.isNull() &&
            !m_outlineEditEnabled) {
            update();
        }
    });

    m_marchingAntsTimer->start();
}

void BrightAreaSelectionWidget::setImage(const QImage &image)
{
    m_image = image;
    updateScaledPixmap();
    update();
}

QImage BrightAreaSelectionWidget::image() const
{
    return m_image;
}

void BrightAreaSelectionWidget::setMask(const QImage &mask)
{
    m_mask = mask;

    if (!m_mask.isNull() && m_mask.size() != m_image.size()) {
        m_mask = QImage();
    } else if (!m_mask.isNull() &&
               m_mask.format() != QImage::Format_Grayscale8) {
        m_mask = m_mask.convertToFormat(QImage::Format_Grayscale8);
    }

    // While outline editing is active, preview refreshes repeatedly send the
    // same composite mask back to the canvas. Preserve the live outline array
    // so dragged nodes do not snap back. Outside outline mode, invalidate the
    // cached outlines so a later Add/Subtract/brush edit is rebuilt as the
    // current set of disconnected regions when outline editing is reopened.
    if (m_outlineEditEnabled) {
        if (m_outlineRegions.isEmpty()) {
            buildEditableOutlinesFromMask();
        }
    } else {
        clearEditableOutlines();
    }

    updateSelectionBoundaryPixmap();
    update();
}

QImage BrightAreaSelectionWidget::mask() const
{
    return m_mask;
}

void BrightAreaSelectionWidget::clearMask()
{
    m_mask = QImage();
    m_selectionBoundaryPixmap = QPixmap();
    clearEditableOutlines();
    m_outlineAction = OutlineAction::Move;

    update();
}

void BrightAreaSelectionWidget::setManualRefineEnabled(bool enabled)
{
    m_manualRefineEnabled = enabled;

    if (enabled) {
        m_outlineEditEnabled = false;
        m_colorSampleEnabled = false;
        clearEditableOutlines();
        m_outlineAction = OutlineAction::Move;
    } else {
        m_painting = false;
    }

    updateInteractionCursor();
    update();
}

void BrightAreaSelectionWidget::setOutlineEditEnabled(bool enabled)
{
    m_outlineEditEnabled = enabled;

    if (enabled) {
        m_manualRefineEnabled = false;
        m_colorSampleEnabled = false;
        m_painting = false;
        m_outlineAction = OutlineAction::Move;
        buildEditableOutlinesFromMask();
    } else {
        // The rebuilt composite mask is kept. Only the temporary node arrays
        // are released, so turning outline editing off never discards the
        // user's manual shape and another automatic area can still be added.
        clearEditableOutlines();
        m_outlineAction = OutlineAction::Move;
    }

    updateInteractionCursor();
    update();
}

void BrightAreaSelectionWidget::setColorSampleEnabled(bool enabled)
{
    m_colorSampleEnabled = enabled;

    if (enabled) {
        m_manualRefineEnabled = false;
        m_outlineEditEnabled = false;
        m_painting = false;
        clearEditableOutlines();
        m_outlineAction = OutlineAction::Move;
    }

    updateInteractionCursor();
    update();
}

void BrightAreaSelectionWidget::armAddOutlineNode()
{
    bool hasEditableRegion = false;

    for (const QVector<QPointF> &region : m_outlineRegions) {
        if (region.size() >= 3) {
            hasEditableRegion = true;
            break;
        }
    }

    if (!m_outlineEditEnabled || !hasEditableRegion) {
        return;
    }

    m_outlineAction = OutlineAction::Add;
    updateInteractionCursor();
}

void BrightAreaSelectionWidget::armRemoveOutlineNode()
{
    bool hasRemovableNode = false;

    for (const QVector<QPointF> &region : m_outlineRegions) {
        if (region.size() > 3) {
            hasRemovableNode = true;
            break;
        }
    }

    if (!m_outlineEditEnabled || !hasRemovableNode) {
        return;
    }

    m_outlineAction = OutlineAction::Remove;
    updateInteractionCursor();
}

void BrightAreaSelectionWidget::setBrushAddsToSelection(bool addsToSelection)
{
    m_brushAddsToSelection = addsToSelection;
}

void BrightAreaSelectionWidget::setBrushSize(int brushSize)
{
    m_brushSize = std::max(1, brushSize);
    update();
}

void BrightAreaSelectionWidget::setBrushSoftness(int brushSoftness)
{
    m_brushSoftness = std::clamp(brushSoftness, 0, 100);
}

void BrightAreaSelectionWidget::setBrushStrength(int brushStrength)
{
    m_brushStrength = std::clamp(brushStrength, 0, 100);
}

void BrightAreaSelectionWidget::setMaskVisible(bool visible)
{
    m_maskVisible = visible;
    update();
}

void BrightAreaSelectionWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScaledPixmap();
}

void BrightAreaSelectionWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    if (!m_scaledPixmap.isNull()) {
        painter.drawPixmap(m_pixmapRect.topLeft(), m_scaledPixmap);
    }

    if (m_maskVisible &&
        !m_mask.isNull() &&
        !m_pixmapRect.isEmpty() &&
        !m_outlineEditEnabled &&
        !m_selectionBoundaryPixmap.isNull()) {
        QImage antsImage(
            m_selectionBoundaryPixmap.size(),
            QImage::Format_ARGB32_Premultiplied
            );
        antsImage.fill(Qt::transparent);

        QPainter antsPainter(&antsImage);
        antsPainter.drawPixmap(0, 0, m_selectionBoundaryPixmap);
        antsPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);

        QBrush antsBrush(m_marchingAntPattern);
        QTransform brushTransform;
        brushTransform.translate(m_marchingAntsPhase, 0);
        antsBrush.setTransform(brushTransform);

        antsPainter.fillRect(antsImage.rect(), antsBrush);
        antsPainter.end();

        painter.drawImage(m_pixmapRect.topLeft(), antsImage);
    }

    if (m_outlineEditEnabled && !m_outlineRegions.isEmpty()) {
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.setPen(QPen(QColor(255, 220, 0), 2, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);

        for (const QVector<QPointF> &region : m_outlineRegions) {
            if (region.size() < 3) {
                continue;
            }

            QPolygonF widgetPolygon;

            for (const QPointF &imagePoint : region) {
                widgetPolygon << QPointF(imagePointToWidgetPoint(imagePoint));
            }

            painter.drawPolygon(widgetPolygon);
        }

        painter.setPen(QPen(QColor(25, 25, 25), 1));
        painter.setBrush(QColor(255, 220, 0));

        const int pointSize = 8;

        for (const QVector<QPointF> &region : m_outlineRegions) {
            for (const QPointF &imagePoint : region) {
                const QPoint widgetPoint = imagePointToWidgetPoint(imagePoint);

                painter.drawEllipse(
                    widgetPoint,
                    pointSize / 2,
                    pointSize / 2
                    );
            }
        }
    }

    if (m_manualRefineEnabled && !m_image.isNull()) {
        const QPoint cursorPoint = mapFromGlobal(QCursor::pos());

        if (rect().contains(cursorPoint)) {
            painter.setRenderHint(QPainter::Antialiasing, true);

            const QColor brushColor =
                m_brushAddsToSelection ?
                    QColor(70, 170, 255) :
                    QColor(255, 90, 90);

            painter.setPen(QPen(brushColor, 2));
            painter.setBrush(Qt::NoBrush);

            const double displayScale =
                (!m_pixmapRect.isEmpty() && m_image.width() > 0 && m_image.height() > 0)
                    ? 0.5 * (
                          static_cast<double>(m_pixmapRect.width()) / m_image.width() +
                          static_cast<double>(m_pixmapRect.height()) / m_image.height()
                          )
                    : 1.0;

            const int displayRadius = std::max(
                2,
                static_cast<int>(std::round((m_brushSize * displayScale) / 2.0))
                );

            painter.drawEllipse(
                cursorPoint,
                displayRadius,
                displayRadius
                );
        }
    }
}

void BrightAreaSelectionWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull() || event->button() != Qt::LeftButton) {
        return;
    }

    if (m_colorSampleEnabled) {
        emit colorSampleRequested(widgetPointToImagePoint(event->pos()));
        return;
    }

    if (m_outlineEditEnabled) {
        if (m_outlineAction == OutlineAction::Add) {
            if (addOutlinePointAt(event->pos())) {
                rebuildMaskFromEditableOutlines();
                emit maskEdited(m_mask);
            }

            m_outlineAction = OutlineAction::Move;
            updateInteractionCursor();
            update();
            emit outlineNodeActionCompleted();
            return;
        }

        if (m_outlineAction == OutlineAction::Remove) {
            if (removeOutlinePointAt(event->pos())) {
                rebuildMaskFromEditableOutlines();
                emit maskEdited(m_mask);
            }

            m_outlineAction = OutlineAction::Move;
            updateInteractionCursor();
            update();
            emit outlineNodeActionCompleted();
            return;
        }

        m_activeOutlinePoint = hitTestOutlinePoint(event->pos());

        if (m_activeOutlinePoint.isValid()) {
            m_draggingOutlinePoint = true;
            setCursor(Qt::SizeAllCursor);
        }

        return;
    }

    if (m_manualRefineEnabled) {
        m_painting = true;
        paintBrushAt(event->pos());
        return;
    }

    emit autoSelectRequested(widgetPointToImagePoint(event->pos()));
}

void BrightAreaSelectionWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_image.isNull()) {
        return;
    }

    if (m_colorSampleEnabled) {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_outlineEditEnabled) {
        if (m_outlineAction != OutlineAction::Move) {
            updateInteractionCursor();
            return;
        }

        if (m_draggingOutlinePoint &&
            m_activeOutlinePoint.isValid() &&
            m_activeOutlinePoint.region < m_outlineRegions.size() &&
            m_activeOutlinePoint.point < m_outlineRegions[m_activeOutlinePoint.region].size()) {

            const QPoint imagePoint = widgetPointToImagePoint(event->pos());

            m_outlineRegions[m_activeOutlinePoint.region][m_activeOutlinePoint.point] =
                QPointF(imagePoint);

            rebuildMaskFromEditableOutlines();

            update();
            emit maskEdited(m_mask);

            return;
        }

        const OutlinePointRef hoverPoint = hitTestOutlinePoint(event->pos());
        setCursor(hoverPoint.isValid() ? Qt::SizeAllCursor : Qt::CrossCursor);

        update();
        return;
    }

    if (m_manualRefineEnabled && m_painting) {
        paintBrushAt(event->pos());
        return;
    }

    update();
}

void BrightAreaSelectionWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)

    if (m_outlineEditEnabled) {
        m_draggingOutlinePoint = false;
        m_activeOutlinePoint = OutlinePointRef();
        setCursor(Qt::CrossCursor);
        return;
    }

    if (!m_manualRefineEnabled) {
        return;
    }

    m_painting = false;
}

void BrightAreaSelectionWidget::updateScaledPixmap()
{
    if (m_image.isNull() || width() <= 0 || height() <= 0) {
        m_scaledPixmap = QPixmap();
        m_pixmapRect = QRect();
        m_selectionBoundaryPixmap = QPixmap();
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

    // A normal recovery-preview refresh changes only the displayed image, not
    // the selection mask. Rebuild the cached boundary only when the displayed
    // geometry actually changes (for example after a resize or zoom-to-fit).
    if (!m_mask.isNull() &&
        m_selectionBoundaryPixmap.size() != m_pixmapRect.size()) {
        updateSelectionBoundaryPixmap();
    }
}

void BrightAreaSelectionWidget::updateSelectionBoundaryPixmap()
{
    m_selectionBoundaryPixmap = QPixmap();

    if (m_mask.isNull() || m_pixmapRect.isEmpty()) {
        return;
    }

    QImage displayMask = m_mask.scaled(
        m_pixmapRect.size(),
        Qt::IgnoreAspectRatio,
        Qt::FastTransformation
        );

    if (displayMask.format() != QImage::Format_Grayscale8) {
        displayMask = displayMask.convertToFormat(QImage::Format_Grayscale8);
    }

    if (displayMask.isNull()) {
        return;
    }

    QImage edgeMap(displayMask.size(), QImage::Format_Grayscale8);
    edgeMap.fill(0);

    const auto selectedAt = [&displayMask](int x, int y) {
        if (x < 0 || y < 0 ||
            x >= displayMask.width() || y >= displayMask.height()) {
            return false;
        }

        return displayMask.constScanLine(y)[x] > 0;
    };

    for (int y = 0; y < displayMask.height(); ++y) {
        const uchar *maskLine = displayMask.constScanLine(y);
        uchar *edgeLine = edgeMap.scanLine(y);

        for (int x = 0; x < displayMask.width(); ++x) {
            if (maskLine[x] <= 0) {
                continue;
            }

            const bool boundary =
                !selectedAt(x - 1, y) ||
                !selectedAt(x + 1, y) ||
                !selectedAt(x, y - 1) ||
                !selectedAt(x, y + 1);

            if (boundary) {
                edgeLine[x] = 255;
            }
        }
    }

    QImage boundaryImage(
        edgeMap.size(),
        QImage::Format_ARGB32_Premultiplied
        );
    boundaryImage.fill(Qt::transparent);

    // A small cross-shaped dilation keeps the ants visible after zooming or
    // fitting a large photograph into the canvas without turning the outline
    // into a heavy solid border.
    constexpr int neighborCount = 5;
    const int neighborX[neighborCount] = {0, -1, 1, 0, 0};
    const int neighborY[neighborCount] = {0, 0, 0, -1, 1};

    for (int y = 0; y < edgeMap.height(); ++y) {
        const uchar *edgeLine = edgeMap.constScanLine(y);

        for (int x = 0; x < edgeMap.width(); ++x) {
            if (edgeLine[x] == 0) {
                continue;
            }

            for (int neighborIndex = 0;
                 neighborIndex < neighborCount;
                 ++neighborIndex) {
                const int drawX = x + neighborX[neighborIndex];
                const int drawY = y + neighborY[neighborIndex];

                if (drawX < 0 || drawY < 0 ||
                    drawX >= boundaryImage.width() ||
                    drawY >= boundaryImage.height()) {
                    continue;
                }

                QRgb *line = reinterpret_cast<QRgb *>(
                    boundaryImage.scanLine(drawY)
                    );
                line[drawX] = qRgba(255, 255, 255, 255);
            }
        }
    }

    m_selectionBoundaryPixmap = QPixmap::fromImage(boundaryImage);
}

QPoint BrightAreaSelectionWidget::widgetPointToImagePoint(const QPoint &point) const
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

QPoint BrightAreaSelectionWidget::imagePointToWidgetPoint(const QPointF &point) const
{
    if (m_image.isNull() || m_pixmapRect.isEmpty()) {
        return QPoint();
    }

    const double xRatio =
        static_cast<double>(m_pixmapRect.width()) /
        static_cast<double>(m_image.width());

    const double yRatio =
        static_cast<double>(m_pixmapRect.height()) /
        static_cast<double>(m_image.height());

    const int widgetX =
        m_pixmapRect.left() + static_cast<int>(std::round(point.x() * xRatio));

    const int widgetY =
        m_pixmapRect.top() + static_cast<int>(std::round(point.y() * yRatio));

    return QPoint(widgetX, widgetY);
}

BrightAreaSelectionWidget::OutlineSegmentRef
BrightAreaSelectionWidget::nearestOutlineSegment(const QPointF &imagePoint) const
{
    OutlineSegmentRef bestRef;
    double bestDistanceSquared = std::numeric_limits<double>::max();

    for (int regionIndex = 0; regionIndex < m_outlineRegions.size(); ++regionIndex) {
        const QVector<QPointF> &region = m_outlineRegions[regionIndex];

        if (region.size() < 2) {
            continue;
        }

        for (int segmentIndex = 0; segmentIndex < region.size(); ++segmentIndex) {
            const QPointF a = region[segmentIndex];
            const QPointF b = region[(segmentIndex + 1) % region.size()];
            const QPointF ab = b - a;
            const QPointF ap = imagePoint - a;

            const double denominator = (ab.x() * ab.x()) + (ab.y() * ab.y());
            double amount = 0.0;

            if (denominator > 0.0) {
                amount = ((ap.x() * ab.x()) + (ap.y() * ab.y())) / denominator;
                amount = std::clamp(amount, 0.0, 1.0);
            }

            const QPointF nearest = a + (ab * amount);
            const double dx = imagePoint.x() - nearest.x();
            const double dy = imagePoint.y() - nearest.y();
            const double distanceSquared = (dx * dx) + (dy * dy);

            if (distanceSquared < bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                bestRef.region = regionIndex;
                bestRef.segment = segmentIndex;
            }
        }
    }

    return bestRef;
}

bool BrightAreaSelectionWidget::addOutlinePointAt(const QPoint &widgetPoint)
{
    const QPoint imagePoint = widgetPointToImagePoint(widgetPoint);
    const OutlineSegmentRef segmentRef = nearestOutlineSegment(QPointF(imagePoint));

    if (!segmentRef.isValid() ||
        segmentRef.region >= m_outlineRegions.size() ||
        m_outlineRegions[segmentRef.region].size() < 3) {
        return false;
    }

    m_outlineRegions[segmentRef.region].insert(
        segmentRef.segment + 1,
        QPointF(imagePoint)
        );

    return true;
}

bool BrightAreaSelectionWidget::removeOutlinePointAt(const QPoint &widgetPoint)
{
    const OutlinePointRef pointRef = hitTestOutlinePoint(widgetPoint);

    if (!pointRef.isValid() ||
        pointRef.region >= m_outlineRegions.size() ||
        m_outlineRegions[pointRef.region].size() <= 3) {
        return false;
    }

    m_outlineRegions[pointRef.region].removeAt(pointRef.point);
    return true;
}

void BrightAreaSelectionWidget::updateInteractionCursor()
{
    if (m_manualRefineEnabled) {
        setCursor(Qt::BlankCursor);
        return;
    }

    if (m_outlineEditEnabled) {
        if (m_outlineAction == OutlineAction::Add) {
            setCursor(Qt::CrossCursor);
        } else if (m_outlineAction == OutlineAction::Remove) {
            setCursor(Qt::ForbiddenCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }

        return;
    }

    setCursor(Qt::CrossCursor);
}

void BrightAreaSelectionWidget::paintBrushAt(const QPoint &widgetPoint)
{
    if (m_image.isNull()) {
        return;
    }

    if (m_mask.isNull() || m_mask.size() != m_image.size()) {
        m_mask = QImage(m_image.size(), QImage::Format_Grayscale8);
        m_mask.fill(0);
    }

    const QPoint imagePoint = widgetPointToImagePoint(widgetPoint);

    m_mask = BrightAreaTools::applyBrushToMask(
        m_mask,
        imagePoint,
        m_brushSize,
        m_brushSoftness,
        m_brushStrength,
        m_brushAddsToSelection
        );

    updateSelectionBoundaryPixmap();
    update();

    emit maskEdited(m_mask);
}

void BrightAreaSelectionWidget::buildEditableOutlinesFromMask()
{
    clearEditableOutlines();

    if (m_mask.isNull() || m_image.isNull()) {
        return;
    }

    if (m_mask.format() != QImage::Format_Grayscale8) {
        m_mask = m_mask.convertToFormat(QImage::Format_Grayscale8);
    }

    const int width = m_mask.width();
    const int height = m_mask.height();

    if (width <= 0 || height <= 0) {
        return;
    }

    QImage visited(width, height, QImage::Format_Grayscale8);
    visited.fill(0);

    constexpr int neighborCount = 8;
    const int neighborX[neighborCount] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int neighborY[neighborCount] = {-1, -1, -1, 0, 0, 1, 1, 1};
    constexpr double pi = 3.14159265358979323846;

    const auto appendRectangleFallback = [this](const QRect &bounds) {
        if (!bounds.isValid() || m_image.isNull()) {
            return;
        }

        const int left = std::max(0, bounds.left() - 1);
        const int top = std::max(0, bounds.top() - 1);
        const int right = std::min(m_image.width() - 1, bounds.right() + 1);
        const int bottom = std::min(m_image.height() - 1, bounds.bottom() + 1);

        QVector<QPointF> region;
        region.append(QPointF(left, top));
        region.append(QPointF(right, top));
        region.append(QPointF(right, bottom));
        region.append(QPointF(left, bottom));
        m_outlineRegions.append(region);
    };

    for (int startY = 0; startY < height; ++startY) {
        const uchar *maskLine = m_mask.constScanLine(startY);
        uchar *visitedLine = visited.scanLine(startY);

        for (int startX = 0; startX < width; ++startX) {
            if (maskLine[startX] <= 0 || visitedLine[startX] != 0) {
                continue;
            }

            QVector<QPoint> queue;
            queue.reserve(2048);
            queue.append(QPoint(startX, startY));
            visitedLine[startX] = 1;

            QVector<QPoint> boundaryPoints;
            boundaryPoints.reserve(1024);

            double sumX = 0.0;
            double sumY = 0.0;
            int selectedCount = 0;
            QRect bounds(startX, startY, 1, 1);

            for (int queueIndex = 0; queueIndex < queue.size(); ++queueIndex) {
                const QPoint point = queue[queueIndex];
                sumX += point.x();
                sumY += point.y();
                ++selectedCount;
                bounds = bounds.united(QRect(point, QSize(1, 1)));

                bool isBoundary = false;

                for (int neighborIndex = 0;
                     neighborIndex < neighborCount;
                     ++neighborIndex) {
                    const int nextX = point.x() + neighborX[neighborIndex];
                    const int nextY = point.y() + neighborY[neighborIndex];

                    if (nextX < 0 || nextY < 0 ||
                        nextX >= width || nextY >= height) {
                        isBoundary = true;
                        continue;
                    }

                    const uchar nextValue = m_mask.constScanLine(nextY)[nextX];

                    if (nextValue <= 0) {
                        isBoundary = true;
                        continue;
                    }

                    uchar *nextVisitedLine = visited.scanLine(nextY);

                    if (nextVisitedLine[nextX] == 0) {
                        nextVisitedLine[nextX] = 1;
                        queue.append(QPoint(nextX, nextY));
                    }
                }

                if (isBoundary) {
                    boundaryPoints.append(point);
                }
            }

            if (selectedCount <= 0 || boundaryPoints.size() < 3) {
                appendRectangleFallback(bounds);
                continue;
            }

            const QPointF center(
                sumX / static_cast<double>(selectedCount),
                sumY / static_cast<double>(selectedCount)
                );

            const int pointCount = std::clamp(
                static_cast<int>(boundaryPoints.size() / 14),
                8,
                32
                );

            QVector<double> bestDistance(pointCount, -1.0);
            QVector<QPointF> bestPoint(pointCount);
            QVector<bool> hasPoint(pointCount, false);

            for (const QPoint &point : boundaryPoints) {
                const double dx = point.x() - center.x();
                const double dy = point.y() - center.y();
                const double distance = std::sqrt((dx * dx) + (dy * dy));

                if (distance < 1.0) {
                    continue;
                }

                double angle = std::atan2(dy, dx);

                if (angle < 0.0) {
                    angle += 2.0 * pi;
                }

                const int index =
                    static_cast<int>(std::floor(
                        (angle / (2.0 * pi)) * pointCount
                        )) % pointCount;

                if (!hasPoint[index] || distance > bestDistance[index]) {
                    hasPoint[index] = true;
                    bestDistance[index] = distance;
                    bestPoint[index] = QPointF(point);
                }
            }

            for (int i = 0; i < pointCount; ++i) {
                if (hasPoint[i]) {
                    continue;
                }

                int previousIndex = -1;
                int nextIndex = -1;
                int previousDistance = 0;
                int nextDistance = 0;

                for (int distance = 1; distance < pointCount; ++distance) {
                    const int testIndex =
                        (i - distance + pointCount) % pointCount;

                    if (hasPoint[testIndex]) {
                        previousIndex = testIndex;
                        previousDistance = distance;
                        break;
                    }
                }

                for (int distance = 1; distance < pointCount; ++distance) {
                    const int testIndex = (i + distance) % pointCount;

                    if (hasPoint[testIndex]) {
                        nextIndex = testIndex;
                        nextDistance = distance;
                        break;
                    }
                }

                if (previousIndex >= 0 && nextIndex >= 0) {
                    const double totalDistance =
                        static_cast<double>(previousDistance + nextDistance);
                    const double amount =
                        static_cast<double>(previousDistance) / totalDistance;

                    bestPoint[i] = QPointF(
                        bestPoint[previousIndex].x() +
                            ((bestPoint[nextIndex].x() -
                              bestPoint[previousIndex].x()) * amount),
                        bestPoint[previousIndex].y() +
                            ((bestPoint[nextIndex].y() -
                              bestPoint[previousIndex].y()) * amount)
                        );
                    hasPoint[i] = true;
                } else if (previousIndex >= 0) {
                    bestPoint[i] = bestPoint[previousIndex];
                    hasPoint[i] = true;
                } else if (nextIndex >= 0) {
                    bestPoint[i] = bestPoint[nextIndex];
                    hasPoint[i] = true;
                }
            }

            QVector<QPointF> region;
            region.reserve(pointCount);

            for (int i = 0; i < pointCount; ++i) {
                if (!hasPoint[i]) {
                    continue;
                }

                if (!region.isEmpty()) {
                    const QPointF difference = bestPoint[i] - region.last();
                    const double distanceSquared =
                        (difference.x() * difference.x()) +
                        (difference.y() * difference.y());

                    if (distanceSquared < 1.0) {
                        continue;
                    }
                }

                region.append(bestPoint[i]);
            }

            if (region.size() >= 2) {
                const QPointF difference = region.first() - region.last();
                const double distanceSquared =
                    (difference.x() * difference.x()) +
                    (difference.y() * difference.y());

                if (distanceSquared < 1.0) {
                    region.removeLast();
                }
            }

            if (region.size() >= 3) {
                m_outlineRegions.append(region);
            } else {
                appendRectangleFallback(bounds);
            }
        }
    }
}

void BrightAreaSelectionWidget::rebuildMaskFromEditableOutlines()
{
    if (m_image.isNull() || m_outlineRegions.isEmpty()) {
        return;
    }

    QImage temporaryMask(m_image.size(), QImage::Format_ARGB32);
    temporaryMask.fill(Qt::black);

    QPainter painter(&temporaryMask);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);

    for (const QVector<QPointF> &region : m_outlineRegions) {
        if (region.size() < 3) {
            continue;
        }

        QPolygonF polygon;

        for (const QPointF &point : region) {
            polygon << point;
        }

        painter.drawPolygon(polygon);
    }

    painter.end();
    m_mask = temporaryMask.convertToFormat(QImage::Format_Grayscale8);
    updateSelectionBoundaryPixmap();
}

BrightAreaSelectionWidget::OutlinePointRef
BrightAreaSelectionWidget::hitTestOutlinePoint(const QPoint &widgetPoint) const
{
    OutlinePointRef result;

    if (!m_outlineEditEnabled || m_outlineRegions.isEmpty()) {
        return result;
    }

    const int hitRadius = 10;
    int bestDistanceSquared = hitRadius * hitRadius + 1;

    for (int regionIndex = 0;
         regionIndex < m_outlineRegions.size();
         ++regionIndex) {
        const QVector<QPointF> &region = m_outlineRegions[regionIndex];

        for (int pointIndex = 0; pointIndex < region.size(); ++pointIndex) {
            const QPoint point = imagePointToWidgetPoint(region[pointIndex]);
            const int dx = point.x() - widgetPoint.x();
            const int dy = point.y() - widgetPoint.y();
            const int distanceSquared = (dx * dx) + (dy * dy);

            if (distanceSquared <= hitRadius * hitRadius &&
                distanceSquared < bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                result.region = regionIndex;
                result.point = pointIndex;
            }
        }
    }

    return result;
}

void BrightAreaSelectionWidget::clearEditableOutlines()
{
    m_outlineRegions.clear();
    m_activeOutlinePoint = OutlinePointRef();
    m_draggingOutlinePoint = false;
}

