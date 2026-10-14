#ifndef PRINTPREVIEWWIDGET_H
#define PRINTPREVIEWWIDGET_H

#include "printservice.h"

#include <QImage>
#include <QWidget>

class QPrinter;

class PrintPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PrintPreviewWidget(QWidget *parent = nullptr);

    void setPrinter(QPrinter *printer);

    void setImage(
        const QImage &image,
        int sourceDotsPerMeterX,
        int sourceDotsPerMeterY
        );

    void setLayoutSettings(
        const OnPointPrint::LayoutSettings &settings
        );

    OnPointPrint::LayoutSettings layoutSettings() const;

signals:
    void layoutSettingsChanged(
        const OnPointPrint::LayoutSettings &settings
        );

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct PreviewGeometry
    {
        QRectF paperRectWidget;
        QRectF contentRectWidget;
        QRectF imageRectWidget;
        double pointsToWidget = 1.0;
        QPointF paperTopLeftWidget;
        OnPointPrint::LayoutResult layout;
    };

    PreviewGeometry previewGeometry() const;
    void updateHoverCursor(const QPointF &position);
    void applyDraggedImageTopLeft(const QPointF &topLeftWidget);

    QPrinter *m_printer = nullptr;
    QImage m_image;
    int m_sourceDotsPerMeterX = 0;
    int m_sourceDotsPerMeterY = 0;
    OnPointPrint::LayoutSettings m_settings;

    bool m_draggingImage = false;
    QPointF m_dragOffset;
};

#endif // PRINTPREVIEWWIDGET_H
