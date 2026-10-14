#ifndef BRIGHTAREASELECTIONWIDGET_H
#define BRIGHTAREASELECTIONWIDGET_H

#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QVector>
#include <QWidget>

class QTimer;

class BrightAreaSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BrightAreaSelectionWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    QImage image() const;

    void setMask(const QImage &mask);
    QImage mask() const;
    void clearMask();

    void setManualRefineEnabled(bool enabled);
    void setOutlineEditEnabled(bool enabled);
    void setColorSampleEnabled(bool enabled);

    void armAddOutlineNode();
    void armRemoveOutlineNode();

    void setBrushAddsToSelection(bool addsToSelection);
    void setBrushSize(int brushSize);
    void setBrushSoftness(int brushSoftness);
    void setBrushStrength(int brushStrength);
    void setMaskVisible(bool visible);

signals:
    void autoSelectRequested(const QPoint &imagePoint);
    void colorSampleRequested(const QPoint &imagePoint);
    void maskEdited(const QImage &mask);
    void outlineNodeActionCompleted();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class OutlineAction
    {
        Move,
        Add,
        Remove
    };

    struct OutlinePointRef
    {
        int region = -1;
        int point = -1;

        bool isValid() const
        {
            return region >= 0 && point >= 0;
        }
    };

    struct OutlineSegmentRef
    {
        int region = -1;
        int segment = -1;

        bool isValid() const
        {
            return region >= 0 && segment >= 0;
        }
    };

    void updateScaledPixmap();
    void updateSelectionBoundaryPixmap();

    QPoint widgetPointToImagePoint(const QPoint &point) const;
    QPoint imagePointToWidgetPoint(const QPointF &point) const;

    void buildEditableOutlinesFromMask();
    void rebuildMaskFromEditableOutlines();
    OutlinePointRef hitTestOutlinePoint(const QPoint &widgetPoint) const;
    OutlineSegmentRef nearestOutlineSegment(const QPointF &imagePoint) const;
    bool addOutlinePointAt(const QPoint &widgetPoint);
    bool removeOutlinePointAt(const QPoint &widgetPoint);
    void clearEditableOutlines();

    void paintBrushAt(const QPoint &widgetPoint);
    void updateInteractionCursor();

    QImage m_image;
    QImage m_mask;

    QPixmap m_scaledPixmap;
    QRect m_pixmapRect;

    // Cached at display size so the animated selection outline does not
    // rescan a full-resolution image on every animation frame.
    QPixmap m_selectionBoundaryPixmap;
    QPixmap m_marchingAntPattern;
    QTimer *m_marchingAntsTimer = nullptr;
    int m_marchingAntsPhase = 0;

    bool m_manualRefineEnabled = false;
    bool m_outlineEditEnabled = false;
    bool m_colorSampleEnabled = false;
    bool m_brushAddsToSelection = true;
    bool m_maskVisible = true;

    int m_brushSize = 36;
    int m_brushSoftness = 65;
    int m_brushStrength = 80;

    // Each disconnected selection island keeps its own editable outline.
    // The composite grayscale mask remains the authoritative selection used
    // by the recovery engine.
    QVector<QVector<QPointF>> m_outlineRegions;
    OutlineAction m_outlineAction = OutlineAction::Move;
    OutlinePointRef m_activeOutlinePoint;
    bool m_draggingOutlinePoint = false;

    bool m_painting = false;
};

#endif // BRIGHTAREASELECTIONWIDGET_H
