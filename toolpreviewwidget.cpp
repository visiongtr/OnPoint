#include "toolpreviewwidget.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

QPointF mouseEventPosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}

QPointF wheelEventPosition(const QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->posF();
#endif
}

bool fuzzyEqual(double first, double second)
{
    const double scale =
        std::max({1.0, std::abs(first), std::abs(second)});

    return std::abs(first - second) <=
           (scale * 0.000001);
}

QIcon makeDefaultToolIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(225, 225, 225), 1.7);
    pen.setCosmetic(true);

    painter.setPen(pen);
    painter.drawEllipse(QPointF(12.0, 12.0), 5.0, 5.0);
    painter.drawLine(QPointF(12.0, 2.5), QPointF(12.0, 7.0));
    painter.drawLine(QPointF(12.0, 17.0), QPointF(12.0, 21.5));
    painter.drawLine(QPointF(2.5, 12.0), QPointF(7.0, 12.0));
    painter.drawLine(QPointF(17.0, 12.0), QPointF(21.5, 12.0));

    return QIcon(pixmap);
}

QIcon makeHandIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath hand;
    hand.moveTo(8.0, 20.0);
    hand.cubicTo(6.4, 17.0, 5.0, 14.5, 4.3, 12.7);
    hand.cubicTo(3.8, 11.3, 4.5, 10.2, 5.6, 10.0);
    hand.cubicTo(6.5, 9.8, 7.2, 10.4, 7.8, 11.5);
    hand.lineTo(7.8, 6.2);
    hand.cubicTo(7.8, 5.2, 8.5, 4.5, 9.4, 4.5);
    hand.cubicTo(10.2, 4.5, 10.8, 5.1, 10.8, 6.0);
    hand.lineTo(10.8, 4.8);
    hand.cubicTo(10.8, 3.8, 11.5, 3.1, 12.4, 3.1);
    hand.cubicTo(13.3, 3.1, 14.0, 3.8, 14.0, 4.8);
    hand.lineTo(14.0, 5.1);
    hand.cubicTo(14.0, 4.1, 14.7, 3.5, 15.6, 3.5);
    hand.cubicTo(16.5, 3.5, 17.2, 4.2, 17.2, 5.1);
    hand.lineTo(17.2, 6.2);
    hand.cubicTo(17.2, 5.3, 17.9, 4.7, 18.7, 4.7);
    hand.cubicTo(19.6, 4.7, 20.2, 5.4, 20.2, 6.3);
    hand.lineTo(20.2, 12.8);
    hand.cubicTo(20.2, 16.7, 18.0, 20.4, 14.1, 21.1);
    hand.cubicTo(11.7, 21.5, 9.4, 21.0, 8.0, 20.0);
    hand.closeSubpath();

    painter.setPen(QPen(QColor(225, 225, 225), 1.4));
    painter.setBrush(QColor(150, 150, 150, 85));
    painter.drawPath(hand);

    return QIcon(pixmap);
}

} // namespace

ToolPreviewWidget::ToolPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(480, 260);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_horizontalScrollBar =
        new QScrollBar(Qt::Horizontal, this);

    m_verticalScrollBar =
        new QScrollBar(Qt::Vertical, this);

    m_horizontalScrollBar->setObjectName(
        "toolPreviewHorizontalScrollBar"
        );

    m_verticalScrollBar->setObjectName(
        "toolPreviewVerticalScrollBar"
        );

    m_horizontalScrollBar->hide();
    m_verticalScrollBar->hide();

    connect(m_horizontalScrollBar,
            &QScrollBar::valueChanged,
            this,
            [this](int) {
                handleScrollBarChanged();
            });

    connect(m_verticalScrollBar,
            &QScrollBar::valueChanged,
            this,
            [this](int) {
                handleScrollBarChanged();
            });

    m_modeBar = new QFrame(this);
    m_modeBar->setObjectName(
        "toolPreviewModeBar"
        );

    m_modeBar->setFrameShape(
        QFrame::StyledPanel
        );

    m_modeBar->setFrameShadow(
        QFrame::Plain
        );

    QHBoxLayout *modeLayout =
        new QHBoxLayout(m_modeBar);

    modeLayout->setContentsMargins(4, 3, 4, 3);
    modeLayout->setSpacing(3);

    m_toolModeButton =
        new QToolButton(m_modeBar);

    m_panModeButton =
        new QToolButton(m_modeBar);

    m_toolModeButton->setObjectName(
        "toolPreviewToolModeButton"
        );

    m_panModeButton->setObjectName(
        "toolPreviewPanModeButton"
        );

    m_toolModeButton->setCheckable(true);
    m_panModeButton->setCheckable(true);

    m_toolModeButton->setAutoRaise(true);
    m_panModeButton->setAutoRaise(true);

    m_toolModeButton->setFixedSize(34, 28);
    m_panModeButton->setFixedSize(34, 28);

    m_toolModeButton->setIcon(
        makeDefaultToolIcon()
        );

    m_panModeButton->setIcon(
        makeHandIcon()
        );

    m_toolModeButton->setIconSize(
        QSize(21, 21)
        );

    m_panModeButton->setIconSize(
        QSize(21, 21)
        );

    m_toolModeButton->setToolTip(
        "Active tool"
        );

    m_panModeButton->setToolTip(
        "Grab and drag the image"
        );

    QButtonGroup *modeGroup =
        new QButtonGroup(m_modeBar);

    modeGroup->setExclusive(true);
    modeGroup->addButton(m_toolModeButton);
    modeGroup->addButton(m_panModeButton);

    modeLayout->addWidget(m_toolModeButton);
    modeLayout->addWidget(m_panModeButton);

    connect(m_toolModeButton,
            &QToolButton::clicked,
            this,
            [this]() {
                setLeftButtonPans(false);
            });

    connect(m_panModeButton,
            &QToolButton::clicked,
            this,
            [this]() {
                setLeftButtonPans(true);
            });

    updateModeButtons();
    updateCursor();
    updateScrollBars();
    positionModeBar();
}

void ToolPreviewWidget::setImage(
    const QImage &image,
    bool preserveView
    )
{
    const bool hadImage =
        !m_image.isNull();

    const QSize previousSize =
        m_image.size();

    m_image = image;

    if (m_image.isNull()) {
        m_viewCenterImagePoint = QPointF();
        m_zoomFactor = 1.0;
        m_fitToWindow = true;
        m_panning = false;

        updateScrollBars();
        updateModeButtons();
        updateCursor();
        update();

        emitViewSignals(true);
        return;
    }

    const bool imageGeometryChanged =
        !hadImage ||
        previousSize != m_image.size();

    if (!preserveView ||
        imageGeometryChanged) {
        m_fitToWindow = true;
        m_zoomFactor =
            fitScaleForCurrentImage();

        m_viewCenterImagePoint = QPointF(
            m_image.width() / 2.0,
            m_image.height() / 2.0
            );
    } else if (m_fitToWindow) {
        m_zoomFactor =
            fitScaleForCurrentImage();
    }

    m_zoomFactor = std::clamp(
        m_zoomFactor,
        m_minimumZoom,
        m_maximumZoom
        );

    updateScrollBars();
    clampViewCenter();
    updateScrollBars();

    updateModeButtons();
    updateCursor();
    positionModeBar();
    update();

    emitViewSignals(
        imageGeometryChanged
        );
}

const QImage &ToolPreviewWidget::image() const
{
    return m_image;
}

void ToolPreviewWidget::setOriginalImage(
    const QImage &image
    )
{
    m_originalImage = image;
    update();
}

const QImage &
ToolPreviewWidget::originalImage() const
{
    return m_originalImage;
}

void ToolPreviewWidget::setShowingOriginal(
    bool showingOriginal
    )
{
    if (m_showingOriginal ==
        showingOriginal) {
        return;
    }

    m_showingOriginal =
        showingOriginal;

    update();
}

bool ToolPreviewWidget::isShowingOriginal() const
{
    return m_showingOriginal;
}

QWidget *ToolPreviewWidget::createZoomControlBar(
    QWidget *parent
    )
{
    QFrame *bar = new QFrame(parent);
    bar->setObjectName(
        "toolPreviewZoomBar"
        );

    bar->setFrameShape(
        QFrame::StyledPanel
        );

    bar->setFrameShadow(
        QFrame::Plain
        );

    QHBoxLayout *layout =
        new QHBoxLayout(bar);

    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(3);

    auto createButton =
        [bar, layout](const QString &text,
                      const QString &objectName,
                      const QString &toolTip,
                      int width) {
            QToolButton *button =
                new QToolButton(bar);

            button->setObjectName(
                objectName
                );

            button->setText(text);
            button->setToolTip(toolTip);
            button->setAutoRaise(true);
            button->setFixedSize(width, 26);

            layout->addWidget(button);

            return button;
        };

    QToolButton *zoomOutButton =
        createButton(
            QString::fromUtf8("−"),
            "toolPreviewZoomOutButton",
            "Zoom out",
            30
            );

    QToolButton *fitButton =
        createButton(
            "Fit",
            "toolPreviewFitButton",
            "Fit image to preview",
            46
            );

    QToolButton *actualSizeButton =
        createButton(
            "1:1",
            "toolPreviewActualSizeButton",
            "Show image at actual pixel size",
            46
            );

    QToolButton *zoomInButton =
        createButton(
            "+",
            "toolPreviewZoomInButton",
            "Zoom in",
            30
            );

    QLabel *zoomLabel =
        new QLabel(bar);

    zoomLabel->setObjectName(
        "toolPreviewZoomLabel"
        );

    zoomLabel->setAlignment(
        Qt::AlignCenter
        );

    zoomLabel->setMinimumWidth(50);
    zoomLabel->setText(
        QString("%1%")
            .arg(zoomPercent())
        );

    layout->addWidget(zoomLabel);

    connect(zoomOutButton,
            &QToolButton::clicked,
            this,
            &ToolPreviewWidget::zoomOut);

    connect(fitButton,
            &QToolButton::clicked,
            this,
            &ToolPreviewWidget::fitToWindow);

    connect(actualSizeButton,
            &QToolButton::clicked,
            this,
            &ToolPreviewWidget::actualSize);

    connect(zoomInButton,
            &QToolButton::clicked,
            this,
            &ToolPreviewWidget::zoomIn);

    connect(this,
            &ToolPreviewWidget::zoomChanged,
            zoomLabel,
            [zoomLabel](double,
                        int zoomPercentValue) {
                zoomLabel->setText(
                    QString("%1%")
                        .arg(zoomPercentValue)
                    );
            });

    connect(this,
            &ToolPreviewWidget::fitModeChanged,
            fitButton,
            [fitButton](bool fitMode) {
                fitButton->setDown(
                    fitMode
                    );
            });

    connect(this,
            &ToolPreviewWidget::zoomChanged,
            actualSizeButton,
            [this, actualSizeButton](double,
                                     int) {
                actualSizeButton->setDown(
                    !m_fitToWindow &&
                    fuzzyEqual(
                        m_zoomFactor,
                        1.0
                        )
                    );
            });

    fitButton->setDown(m_fitToWindow);

    actualSizeButton->setDown(
        !m_fitToWindow &&
        fuzzyEqual(
            m_zoomFactor,
            1.0
            )
        );

    return bar;
}

void ToolPreviewWidget::setNavigationControlsVisible(
    bool visible
    )
{
    if (m_modeBar) {
        m_modeBar->setVisible(
            visible
            );
    }
}

bool ToolPreviewWidget::navigationControlsVisible() const
{
    return m_modeBar &&
           m_modeBar->isVisible();
}

void ToolPreviewWidget::setPanEnabled(
    bool enabled
    )
{
    if (m_panEnabled == enabled) {
        return;
    }

    m_panEnabled = enabled;

    if (!m_panEnabled) {
        setLeftButtonPans(false);
        endPan();
    }

    updateModeButtons();
    updateCursor();
}

bool ToolPreviewWidget::panEnabled() const
{
    return m_panEnabled;
}

void ToolPreviewWidget::setLeftButtonPans(
    bool enabled
    )
{
    if (!m_panEnabled) {
        enabled = false;
    }

    if (m_leftButtonPans == enabled) {
        updateModeButtons();
        return;
    }

    m_leftButtonPans = enabled;

    updateModeButtons();
    updateCursor();

    emit interactionModeChanged(
        m_leftButtonPans
        );
}

bool ToolPreviewWidget::leftButtonPans() const
{
    return m_leftButtonPans;
}

void ToolPreviewWidget::setToolModeIcon(
    const QIcon &icon
    )
{
    if (m_toolModeButton) {
        m_toolModeButton->setIcon(
            icon
            );
    }
}

void ToolPreviewWidget::setToolModeToolTip(
    const QString &toolTip
    )
{
    if (m_toolModeButton) {
        m_toolModeButton->setToolTip(
            toolTip
            );
    }
}

void ToolPreviewWidget::setToolCursor(
    Qt::CursorShape cursorShape
    )
{
    m_toolCursor = cursorShape;
    updateCursor();
}

Qt::CursorShape
ToolPreviewWidget::toolCursor() const
{
    return m_toolCursor;
}

void ToolPreviewWidget::setSmoothScaling(
    bool enabled
    )
{
    if (m_smoothScaling == enabled) {
        return;
    }

    m_smoothScaling = enabled;
    update();
}

bool ToolPreviewWidget::smoothScaling() const
{
    return m_smoothScaling;
}

void ToolPreviewWidget::setBackgroundColor(
    const QColor &color
    )
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    update();
}

QColor ToolPreviewWidget::backgroundColor() const
{
    return m_backgroundColor;
}

void ToolPreviewWidget::setZoomRange(
    double minimumZoom,
    double maximumZoom
    )
{
    minimumZoom =
        std::max(0.001, minimumZoom);

    maximumZoom =
        std::max(
            minimumZoom,
            maximumZoom
            );

    m_minimumZoom = minimumZoom;
    m_maximumZoom = maximumZoom;

    setZoomFactor(m_zoomFactor);
}

double ToolPreviewWidget::minimumZoom() const
{
    return m_minimumZoom;
}

double ToolPreviewWidget::maximumZoom() const
{
    return m_maximumZoom;
}

void ToolPreviewWidget::setZoomStep(
    double zoomStep
    )
{
    m_zoomStep =
        std::max(1.01, zoomStep);
}

double ToolPreviewWidget::zoomStep() const
{
    return m_zoomStep;
}

double ToolPreviewWidget::zoomFactor() const
{
    return m_zoomFactor;
}

int ToolPreviewWidget::zoomPercent() const
{
    return static_cast<int>(
        std::round(
            m_zoomFactor * 100.0
            )
        );
}

bool ToolPreviewWidget::isFitToWindow() const
{
    return m_fitToWindow;
}

QPointF
ToolPreviewWidget::viewCenterImagePoint() const
{
    return m_viewCenterImagePoint;
}

QRectF ToolPreviewWidget::viewportRect() const
{
    const int verticalInset =
        m_verticalScrollBar &&
        m_verticalScrollBar->isVisible()
            ? m_verticalScrollBar->width()
            : 0;

    const int horizontalInset =
        m_horizontalScrollBar &&
        m_horizontalScrollBar->isVisible()
            ? m_horizontalScrollBar->height()
            : 0;

    return QRectF(
        0.0,
        0.0,
        std::max(
            0,
            width() - verticalInset
            ),
        std::max(
            0,
            height() - horizontalInset
            )
        );
}

QRectF ToolPreviewWidget::imageRectInWidget() const
{
    if (m_image.isNull() ||
        m_zoomFactor <= 0.0) {
        return QRectF();
    }

    const QPointF viewportCenter =
        viewportRect().center();

    const QPointF topLeft =
        viewportCenter -
        QPointF(
            m_viewCenterImagePoint.x() *
                m_zoomFactor,
            m_viewCenterImagePoint.y() *
                m_zoomFactor
            );

    return QRectF(
        topLeft,
        QSizeF(
            m_image.width() *
                m_zoomFactor,
            m_image.height() *
                m_zoomFactor
            )
        );
}

QTransform
ToolPreviewWidget::imageToWidgetTransform() const
{
    const QRectF imageRect =
        imageRectInWidget();

    QTransform transform;

    transform.translate(
        imageRect.left(),
        imageRect.top()
        );

    transform.scale(
        m_zoomFactor,
        m_zoomFactor
        );

    return transform;
}

bool ToolPreviewWidget::widgetPointToImagePoint(
    const QPointF &widgetPoint,
    QPointF &imagePoint,
    bool clampToImage
    ) const
{
    if (m_image.isNull() ||
        m_zoomFactor <= 0.0) {
        imagePoint = QPointF();
        return false;
    }

    const QRectF imageRect =
        imageRectInWidget();

    imagePoint = QPointF(
        (widgetPoint.x() -
         imageRect.left()) /
            m_zoomFactor,

        (widgetPoint.y() -
         imageRect.top()) /
            m_zoomFactor
        );

    const bool insideImage =
        viewportRect().contains(widgetPoint) &&
        imagePoint.x() >= 0.0 &&
        imagePoint.y() >= 0.0 &&
        imagePoint.x() < m_image.width() &&
        imagePoint.y() < m_image.height();

    if (clampToImage) {
        imagePoint.setX(
            std::clamp(
                imagePoint.x(),
                0.0,
                std::max(
                    0.0,
                    m_image.width() - 1.0
                    )
                )
            );

        imagePoint.setY(
            std::clamp(
                imagePoint.y(),
                0.0,
                std::max(
                    0.0,
                    m_image.height() - 1.0
                    )
                )
            );
    }

    return insideImage;
}

QPointF ToolPreviewWidget::imagePointToWidgetPoint(
    const QPointF &imagePoint
    ) const
{
    return imageToWidgetTransform().map(
        imagePoint
        );
}

bool ToolPreviewWidget::isWidgetPointOverImage(
    const QPointF &widgetPoint
    ) const
{
    return viewportRect().contains(
               widgetPoint
               ) &&
           imageRectInWidget().contains(
               widgetPoint
               );
}

void ToolPreviewWidget::fitToWindow()
{
    if (m_image.isNull()) {
        return;
    }

    const bool fitModeChanged =
        !m_fitToWindow;

    m_fitToWindow = true;
    m_zoomFactor =
        fitScaleForCurrentImage();

    m_viewCenterImagePoint = QPointF(
        m_image.width() / 2.0,
        m_image.height() / 2.0
        );

    updateScrollBars();
    clampViewCenter();
    positionModeBar();
    update();

    emitViewSignals(
        fitModeChanged
        );
}

void ToolPreviewWidget::actualSize()
{
    if (m_image.isNull()) {
        return;
    }

    setZoomFactorAt(
        1.0,
        viewportRect().center()
        );
}

void ToolPreviewWidget::zoomIn()
{
    if (m_image.isNull()) {
        return;
    }

    setZoomFactorAt(
        m_zoomFactor * m_zoomStep,
        viewportRect().center()
        );
}

void ToolPreviewWidget::zoomOut()
{
    if (m_image.isNull()) {
        return;
    }

    setZoomFactorAt(
        m_zoomFactor / m_zoomStep,
        viewportRect().center()
        );
}

void ToolPreviewWidget::setZoomFactor(
    double zoomFactor
    )
{
    setZoomFactorAt(
        zoomFactor,
        viewportRect().center()
        );
}

void ToolPreviewWidget::resetView()
{
    fitToWindow();
}

void ToolPreviewWidget::paintEvent(
    QPaintEvent *
    )
{
    QPainter painter(this);
    painter.fillRect(
        rect(),
        m_backgroundColor
        );

    const QRectF activeViewport =
        viewportRect();

    painter.save();
    painter.setClipRect(
        activeViewport
        );

    const QImage &previewImage =
        displayedImage();

    if (m_image.isNull() ||
        previewImage.isNull()) {
        painter.setPen(
            palette().color(
                QPalette::Disabled,
                QPalette::Text
                )
            );

        painter.drawText(
            activeViewport,
            Qt::AlignCenter,
            "No preview image"
            );

        painter.restore();
        return;
    }

    painter.setRenderHint(
        QPainter::SmoothPixmapTransform,
        m_smoothScaling
        );

    painter.drawImage(
        imageRectInWidget(),
        previewImage
        );

    painter.save();

    painter.setTransform(
        imageToWidgetTransform(),
        true
        );

    painter.setClipRect(
        QRectF(
            0.0,
            0.0,
            static_cast<double>(
                m_image.width()
                ),
            static_cast<double>(
                m_image.height()
                )
            )
        );

    paintImageOverlay(painter);
    painter.restore();

    painter.save();
    painter.setTransform(
        QTransform()
        );
    paintWidgetOverlay(painter);
    painter.restore();

    painter.restore();
}

void ToolPreviewWidget::resizeEvent(
    QResizeEvent *event
    )
{
    QWidget::resizeEvent(event);

    if (!m_image.isNull() &&
        m_fitToWindow) {
        m_zoomFactor =
            fitScaleForCurrentImage();
    }

    updateScrollBars();
    clampViewCenter();
    updateScrollBars();

    positionModeBar();
    update();

    emitViewSignals(false);
}

void ToolPreviewWidget::wheelEvent(
    QWheelEvent *event
    )
{
    if (m_image.isNull() ||
        event->angleDelta().y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    const double wheelSteps =
        static_cast<double>(
            event->angleDelta().y()
            ) / 120.0;

    const double factor =
        std::pow(
            1.15,
            wheelSteps
            );

    setZoomFactorAt(
        m_zoomFactor * factor,
        wheelEventPosition(event)
        );

    event->accept();
}

void ToolPreviewWidget::mousePressEvent(
    QMouseEvent *event
    )
{
    setFocus(Qt::MouseFocusReason);

    const QPointF widgetPoint =
        mouseEventPosition(event);

    if (shouldBeginPan(event)) {
        beginPan(widgetPoint);
        event->accept();
        return;
    }

    QPointF imagePoint;

    const bool insideImage =
        widgetPointToImagePoint(
            widgetPoint,
            imagePoint,
            false
            );

    emit imageMousePressed(
        imagePoint,
        event->button(),
        event->modifiers(),
        insideImage
        );

    toolMousePressEvent(
        event,
        imagePoint,
        insideImage
        );

    if (!event->isAccepted()) {
        QWidget::mousePressEvent(event);
    }
}

void ToolPreviewWidget::mouseMoveEvent(
    QMouseEvent *event
    )
{
    const QPointF widgetPoint =
        mouseEventPosition(event);

    if (m_panning) {
        updatePan(widgetPoint);
        event->accept();
        return;
    }

    QPointF imagePoint;

    const bool insideImage =
        widgetPointToImagePoint(
            widgetPoint,
            imagePoint,
            false
            );

    emit imageMouseMoved(
        imagePoint,
        event->buttons(),
        event->modifiers(),
        insideImage
        );

    toolMouseMoveEvent(
        event,
        imagePoint,
        insideImage
        );

    if (!event->isAccepted()) {
        QWidget::mouseMoveEvent(event);
    }
}

void ToolPreviewWidget::mouseReleaseEvent(
    QMouseEvent *event
    )
{
    const QPointF widgetPoint =
        mouseEventPosition(event);

    if (m_panning) {
        endPan();
        event->accept();
        return;
    }

    QPointF imagePoint;

    const bool insideImage =
        widgetPointToImagePoint(
            widgetPoint,
            imagePoint,
            false
            );

    emit imageMouseReleased(
        imagePoint,
        event->button(),
        event->modifiers(),
        insideImage
        );

    toolMouseReleaseEvent(
        event,
        imagePoint,
        insideImage
        );

    if (!event->isAccepted()) {
        QWidget::mouseReleaseEvent(event);
    }
}

void ToolPreviewWidget::mouseDoubleClickEvent(
    QMouseEvent *event
    )
{
    const QPointF widgetPoint =
        mouseEventPosition(event);

    QPointF imagePoint;

    const bool insideImage =
        widgetPointToImagePoint(
            widgetPoint,
            imagePoint,
            false
            );

    emit imageMouseDoubleClicked(
        imagePoint,
        event->button(),
        event->modifiers(),
        insideImage
        );

    toolMouseDoubleClickEvent(
        event,
        imagePoint,
        insideImage
        );

    if (!event->isAccepted()) {
        QWidget::mouseDoubleClickEvent(
            event
            );
    }
}

void ToolPreviewWidget::keyPressEvent(
    QKeyEvent *event
    )
{
    if (event->key() == Qt::Key_Space &&
        !event->isAutoRepeat()) {
        m_spaceHeld = true;
        updateCursor();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Plus ||
        event->key() == Qt::Key_Equal) {
        zoomIn();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Minus) {
        zoomOut();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_0) {
        fitToWindow();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_1) {
        actualSize();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_H &&
        !event->isAutoRepeat()) {
        setLeftButtonPans(
            !m_leftButtonPans
            );

        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ToolPreviewWidget::keyReleaseEvent(
    QKeyEvent *event
    )
{
    if (event->key() == Qt::Key_Space &&
        !event->isAutoRepeat()) {
        m_spaceHeld = false;

        if (m_panning) {
            endPan();
        }

        updateCursor();
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void ToolPreviewWidget::paintImageOverlay(
    QPainter &
    )
{
}

void ToolPreviewWidget::paintWidgetOverlay(
    QPainter &
    )
{
}

void ToolPreviewWidget::toolMousePressEvent(
    QMouseEvent *,
    const QPointF &,
    bool
    )
{
}

void ToolPreviewWidget::toolMouseMoveEvent(
    QMouseEvent *,
    const QPointF &,
    bool
    )
{
}

void ToolPreviewWidget::toolMouseReleaseEvent(
    QMouseEvent *,
    const QPointF &,
    bool
    )
{
}

void ToolPreviewWidget::toolMouseDoubleClickEvent(
    QMouseEvent *,
    const QPointF &,
    bool
    )
{
}

const QImage &
ToolPreviewWidget::displayedImage() const
{
    if (m_showingOriginal &&
        !m_originalImage.isNull()) {
        return m_originalImage;
    }

    return m_image;
}

int ToolPreviewWidget::scrollBarExtent() const
{
    return style()->pixelMetric(
        QStyle::PM_ScrollBarExtent,
        nullptr,
        this
        );
}

double
ToolPreviewWidget::fitScaleForCurrentImage() const
{
    if (m_image.isNull() ||
        width() <= 0 ||
        height() <= 0) {
        return 1.0;
    }

    const double widthScale =
        static_cast<double>(width()) /
        static_cast<double>(
            m_image.width()
            );

    const double heightScale =
        static_cast<double>(height()) /
        static_cast<double>(
            m_image.height()
            );

    return std::clamp(
        std::min(
            widthScale,
            heightScale
            ),
        m_minimumZoom,
        m_maximumZoom
        );
}

void ToolPreviewWidget::setZoomFactorAt(
    double zoomFactor,
    const QPointF &anchorWidgetPoint
    )
{
    if (m_image.isNull()) {
        return;
    }

    QPointF anchorImagePoint;

    widgetPointToImagePoint(
        anchorWidgetPoint,
        anchorImagePoint,
        false
        );

    const double boundedZoom =
        std::clamp(
            zoomFactor,
            m_minimumZoom,
            m_maximumZoom
            );

    const bool wasFitToWindow =
        m_fitToWindow;

    const bool zoomActuallyChanged =
        !fuzzyEqual(
            m_zoomFactor,
            boundedZoom
            );

    m_fitToWindow = false;
    m_zoomFactor = boundedZoom;

    updateScrollBars();

    const QPointF viewportCenter =
        viewportRect().center();

    m_viewCenterImagePoint =
        anchorImagePoint -
        QPointF(
            (anchorWidgetPoint.x() -
             viewportCenter.x()) /
                m_zoomFactor,

            (anchorWidgetPoint.y() -
             viewportCenter.y()) /
                m_zoomFactor
            );

    clampViewCenter();
    updateScrollBars();
    positionModeBar();
    update();

    if (zoomActuallyChanged ||
        wasFitToWindow) {
        emitViewSignals(
            wasFitToWindow
            );
    }
}

void ToolPreviewWidget::clampViewCenter()
{
    if (m_image.isNull() ||
        m_zoomFactor <= 0.0) {
        return;
    }

    const QRectF activeViewport =
        viewportRect();

    const double halfViewportWidthInImage =
        activeViewport.width() /
        (2.0 * m_zoomFactor);

    const double halfViewportHeightInImage =
        activeViewport.height() /
        (2.0 * m_zoomFactor);

    if (m_image.width() <=
        (halfViewportWidthInImage *
         2.0)) {
        m_viewCenterImagePoint.setX(
            m_image.width() / 2.0
            );
    } else {
        m_viewCenterImagePoint.setX(
            std::clamp(
                m_viewCenterImagePoint.x(),
                halfViewportWidthInImage,
                m_image.width() -
                    halfViewportWidthInImage
                )
            );
    }

    if (m_image.height() <=
        (halfViewportHeightInImage *
         2.0)) {
        m_viewCenterImagePoint.setY(
            m_image.height() / 2.0
            );
    } else {
        m_viewCenterImagePoint.setY(
            std::clamp(
                m_viewCenterImagePoint.y(),
                halfViewportHeightInImage,
                m_image.height() -
                    halfViewportHeightInImage
                )
            );
    }
}

void ToolPreviewWidget::beginPan(
    const QPointF &widgetPoint
    )
{
    if (!m_panEnabled ||
        m_image.isNull()) {
        return;
    }

    m_panning = true;
    m_lastPanWidgetPoint =
        widgetPoint;

    updateCursor();
}

void ToolPreviewWidget::updatePan(
    const QPointF &widgetPoint
    )
{
    if (!m_panning ||
        m_image.isNull() ||
        m_zoomFactor <= 0.0) {
        return;
    }

    const QPointF widgetDelta =
        widgetPoint -
        m_lastPanWidgetPoint;

    m_lastPanWidgetPoint =
        widgetPoint;

    m_viewCenterImagePoint -=
        QPointF(
            widgetDelta.x() /
                m_zoomFactor,
            widgetDelta.y() /
                m_zoomFactor
            );

    const bool wasFitToWindow =
        m_fitToWindow;

    m_fitToWindow = false;

    clampViewCenter();
    updateScrollBars();
    update();

    emitViewSignals(
        wasFitToWindow
        );
}

void ToolPreviewWidget::endPan()
{
    if (!m_panning) {
        updateCursor();
        return;
    }

    m_panning = false;
    updateCursor();
}

bool ToolPreviewWidget::shouldBeginPan(
    const QMouseEvent *event
    ) const
{
    if (!m_panEnabled ||
        m_image.isNull()) {
        return false;
    }

    if (event->button() ==
        Qt::MiddleButton) {
        return true;
    }

    if (event->button() !=
        Qt::LeftButton) {
        return false;
    }

    return m_leftButtonPans ||
           m_spaceHeld;
}

void ToolPreviewWidget::updateCursor()
{
    if (!m_panEnabled) {
        setCursor(m_toolCursor);
        return;
    }

    if (m_panning) {
        setCursor(
            Qt::ClosedHandCursor
            );
        return;
    }

    if (m_leftButtonPans ||
        m_spaceHeld) {
        setCursor(
            Qt::OpenHandCursor
            );
        return;
    }

    setCursor(m_toolCursor);
}

void ToolPreviewWidget::updateScrollBars()
{
    if (!m_horizontalScrollBar ||
        !m_verticalScrollBar) {
        return;
    }

    if (m_image.isNull() ||
        m_fitToWindow) {
        m_horizontalScrollBar->hide();
        m_verticalScrollBar->hide();

        positionModeBar();
        return;
    }

    const int extent =
        std::max(
            1,
            scrollBarExtent()
            );

    const double contentWidth =
        m_image.width() *
        m_zoomFactor;

    const double contentHeight =
        m_image.height() *
        m_zoomFactor;

    bool showHorizontal =
        contentWidth >
        static_cast<double>(width()) +
            0.5;

    bool showVertical =
        contentHeight >
        static_cast<double>(height()) +
            0.5;

    for (int pass = 0;
         pass < 3;
         ++pass) {
        const int availableWidth =
            std::max(
                0,
                width() -
                    (showVertical
                         ? extent
                         : 0)
                );

        const int availableHeight =
            std::max(
                0,
                height() -
                    (showHorizontal
                         ? extent
                         : 0)
                );

        const bool nextHorizontal =
            contentWidth >
            static_cast<double>(
                availableWidth
                ) + 0.5;

        const bool nextVertical =
            contentHeight >
            static_cast<double>(
                availableHeight
                ) + 0.5;

        if (nextHorizontal ==
                showHorizontal &&
            nextVertical ==
                showVertical) {
            break;
        }

        showHorizontal =
            nextHorizontal;

        showVertical =
            nextVertical;
    }

    m_horizontalScrollBar->setVisible(
        showHorizontal
        );

    m_verticalScrollBar->setVisible(
        showVertical
        );

    const int availableWidth =
        std::max(
            0,
            width() -
                (showVertical
                     ? extent
                     : 0)
            );

    const int availableHeight =
        std::max(
            0,
            height() -
                (showHorizontal
                     ? extent
                     : 0)
            );

    if (showHorizontal) {
        m_horizontalScrollBar->setGeometry(
            0,
            availableHeight,
            availableWidth,
            extent
            );
    }

    if (showVertical) {
        m_verticalScrollBar->setGeometry(
            availableWidth,
            0,
            extent,
            availableHeight
            );
    }

    clampViewCenter();

    const QRectF activeViewport =
        viewportRect();

    const int maximumHorizontal =
        std::max(
            0,
            static_cast<int>(
                std::ceil(
                    contentWidth -
                    activeViewport.width()
                    )
                )
            );

    const int maximumVertical =
        std::max(
            0,
            static_cast<int>(
                std::ceil(
                    contentHeight -
                    activeViewport.height()
                    )
                )
            );

    const int horizontalValue =
        std::clamp(
            static_cast<int>(
                std::round(
                    (m_viewCenterImagePoint.x() *
                     m_zoomFactor) -
                    (activeViewport.width() /
                     2.0)
                    )
                ),
            0,
            maximumHorizontal
            );

    const int verticalValue =
        std::clamp(
            static_cast<int>(
                std::round(
                    (m_viewCenterImagePoint.y() *
                     m_zoomFactor) -
                    (activeViewport.height() /
                     2.0)
                    )
                ),
            0,
            maximumVertical
            );

    m_updatingScrollBars = true;

    {
        QSignalBlocker horizontalBlocker(
            m_horizontalScrollBar
            );

        QSignalBlocker verticalBlocker(
            m_verticalScrollBar
            );

        m_horizontalScrollBar->setRange(
            0,
            maximumHorizontal
            );

        m_verticalScrollBar->setRange(
            0,
            maximumVertical
            );

        m_horizontalScrollBar->setPageStep(
            std::max(
                1,
                static_cast<int>(
                    activeViewport.width()
                    )
                )
            );

        m_verticalScrollBar->setPageStep(
            std::max(
                1,
                static_cast<int>(
                    activeViewport.height()
                    )
                )
            );

        m_horizontalScrollBar->setSingleStep(
            std::max(
                1,
                static_cast<int>(
                    activeViewport.width() /
                    20.0
                    )
                )
            );

        m_verticalScrollBar->setSingleStep(
            std::max(
                1,
                static_cast<int>(
                    activeViewport.height() /
                    20.0
                    )
                )
            );

        m_horizontalScrollBar->setValue(
            horizontalValue
            );

        m_verticalScrollBar->setValue(
            verticalValue
            );
    }

    m_updatingScrollBars = false;

    m_horizontalScrollBar->raise();
    m_verticalScrollBar->raise();

    positionModeBar();
}

void ToolPreviewWidget::handleScrollBarChanged()
{
    if (m_updatingScrollBars ||
        m_image.isNull() ||
        m_zoomFactor <= 0.0) {
        return;
    }

    const QRectF activeViewport =
        viewportRect();

    if (m_horizontalScrollBar->isVisible()) {
        m_viewCenterImagePoint.setX(
            (m_horizontalScrollBar->value() +
             (activeViewport.width() /
              2.0)) /
                m_zoomFactor
            );
    } else {
        m_viewCenterImagePoint.setX(
            m_image.width() / 2.0
            );
    }

    if (m_verticalScrollBar->isVisible()) {
        m_viewCenterImagePoint.setY(
            (m_verticalScrollBar->value() +
             (activeViewport.height() /
              2.0)) /
                m_zoomFactor
            );
    } else {
        m_viewCenterImagePoint.setY(
            m_image.height() / 2.0
            );
    }

    const bool wasFitToWindow =
        m_fitToWindow;

    m_fitToWindow = false;

    clampViewCenter();
    updateScrollBars();
    update();

    emitViewSignals(
        wasFitToWindow
        );
}

void ToolPreviewWidget::updateModeButtons()
{
    const bool hasImage =
        !m_image.isNull();

    if (m_toolModeButton) {
        QSignalBlocker blocker(
            m_toolModeButton
            );

        m_toolModeButton->setEnabled(
            hasImage
            );

        m_toolModeButton->setChecked(
            !m_leftButtonPans
            );
    }

    if (m_panModeButton) {
        QSignalBlocker blocker(
            m_panModeButton
            );

        m_panModeButton->setEnabled(
            hasImage &&
            m_panEnabled
            );

        m_panModeButton->setChecked(
            m_leftButtonPans
            );
    }
}

void ToolPreviewWidget::positionModeBar()
{
    if (!m_modeBar) {
        return;
    }

    m_modeBar->adjustSize();

    constexpr int margin = 10;

    const QRectF activeViewport =
        viewportRect();

    const int x =
        std::max(
            margin,
            static_cast<int>(
                activeViewport.right()
                ) -
                m_modeBar->width() -
                margin +
                1
            );

    const int y =
        std::max(
            margin,
            static_cast<int>(
                activeViewport.bottom()
                ) -
                m_modeBar->height() -
                margin +
                1
            );

    m_modeBar->move(x, y);
    m_modeBar->raise();
}

void ToolPreviewWidget::emitViewSignals(
    bool fitModeMayHaveChanged
    )
{
    emit zoomChanged(
        m_zoomFactor,
        zoomPercent()
        );

    if (fitModeMayHaveChanged) {
        emit fitModeChanged(
            m_fitToWindow
            );
    }

    emit viewChanged();
}
