#ifndef TOOLPREVIEWWIDGET_H
#define TOOLPREVIEWWIDGET_H

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QWidget>

class QFrame;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QScrollBar;
class QToolButton;
class QWheelEvent;

class ToolPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ToolPreviewWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image, bool preserveView = true);
    const QImage &image() const;

    void setOriginalImage(const QImage &image);
    const QImage &originalImage() const;

    void setShowingOriginal(bool showingOriginal);
    bool isShowingOriginal() const;

    // Creates the reusable external zoom bar:
    //   -  Fit  1:1  +  percentage
    QWidget *createZoomControlBar(QWidget *parent = nullptr);

    void setNavigationControlsVisible(bool visible);
    bool navigationControlsVisible() const;

    void setPanEnabled(bool enabled);
    bool panEnabled() const;

    // False = active tool uses left drag.
    // True  = left drag grabs and pans the image.
    void setLeftButtonPans(bool enabled);
    bool leftButtonPans() const;

    void setToolModeIcon(const QIcon &icon);
    void setToolModeToolTip(const QString &toolTip);

    void setToolCursor(Qt::CursorShape cursorShape);
    Qt::CursorShape toolCursor() const;

    void setSmoothScaling(bool enabled);
    bool smoothScaling() const;

    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const;

    void setZoomRange(double minimumZoom, double maximumZoom);
    double minimumZoom() const;
    double maximumZoom() const;

    void setZoomStep(double zoomStep);
    double zoomStep() const;

    double zoomFactor() const;
    int zoomPercent() const;
    bool isFitToWindow() const;

    QPointF viewCenterImagePoint() const;
    QRectF viewportRect() const;
    QRectF imageRectInWidget() const;
    QTransform imageToWidgetTransform() const;

    bool widgetPointToImagePoint(const QPointF &widgetPoint,
                                 QPointF &imagePoint,
                                 bool clampToImage = false) const;

    QPointF imagePointToWidgetPoint(const QPointF &imagePoint) const;
    bool isWidgetPointOverImage(const QPointF &widgetPoint) const;

public slots:
    void fitToWindow();
    void actualSize();
    void zoomIn();
    void zoomOut();
    void setZoomFactor(double zoomFactor);
    void resetView();

signals:
    void zoomChanged(double zoomFactor, int zoomPercent);
    void fitModeChanged(bool fitToWindow);
    void viewChanged();
    void interactionModeChanged(bool panMode);

    void imageMousePressed(const QPointF &imagePoint,
                           Qt::MouseButton button,
                           Qt::KeyboardModifiers modifiers,
                           bool insideImage);

    void imageMouseMoved(const QPointF &imagePoint,
                         Qt::MouseButtons buttons,
                         Qt::KeyboardModifiers modifiers,
                         bool insideImage);

    void imageMouseReleased(const QPointF &imagePoint,
                            Qt::MouseButton button,
                            Qt::KeyboardModifiers modifiers,
                            bool insideImage);

    void imageMouseDoubleClicked(const QPointF &imagePoint,
                                 Qt::MouseButton button,
                                 Qt::KeyboardModifiers modifiers,
                                 bool insideImage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    // Painter is transformed into image coordinates for this hook.
    virtual void paintImageOverlay(QPainter &painter);

    // Painter remains in normal widget coordinates for this hook.
    virtual void paintWidgetOverlay(QPainter &painter);

    virtual void toolMousePressEvent(QMouseEvent *event,
                                     const QPointF &imagePoint,
                                     bool insideImage);

    virtual void toolMouseMoveEvent(QMouseEvent *event,
                                    const QPointF &imagePoint,
                                    bool insideImage);

    virtual void toolMouseReleaseEvent(QMouseEvent *event,
                                       const QPointF &imagePoint,
                                       bool insideImage);

    virtual void toolMouseDoubleClickEvent(QMouseEvent *event,
                                           const QPointF &imagePoint,
                                           bool insideImage);

private:
    const QImage &displayedImage() const;

    int scrollBarExtent() const;
    double fitScaleForCurrentImage() const;

    void setZoomFactorAt(double zoomFactor,
                         const QPointF &anchorWidgetPoint);

    void clampViewCenter();

    void beginPan(const QPointF &widgetPoint);
    void updatePan(const QPointF &widgetPoint);
    void endPan();
    bool shouldBeginPan(const QMouseEvent *event) const;

    void updateCursor();
    void updateScrollBars();
    void handleScrollBarChanged();
    void updateModeButtons();
    void positionModeBar();
    void emitViewSignals(bool fitModeMayHaveChanged = false);

    QImage m_image;
    QImage m_originalImage;

    QColor m_backgroundColor = QColor(0, 0, 0);

    QPointF m_viewCenterImagePoint;

    double m_zoomFactor = 1.0;
    double m_minimumZoom = 0.02;
    double m_maximumZoom = 32.0;
    double m_zoomStep = 1.25;

    bool m_fitToWindow = true;
    bool m_showingOriginal = false;
    bool m_smoothScaling = true;

    bool m_panEnabled = true;
    bool m_leftButtonPans = false;
    bool m_spaceHeld = false;
    bool m_panning = false;
    bool m_updatingScrollBars = false;

    QPointF m_lastPanWidgetPoint;
    Qt::CursorShape m_toolCursor = Qt::CrossCursor;

    QScrollBar *m_horizontalScrollBar = nullptr;
    QScrollBar *m_verticalScrollBar = nullptr;

    QFrame *m_modeBar = nullptr;
    QToolButton *m_toolModeButton = nullptr;
    QToolButton *m_panModeButton = nullptr;
};

#endif // TOOLPREVIEWWIDGET_H
