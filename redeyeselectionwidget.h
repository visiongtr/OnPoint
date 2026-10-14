#ifndef REDEYESELECTIONWIDGET_H
#define REDEYESELECTIONWIDGET_H

#include "toolpreviewwidget.h"

#include <QPoint>
#include <QRect>

class QMouseEvent;
class QPainter;

class RedEyeSelectionWidget : public ToolPreviewWidget
{
    Q_OBJECT

public:
    explicit RedEyeSelectionWidget(QWidget *parent = nullptr);

    QRect selectionRect() const;
    void clearSelection();

    void setColorPickerActive(bool active);
    bool colorPickerActive() const;

signals:
    void selectionChanged();
    void colorSampleRequested(const QPoint &imagePoint);
    void colorPickerActiveChanged(bool active);

protected:
    void paintImageOverlay(QPainter &painter) override;

    void toolMousePressEvent(QMouseEvent *event,
                             const QPointF &imagePoint,
                             bool insideImage) override;

    void toolMouseMoveEvent(QMouseEvent *event,
                            const QPointF &imagePoint,
                            bool insideImage) override;

    void toolMouseReleaseEvent(QMouseEvent *event,
                               const QPointF &imagePoint,
                               bool insideImage) override;

private:
    QPoint boundedImagePixel(const QPointF &imagePoint) const;

    bool m_selecting = false;
    bool m_colorPickerActive = false;
    QPoint m_startImagePoint;
    QRect m_selectionImageRect;
};

#endif // REDEYESELECTIONWIDGET_H
