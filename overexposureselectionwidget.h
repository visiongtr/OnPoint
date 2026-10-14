#ifndef OVEREXPOSURESELECTIONWIDGET_H
#define OVEREXPOSURESELECTIONWIDGET_H

#include "overexposure_tools.h"

#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

class OverexposureSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverexposureSelectionWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void setOverlaySettings(int sampleDistance,
                            int sampleWidth,
                            OverexposureTools::RegionShape regionShape);

    QRect selectionRect() const;
    void clearSelection();

signals:
    void selectionChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class DragMode
    {
        None,
        NewSelection,
        Move,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    void updateScaledPixmap();

    QPoint widgetPointToImagePoint(const QPoint &point) const;
    QRect imageRectToWidgetRect(const QRect &imageRect) const;
    QRect effectiveSelectionRect(const QRect &rect) const;

    DragMode hitTest(const QPoint &widgetPoint) const;
    QRect adjustedSelectionForDrag(const QPoint &currentImagePoint) const;
    QRect clampSelectionToImage(const QRect &rect) const;

    void drawSelection(QPainter &painter);

    QImage m_image;
    QPixmap m_scaledPixmap;
    QRect m_pixmapRect;

    QRect m_selectionImageRect;

    bool m_dragging = false;
    DragMode m_dragMode = DragMode::None;
    QPoint m_dragStartImagePoint;
    QRect m_dragStartRect;

    int m_sampleDistance = 30;
    int m_sampleWidth = 20;
    OverexposureTools::RegionShape m_regionShape = OverexposureTools::RegionShape::Oval;
};

#endif // OVEREXPOSURESELECTIONWIDGET_H
