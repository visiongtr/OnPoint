#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "image_tools.h"
#include "imageprocessingruntime.h"
#include "redeyedialog.h"
#include "overexposurerecoveryworkspace.h"
#include "overexposureselectionwidget.h"
#include "brightareaselectionwidget.h"
#include "toolpreviewwidget.h"
#include "whitebalancedialog.h"
#include "noisereductiondialog.h"
#include "focusdialog.h"
#include "appsettings.h"
#include "preferencesdialog.h"
#include "imageoutputdialog.h"
#include "imageoutputservice.h"
#include "imageimportservice.h"
#include "printlayoutdialog.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QAbstractSpinBox>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QStatusBar>
#include <QUrl>
#include <QToolButton>
#include <QTransform>
#include <QTimer>
#include <algorithm>
#include <array>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>
#include <QAction>
#include <QDockWidget>
#include <QSizePolicy>
#include <QWidget>
#include <QFont>
#include <QFontMetrics>
#include <QSlider>
#include <QSizePolicy>
#include <QCloseEvent>
#include <QButtonGroup>
#include <QStyle>
#include <QIcon>
#include <QCursor>
#include <functional>

namespace {

QIcon makeHueEyedropperIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(
        QPainter::Antialiasing,
        true
        );

    QPen bodyPen(
        QColor("#DDE2E4"),
        2.0
        );

    bodyPen.setCapStyle(
        Qt::RoundCap
        );

    bodyPen.setJoinStyle(
        Qt::RoundJoin
        );

    painter.setPen(bodyPen);

    // Eyedropper body.
    painter.drawLine(
        QPointF(7.0, 17.0),
        QPointF(16.5, 7.5)
        );

    painter.drawLine(
        QPointF(14.0, 5.0),
        QPointF(19.0, 10.0)
        );

    painter.drawLine(
        QPointF(16.5, 7.5),
        QPointF(18.3, 5.7)
        );

    painter.drawLine(
        QPointF(18.3, 5.7),
        QPointF(19.8, 7.2)
        );

    // Sample tip and a small amber color drop.
    QPen tipPen(
        QColor("#B8894F"),
        2.0
        );

    tipPen.setCapStyle(
        Qt::RoundCap
        );

    painter.setPen(tipPen);

    painter.drawLine(
        QPointF(5.0, 19.0),
        QPointF(8.0, 16.0)
        );

    painter.setPen(Qt::NoPen);
    painter.setBrush(
        QColor("#B8894F")
        );

    painter.drawEllipse(
        QPointF(4.5, 20.0),
        1.5,
        1.5
        );

    return QIcon(pixmap);
}

Qt::DockWidgetArea preferredToolsDockArea()
{
    return AppSettings::toolsPanelSide() ==
                   AppSettings::ToolsPanelSide::Left
        ? Qt::LeftDockWidgetArea
        : Qt::RightDockWidgetArea;
}

QString toolsToolbarText(
    bool toolsPanelVisible
    )
{
    const bool toolsOnLeft =
        AppSettings::toolsPanelSide() ==
        AppSettings::ToolsPanelSide::Left;

    if (toolsOnLeft) {
        return toolsPanelVisible
            ? "◀ Tools"
            : "Tools ▶";
    }

    return toolsPanelVisible
        ? "Tools ▶"
        : "◀ Tools";
}

class MainImageSampleFilter : public QObject
{
public:
    using Handler =
        std::function<bool(const QPoint &, Qt::MouseButton)>;

    explicit MainImageSampleFilter(
        const Handler &handler,
        QObject *parent = nullptr
        )
        : QObject(parent)
        , m_handler(handler)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent =
                static_cast<QMouseEvent *>(event);

            if (m_handler &&
                m_handler(mouseEvent->pos(), mouseEvent->button())) {
                event->accept();
                return true;
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    Handler m_handler;
};

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    ui->setupUi(this);

    // Apply the saved appearance before building the rest of the window.
    // Dark remains the default; Light is the optional companion theme.
    AppSettings::applyTheme();

    // HMS now opens in a wide tool window, so use the full color-pair
    // names instead of the older colored abbreviation icons.
    ui->hmsTabWidget->setTabIcon(0, QIcon());
    ui->hmsTabWidget->setTabIcon(1, QIcon());
    ui->hmsTabWidget->setTabIcon(2, QIcon());
    ui->hmsTabWidget->setTabIcon(3, QIcon());

    ui->hmsTabWidget->setTabText(0, "Base");
    ui->hmsTabWidget->setTabText(1, "Cyan / Red");
    ui->hmsTabWidget->setTabText(2, "Magenta / Green");
    ui->hmsTabWidget->setTabText(3, "Yellow / Blue");

    // Keep every Basic Adjustment control on the same symmetrical range.
    // This places zero at the visual center of every slider.
    auto configureBasicControl =
        [](QSlider *slider, QSpinBox *spinBox) {
            slider->setRange(-100, 100);
            slider->setSingleStep(1);
            slider->setPageStep(10);

            spinBox->setRange(-100, 100);
            spinBox->setSingleStep(1);
        };

    configureBasicControl(
        ui->brightnessSlider,
        ui->brightnessSpinBox
        );

    configureBasicControl(
        ui->contrastSlider,
        ui->contrastSpinBox
        );

    configureBasicControl(
        ui->gammaSlider,
        ui->gammaSpinBox
        );

    configureBasicControl(
        ui->saturationSlider,
        ui->saturationSpinBox
        );

    // Give all Basic Adjustment labels the same width so every slider
    // begins at the same horizontal position.
    const QStringList basicLabelTexts = {
        "Brightness",
        "Contrast",
        "Gamma",
        "Saturation"
    };

    QList<QLabel *> basicAdjustmentLabels;
    int widestBasicLabel = 0;

    const QList<QLabel *> toolLabels =
        ui->toolsDock->findChildren<QLabel *>();

    for (QLabel *label : toolLabels) {
        if (!basicLabelTexts.contains(label->text())) {
            continue;
        }

        basicAdjustmentLabels.append(label);

        widestBasicLabel = std::max(
            widestBasicLabel,
            label->sizeHint().width()
            );
    }

    for (QLabel *label : basicAdjustmentLabels) {
        label->setFixedWidth(widestBasicLabel);
        label->setAlignment(
            Qt::AlignLeft | Qt::AlignVCenter
            );
    }

    // Tools dock behavior:
    // - resizable by dragging its edge
    // - no default close/float buttons
    // - no dock title bar
    // - far-right toolbar button handles show/hide
    ui->toolsDock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    // Hide the dock title bar completely.
    // The far-right toolbar button handles showing/hiding the Tools panel.
    QWidget *emptyToolsTitleBar = new QWidget(ui->toolsDock);
    emptyToolsTitleBar->setFixedHeight(0);
    ui->toolsDock->setTitleBarWidget(emptyToolsTitleBar);

    addDockWidget(
        preferredToolsDockArea(),
        ui->toolsDock
        );

    // View menu checkbox/action
    m_viewToolsPanelAction = new QAction("Show Tools Panel", this);
    m_viewToolsPanelAction->setCheckable(true);
    m_viewToolsPanelAction->setChecked(true);

    ui->menuView->addSeparator();
    ui->menuView->addAction(m_viewToolsPanelAction);

    connect(m_viewToolsPanelAction, &QAction::toggled,
            this, &MainWindow::setToolsPanelVisible);


    // Keep View menu and far-right toolbar button in sync if visibility changes.
    connect(ui->toolsDock, &QDockWidget::visibilityChanged,
            this,
            [this](bool visible) {
                if (m_viewToolsPanelAction) {
                    m_viewToolsPanelAction->blockSignals(true);
                    m_viewToolsPanelAction->setChecked(visible);
                    m_viewToolsPanelAction->blockSignals(false);
                }

                if (m_toolbarShowToolsAction) {
                    m_toolbarShowToolsAction->setText(
                        toolsToolbarText(visible)
                        );
                    m_toolbarShowToolsAction->setToolTip(
                        visible ? "Hide tools panel" : "Show tools panel"
                    );
                }
            });

    setWindowTitle("On Point Photo Editor");
    setAcceptDrops(true);

    ui->imagePreviewLabel->installEventFilter(this);
    ui->imagePreviewLabel->setMouseTracking(true);

    // Right-click menu for the photo and surrounding preview workspace.
    auto showPreviewContextMenu =
        [this](QWidget *sourceWidget, const QPoint &position) {
            QMenu menu(this);

            menu.addAction(ui->actionOpenImage);

            if (!m_currentImage.isNull()) {
                menu.addSeparator();

                menu.addAction(ui->actionSave);
                menu.addAction(ui->actionSaveAs);
                menu.addAction(ui->actionExportAs);

                menu.addSeparator();

                menu.addAction(ui->actionCloseImage);

                menu.addSeparator();

                menu.addAction(ui->actionUndo);
                menu.addAction(ui->actionRedo);

                menu.addSeparator();

                menu.addAction(ui->actionFitToWindow);
                menu.addAction(ui->actionActualSize);
                menu.addAction(ui->actionZoomIn);
                menu.addAction(ui->actionZoomOut);
                menu.addAction(ui->actionCompare);

                menu.addSeparator();

                menu.addAction(ui->actionCrop);
                menu.addAction(ui->actionResize);
            }

            menu.exec(
                sourceWidget->mapToGlobal(position)
                );
        };

    ui->imagePreviewLabel->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(
        ui->imagePreviewLabel,
        &QWidget::customContextMenuRequested,
        this,
        [this, showPreviewContextMenu](const QPoint &position) {
            showPreviewContextMenu(
                ui->imagePreviewLabel,
                position
                );
        }
        );

    ui->previewScrollArea->viewport()->setContextMenuPolicy(
        Qt::CustomContextMenu
        );

    connect(
        ui->previewScrollArea->viewport(),
        &QWidget::customContextMenuRequested,
        this,
        [this, showPreviewContextMenu](const QPoint &position) {
            showPreviewContextMenu(
                ui->previewScrollArea->viewport(),
                position
                );
        }
        );

    // Prevent the mouse wheel from accidentally changing adjustment
    // sliders or number boxes. Wheel movement over these controls will
    // scroll the Tools panel instead.
    const QList<QSlider *> toolSliders =
        ui->toolsDock->findChildren<QSlider *>();

    for (QSlider *slider : toolSliders) {
        slider->installEventFilter(this);

        // Link each slider to the label that shares its object-name prefix:
        // brightnessSlider -> brightnessLabel
        // baseHighlightSlider -> baseHighlightLabel
        QString labelObjectName =
            slider->objectName();

        if (labelObjectName.endsWith("Slider")) {
            labelObjectName.chop(6);
            labelObjectName += "Label";

            slider->setProperty(
                "linkedLabelObjectName",
                labelObjectName
                );
        }
    }

    const QList<QAbstractSpinBox *> toolSpinBoxes =
        ui->toolsDock->findChildren<QAbstractSpinBox *>();

    for (QAbstractSpinBox *spinBox : toolSpinBoxes) {
        spinBox->installEventFilter(this);
    }

    // Let zoomed images become larger than the viewport so scrollbars can appear.
    ui->previewScrollArea->setWidgetResizable(false);

    ui->previewLayout->setContentsMargins(0, 0, 0, 0);
    ui->previewLayout->setSpacing(0);
    ui->previewLayout->setAlignment(ui->imagePreviewLabel, Qt::AlignCenter);

    ui->imagePreviewLabel->setMinimumSize(0, 0);
    ui->imagePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    ui->previewScrollAreaContents->setMinimumSize(0, 0);
    ui->previewScrollAreaContents->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(ui->actionOpenImage, &QAction::triggered,
            this, &MainWindow::openImage);

    connect(ui->actionUndo, &QAction::triggered,
            this, &MainWindow::undoEdit);

    connect(ui->actionRedo, &QAction::triggered,
            this, &MainWindow::redoEdit);

    ui->actionUndo->setEnabled(false);
    ui->actionRedo->setEnabled(false);

    connect(ui->actionSave, &QAction::triggered,
            this, &MainWindow::saveImage);

    connect(ui->actionSaveAs, &QAction::triggered,
            this, &MainWindow::saveImageAs);

    connect(ui->actionExportAs, &QAction::triggered,
            this, &MainWindow::exportImageAs);

    connect(ui->actionPrintPreview, &QAction::triggered,
            this, &MainWindow::showPrintPreviewDialog);

    connect(ui->actionPrint, &QAction::triggered,
            this, &MainWindow::showPrintDialog);

    ui->actionPrintPreview->setEnabled(false);
    ui->actionPrint->setEnabled(false);

    connect(ui->actionCloseImage, &QAction::triggered,
            this, &MainWindow::closeImage);

    connect(ui->actionExit, &QAction::triggered,
            this, &MainWindow::close);

    connect(ui->actionFitToWindow, &QAction::triggered,
            this, &MainWindow::fitToWindow);

    connect(ui->actionActualSize, &QAction::triggered,
            this, &MainWindow::actualSize);

    connect(ui->actionZoomIn, &QAction::triggered,
            this, &MainWindow::zoomIn);

    // Add Actual Size / 1:1 to the toolbar between Fit and Zoom In.
    ui->actionActualSize->setText("1:1");
    ui->actionActualSize->setToolTip("Show image at actual pixel size");

    ui->mainToolBar->insertAction(ui->actionZoomIn, ui->actionActualSize);

    // Keep Undo and Redo together between the file actions
    // and the complete zoom-control group.
    ui->mainToolBar->insertAction(ui->actionZoomOut, ui->actionUndo);
    ui->mainToolBar->insertAction(ui->actionZoomOut, ui->actionRedo);
    ui->mainToolBar->insertSeparator(ui->actionZoomOut);

    // Use compact icons for file operations while preserving their
    // normal text in menus and context menus.
    auto findThemeIcon =
        [this](const QStringList &iconNames,
               QStyle::StandardPixmap fallbackIcon) {
            for (const QString &iconName : iconNames) {
                const QIcon icon =
                    QIcon::fromTheme(iconName);

                if (!icon.isNull()) {
                    return icon;
                }
            }

            return style()->standardIcon(
                fallbackIcon
                );
        };

    auto makeFileToolbarActionIconOnly =
        [this](QAction *action,
               const QIcon &icon,
               const QString &toolTip) {
            action->setIcon(icon);
            action->setToolTip(toolTip);

            QToolButton *toolbarButton =
                qobject_cast<QToolButton *>(
                    ui->mainToolBar->widgetForAction(
                        action
                        )
                    );

            if (!toolbarButton) {
                return;
            }

            toolbarButton->setToolButtonStyle(
                Qt::ToolButtonIconOnly
                );

            toolbarButton->setIconSize(
                QSize(24, 24)
                );

            toolbarButton->setFixedSize(
                34,
                30
                );

            toolbarButton->setAutoRaise(true);
        };

    auto makeExportToolbarIcon = []() {
        const QSize iconSize(24, 24);

        QPixmap pixmap(iconSize);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(
            QPainter::Antialiasing,
            true
            );

        // Export tray.
        QPen trayPen(
            QColor("#D7DDE0")
            );

        trayPen.setWidthF(1.7);
        trayPen.setCapStyle(Qt::RoundCap);
        trayPen.setJoinStyle(Qt::RoundJoin);

        painter.setPen(trayPen);

        painter.drawLine(
            QPointF(4.5, 14.0),
            QPointF(4.5, 20.0)
            );

        painter.drawLine(
            QPointF(4.5, 20.0),
            QPointF(19.5, 20.0)
            );

        painter.drawLine(
            QPointF(19.5, 20.0),
            QPointF(19.5, 14.0)
            );

        // Amber arrow leaving the tray.
        QPen arrowPen(
            QColor("#B8894F")
            );

        arrowPen.setWidthF(2.0);
        arrowPen.setCapStyle(Qt::RoundCap);
        arrowPen.setJoinStyle(Qt::RoundJoin);

        painter.setPen(arrowPen);

        painter.drawLine(
            QPointF(9.0, 15.0),
            QPointF(17.0, 7.0)
            );

        painter.drawLine(
            QPointF(12.5, 7.0),
            QPointF(17.0, 7.0)
            );

        painter.drawLine(
            QPointF(17.0, 7.0),
            QPointF(17.0, 11.5)
            );

        painter.end();

        return QIcon(pixmap);
    };

    makeFileToolbarActionIconOnly(
        ui->actionOpenImage,
        findThemeIcon(
            {
                "document-open-symbolic",
                "document-open"
            },
            QStyle::SP_DialogOpenButton
            ),
        "Open Image"
        );

    makeFileToolbarActionIconOnly(
        ui->actionSave,
        findThemeIcon(
            {
                "document-save-symbolic",
                "document-save"
            },
            QStyle::SP_DialogSaveButton
            ),
        "Save"
        );

    makeFileToolbarActionIconOnly(
        ui->actionExportAs,
        makeExportToolbarIcon(),
        "Export As"
        );

    makeFileToolbarActionIconOnly(
        ui->actionUndo,
        findThemeIcon(
            {
                "edit-undo-symbolic",
                "edit-undo"
            },
            QStyle::SP_ArrowBack
            ),
        "Undo (Ctrl+Z)"
        );

    makeFileToolbarActionIconOnly(
        ui->actionRedo,
        findThemeIcon(
            {
                "edit-redo-symbolic",
                "edit-redo"
            },
            QStyle::SP_ArrowForward
            ),
        "Redo (Ctrl+Y)"
        );

    connect(ui->actionZoomOut, &QAction::triggered,
            this, &MainWindow::zoomOut);

    connect(ui->actionCrop, &QAction::triggered,
            this, &MainWindow::selectCropArea);

    connect(ui->selectCropButton, &QPushButton::clicked,
            this, &MainWindow::selectCropArea);

    connect(ui->applyCropButton, &QPushButton::clicked,
            this, &MainWindow::applyCrop);

    connect(ui->cancelCropButton, &QPushButton::clicked,
            this, &MainWindow::cancelCrop);

    connect(ui->cropXSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::cropControlsChanged);

    connect(ui->cropYSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::cropControlsChanged);

    connect(ui->cropWidthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::cropControlsChanged);

    connect(ui->cropHeightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::cropControlsChanged);
    connect(ui->brightnessSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->brightnessSpinBox);
                ui->brightnessSpinBox->setValue(value);
                brightnessControlsChanged(value);
            });

    connect(ui->brightnessSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->brightnessSlider);
                ui->brightnessSlider->setValue(value);
                brightnessControlsChanged(value);
            });

    connect(ui->contrastSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->contrastSpinBox);
                ui->contrastSpinBox->setValue(value);
                contrastControlsChanged(value);
            });

    connect(ui->contrastSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->contrastSlider);
                ui->contrastSlider->setValue(value);
                contrastControlsChanged(value);
            });

    connect(ui->gammaSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->gammaSpinBox);
                ui->gammaSpinBox->setValue(value);
                gammaControlsChanged(value);
            });

    connect(ui->gammaSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->gammaSlider);
                ui->gammaSlider->setValue(value);
                gammaControlsChanged(value);
            });

    connect(ui->saturationSlider, &QSlider::valueChanged,
            this, &MainWindow::saturationControlsChanged);

    connect(ui->saturationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::saturationControlsChanged);

    connect(ui->baseHighlightSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->baseHighlightSpinBox);
                ui->baseHighlightSpinBox->setValue(value);
                baseHighlightControlsChanged(value);
            });

    connect(ui->baseHighlightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->baseHighlightSlider);
                ui->baseHighlightSlider->setValue(value);
                baseHighlightControlsChanged(value);
            });

    connect(ui->baseMidtoneSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->baseMidtoneSpinBox);
                ui->baseMidtoneSpinBox->setValue(value);
                baseMidtoneControlsChanged(value);
            });

    connect(ui->baseMidtoneSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->baseMidtoneSlider);
                ui->baseMidtoneSlider->setValue(value);
                baseMidtoneControlsChanged(value);
            });

    connect(ui->baseShadowSlider, &QSlider::valueChanged,
            this,
            [this](int value) {
                QSignalBlocker blockSpin(ui->baseShadowSpinBox);
                ui->baseShadowSpinBox->setValue(value);
                baseShadowControlsChanged(value);
            });

    connect(ui->baseShadowSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                QSignalBlocker blockSlider(ui->baseShadowSlider);
                ui->baseShadowSlider->setValue(value);
                baseShadowControlsChanged(value);
            });

    connect(ui->resetBaseTabButton, &QPushButton::clicked,
            this, &MainWindow::resetBaseHmsAdjustments);

    auto connectColorHmsPair =
        [this](QSlider *slider,
               QSpinBox *spinBox,
               int MainWindow::*storedAmount,
               const QString &label) {
            connect(slider, &QSlider::valueChanged,
                    this,
                    [this, spinBox, storedAmount, label](int value) {
                        QSignalBlocker blockSpin(spinBox);
                        spinBox->setValue(value);
                        hmsColorControlChanged(this->*storedAmount, value, label);
                    });

            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                    this,
                    [this, slider, storedAmount, label](int value) {
                        QSignalBlocker blockSlider(slider);
                        slider->setValue(value);
                        hmsColorControlChanged(this->*storedAmount, value, label);
                    });
        };

    connectColorHmsPair(
        ui->cyanRedHighlightSlider,
        ui->cyanRedHighlightSpinBox,
        &MainWindow::m_cyanRedHighlightAmount,
        "Cyan / Red Highlight"
        );

    connectColorHmsPair(
        ui->cyanRedMidtoneSlider,
        ui->cyanRedMidtoneSpinBox,
        &MainWindow::m_cyanRedMidtoneAmount,
        "Cyan / Red Midtone"
        );

    connectColorHmsPair(
        ui->cyanRedShadowSlider,
        ui->cyanRedShadowSpinBox,
        &MainWindow::m_cyanRedShadowAmount,
        "Cyan / Red Shadow"
        );

    connectColorHmsPair(
        ui->magentaGreenHighlightSlider,
        ui->magentaGreenHighlightSpinBox,
        &MainWindow::m_magentaGreenHighlightAmount,
        "Magenta / Green Highlight"
        );

    connectColorHmsPair(
        ui->magentaGreenMidtoneSlider,
        ui->magentaGreenMidtoneSpinBox,
        &MainWindow::m_magentaGreenMidtoneAmount,
        "Magenta / Green Midtone"
        );

    connectColorHmsPair(
        ui->magentaGreenShadowSlider,
        ui->magentaGreenShadowSpinBox,
        &MainWindow::m_magentaGreenShadowAmount,
        "Magenta / Green Shadow"
        );

    connectColorHmsPair(
        ui->yellowBlueHighlightSlider,
        ui->yellowBlueHighlightSpinBox,
        &MainWindow::m_yellowBlueHighlightAmount,
        "Yellow / Blue Highlight"
        );

    connectColorHmsPair(
        ui->yellowBlueMidtoneSlider,
        ui->yellowBlueMidtoneSpinBox,
        &MainWindow::m_yellowBlueMidtoneAmount,
        "Yellow / Blue Midtone"
        );

    connectColorHmsPair(
        ui->yellowBlueShadowSlider,
        ui->yellowBlueShadowSpinBox,
        &MainWindow::m_yellowBlueShadowAmount,
        "Yellow / Blue Shadow"
        );

    connect(ui->resetCyanRedTabButton, &QPushButton::clicked,
            this, &MainWindow::resetCyanRedHmsAdjustments);

    connect(ui->resetMagentaGreenTabButton, &QPushButton::clicked,
            this, &MainWindow::resetMagentaGreenHmsAdjustments);

    connect(ui->resetYellowBlueTabButton, &QPushButton::clicked,
            this, &MainWindow::resetYellowBlueHmsAdjustments);

    connect(ui->resetBasicButton, &QPushButton::clicked,
            this, &MainWindow::resetBasicAdjustments);

    connect(ui->actionRotateLeft, &QAction::triggered,
            this, &MainWindow::rotateLeft);

    connect(ui->actionRotateRight, &QAction::triggered,
            this, &MainWindow::rotateRight);

    connect(ui->actionFlipHorizontal, &QAction::triggered,
            this, &MainWindow::flipHorizontal);

    connect(ui->actionFlipVertical, &QAction::triggered,
            this, &MainWindow::flipVertical);

    connect(ui->rotateLeftButton, &QToolButton::clicked,
            this, &MainWindow::rotateLeft);

    connect(ui->rotateRightButton, &QToolButton::clicked,
            this, &MainWindow::rotateRight);

    connect(ui->flipHorizontalButton, &QToolButton::clicked,
            this, &MainWindow::flipHorizontal);

    connect(ui->flipVerticalButton, &QToolButton::clicked,
            this, &MainWindow::flipVertical);

    connect(ui->actionResize, &QAction::triggered,
            this, &MainWindow::openResizeDialog);

    connect(ui->resizeButton, &QPushButton::clicked,
            this, &MainWindow::openResizeDialog);

    connect(ui->showOriginalButton, &QPushButton::pressed,
            this, &MainWindow::showOriginalPressed);

    connect(ui->showOriginalButton, &QPushButton::released,
            this, &MainWindow::showOriginalReleased);

    connect(ui->actionCompare, &QAction::toggled,
            this, &MainWindow::toggleSideBySideCompare);

    connect(
        ui->actionPreferences,
        &QAction::triggered,
        this,
        [this]() {
            openPreferences(false);
        });

    connect(
        ui->actionSystemOptimizer,
        &QAction::triggered,
        this,
        [this]() {
            openPreferences(true);
        });

    ui->showOriginalButton->setEnabled(false);
    ui->actionCompare->setEnabled(false);

    // Keep the current document name centered in the open toolbar space,
    // with the Tools toggle anchored at the far right.
    QWidget *toolbarLeftSpacer = new QWidget(this);
    toolbarLeftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(toolbarLeftSpacer);

    m_openFileToolbarWidget = new QWidget(this);
    m_openFileToolbarWidget->setObjectName("openFileToolbarWidget");
    m_openFileToolbarWidget->setVisible(false);

    QHBoxLayout *openFileToolbarLayout =
        new QHBoxLayout(m_openFileToolbarWidget);

    openFileToolbarLayout->setContentsMargins(8, 1, 2, 1);
    openFileToolbarLayout->setSpacing(5);

    m_openFileNameLabel = new QLabel(m_openFileToolbarWidget);
    m_openFileNameLabel->setObjectName("openFileNameLabel");
    m_openFileNameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_openFileNameLabel->setMinimumWidth(140);
    m_openFileNameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_closeFileToolbarButton = new QToolButton(m_openFileToolbarWidget);
    m_closeFileToolbarButton->setObjectName("closeFileToolbarButton");
    m_closeFileToolbarButton->setText(QString(QChar(0x00D7)));
    m_closeFileToolbarButton->setToolTip("Close image");
    m_closeFileToolbarButton->setAutoRaise(true);
    m_closeFileToolbarButton->setFixedSize(24, 24);

    openFileToolbarLayout->addWidget(m_openFileNameLabel);
    openFileToolbarLayout->addWidget(m_closeFileToolbarButton);

    ui->mainToolBar->addWidget(m_openFileToolbarWidget);

    connect(m_closeFileToolbarButton, &QToolButton::clicked,
            this, &MainWindow::closeImage);

    QWidget *toolbarRightSpacer = new QWidget(this);
    toolbarRightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(toolbarRightSpacer);

    m_toolbarShowToolsAction = new QAction(
        toolsToolbarText(true),
        this
        );
    m_toolbarShowToolsAction->setToolTip("Hide tools panel");
    ui->mainToolBar->addAction(m_toolbarShowToolsAction);

    connect(m_toolbarShowToolsAction, &QAction::triggered,
            this,
            [this]() {
                setToolsPanelVisible(!ui->toolsDock->isVisible());
            });

    setupQuickToolsPanel();
    setupHistoryMenu();
    setQuickToolsEnabled(false);

    resetCropControlsToImage();

    updateUndoRedoActions();
    updateOpenFileToolbar();

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openImage()
{
    const QString filePath =
        QFileDialog::getOpenFileName(
            this,
            "Open Image",
            QString(),
            ImageImport::openFileFilter()
            );

    if (filePath.isEmpty()) {
        return;
    }

    // Opening a new source replaces the current document.
    if (!confirmUnsavedChanges()) {
        return;
    }

    loadImageFile(
        filePath
        );
}

void MainWindow::closeImage()
{
    if (!confirmUnsavedChanges()) {
        return;
    }

    m_originalImage = QImage();
    m_adjustmentBaseImage = QImage();
    m_currentImage = QImage();
    m_currentFilePath.clear();
    m_lastSavePath.clear();
    m_sourceInfo = ImageImport::SourceInfo();

    m_hasUnsavedChanges = false;

    m_brightnessAmount = 0;
    m_contrastAmount = 0;
    m_gammaAmount = 0;

    m_baseHighlightAmount = 0;
    m_baseMidtoneAmount = 0;
    m_baseShadowAmount = 0;

    m_cyanRedHighlightAmount = 0;
    m_cyanRedMidtoneAmount = 0;
    m_cyanRedShadowAmount = 0;

    m_magentaGreenHighlightAmount = 0;
    m_magentaGreenMidtoneAmount = 0;
    m_magentaGreenShadowAmount = 0;

    m_yellowBlueHighlightAmount = 0;
    m_yellowBlueMidtoneAmount = 0;
    m_yellowBlueShadowAmount = 0;

    m_basicAdjustmentUndoCaptured = false;

    setBasicAdjustmentControls(0, 0, 0, 0);
    setBaseHmsControls(0, 0, 0);

    setCyanRedHmsControls(0, 0, 0);
    setMagentaGreenHmsControls(0, 0, 0);
    setYellowBlueHmsControls(0, 0, 0);

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();
    m_cropDragStartRect = QRect();
    m_lastPreviewPixmapSize = QSize();

    m_showOriginalTemporarily = false;
    m_sideBySideCompare = false;

    ui->actionCompare->blockSignals(true);
    ui->actionCompare->setChecked(false);
    ui->actionCompare->setEnabled(false);
    ui->actionCompare->blockSignals(false);

    ui->showOriginalButton->setEnabled(false);

    ui->imagePreviewLabel->unsetCursor();

    clearHistory();

    setQuickToolsEnabled(false);
    ui->actionPrintPreview->setEnabled(false);
    ui->actionPrint->setEnabled(false);
    resetCropControlsToImage();
    updatePreview();
    updateOpenFileToolbar();

    statusBar()->showMessage("Image closed");
}

bool MainWindow::confirmUnsavedChanges()
{
    if (m_currentImage.isNull() || !m_hasUnsavedChanges) {
        return true;
    }

    QMessageBox messageBox(this);
    messageBox.setWindowTitle("Unsaved Changes");
    messageBox.setText("This image has unsaved changes.");
    messageBox.setInformativeText("Do you want to save before closing it?");
    messageBox.setIcon(QMessageBox::Warning);

    QPushButton *saveButton = messageBox.addButton("Save", QMessageBox::AcceptRole);
    QPushButton *discardButton = messageBox.addButton("Close Without Saving", QMessageBox::DestructiveRole);
    QPushButton *cancelButton = messageBox.addButton("Cancel", QMessageBox::RejectRole);

    messageBox.setDefaultButton(saveButton);
    messageBox.exec();

    if (messageBox.clickedButton() == cancelButton) {
        return false;
    }

    if (messageBox.clickedButton() == discardButton) {
        return true;
    }

    if (messageBox.clickedButton() == saveButton) {
        saveImage();
        return !m_hasUnsavedChanges;
    }

    return false;
}

void MainWindow::saveImage()
{
    if (m_currentImage.isNull()) {
        QMessageBox::information(
            this,
            "Save Image",
            "There is no image loaded to save."
            );
        return;
    }

    if (m_lastSavePath.isEmpty()) {
        saveImageAs();
        return;
    }

    bool recognized = false;

    const ImageOutput::Format format =
        ImageOutput::formatFromExtension(
            QFileInfo(
                m_lastSavePath
                ).suffix(),
            &recognized
            );

    if (!recognized ||
        !ImageOutput::isFormatSupported(
            format
            )) {
        saveImageAs();
        return;
    }

    const ImageOutput::Options options =
        ImageOutput::defaultOptions(
            format
            );

    if (writeImageToFile(
            m_lastSavePath,
            options,
            "Save Failed"
            )) {
        m_hasUnsavedChanges = false;
        m_basicAdjustmentUndoCaptured = false;

        statusBar()->showMessage(
            QString("Saved %1")
                .arg(
                    QFileInfo(
                        m_lastSavePath
                        ).fileName()
                    )
            );
    }
}

void MainWindow::saveImageAs()
{
    if (m_currentImage.isNull()) {
        QMessageBox::information(
            this,
            "Save Image As",
            "There is no image loaded to save."
            );
        return;
    }

    ImageOutputDialog dialog(
        ImageOutputDialog::Mode::SaveAs,
        m_currentImage,
        m_currentFilePath,
        this
        );

    if (dialog.exec() !=
        QDialog::Accepted) {
        return;
    }

    const QString filePath =
        dialog.selectedFilePath();

    const ImageOutput::Options options =
        dialog.outputOptions();

    if (writeImageToFile(
            filePath,
            options,
            "Save Failed"
            )) {
        m_currentFilePath = filePath;
        m_lastSavePath = filePath;
        updateOpenFileToolbar();

        // Saving under a new name changes the document identity, not the
        // image-edit history. Keep Undo and Redo from restoring the old path.
        for (ImageState &state : m_undoStack) {
            state.filePath = filePath;
        }

        for (ImageState &state : m_redoStack) {
            state.filePath = filePath;
        }

        m_hasUnsavedChanges = false;
        m_basicAdjustmentUndoCaptured = false;

        statusBar()->showMessage(
            QString("Saved as %1")
                .arg(
                    QFileInfo(
                        filePath
                        ).fileName()
                    )
            );
    }
}

void MainWindow::exportImageAs()
{
    if (m_currentImage.isNull()) {
        QMessageBox::information(
            this,
            "Export Image",
            "There is no image loaded to export."
            );
        return;
    }

    ImageOutputDialog dialog(
        ImageOutputDialog::Mode::Export,
        m_currentImage,
        m_currentFilePath,
        this
        );

    if (dialog.exec() !=
        QDialog::Accepted) {
        return;
    }

    const QString filePath =
        dialog.selectedFilePath();

    const ImageOutput::Options options =
        dialog.outputOptions();

    if (writeImageToFile(
            filePath,
            options,
            "Export Failed"
            )) {
        // Preserve the current document path, but an exported copy counts as
        // a saved result for the current correction session.
        m_hasUnsavedChanges = false;
        m_basicAdjustmentUndoCaptured = false;

        statusBar()->showMessage(
            QString("Exported %1")
                .arg(
                    QFileInfo(
                        filePath
                        ).fileName()
                    )
            );
    }
}

bool MainWindow::writeImageToFile(
    const QString &filePath,
    const ImageOutput::Options &options,
    const QString &operationTitle
    )
{
    if (m_currentImage.isNull()) {
        return false;
    }

    const ImageOutput::WriteResult result =
        ImageOutput::writeImage(
            m_currentImage,
            filePath,
            options,
            m_sourceInfo.sourceFilePath.isEmpty()
                ? m_currentFilePath
                : m_sourceInfo.sourceFilePath
            );

    if (!result.success) {
        QMessageBox::warning(
            this,
            operationTitle,
            "Could not write the image file:\n\n" +
                result.errorMessage
            );
        return false;
    }

    return true;
}

void MainWindow::updateOpenFileToolbar()
{
    if (!m_openFileToolbarWidget ||
        !m_openFileNameLabel) {
        return;
    }

    if (m_currentImage.isNull()) {
        m_openFileNameLabel->clear();
        m_openFileNameLabel->setToolTip(QString());
        m_openFileToolbarWidget->setVisible(false);
        return;
    }

    QString fileName;

    if (!m_currentFilePath.isEmpty()) {
        fileName = QFileInfo(m_currentFilePath).fileName();
    } else if (!m_sourceInfo.sourceFileName.isEmpty()) {
        fileName = m_sourceInfo.sourceFileName;
    } else {
        fileName = "Untitled";
    }

    const int maximumLabelWidth =
        std::clamp(width() / 3, 180, 520);

    m_openFileNameLabel->setMaximumWidth(maximumLabelWidth);
    m_openFileNameLabel->setText(
        m_openFileNameLabel->fontMetrics().elidedText(
            fileName,
            Qt::ElideMiddle,
            maximumLabelWidth
            )
        );
    m_openFileNameLabel->setToolTip(fileName);
    m_openFileToolbarWidget->setVisible(true);
}

void MainWindow::openPreferences(
    bool showPerformancePage
    )
{
    PreferencesDialog dialog(
        this,
        showPerformancePage
        );

    connect(
        &dialog,
        &PreferencesDialog::settingsApplied,
        this,
        [this]() {
            AppSettings::applyTheme();

            const bool toolsWereVisible =
                ui->toolsDock->isVisible();

            addDockWidget(
                preferredToolsDockArea(),
                ui->toolsDock
                );

            ui->toolsDock->setVisible(
                toolsWereVisible
                );

            if (m_toolbarShowToolsAction) {
                m_toolbarShowToolsAction->setText(
                    toolsToolbarText(
                        toolsWereVisible
                        )
                    );

                m_toolbarShowToolsAction->setToolTip(
                    toolsWereVisible
                        ? "Hide tools panel"
                        : "Show tools panel"
                    );
            }

            statusBar()->showMessage(
                "Preferences applied"
                );
        });

    dialog.exec();
}

void MainWindow::setToolsPanelVisible(bool visible)
{
    ui->toolsDock->setVisible(visible);

    if (visible) {
        ui->toolsDock->raise();
    }

    if (m_viewToolsPanelAction) {
        m_viewToolsPanelAction->blockSignals(true);
        m_viewToolsPanelAction->setChecked(visible);
        m_viewToolsPanelAction->blockSignals(false);
    }

    if (m_toolbarShowToolsAction) {
        m_toolbarShowToolsAction->setText(
                        toolsToolbarText(visible)
                        );
        m_toolbarShowToolsAction->setToolTip(
            visible ? "Hide tools panel" : "Show tools panel"
        );
    }

    statusBar()->showMessage(
        visible ? "Tools panel shown" : "Tools panel hidden"
    );
}

bool MainWindow::loadImageFile(const QString &filePath)
{
    ImageImport::ImportResult importResult =
        ImageImport::importImage(
            filePath
            );

    if (!importResult.success ||
        importResult.image.isNull()) {
        QMessageBox::warning(
            this,
            "Open Image Failed",
            "Could not open this image:\n\n" +
                importResult.errorMessage
            );

        return false;
    }

    QImage loadedImage =
        std::move(
            importResult.image
            );

    m_sourceInfo =
        std::move(
            importResult.sourceInfo
            );

    m_originalImage = loadedImage;
    m_adjustmentBaseImage = loadedImage;
    m_currentImage = loadedImage;

    m_currentFilePath =
        m_sourceInfo.sourceFilePath;

    m_lastSavePath.clear();
    updateOpenFileToolbar();

    m_hasUnsavedChanges = false;

    m_brightnessAmount = 0;
    m_contrastAmount = 0;
    m_gammaAmount = 0;

    m_baseHighlightAmount = 0;
    m_baseMidtoneAmount = 0;
    m_baseShadowAmount = 0;

    m_cyanRedHighlightAmount = 0;
    m_cyanRedMidtoneAmount = 0;
    m_cyanRedShadowAmount = 0;

    m_magentaGreenHighlightAmount = 0;
    m_magentaGreenMidtoneAmount = 0;
    m_magentaGreenShadowAmount = 0;

    m_yellowBlueHighlightAmount = 0;
    m_yellowBlueMidtoneAmount = 0;
    m_yellowBlueShadowAmount = 0;

    m_basicAdjustmentUndoCaptured = false;

    setBasicAdjustmentControls(0, 0, 0, 0);
    setBaseHmsControls(0, 0, 0);

    setCyanRedHmsControls(0, 0, 0);
    setMagentaGreenHmsControls(0, 0, 0);
    setYellowBlueHmsControls(0, 0, 0);

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    m_showOriginalTemporarily = false;
    m_sideBySideCompare = false;

    ui->actionCompare->blockSignals(true);
    ui->actionCompare->setChecked(false);
    ui->actionCompare->setEnabled(true);
    ui->actionCompare->blockSignals(false);

    ui->showOriginalButton->setEnabled(true);

    ui->imagePreviewLabel->unsetCursor();

    clearHistory();

    setQuickToolsEnabled(true);
    ui->actionPrintPreview->setEnabled(true);
    ui->actionPrint->setEnabled(true);
    resetCropControlsToImage();
    updatePreview();

    const QString formatName =
        ImageImport::formatDisplayName(
            m_sourceInfo.sourceFormat
            );

    statusBar()->showMessage(
        QString("Opened %1  —  %2 x %3  —  %4")
            .arg(
                m_sourceInfo.sourceFileName
                )
            .arg(
                m_currentImage.width()
                )
            .arg(
                m_currentImage.height()
                )
            .arg(
                formatName
                )
        );

    return true;
}

void MainWindow::showPrintPreviewDialog()
{
    openPrintLayoutDialog(false);
}

void MainWindow::showPrintDialog()
{
    openPrintLayoutDialog(true);
}

void MainWindow::openPrintLayoutDialog(bool focusPrintButton)
{
    if (m_currentImage.isNull()) {
        statusBar()->showMessage(
            "Open an image before printing"
            );
        return;
    }

    QString documentName =
        m_sourceInfo.sourceBaseName;

    if (documentName.trimmed().isEmpty()) {
        documentName =
            QFileInfo(m_currentFilePath).completeBaseName();
    }

    PrintLayoutDialog dialog(
        m_currentImage,
        m_sourceInfo.dotsPerMeterX,
        m_sourceInfo.dotsPerMeterY,
        documentName,
        this
        );

    if (focusPrintButton) {
        dialog.focusPrintButton();
    }

    dialog.exec();
}

void MainWindow::fitToWindow()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_fitToWindow = true;
    updatePreview();

    statusBar()->showMessage("Fit to window");
}

void MainWindow::actualSize()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_fitToWindow = false;
    m_zoomFactor = 1.0;
    updatePreview();

    statusBar()->showMessage("Actual size: 100%");
}

void MainWindow::zoomIn()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_fitToWindow = false;
    m_zoomFactor *= 1.25;

    if (m_zoomFactor > 8.0) {
        m_zoomFactor = 8.0;
    }

    updatePreview();

    statusBar()->showMessage(
        QString("Zoom: %1%").arg(static_cast<int>(m_zoomFactor * 100.0))
        );
}

void MainWindow::zoomOut()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_fitToWindow = false;
    m_zoomFactor /= 1.25;

    if (m_zoomFactor < 0.05) {
        m_zoomFactor = 0.05;
    }

    updatePreview();

    statusBar()->showMessage(
        QString("Zoom: %1%").arg(static_cast<int>(m_zoomFactor * 100.0))
        );
}

void MainWindow::showOriginalPressed()
{
    if (m_overexposureWorkspace) {
        if (m_overexposureEntryImage.isNull()) {
            return;
        }

        m_showOriginalTemporarily = true;
        m_overexposureWorkspace->setShowOriginal(true);
        statusBar()->showMessage("Showing image before Overexposure Recovery");
        return;
    }

    if (m_currentImage.isNull() || m_originalImage.isNull()) {
        return;
    }

    if (m_cropMode) {
        statusBar()->showMessage("Finish or cancel crop mode before showing original.");
        return;
    }

    m_showOriginalTemporarily = true;
    updatePreview();

    statusBar()->showMessage("Showing original");
}

void MainWindow::showOriginalReleased()
{
    if (m_overexposureWorkspace) {
        if (m_showOriginalTemporarily) {
            m_showOriginalTemporarily = false;
            m_overexposureWorkspace->setShowOriginal(false);
            statusBar()->showMessage("Showing Overexposure Recovery preview");
        }
        return;
    }

    if (m_showOriginalTemporarily) {
        m_showOriginalTemporarily = false;
        updatePreview();

        statusBar()->showMessage("Showing current image");
    }
}

void MainWindow::toggleSideBySideCompare(bool checked)
{
    if (m_currentImage.isNull() || m_originalImage.isNull()) {
        m_sideBySideCompare = false;

        ui->actionCompare->blockSignals(true);
        ui->actionCompare->setChecked(false);
        ui->actionCompare->blockSignals(false);

        return;
    }

    m_sideBySideCompare = checked;
    m_showOriginalTemporarily = false;

    if (m_sideBySideCompare) {
        // Compare mode should always start fitted so both images are visible.
        m_fitToWindow = true;
        m_zoomFactor = 1.0;
        m_cropMode = false;
        m_cropDragActive = false;
        m_cropDragMode = CropDragMode::None;
        m_cropSelectionVisible = false;
        m_cropRect = QRect();
        m_cropDragStartRect = QRect();

        ui->imagePreviewLabel->unsetCursor();

        statusBar()->showMessage("Compare: Original | Current");
    } else {
        statusBar()->showMessage("Compare off");
    }

    updatePreview();
}

void MainWindow::selectCropArea()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_cropMode = true;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = true;

    m_cropRect = QRect(0, 0, m_currentImage.width(), m_currentImage.height());
    setCropControlsFromRect(m_cropRect);

    ui->imagePreviewLabel->setCursor(Qt::CrossCursor);

    updatePreview();

    statusBar()->showMessage("Crop mode: click and drag on the image, then click Apply Crop.");
}

void MainWindow::applyCrop()
{
    if (m_currentImage.isNull()) {
        return;
    }

    const QRect imageRect(0, 0, m_currentImage.width(), m_currentImage.height());
    const QRect cropRect = currentCropRectFromControls();

    if (!imageRect.contains(cropRect) || cropRect.width() < 1 || cropRect.height() < 1) {
        QMessageBox::warning(
            this,
            "Invalid Crop Area",
            "The crop area goes outside the image.\n\n"
            "Adjust X, Y, W, and H so the crop rectangle stays inside the photo."
            );
        return;
    }

    pushUndoState("Crop");

    m_currentImage = m_adjustmentBaseImage.copy(cropRect);
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage(
        QString("Cropped image to %1 x %2")
            .arg(m_currentImage.width())
            .arg(m_currentImage.height())
        );
}

void MainWindow::cancelCrop()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage("Crop canceled");
}

void MainWindow::cropControlsChanged()
{
    if (m_currentImage.isNull()) {
        return;
    }

    m_cropRect = currentCropRectFromControls();
    m_cropSelectionVisible = true;

    updatePreview();
}

void MainWindow::resetCropControlsToImage()
{
    const bool hasImage = !m_currentImage.isNull();

    ui->selectCropButton->setEnabled(hasImage);
    ui->applyCropButton->setEnabled(hasImage);
    ui->cancelCropButton->setEnabled(hasImage);

    ui->cropXSpinBox->setEnabled(hasImage);
    ui->cropYSpinBox->setEnabled(hasImage);
    ui->cropWidthSpinBox->setEnabled(hasImage);
    ui->cropHeightSpinBox->setEnabled(hasImage);

    QSignalBlocker blockX(ui->cropXSpinBox);
    QSignalBlocker blockY(ui->cropYSpinBox);
    QSignalBlocker blockW(ui->cropWidthSpinBox);
    QSignalBlocker blockH(ui->cropHeightSpinBox);

    if (!hasImage) {
        ui->cropXSpinBox->setRange(0, 0);
        ui->cropYSpinBox->setRange(0, 0);
        ui->cropWidthSpinBox->setRange(1, 1);
        ui->cropHeightSpinBox->setRange(1, 1);

        ui->cropXSpinBox->setValue(0);
        ui->cropYSpinBox->setValue(0);
        ui->cropWidthSpinBox->setValue(1);
        ui->cropHeightSpinBox->setValue(1);
        return;
    }

    const int imageWidth = m_currentImage.width();
    const int imageHeight = m_currentImage.height();

    ui->cropXSpinBox->setRange(0, imageWidth > 0 ? imageWidth - 1 : 0);
    ui->cropYSpinBox->setRange(0, imageHeight > 0 ? imageHeight - 1 : 0);
    ui->cropWidthSpinBox->setRange(1, imageWidth);
    ui->cropHeightSpinBox->setRange(1, imageHeight);

    ui->cropXSpinBox->setValue(0);
    ui->cropYSpinBox->setValue(0);
    ui->cropWidthSpinBox->setValue(imageWidth);
    ui->cropHeightSpinBox->setValue(imageHeight);
}

void MainWindow::setCropControlsFromRect(const QRect &rect)
{
    QSignalBlocker blockX(ui->cropXSpinBox);
    QSignalBlocker blockY(ui->cropYSpinBox);
    QSignalBlocker blockW(ui->cropWidthSpinBox);
    QSignalBlocker blockH(ui->cropHeightSpinBox);

    ui->cropXSpinBox->setValue(rect.x());
    ui->cropYSpinBox->setValue(rect.y());
    ui->cropWidthSpinBox->setValue(rect.width());
    ui->cropHeightSpinBox->setValue(rect.height());
}

QRect MainWindow::currentCropRectFromControls() const
{
    return QRect(
        ui->cropXSpinBox->value(),
        ui->cropYSpinBox->value(),
        ui->cropWidthSpinBox->value(),
        ui->cropHeightSpinBox->value()
        );
}

bool MainWindow::labelPointToImagePoint(const QPoint &labelPoint, QPoint &imagePoint) const
{
    if (m_currentImage.isNull() || m_lastPreviewPixmapSize.isEmpty()) {
        return false;
    }

    const QSize labelSize = ui->imagePreviewLabel->size();

    const int pixmapX = (labelSize.width() - m_lastPreviewPixmapSize.width()) / 2;
    const int pixmapY = (labelSize.height() - m_lastPreviewPixmapSize.height()) / 2;

    const QRect pixmapRect(
        pixmapX,
        pixmapY,
        m_lastPreviewPixmapSize.width(),
        m_lastPreviewPixmapSize.height()
        );

    if (!pixmapRect.contains(labelPoint)) {
        return false;
    }

    const int localX = labelPoint.x() - pixmapRect.x();
    const int localY = labelPoint.y() - pixmapRect.y();

    const double scaleX = static_cast<double>(m_currentImage.width()) /
                          static_cast<double>(m_lastPreviewPixmapSize.width());

    const double scaleY = static_cast<double>(m_currentImage.height()) /
                          static_cast<double>(m_lastPreviewPixmapSize.height());

    int imageX = static_cast<int>(localX * scaleX);
    int imageY = static_cast<int>(localY * scaleY);

    imageX = std::clamp(imageX, 0, m_currentImage.width() - 1);
    imageY = std::clamp(imageY, 0, m_currentImage.height() - 1);

    imagePoint = QPoint(imageX, imageY);
    return true;
}

void MainWindow::setCropRectFromImagePoints(const QPoint &startPoint, const QPoint &endPoint)
{
    if (m_currentImage.isNull()) {
        return;
    }

    const int left = std::min(startPoint.x(), endPoint.x());
    const int top = std::min(startPoint.y(), endPoint.y());
    const int right = std::max(startPoint.x(), endPoint.x());
    const int bottom = std::max(startPoint.y(), endPoint.y());

    QRect rect(left, top, right - left + 1, bottom - top + 1);

    const QRect imageRect(0, 0, m_currentImage.width(), m_currentImage.height());
    rect = rect.intersected(imageRect);

    if (rect.width() < 1 || rect.height() < 1) {
        return;
    }

    m_cropRect = rect;
    m_cropSelectionVisible = true;

    setCropControlsFromRect(m_cropRect);
    updatePreview();
}

QRect MainWindow::scaledCropRectForPreview(const QSize &previewSize) const
{
    if (m_currentImage.isNull() || previewSize.isEmpty() || m_cropRect.isNull()) {
        return QRect();
    }

    const double scaleX = static_cast<double>(previewSize.width()) /
                          static_cast<double>(m_currentImage.width());

    const double scaleY = static_cast<double>(previewSize.height()) /
                          static_cast<double>(m_currentImage.height());

    return QRect(
        static_cast<int>(m_cropRect.x() * scaleX),
        static_cast<int>(m_cropRect.y() * scaleY),
        static_cast<int>(m_cropRect.width() * scaleX),
        static_cast<int>(m_cropRect.height() * scaleY)
        );
}

QRect MainWindow::cropRectInLabelCoordinates() const
{
    if (m_currentImage.isNull() || m_lastPreviewPixmapSize.isEmpty() || m_cropRect.isNull()) {
        return QRect();
    }

    const QRect previewCropRect = scaledCropRectForPreview(m_lastPreviewPixmapSize);
    const QSize labelSize = ui->imagePreviewLabel->size();

    const int pixmapX = (labelSize.width() - m_lastPreviewPixmapSize.width()) / 2;
    const int pixmapY = (labelSize.height() - m_lastPreviewPixmapSize.height()) / 2;

    return previewCropRect.translated(pixmapX, pixmapY);
}

MainWindow::CropDragMode MainWindow::hitTestCropHandle(const QPoint &labelPoint) const
{
    if (!m_cropSelectionVisible || m_cropRect.isNull()) {
        return CropDragMode::None;
    }

    const QRect rect = cropRectInLabelCoordinates();

    if (rect.isNull()) {
        return CropDragMode::None;
    }

    const int handleRange = 8;

    const QRect hitArea = rect.adjusted(
        -handleRange,
        -handleRange,
        handleRange,
        handleRange
        );

    if (!hitArea.contains(labelPoint)) {
        return CropDragMode::None;
    }

    const bool nearLeft = std::abs(labelPoint.x() - rect.left()) <= handleRange;
    const bool nearRight = std::abs(labelPoint.x() - rect.right()) <= handleRange;
    const bool nearTop = std::abs(labelPoint.y() - rect.top()) <= handleRange;
    const bool nearBottom = std::abs(labelPoint.y() - rect.bottom()) <= handleRange;

    if (nearLeft && nearTop) {
        return CropDragMode::TopLeft;
    }

    if (nearRight && nearTop) {
        return CropDragMode::TopRight;
    }

    if (nearLeft && nearBottom) {
        return CropDragMode::BottomLeft;
    }

    if (nearRight && nearBottom) {
        return CropDragMode::BottomRight;
    }

    if (nearLeft) {
        return CropDragMode::Left;
    }

    if (nearRight) {
        return CropDragMode::Right;
    }

    if (nearTop) {
        return CropDragMode::Top;
    }

    if (nearBottom) {
        return CropDragMode::Bottom;
    }

    if (rect.contains(labelPoint)) {
        return CropDragMode::Move;
    }

    return CropDragMode::None;
}

void MainWindow::updateCropCursor(const QPoint &labelPoint)
{
    if (!m_cropMode || m_currentImage.isNull()) {
        ui->imagePreviewLabel->unsetCursor();
        return;
    }

    const CropDragMode mode = hitTestCropHandle(labelPoint);

    switch (mode) {
    case CropDragMode::Left:
    case CropDragMode::Right:
        ui->imagePreviewLabel->setCursor(Qt::SizeHorCursor);
        break;

    case CropDragMode::Top:
    case CropDragMode::Bottom:
        ui->imagePreviewLabel->setCursor(Qt::SizeVerCursor);
        break;

    case CropDragMode::TopLeft:
    case CropDragMode::BottomRight:
        ui->imagePreviewLabel->setCursor(Qt::SizeFDiagCursor);
        break;

    case CropDragMode::TopRight:
    case CropDragMode::BottomLeft:
        ui->imagePreviewLabel->setCursor(Qt::SizeBDiagCursor);
        break;

    case CropDragMode::Move:
        ui->imagePreviewLabel->setCursor(Qt::SizeAllCursor);
        break;

    case CropDragMode::None:
    case CropDragMode::NewSelection:
    default:
        ui->imagePreviewLabel->setCursor(Qt::CrossCursor);
        break;
    }
}

QRect MainWindow::adjustedCropRectForDrag(const QPoint &currentImagePoint) const
{
    if (m_currentImage.isNull() || m_cropDragStartRect.isNull()) {
        return QRect();
    }

    QRect rect = m_cropDragStartRect;

    const QPoint delta = currentImagePoint - m_cropDragStartImagePoint;

    const int imageLeft = 0;
    const int imageTop = 0;
    const int imageRight = m_currentImage.width() - 1;
    const int imageBottom = m_currentImage.height() - 1;

    const int minWidth = 1;
    const int minHeight = 1;

    switch (m_cropDragMode) {
    case CropDragMode::Move:
    {
        rect.translate(delta);

        if (rect.left() < imageLeft) {
            rect.moveLeft(imageLeft);
        }

        if (rect.top() < imageTop) {
            rect.moveTop(imageTop);
        }

        if (rect.right() > imageRight) {
            rect.moveRight(imageRight);
        }

        if (rect.bottom() > imageBottom) {
            rect.moveBottom(imageBottom);
        }

        break;
    }

    case CropDragMode::Left:
        rect.setLeft(std::clamp(
            m_cropDragStartRect.left() + delta.x(),
            imageLeft,
            m_cropDragStartRect.right() - minWidth + 1
            ));
        break;

    case CropDragMode::Right:
        rect.setRight(std::clamp(
            m_cropDragStartRect.right() + delta.x(),
            m_cropDragStartRect.left() + minWidth - 1,
            imageRight
            ));
        break;

    case CropDragMode::Top:
        rect.setTop(std::clamp(
            m_cropDragStartRect.top() + delta.y(),
            imageTop,
            m_cropDragStartRect.bottom() - minHeight + 1
            ));
        break;

    case CropDragMode::Bottom:
        rect.setBottom(std::clamp(
            m_cropDragStartRect.bottom() + delta.y(),
            m_cropDragStartRect.top() + minHeight - 1,
            imageBottom
            ));
        break;

    case CropDragMode::TopLeft:
        rect.setLeft(std::clamp(
            m_cropDragStartRect.left() + delta.x(),
            imageLeft,
            m_cropDragStartRect.right() - minWidth + 1
            ));
        rect.setTop(std::clamp(
            m_cropDragStartRect.top() + delta.y(),
            imageTop,
            m_cropDragStartRect.bottom() - minHeight + 1
            ));
        break;

    case CropDragMode::TopRight:
        rect.setRight(std::clamp(
            m_cropDragStartRect.right() + delta.x(),
            m_cropDragStartRect.left() + minWidth - 1,
            imageRight
            ));
        rect.setTop(std::clamp(
            m_cropDragStartRect.top() + delta.y(),
            imageTop,
            m_cropDragStartRect.bottom() - minHeight + 1
            ));
        break;

    case CropDragMode::BottomLeft:
        rect.setLeft(std::clamp(
            m_cropDragStartRect.left() + delta.x(),
            imageLeft,
            m_cropDragStartRect.right() - minWidth + 1
            ));
        rect.setBottom(std::clamp(
            m_cropDragStartRect.bottom() + delta.y(),
            m_cropDragStartRect.top() + minHeight - 1,
            imageBottom
            ));
        break;

    case CropDragMode::BottomRight:
        rect.setRight(std::clamp(
            m_cropDragStartRect.right() + delta.x(),
            m_cropDragStartRect.left() + minWidth - 1,
            imageRight
            ));
        rect.setBottom(std::clamp(
            m_cropDragStartRect.bottom() + delta.y(),
            m_cropDragStartRect.top() + minHeight - 1,
            imageBottom
            ));
        break;

    case CropDragMode::None:
    case CropDragMode::NewSelection:
    default:
        break;
    }

    return rect.intersected(QRect(0, 0, m_currentImage.width(), m_currentImage.height()));
}

QImage MainWindow::makeSideBySideImage() const
{
    if (m_originalImage.isNull() || m_currentImage.isNull()) {
        return m_currentImage;
    }

    QImage leftImage = m_originalImage.convertToFormat(QImage::Format_ARGB32);
    QImage rightImage = m_currentImage.convertToFormat(QImage::Format_ARGB32);

    const int commonHeight = std::max(leftImage.height(), rightImage.height());

    if (leftImage.height() != commonHeight) {
        leftImage = leftImage.scaledToHeight(commonHeight, Qt::SmoothTransformation);
    }

    if (rightImage.height() != commonHeight) {
        rightImage = rightImage.scaledToHeight(commonHeight, Qt::SmoothTransformation);
    }

    const int labelHeight = 34;
    const int gap = 16;

    const int combinedWidth = leftImage.width() + gap + rightImage.width();
    const int combinedHeight = commonHeight + labelHeight;

    QImage combinedImage(combinedWidth, combinedHeight, QImage::Format_ARGB32);
    combinedImage.fill(QColor(32, 32, 32));

    QPainter painter(&combinedImage);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(QColor(235, 235, 235));

    painter.drawText(
        QRect(0, 0, leftImage.width(), labelHeight),
        Qt::AlignCenter,
        "Original"
        );

    painter.drawText(
        QRect(leftImage.width() + gap, 0, rightImage.width(), labelHeight),
        Qt::AlignCenter,
        "Current"
        );

    painter.drawImage(0, labelHeight, leftImage);
    painter.drawImage(leftImage.width() + gap, labelHeight, rightImage);

    QPen dividerPen(QColor(100, 100, 100));
    dividerPen.setWidth(1);
    painter.setPen(dividerPen);

    const int dividerX = leftImage.width() + gap / 2;
    painter.drawLine(dividerX, 0, dividerX, combinedHeight);

    return combinedImage;
}

void MainWindow::updatePreview()
{
    if (m_overexposureWorkspace) {
        updateOverexposureCanvasGeometry();
        return;
    }

    if (m_currentImage.isNull()) {
        const QSize viewportSize = ui->previewScrollArea->viewport()->size();

        ui->imagePreviewLabel->setText("Open an image to begin\n\nYou can also drag and drop an image here.");
        ui->imagePreviewLabel->setPixmap(QPixmap());
        ui->imagePreviewLabel->setFixedSize(viewportSize);

        ui->previewScrollAreaContents->resize(viewportSize);

        m_lastPreviewPixmapSize = QSize();
        return;
    }

    ui->imagePreviewLabel->setText(QString());

    const bool showingOriginalOnly =
        m_showOriginalTemporarily &&
        !m_originalImage.isNull();

    const bool showingSideBySide =
        m_sideBySideCompare &&
        !showingOriginalOnly &&
        !m_originalImage.isNull() &&
        !m_currentImage.isNull();

    const QImage previewSourceImage =
        showingSideBySide
            ? makeSideBySideImage()
            : (showingOriginalOnly ? m_originalImage : m_currentImage);

    QPixmap pixmap = QPixmap::fromImage(previewSourceImage);
    QPixmap displayPixmap;

    if (m_fitToWindow) {
        QSize availableSize = ui->previewScrollArea->viewport()->size();

        if (availableSize.width() > 20 && availableSize.height() > 20) {
            displayPixmap = pixmap.scaled(
                availableSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                );
        } else {
            displayPixmap = pixmap;
        }
    } else {
        QSize scaledSize(
            static_cast<int>(pixmap.width() * m_zoomFactor),
            static_cast<int>(pixmap.height() * m_zoomFactor)
            );

        displayPixmap = pixmap.scaled(
            scaledSize,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            );
    }

    m_lastPreviewPixmapSize = displayPixmap.size();

    if (!showingOriginalOnly && !showingSideBySide && m_cropSelectionVisible && !m_cropRect.isNull()) {
        QPainter painter(&displayPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRect previewCropRect = scaledCropRectForPreview(displayPixmap.size());

        painter.fillRect(previewCropRect, QColor(255, 255, 255, 35));

        QPen pen(Qt::yellow);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);

        painter.setPen(pen);
        painter.drawRect(previewCropRect.adjusted(0, 0, -1, -1));
        // Draw small crop handles so the user can see the rectangle is adjustable.
        const int handleSize = 8;
        const int halfHandle = handleSize / 2;

        auto drawHandle = [&](const QPoint &center) {
            QRect handleRect(
                center.x() - halfHandle,
                center.y() - halfHandle,
                handleSize,
                handleSize
                );

            painter.fillRect(handleRect, QColor(255, 255, 0, 190));

            QPen handlePen(Qt::black);
            handlePen.setWidth(1);
            painter.setPen(handlePen);
            painter.drawRect(handleRect.adjusted(0, 0, -1, -1));

            painter.setPen(pen);
        };

        const int left = previewCropRect.left();
        const int right = previewCropRect.right();
        const int top = previewCropRect.top();
        const int bottom = previewCropRect.bottom();
        const int centerX = previewCropRect.center().x();
        const int centerY = previewCropRect.center().y();

        drawHandle(QPoint(left, top));
        drawHandle(QPoint(centerX, top));
        drawHandle(QPoint(right, top));

        drawHandle(QPoint(left, centerY));
        drawHandle(QPoint(right, centerY));

        drawHandle(QPoint(left, bottom));
        drawHandle(QPoint(centerX, bottom));
        drawHandle(QPoint(right, bottom));
    }

    const QSize viewportSize = ui->previewScrollArea->viewport()->size();

    const int contentWidth = std::max(displayPixmap.width(), viewportSize.width());
    const int contentHeight = std::max(displayPixmap.height(), viewportSize.height());

    ui->imagePreviewLabel->setPixmap(displayPixmap);
    ui->imagePreviewLabel->setFixedSize(displayPixmap.size());

    ui->previewScrollAreaContents->resize(contentWidth, contentHeight);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_overexposureWorkspace) {
        closeOverexposureWorkspace();
    }

    if (confirmUnsavedChanges()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateOpenFileToolbar();

    if (m_overexposureWorkspace) {
        updateOverexposureCanvasGeometry();
        return;
    }

    if (m_fitToWindow && !m_currentImage.isNull()) {
        updatePreview();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Highlight the text label associated with a slider while the
    // pointer is over that slider.
    if ((event->type() == QEvent::Enter ||
         event->type() == QEvent::Leave)) {

        QSlider *slider =
            qobject_cast<QSlider *>(watched);

        if (slider) {
            const QString labelObjectName =
                slider->property(
                          "linkedLabelObjectName"
                          ).toString();

            if (!labelObjectName.isEmpty()) {
                QLabel *linkedLabel =
                    ui->toolsDock->findChild<QLabel *>(
                        labelObjectName
                        );

                if (linkedLabel) {
                    const bool hovered =
                        event->type() == QEvent::Enter;

                    linkedLabel->setProperty(
                        "sliderHover",
                        hovered
                        );

                    linkedLabel->style()->unpolish(
                        linkedLabel
                        );

                    linkedLabel->style()->polish(
                        linkedLabel
                        );

                    linkedLabel->update();
                }
            }
        }
    }

    // Adjustment controls must not react to accidental wheel movement.
    // Instead, use that wheel movement to scroll the containing panel.
    if (event->type() == QEvent::Wheel &&
        (qobject_cast<QSlider *>(watched) ||
         qobject_cast<QAbstractSpinBox *>(watched))) {

        QWidget *controlWidget =
            qobject_cast<QWidget *>(watched);

        QAbstractScrollArea *containingScrollArea = nullptr;

        for (QWidget *parentWidget =
             controlWidget
                 ? controlWidget->parentWidget()
                 : nullptr;
             parentWidget;
             parentWidget = parentWidget->parentWidget()) {

            containingScrollArea =
                qobject_cast<QAbstractScrollArea *>(
                    parentWidget
                    );

            if (containingScrollArea) {
                break;
            }
        }

        if (containingScrollArea) {
            QWheelEvent *wheelEvent =
                static_cast<QWheelEvent *>(event);

            QScrollBar *verticalBar =
                containingScrollArea->verticalScrollBar();

            int scrollAmount = 0;

            if (!wheelEvent->pixelDelta().isNull()) {
                // Smooth touchpad or high-resolution wheel movement.
                scrollAmount =
                    wheelEvent->pixelDelta().y();
            } else {
                // Traditional mouse-wheel movement.
                const int wheelSteps =
                    wheelEvent->angleDelta().y() / 120;

                scrollAmount =
                    wheelSteps *
                    verticalBar->singleStep() *
                    3;
            }

            if (scrollAmount != 0) {
                verticalBar->setValue(
                    verticalBar->value() - scrollAmount
                    );
            }
        }

        event->accept();
        return true;
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        QSlider *resettableSlider =
            qobject_cast<QSlider *>(watched);

        if (resettableSlider &&
            resettableSlider
                ->property("resetOnDoubleClick")
                .toBool()) {

            resettableSlider->setValue(0);

            const QString resetLabel =
                resettableSlider
                    ->property("resetLabel")
                    .toString();

            statusBar()->showMessage(
                QString("%1 reset to 0")
                    .arg(
                        resetLabel.isEmpty()
                            ? "Hue/Tone control"
                            : resetLabel
                        )
                );

            return true;
        }

        if (watched == ui->brightnessSlider) {
            ui->brightnessSlider->setValue(0);
            statusBar()->showMessage("Brightness reset to 0");
            return true;
        }

        if (watched == ui->contrastSlider) {
            ui->contrastSlider->setValue(0);
            statusBar()->showMessage("Contrast reset to 0");
            return true;
        }

        if (watched == ui->gammaSlider) {
            ui->gammaSlider->setValue(0);
            statusBar()->showMessage("Gamma reset to 0");
            return true;
        }

        if (watched == ui->baseHighlightSlider) {
            ui->baseHighlightSlider->setValue(0);
            statusBar()->showMessage("Base Highlight reset to 0");
            return true;
        }

        if (watched == ui->baseMidtoneSlider) {
            ui->baseMidtoneSlider->setValue(0);
            statusBar()->showMessage("Base Midtone reset to 0");
            return true;
        }

        if (watched == ui->baseShadowSlider) {
            ui->baseShadowSlider->setValue(0);
            statusBar()->showMessage("Base Shadow reset to 0");
            return true;
        }
        if (watched == ui->cyanRedHighlightSlider) {
            ui->cyanRedHighlightSlider->setValue(0);
            statusBar()->showMessage("Cyan / Red Highlight reset to 0");
            return true;
        }

        if (watched == ui->cyanRedMidtoneSlider) {
            ui->cyanRedMidtoneSlider->setValue(0);
            statusBar()->showMessage("Cyan / Red Midtone reset to 0");
            return true;
        }

        if (watched == ui->cyanRedShadowSlider) {
            ui->cyanRedShadowSlider->setValue(0);
            statusBar()->showMessage("Cyan / Red Shadow reset to 0");
            return true;
        }

        if (watched == ui->magentaGreenHighlightSlider) {
            ui->magentaGreenHighlightSlider->setValue(0);
            statusBar()->showMessage("Magenta / Green Highlight reset to 0");
            return true;
        }

        if (watched == ui->magentaGreenMidtoneSlider) {
            ui->magentaGreenMidtoneSlider->setValue(0);
            statusBar()->showMessage("Magenta / Green Midtone reset to 0");
            return true;
        }

        if (watched == ui->magentaGreenShadowSlider) {
            ui->magentaGreenShadowSlider->setValue(0);
            statusBar()->showMessage("Magenta / Green Shadow reset to 0");
            return true;
        }

        if (watched == ui->yellowBlueHighlightSlider) {
            ui->yellowBlueHighlightSlider->setValue(0);
            statusBar()->showMessage("Yellow / Blue Highlight reset to 0");
            return true;
        }

        if (watched == ui->yellowBlueMidtoneSlider) {
            ui->yellowBlueMidtoneSlider->setValue(0);
            statusBar()->showMessage("Yellow / Blue Midtone reset to 0");
            return true;
        }

        if (watched == ui->yellowBlueShadowSlider) {
            ui->yellowBlueShadowSlider->setValue(0);
            statusBar()->showMessage("Yellow / Blue Shadow reset to 0");
            return true;
        }
        if (watched == ui->saturationSlider) {
            ui->saturationSlider->setValue(0);
            statusBar()->showMessage("Saturation reset to 0");
            return true;
        }

    }
    if (watched == ui->imagePreviewLabel && m_cropMode && !m_currentImage.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            if (mouseEvent->button() == Qt::LeftButton) {
                QPoint imagePoint;

                if (labelPointToImagePoint(mouseEvent->pos(), imagePoint)) {
                    const CropDragMode hitMode = hitTestCropHandle(mouseEvent->pos());

                    m_cropDragActive = true;
                    m_cropDragStartImagePoint = imagePoint;
                    m_cropDragStartRect = m_cropRect;

                    if (hitMode != CropDragMode::None && m_cropSelectionVisible && !m_cropRect.isNull()) {
                        m_cropDragMode = hitMode;
                    } else {
                        m_cropDragMode = CropDragMode::NewSelection;
                        setCropRectFromImagePoints(imagePoint, imagePoint);
                    }

                    return true;
                }
            }
        }

        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            if (m_cropDragActive && (mouseEvent->buttons() & Qt::LeftButton)) {
                QPoint imagePoint;

                if (labelPointToImagePoint(mouseEvent->pos(), imagePoint)) {
                    if (m_cropDragMode == CropDragMode::NewSelection) {
                        setCropRectFromImagePoints(m_cropDragStartImagePoint, imagePoint);
                    } else {
                        const QRect adjustedRect = adjustedCropRectForDrag(imagePoint);

                        if (!adjustedRect.isNull()) {
                            m_cropRect = adjustedRect;
                            m_cropSelectionVisible = true;

                            setCropControlsFromRect(m_cropRect);
                            updatePreview();
                        }
                    }

                    return true;
                }
            }

            updateCropCursor(mouseEvent->pos());
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            if (mouseEvent->button() == Qt::LeftButton && m_cropDragActive) {
                QPoint imagePoint;

                if (labelPointToImagePoint(mouseEvent->pos(), imagePoint)) {
                    if (m_cropDragMode == CropDragMode::NewSelection) {
                        setCropRectFromImagePoints(m_cropDragStartImagePoint, imagePoint);
                    } else {
                        const QRect adjustedRect = adjustedCropRectForDrag(imagePoint);

                        if (!adjustedRect.isNull()) {
                            m_cropRect = adjustedRect;
                            m_cropSelectionVisible = true;

                            setCropControlsFromRect(m_cropRect);
                            updatePreview();
                        }
                    }
                }

                m_cropDragActive = false;
                m_cropDragMode = CropDragMode::None;
                m_cropDragStartRect = QRect();

                updateCropCursor(mouseEvent->pos());

                statusBar()->showMessage(
                    QString("Crop area: X %1, Y %2, W %3, H %4")
                        .arg(ui->cropXSpinBox->value())
                        .arg(ui->cropYSpinBox->value())
                        .arg(ui->cropWidthSpinBox->value())
                        .arg(ui->cropHeightSpinBox->value())
                    );

                return true;
            }
        }

        if (event->type() == QEvent::Leave) {
            if (!m_cropDragActive) {
                ui->imagePreviewLabel->setCursor(Qt::CrossCursor);
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();

    if (urls.isEmpty()) {
        event->ignore();
        return;
    }

    const QString filePath =
        urls.first().toLocalFile();

    if (ImageImport::canImportFile(
            filePath
            )) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();

    if (urls.isEmpty()) {
        event->ignore();
        return;
    }

    const QString filePath = urls.first().toLocalFile();

    if (filePath.isEmpty()) {
        event->ignore();
        return;
    }

    if (!ImageImport::canImportFile(
            filePath
            )) {
        event->ignore();
        return;
    }

    if (!confirmUnsavedChanges()) {
        event->ignore();
        return;
    }

    if (loadImageFile(
            filePath
            )) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::rotateLeft()
{
    if (m_currentImage.isNull()) {
        return;
    }

    pushUndoState("Rotate Left");

    QTransform transform;
    transform.rotate(-90);

    m_currentImage = m_adjustmentBaseImage.transformed(transform, Qt::SmoothTransformation);
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage("Rotated left 90°");
}

void MainWindow::rotateRight()
{
    if (m_currentImage.isNull()) {
        return;
    }

    pushUndoState("Rotate Right");

    QTransform transform;
    transform.rotate(90);

    m_currentImage = m_adjustmentBaseImage.transformed(transform, Qt::SmoothTransformation);
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage("Rotated right 90°");
}

void MainWindow::flipHorizontal()
{
    if (m_currentImage.isNull()) {
        return;
    }

    pushUndoState("Flip Horizontal");

    m_currentImage = m_adjustmentBaseImage.mirrored(true, false);
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage("Flipped horizontal");
}

void MainWindow::flipVertical()
{
    if (m_currentImage.isNull()) {
        return;
    }

    pushUndoState("Flip Vertical");

    m_currentImage = m_adjustmentBaseImage.mirrored(false, true);
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage("Flipped vertical");
}

void MainWindow::saturationControlsChanged(int value)
{
    QSignalBlocker sliderBlocker(ui->saturationSlider);
    QSignalBlocker spinBoxBlocker(ui->saturationSpinBox);

    ui->saturationSlider->setValue(value);
    ui->saturationSpinBox->setValue(value);

    if (m_saturationAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured && !m_currentImage.isNull()) {
        pushUndoState("Basic Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_saturationAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Saturation: %1").arg(value)
        );
}

void MainWindow::openResizeDialog()
{
    if (m_currentImage.isNull()) {
        return;
    }

    const int currentWidth = m_currentImage.width();
    const int currentHeight = m_currentImage.height();

    QDialog dialog(this);
    dialog.setWindowTitle("Resize Image");
    dialog.setModal(true);
    dialog.resize(460, 320);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // Current size display
    QGroupBox *currentSizeGroup = new QGroupBox("Current Size");
    QGridLayout *currentSizeLayout = new QGridLayout(currentSizeGroup);

    QLabel *currentWidthValue = new QLabel(QString::number(currentWidth));
    currentWidthValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    currentWidthValue->setMinimumWidth(120);

    QLabel *currentHeightValue = new QLabel(QString::number(currentHeight));
    currentHeightValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    currentHeightValue->setMinimumWidth(120);

    QFont currentSizeFont = currentWidthValue->font();
    currentSizeFont.setBold(true);

    currentWidthValue->setFont(currentSizeFont);
    currentHeightValue->setFont(currentSizeFont);

    currentSizeLayout->addWidget(new QLabel("Width"), 0, 0);
    currentSizeLayout->addWidget(currentWidthValue, 0, 1);
    currentSizeLayout->addWidget(new QLabel("Height"), 1, 0);
    currentSizeLayout->addWidget(currentHeightValue, 1, 1);

    mainLayout->addWidget(currentSizeGroup);

    // New size controls
    QGroupBox *newSizeGroup = new QGroupBox("New Size");
    QGridLayout *newSizeLayout = new QGridLayout(newSizeGroup);

    QSpinBox *newWidthBox = new QSpinBox();
    newWidthBox->setRange(1, 99999);
    newWidthBox->setValue(currentWidth);

    QSpinBox *newHeightBox = new QSpinBox();
    newHeightBox->setRange(1, 99999);
    newHeightBox->setValue(currentHeight);

    QCheckBox *lockAspectCheckBox = new QCheckBox("Lock aspect ratio");
    lockAspectCheckBox->setChecked(true);

    newSizeLayout->addWidget(new QLabel("Width"), 0, 0);
    newSizeLayout->addWidget(newWidthBox, 0, 1);
    newSizeLayout->addWidget(new QLabel("Height"), 1, 0);
    newSizeLayout->addWidget(newHeightBox, 1, 1);
    newSizeLayout->addWidget(lockAspectCheckBox, 2, 0, 1, 2);

    mainLayout->addWidget(newSizeGroup);

    const double aspectRatio =
        static_cast<double>(currentWidth) / static_cast<double>(currentHeight);

    bool updatingSizeBoxes = false;

    connect(newWidthBox, QOverload<int>::of(&QSpinBox::valueChanged),
            &dialog,
            [newHeightBox, lockAspectCheckBox, aspectRatio, &updatingSizeBoxes](int width) {
                if (updatingSizeBoxes || !lockAspectCheckBox->isChecked()) {
                    return;
                }

                updatingSizeBoxes = true;
                newHeightBox->setValue(
                    std::max(1, static_cast<int>(std::round(width / aspectRatio)))
                    );
                updatingSizeBoxes = false;
            });

    connect(newHeightBox, QOverload<int>::of(&QSpinBox::valueChanged),
            &dialog,
            [newWidthBox, lockAspectCheckBox, aspectRatio, &updatingSizeBoxes](int height) {
                if (updatingSizeBoxes || !lockAspectCheckBox->isChecked()) {
                    return;
                }

                updatingSizeBoxes = true;
                newWidthBox->setValue(
                    std::max(1, static_cast<int>(std::round(height * aspectRatio)))
                    );
                updatingSizeBoxes = false;
            });

    // Quick scale buttons
    QGroupBox *quickScaleGroup = new QGroupBox("Quick Scale");
    QHBoxLayout *quickScaleLayout = new QHBoxLayout(quickScaleGroup);

    auto addScaleButton = [&](const QString &text, double factor) {
        QPushButton *button = new QPushButton(text);
        quickScaleLayout->addWidget(button);

        connect(button, &QPushButton::clicked,
                &dialog,
                [=, &updatingSizeBoxes]() {
                    const int scaledWidth =
                        std::max(1, static_cast<int>(std::round(currentWidth * factor)));

                    const int scaledHeight =
                        std::max(1, static_cast<int>(std::round(currentHeight * factor)));

                    updatingSizeBoxes = true;
                    newWidthBox->setValue(scaledWidth);
                    newHeightBox->setValue(scaledHeight);
                    updatingSizeBoxes = false;
                });
    };

    addScaleButton("½x", 0.5);
    addScaleButton("1x", 1.0);
    addScaleButton("2x", 2.0);
    addScaleButton("3x", 3.0);
    addScaleButton("5x", 5.0);

    mainLayout->addWidget(quickScaleGroup);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok
        );

    buttonBox->button(QDialogButtonBox::Ok)->setText("Resize");

    connect(buttonBox, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);

    connect(buttonBox, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const int newWidth = newWidthBox->value();
    const int newHeight = newHeightBox->value();

    if (newWidth == currentWidth && newHeight == currentHeight) {
        statusBar()->showMessage("Resize canceled: image already matches that size");
        return;
    }

    pushUndoState(
        QString("Resize to %1 x %2")
            .arg(newWidth)
            .arg(newHeight)
        );

    m_currentImage = m_adjustmentBaseImage.scaled(
        newWidth,
        newHeight,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
        );

    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;
    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();

    statusBar()->showMessage(
        QString("Resized image to %1 x %2")
            .arg(newWidth)
            .arg(newHeight)
        );
}

void MainWindow::brightnessControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_brightnessAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Basic Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_brightnessAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Brightness: %1").arg(m_brightnessAmount)
        );
}

void MainWindow::contrastControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_contrastAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Basic Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_contrastAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Contrast: %1").arg(m_contrastAmount)
        );
}

void MainWindow::gammaControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_gammaAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Basic Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_gammaAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Gamma: %1").arg(m_gammaAmount)
        );
}

void MainWindow::baseHighlightControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_baseHighlightAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Tone Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_baseHighlightAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Base Highlight: %1").arg(m_baseHighlightAmount)
        );
}

void MainWindow::baseMidtoneControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_baseMidtoneAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Tone Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_baseMidtoneAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Base Midtone: %1").arg(m_baseMidtoneAmount)
        );
}

void MainWindow::baseShadowControlsChanged(int value)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_baseShadowAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Tone Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_baseShadowAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("Base Shadow: %1").arg(m_baseShadowAmount)
        );
}

void MainWindow::resetBaseHmsAdjustments()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_baseHighlightAmount == 0 &&
        m_baseMidtoneAmount == 0 &&
        m_baseShadowAmount == 0) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Reset Base Tone");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_baseHighlightAmount = 0;
    m_baseMidtoneAmount = 0;
    m_baseShadowAmount = 0;

    m_cyanRedHighlightAmount = 0;
    m_cyanRedMidtoneAmount = 0;
    m_cyanRedShadowAmount = 0;

    m_magentaGreenHighlightAmount = 0;
    m_magentaGreenMidtoneAmount = 0;
    m_magentaGreenShadowAmount = 0;

    m_yellowBlueHighlightAmount = 0;
    m_yellowBlueMidtoneAmount = 0;
    m_yellowBlueShadowAmount = 0;

    setBaseHmsControls(0, 0, 0);

    setCyanRedHmsControls(0, 0, 0);
    setMagentaGreenHmsControls(0, 0, 0);
    setYellowBlueHmsControls(0, 0, 0);

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Base tone reset");
}

void MainWindow::hmsColorControlChanged(int &storedAmount, int value, const QString &label)
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (storedAmount == value) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Color HMS Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    storedAmount = value;

    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage(
        QString("%1: %2").arg(label).arg(value)
        );
}

void MainWindow::setCyanRedHmsControls(int highlight, int midtone, int shadow)
{
    QSignalBlocker blockHighlightSlider(ui->cyanRedHighlightSlider);
    QSignalBlocker blockHighlightSpinBox(ui->cyanRedHighlightSpinBox);
    QSignalBlocker blockMidtoneSlider(ui->cyanRedMidtoneSlider);
    QSignalBlocker blockMidtoneSpinBox(ui->cyanRedMidtoneSpinBox);
    QSignalBlocker blockShadowSlider(ui->cyanRedShadowSlider);
    QSignalBlocker blockShadowSpinBox(ui->cyanRedShadowSpinBox);

    ui->cyanRedHighlightSlider->setValue(highlight);
    ui->cyanRedHighlightSpinBox->setValue(highlight);

    ui->cyanRedMidtoneSlider->setValue(midtone);
    ui->cyanRedMidtoneSpinBox->setValue(midtone);

    ui->cyanRedShadowSlider->setValue(shadow);
    ui->cyanRedShadowSpinBox->setValue(shadow);
}

void MainWindow::setMagentaGreenHmsControls(int highlight, int midtone, int shadow)
{
    QSignalBlocker blockHighlightSlider(ui->magentaGreenHighlightSlider);
    QSignalBlocker blockHighlightSpinBox(ui->magentaGreenHighlightSpinBox);
    QSignalBlocker blockMidtoneSlider(ui->magentaGreenMidtoneSlider);
    QSignalBlocker blockMidtoneSpinBox(ui->magentaGreenMidtoneSpinBox);
    QSignalBlocker blockShadowSlider(ui->magentaGreenShadowSlider);
    QSignalBlocker blockShadowSpinBox(ui->magentaGreenShadowSpinBox);

    ui->magentaGreenHighlightSlider->setValue(highlight);
    ui->magentaGreenHighlightSpinBox->setValue(highlight);

    ui->magentaGreenMidtoneSlider->setValue(midtone);
    ui->magentaGreenMidtoneSpinBox->setValue(midtone);

    ui->magentaGreenShadowSlider->setValue(shadow);
    ui->magentaGreenShadowSpinBox->setValue(shadow);
}

void MainWindow::setYellowBlueHmsControls(int highlight, int midtone, int shadow)
{
    QSignalBlocker blockHighlightSlider(ui->yellowBlueHighlightSlider);
    QSignalBlocker blockHighlightSpinBox(ui->yellowBlueHighlightSpinBox);
    QSignalBlocker blockMidtoneSlider(ui->yellowBlueMidtoneSlider);
    QSignalBlocker blockMidtoneSpinBox(ui->yellowBlueMidtoneSpinBox);
    QSignalBlocker blockShadowSlider(ui->yellowBlueShadowSlider);
    QSignalBlocker blockShadowSpinBox(ui->yellowBlueShadowSpinBox);

    ui->yellowBlueHighlightSlider->setValue(highlight);
    ui->yellowBlueHighlightSpinBox->setValue(highlight);

    ui->yellowBlueMidtoneSlider->setValue(midtone);
    ui->yellowBlueMidtoneSpinBox->setValue(midtone);

    ui->yellowBlueShadowSlider->setValue(shadow);
    ui->yellowBlueShadowSpinBox->setValue(shadow);
}

void MainWindow::resetCyanRedHmsAdjustments()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_cyanRedHighlightAmount == 0 &&
        m_cyanRedMidtoneAmount == 0 &&
        m_cyanRedShadowAmount == 0) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Reset Cyan / Red HMS");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_cyanRedHighlightAmount = 0;
    m_cyanRedMidtoneAmount = 0;
    m_cyanRedShadowAmount = 0;

    setCyanRedHmsControls(0, 0, 0);
    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Cyan / Red reset");
}

void MainWindow::resetMagentaGreenHmsAdjustments()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_magentaGreenHighlightAmount == 0 &&
        m_magentaGreenMidtoneAmount == 0 &&
        m_magentaGreenShadowAmount == 0) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Reset Magenta / Green HMS");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_magentaGreenHighlightAmount = 0;
    m_magentaGreenMidtoneAmount = 0;
    m_magentaGreenShadowAmount = 0;

    setMagentaGreenHmsControls(0, 0, 0);
    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Magenta / Green reset");
}

void MainWindow::resetYellowBlueHmsAdjustments()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_yellowBlueHighlightAmount == 0 &&
        m_yellowBlueMidtoneAmount == 0 &&
        m_yellowBlueShadowAmount == 0) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Reset Yellow / Blue HMS");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_yellowBlueHighlightAmount = 0;
    m_yellowBlueMidtoneAmount = 0;
    m_yellowBlueShadowAmount = 0;

    setYellowBlueHmsControls(0, 0, 0);
    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Yellow / Blue reset");
}

void MainWindow::resetBasicAdjustments()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_brightnessAmount == 0 &&
        m_contrastAmount == 0 &&
        m_gammaAmount == 0 &&
        m_saturationAmount == 0) {
        return;
    }

    if (!m_basicAdjustmentUndoCaptured) {
        pushUndoState("Reset Basic Adjustments");
        m_basicAdjustmentUndoCaptured = true;
    }

    m_brightnessAmount = 0;
    m_contrastAmount = 0;
    m_gammaAmount = 0;
    m_saturationAmount = 0;

    setBasicAdjustmentControls(0, 0, 0, 0);
    applyBasicAdjustments();

    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Basic adjustments reset");
}

QImage MainWindow::buildAdjustedImage(const QImage &baseImage) const
{
    if (baseImage.isNull()) {
        return QImage();
    }

    // GPU Stage 2 fuses the regular correction stack into one image pass:
    // Basic Tone -> Base HMS -> Color HMS -> Saturation. The optimized CPU
    // engine preserves the same established order whenever GPU processing is
    // unavailable or not worthwhile for the current image size.
    return ImageTools::applyMainAdjustments(
        baseImage,
        m_brightnessAmount,
        m_contrastAmount,
        m_gammaAmount,
        m_saturationAmount,

        m_baseHighlightAmount,
        m_baseMidtoneAmount,
        m_baseShadowAmount,

        m_cyanRedHighlightAmount,
        m_cyanRedMidtoneAmount,
        m_cyanRedShadowAmount,

        m_magentaGreenHighlightAmount,
        m_magentaGreenMidtoneAmount,
        m_magentaGreenShadowAmount,

        m_yellowBlueHighlightAmount,
        m_yellowBlueMidtoneAmount,
        m_yellowBlueShadowAmount
        );
}

void MainWindow::applyBasicAdjustments()
{
    m_currentImage = buildAdjustedImage(m_adjustmentBaseImage);
    updatePreview();
}

void MainWindow::setBasicAdjustmentControls(int brightness, int contrast, int gamma, int saturation)
{
    QSignalBlocker brightnessSliderBlocker(ui->brightnessSlider);
    QSignalBlocker brightnessSpinBoxBlocker(ui->brightnessSpinBox);

    QSignalBlocker contrastSliderBlocker(ui->contrastSlider);
    QSignalBlocker contrastSpinBoxBlocker(ui->contrastSpinBox);

    QSignalBlocker gammaSliderBlocker(ui->gammaSlider);
    QSignalBlocker gammaSpinBoxBlocker(ui->gammaSpinBox);

    QSignalBlocker saturationSliderBlocker(ui->saturationSlider);
    QSignalBlocker saturationSpinBoxBlocker(ui->saturationSpinBox);

    ui->brightnessSlider->setValue(brightness);
    ui->brightnessSpinBox->setValue(brightness);

    ui->contrastSlider->setValue(contrast);
    ui->contrastSpinBox->setValue(contrast);

    ui->gammaSlider->setValue(gamma);
    ui->gammaSpinBox->setValue(gamma);

    ui->saturationSlider->setValue(saturation);
    ui->saturationSpinBox->setValue(saturation);

    m_brightnessAmount = brightness;
    m_contrastAmount = contrast;
    m_gammaAmount = gamma;
    m_saturationAmount = saturation;
}

void MainWindow::setBaseHmsControls(int highlight, int midtone, int shadow)
{
    QSignalBlocker blockHighlightSlider(ui->baseHighlightSlider);
    QSignalBlocker blockHighlightSpinBox(ui->baseHighlightSpinBox);
    QSignalBlocker blockMidtoneSlider(ui->baseMidtoneSlider);
    QSignalBlocker blockMidtoneSpinBox(ui->baseMidtoneSpinBox);
    QSignalBlocker blockShadowSlider(ui->baseShadowSlider);
    QSignalBlocker blockShadowSpinBox(ui->baseShadowSpinBox);

    ui->baseHighlightSlider->setValue(highlight);
    ui->baseHighlightSpinBox->setValue(highlight);

    ui->baseMidtoneSlider->setValue(midtone);
    ui->baseMidtoneSpinBox->setValue(midtone);

    ui->baseShadowSlider->setValue(shadow);
    ui->baseShadowSpinBox->setValue(shadow);
}

void MainWindow::commitCurrentImageAsAdjustmentBase()
{
    if (m_currentImage.isNull()) {
        m_adjustmentBaseImage = QImage();
        return;
    }

    // m_currentImage must contain the newly edited BASE image here.
    // Keep all live Basic, Saturation, and HMS values intact, then rebuild
    // the displayed working image from that new base.
    m_adjustmentBaseImage = m_currentImage;
    m_basicAdjustmentUndoCaptured = false;
    m_currentImage = buildAdjustedImage(m_adjustmentBaseImage);
}

void MainWindow::pushUndoState(const QString &label)
{
    if (m_currentImage.isNull()) {
        return;
    }

    ImageState state;
    state.originalImage = m_originalImage;
    state.adjustmentBaseImage = m_adjustmentBaseImage;
    state.currentImage = m_currentImage;

    state.brightnessAmount = m_brightnessAmount;
    state.contrastAmount = m_contrastAmount;
    state.gammaAmount = m_gammaAmount;
    state.saturationAmount = m_saturationAmount;

    state.baseHighlightAmount = m_baseHighlightAmount;
    state.baseMidtoneAmount = m_baseMidtoneAmount;
    state.baseShadowAmount = m_baseShadowAmount;

    state.cyanRedHighlightAmount = m_cyanRedHighlightAmount;
    state.cyanRedMidtoneAmount = m_cyanRedMidtoneAmount;
    state.cyanRedShadowAmount = m_cyanRedShadowAmount;

    state.magentaGreenHighlightAmount = m_magentaGreenHighlightAmount;
    state.magentaGreenMidtoneAmount = m_magentaGreenMidtoneAmount;
    state.magentaGreenShadowAmount = m_magentaGreenShadowAmount;

    state.yellowBlueHighlightAmount = m_yellowBlueHighlightAmount;
    state.yellowBlueMidtoneAmount = m_yellowBlueMidtoneAmount;
    state.yellowBlueShadowAmount = m_yellowBlueShadowAmount;

    state.filePath = m_currentFilePath;
    state.label = label;

    m_undoStack.push_back(state);

    while (m_undoStack.size() > m_maxUndoStates) {
        m_undoStack.removeAt(0);
        m_historyWasTrimmed = true;
    }

    m_redoStack.clear();

    updateUndoRedoActions();
}

void MainWindow::restoreImageState(const ImageState &state)
{
    m_originalImage = state.originalImage;
    m_adjustmentBaseImage = state.adjustmentBaseImage;
    m_currentImage = state.currentImage;

    m_brightnessAmount = state.brightnessAmount;
    m_contrastAmount = state.contrastAmount;
    m_gammaAmount = state.gammaAmount;
    m_saturationAmount = state.saturationAmount;

    m_baseHighlightAmount = state.baseHighlightAmount;
    m_baseMidtoneAmount = state.baseMidtoneAmount;
    m_baseShadowAmount = state.baseShadowAmount;

    m_cyanRedHighlightAmount = state.cyanRedHighlightAmount;
    m_cyanRedMidtoneAmount = state.cyanRedMidtoneAmount;
    m_cyanRedShadowAmount = state.cyanRedShadowAmount;

    m_magentaGreenHighlightAmount = state.magentaGreenHighlightAmount;
    m_magentaGreenMidtoneAmount = state.magentaGreenMidtoneAmount;
    m_magentaGreenShadowAmount = state.magentaGreenShadowAmount;

    m_yellowBlueHighlightAmount = state.yellowBlueHighlightAmount;
    m_yellowBlueMidtoneAmount = state.yellowBlueMidtoneAmount;
    m_yellowBlueShadowAmount = state.yellowBlueShadowAmount;

    m_basicAdjustmentUndoCaptured = false;

    m_currentFilePath = state.filePath;
    updateOpenFileToolbar();

    setBasicAdjustmentControls(
        m_brightnessAmount,
        m_contrastAmount,
        m_gammaAmount,
        m_saturationAmount
        );

    setBaseHmsControls(
        m_baseHighlightAmount,
        m_baseMidtoneAmount,
        m_baseShadowAmount
        );

    setCyanRedHmsControls(
        m_cyanRedHighlightAmount,
        m_cyanRedMidtoneAmount,
        m_cyanRedShadowAmount
        );

    setMagentaGreenHmsControls(
        m_magentaGreenHighlightAmount,
        m_magentaGreenMidtoneAmount,
        m_magentaGreenShadowAmount
        );

    setYellowBlueHmsControls(
        m_yellowBlueHighlightAmount,
        m_yellowBlueMidtoneAmount,
        m_yellowBlueShadowAmount
        );

    m_fitToWindow = true;
    m_zoomFactor = 1.0;

    m_cropMode = false;
    m_cropDragActive = false;
    m_cropDragMode = CropDragMode::None;
    m_cropSelectionVisible = false;
    m_cropRect = QRect();

    ui->imagePreviewLabel->unsetCursor();

    resetCropControlsToImage();
    updatePreview();
    updateUndoRedoActions();
}

void MainWindow::updateUndoRedoActions()
{
    ui->actionUndo->setEnabled(
        !m_undoStack.isEmpty()
        );

    ui->actionRedo->setEnabled(
        !m_redoStack.isEmpty()
        );

    updateHistoryMenu();
}

void MainWindow::clearHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
    m_historyWasTrimmed = false;

    updateUndoRedoActions();
}

void MainWindow::undoEdit()
{
    if (m_undoStack.isEmpty() || m_currentImage.isNull()) {
        return;
    }

    ImageState restoreState = m_undoStack.takeLast();

    ImageState redoState;
    redoState.originalImage = m_originalImage;
    redoState.adjustmentBaseImage = m_adjustmentBaseImage;
    redoState.currentImage = m_currentImage;

    redoState.brightnessAmount = m_brightnessAmount;
    redoState.contrastAmount = m_contrastAmount;
    redoState.gammaAmount = m_gammaAmount;
    redoState.saturationAmount = m_saturationAmount;

    redoState.baseHighlightAmount = m_baseHighlightAmount;
    redoState.baseMidtoneAmount = m_baseMidtoneAmount;
    redoState.baseShadowAmount = m_baseShadowAmount;

    redoState.cyanRedHighlightAmount = m_cyanRedHighlightAmount;
    redoState.cyanRedMidtoneAmount = m_cyanRedMidtoneAmount;
    redoState.cyanRedShadowAmount = m_cyanRedShadowAmount;

    redoState.magentaGreenHighlightAmount = m_magentaGreenHighlightAmount;
    redoState.magentaGreenMidtoneAmount = m_magentaGreenMidtoneAmount;
    redoState.magentaGreenShadowAmount = m_magentaGreenShadowAmount;

    redoState.yellowBlueHighlightAmount = m_yellowBlueHighlightAmount;
    redoState.yellowBlueMidtoneAmount = m_yellowBlueMidtoneAmount;
    redoState.yellowBlueShadowAmount = m_yellowBlueShadowAmount;

    redoState.filePath = m_currentFilePath;
    redoState.label = restoreState.label;

    m_redoStack.push_back(redoState);

    restoreImageState(restoreState);
    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Undo: " + restoreState.label);
}

void MainWindow::redoEdit()
{
    if (m_redoStack.isEmpty() || m_currentImage.isNull()) {
        return;
    }

    ImageState restoreState = m_redoStack.takeLast();

    ImageState undoState;
    undoState.originalImage = m_originalImage;
    undoState.adjustmentBaseImage = m_adjustmentBaseImage;
    undoState.currentImage = m_currentImage;

    undoState.brightnessAmount = m_brightnessAmount;
    undoState.contrastAmount = m_contrastAmount;
    undoState.gammaAmount = m_gammaAmount;
    undoState.saturationAmount = m_saturationAmount;

    undoState.baseHighlightAmount = m_baseHighlightAmount;
    undoState.baseMidtoneAmount = m_baseMidtoneAmount;
    undoState.baseShadowAmount = m_baseShadowAmount;

    undoState.cyanRedHighlightAmount = m_cyanRedHighlightAmount;
    undoState.cyanRedMidtoneAmount = m_cyanRedMidtoneAmount;
    undoState.cyanRedShadowAmount = m_cyanRedShadowAmount;

    undoState.magentaGreenHighlightAmount = m_magentaGreenHighlightAmount;
    undoState.magentaGreenMidtoneAmount = m_magentaGreenMidtoneAmount;
    undoState.magentaGreenShadowAmount = m_magentaGreenShadowAmount;

    undoState.yellowBlueHighlightAmount = m_yellowBlueHighlightAmount;
    undoState.yellowBlueMidtoneAmount = m_yellowBlueMidtoneAmount;
    undoState.yellowBlueShadowAmount = m_yellowBlueShadowAmount;

    undoState.filePath = m_currentFilePath;
    undoState.label = restoreState.label;

    m_undoStack.push_back(undoState);

    restoreImageState(restoreState);
    m_hasUnsavedChanges = true;

    statusBar()->showMessage("Redo: " + restoreState.label);
}

QStringList MainWindow::historyActionLabels() const
{
    QStringList labels;

    labels.append(
        m_historyWasTrimmed
            ? "Earlier State"
            : "Opened Image"
        );

    for (const ImageState &state : m_undoStack) {
        labels.append(
            state.label
            );
    }

    for (int index =
             m_redoStack.size() - 1;
         index >= 0;
         --index) {
        labels.append(
            m_redoStack.at(index).label
            );
    }

    return labels;
}

void MainWindow::setupHistoryMenu()
{
    if (m_historyMenu ||
        !ui->menuEdit) {
        return;
    }

    m_historyMenu =
        new QMenu(
            "History",
            ui->menuEdit
            );

    m_historyMenu->setObjectName(
        "historyMenu"
        );

    // Keep the Edit menu in the requested order:
    // History >, then Undo, then Redo.
    ui->menuEdit->insertMenu(
        ui->actionUndo,
        m_historyMenu
        );

    updateHistoryMenu();
}

void MainWindow::updateHistoryMenu()
{
    if (!m_historyMenu) {
        return;
    }

    m_historyMenu->clear();

    const bool hasDocument =
        !m_currentImage.isNull();

    m_historyMenu->setEnabled(
        hasDocument
        );

    if (!hasDocument) {
        QAction *emptyAction =
            m_historyMenu->addAction(
                "No image open"
                );

        emptyAction->setEnabled(
            false
            );

        return;
    }

    const QStringList labels =
        historyActionLabels();

    const int currentIndex =
        m_undoStack.size();

    // Show newest-to-oldest so the most recent action appears first
    // when the History submenu opens.
    for (int index = labels.size() - 1;
         index >= 0;
         --index) {
        QAction *historyAction =
            m_historyMenu->addAction(
                labels.at(index)
                );

        historyAction->setCheckable(
            true
            );

        historyAction->setChecked(
            index == currentIndex
            );

        QFont actionFont =
            historyAction->font();

        actionFont.setBold(
            index == currentIndex
            );

        // Entries below the current state are available through Redo.
        actionFont.setItalic(
            index > currentIndex
            );

        historyAction->setFont(
            actionFont
            );

        connect(
            historyAction,
            &QAction::triggered,
            this,
            [this, index]() {
                goToHistoryIndex(
                    index
                    );
            });
    }
}

void MainWindow::goToHistoryIndex(
    int targetIndex
    )
{
    if (m_currentImage.isNull()) {
        return;
    }

    const int totalHistoryStates =
        1 +
        m_undoStack.size() +
        m_redoStack.size();

    if (targetIndex < 0 ||
        targetIndex >= totalHistoryStates) {
        updateHistoryMenu();
        return;
    }

    int currentIndex =
        m_undoStack.size();

    while (currentIndex > targetIndex &&
           !m_undoStack.isEmpty()) {
        undoEdit();
        currentIndex =
            m_undoStack.size();
    }

    while (currentIndex < targetIndex &&
           !m_redoStack.isEmpty()) {
        redoEdit();
        currentIndex =
            m_undoStack.size();
    }

    const QStringList labels =
        historyActionLabels();

    if (currentIndex >= 0 &&
        currentIndex < labels.size()) {
        statusBar()->showMessage(
            "History: " +
            labels.at(
                currentIndex
                )
            );
    }
}

void MainWindow::setupQuickToolsPanel()
{
    QGroupBox *quickToolsGroup = new QGroupBox("Quick Tools", ui->toolsDockContents);
    QGridLayout *quickToolsLayout = new QGridLayout(quickToolsGroup);

    quickToolsLayout->setContentsMargins(8, 8, 8, 8);
    quickToolsLayout->setSpacing(6);

    auto makeToolButton = [quickToolsGroup](const QString &text, const QString &toolTip) {
        QToolButton *button = new QToolButton(quickToolsGroup);
        button->setText(text);
        button->setToolTip(toolTip);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(52);
        button->setStyleSheet(
            "QToolButton {"
            "border: 1px solid #765638;"
            "border-radius: 4px;"
            "}"
            "QToolButton:hover {"
            "border: 1px solid #A58B69;"
            "}"
            "QToolButton:pressed {"
            "border: 1px solid #B8894F;"
            "}"
            "QToolButton:disabled {"
            "border: 1px solid #4A4036;"
            "}"
            );
        return button;
    };

    m_redEyeToolButton = makeToolButton(
        "Red Eye",
        "Remove red eye from flash photos."
        );

    m_overexposureToolButton = makeToolButton(
        "Overexposure\nRecovery",
        "Recover bright overexposed areas using threshold, amount, and feather."
        );

    m_whiteBalanceToolButton = makeToolButton(
        "White\nBalance",
        "Correct color cast using white balance controls."
        );

    m_hueSaturationToolButton = makeToolButton(
        "Hue / Tone",
        "Open hue and color correction controls."
        );

    QToolButton *noiseReductionToolButton = makeToolButton(
        "Noise Reduction",
        "Reduce luminance grain and color noise while protecting real detail."
        );

    noiseReductionToolButton->setObjectName(
        "noiseReductionToolButton"
        );

    QToolButton *focusToolButton = makeToolButton(
        "Focus",
        "Restore natural edge definition without sharpening noise or creating halos."
        );

    focusToolButton->setObjectName(
        "focusToolButton"
        );

    m_hmsColorBalanceToolButton = makeToolButton(
        "Base + HMS Color Balance",
        "Adjust color globally in Base, or separately in highlights, midtones, and shadows."
        );

    m_hmsColorBalanceToolButton->setObjectName(
        "hmsColorBalanceToolButton"
        );

    quickToolsLayout->addWidget(m_redEyeToolButton, 0, 0);
    quickToolsLayout->addWidget(m_overexposureToolButton, 0, 1);
    quickToolsLayout->addWidget(m_whiteBalanceToolButton, 1, 0);
    quickToolsLayout->addWidget(m_hueSaturationToolButton, 1, 1);

    quickToolsLayout->addWidget(noiseReductionToolButton, 2, 0);
    quickToolsLayout->addWidget(focusToolButton, 2, 1);
    quickToolsLayout->addWidget(m_hmsColorBalanceToolButton, 3, 0, 1, 2);

    ui->transformLayout->addWidget(quickToolsGroup);

    connect(m_redEyeToolButton, &QToolButton::clicked,
            this, &MainWindow::showRedEyeDialog);

    connect(m_overexposureToolButton, &QToolButton::clicked,
            this, &MainWindow::showOverexposureRecoveryDialog);

    connect(m_whiteBalanceToolButton, &QToolButton::clicked,
            this, &MainWindow::showWhiteBalanceDialog);

    connect(m_hueSaturationToolButton, &QToolButton::clicked,
            this, &MainWindow::showHueSaturationDialog);

    connect(m_hmsColorBalanceToolButton, &QToolButton::clicked,
            this, &MainWindow::showHmsColorBalanceDialog);

    connect(
        noiseReductionToolButton,
        &QToolButton::clicked,
        this,
        [this]() {
            if (m_currentImage.isNull() ||
                m_adjustmentBaseImage.isNull()) {
                statusBar()->showMessage(
                    "Open an image before using Noise Reduction"
                    );

                return;
            }

            NoiseReductionDialog dialog(
                m_adjustmentBaseImage,
                [this](const QImage &correctedBaseImage) {
                    return buildAdjustedImage(
                        correctedBaseImage
                        );
                },
                this
                );

            if (dialog.exec() !=
                QDialog::Accepted) {
                statusBar()->showMessage(
                    "Noise Reduction canceled"
                    );

                return;
            }

            if (!dialog.hasAdjustment()) {
                statusBar()->showMessage(
                    "Noise Reduction unchanged"
                    );

                return;
            }

            pushUndoState(
                "Noise Reduction"
                );

            m_currentImage =
                dialog.resultImage();

            commitCurrentImageAsAdjustmentBase();

            m_hasUnsavedChanges = true;

            updatePreview();

            statusBar()->showMessage(
                "Noise Reduction finished"
                );
        });

    connect(
        focusToolButton,
        &QToolButton::clicked,
        this,
        [this]() {
            if (m_currentImage.isNull() ||
                m_adjustmentBaseImage.isNull()) {
                statusBar()->showMessage(
                    "Open an image before using Focus"
                    );

                return;
            }

            FocusDialog dialog(
                m_adjustmentBaseImage,
                [this](const QImage &correctedBaseImage) {
                    return buildAdjustedImage(
                        correctedBaseImage
                        );
                },
                this
                );

            if (dialog.exec() !=
                QDialog::Accepted) {
                statusBar()->showMessage(
                    "Focus canceled"
                    );

                return;
            }

            if (!dialog.hasAdjustment()) {
                statusBar()->showMessage(
                    "Focus unchanged"
                    );

                return;
            }

            pushUndoState(
                "Focus"
                );

            m_currentImage =
                dialog.resultImage();

            commitCurrentImageAsAdjustmentBase();

            m_hasUnsavedChanges = true;

            updatePreview();

            statusBar()->showMessage(
                "Focus finished"
                );
        });
}

void MainWindow::setQuickToolsEnabled(bool enabled)
{
    if (m_redEyeToolButton) {
        m_redEyeToolButton->setEnabled(enabled);
    }

    if (m_overexposureToolButton) {
        m_overexposureToolButton->setEnabled(enabled);
    }

    if (m_whiteBalanceToolButton) {
        m_whiteBalanceToolButton->setEnabled(enabled);
    }

    if (m_hueSaturationToolButton) {
        m_hueSaturationToolButton->setEnabled(enabled);
    }

    if (m_hmsColorBalanceToolButton) {
        m_hmsColorBalanceToolButton->setEnabled(enabled);
    }

    if (QToolButton *noiseReductionToolButton =
            findChild<QToolButton *>(
                "noiseReductionToolButton"
                )) {
        noiseReductionToolButton->setEnabled(
            enabled
            );
    }

    if (QToolButton *focusToolButton =
            findChild<QToolButton *>(
                "focusToolButton"
                )) {
        focusToolButton->setEnabled(
            enabled
            );
    }
}

void MainWindow::showHmsColorBalanceDialog()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        statusBar()->showMessage(
            "Open an image before using Color Balance (HMS)"
            );

        return;
    }

    // The HMS controls are still the original live adjustment controls.
    // They are simply borrowed from the sidebar and placed in a wider
    // modal tool window while the user is working with them.
    QDialog dialog(this);
    dialog.setWindowTitle("Base + HMS Color Balance");
    dialog.setWindowFlag(
        Qt::WindowContextHelpButtonHint,
        false
        );
    dialog.setModal(true);
    dialog.setSizeGripEnabled(true);
    dialog.setMinimumWidth(640);

    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(12, 12, 12, 12);
    dialogLayout->setSpacing(10);

    QWidget *storageParent = ui->toolsDockContents;

    ui->hmsGroup->setParent(&dialog);
    ui->hmsGroup->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Preferred
        );
    ui->hmsGroup->show();

    dialogLayout->addWidget(ui->hmsGroup);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Close,
        Qt::Horizontal,
        &dialog
        );

    dialogLayout->addWidget(buttonBox);

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject
        );

    dialog.resize(
        700,
        std::max(430, ui->hmsGroup->sizeHint().height() + 90)
        );

    statusBar()->showMessage(
        "Color Balance (HMS) — fine-tune highlights, midtones, and shadows"
        );

    dialog.exec();

    // Keep the UI-owned HMS widget alive after the temporary dialog closes.
    dialogLayout->removeWidget(ui->hmsGroup);
    ui->hmsGroup->hide();
    ui->hmsGroup->setParent(storageParent);

    statusBar()->showMessage(
        "Color Balance (HMS) closed"
        );
}

void MainWindow::showRedEyeDialog()
{
    if (m_currentImage.isNull()) {
        statusBar()->showMessage("Open an image before using Red Eye");
        return;
    }

    RedEyeDialog dialog(m_adjustmentBaseImage, this);

    if (dialog.exec() == QDialog::Accepted) {
        pushUndoState("Red Eye");

        m_currentImage = dialog.resultImage();
        commitCurrentImageAsAdjustmentBase();

        m_hasUnsavedChanges = true;
        updatePreview();

        statusBar()->showMessage("Red Eye repair applied");
    } else {
        statusBar()->showMessage("Red Eye repair canceled");
    }
}

void MainWindow::showOverexposureRecoveryDialog()
{
    if (m_currentImage.isNull() || m_adjustmentBaseImage.isNull()) {
        statusBar()->showMessage("Open an image before using Overexposure Recovery");
        return;
    }

    if (m_overexposureWorkspace) {
        statusBar()->showMessage("Overexposure Recovery is already open");
        return;
    }

    startOverexposureWorkspace();
}

void MainWindow::startOverexposureWorkspace()
{
    if (m_overexposureWorkspace || m_adjustmentBaseImage.isNull()) {
        return;
    }

    if (m_cropMode) {
        cancelCrop();
    }

    if (m_sideBySideCompare) {
        ui->actionCompare->blockSignals(true);
        ui->actionCompare->setChecked(false);
        ui->actionCompare->blockSignals(false);
        m_sideBySideCompare = false;
    }

    m_showOriginalTemporarily = false;
    m_overexposureEntryImage = m_adjustmentBaseImage;
    m_toolsPanelWasVisibleBeforeOverexposure = ui->toolsDock->isVisible();

    m_overexposureGlareCanvas = new OverexposureSelectionWidget(
        ui->previewScrollAreaContents
        );
    m_overexposureBrightCanvas = new BrightAreaSelectionWidget(
        ui->previewScrollAreaContents
        );

    for (QWidget *canvas : {
             static_cast<QWidget *>(m_overexposureGlareCanvas),
             static_cast<QWidget *>(m_overexposureBrightCanvas)}) {
        canvas->setMinimumSize(0, 0);
        canvas->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        canvas->hide();
        ui->previewLayout->addWidget(canvas);
        ui->previewLayout->setAlignment(canvas, Qt::AlignCenter);
    }

    ui->imagePreviewLabel->hide();

    m_overexposureWorkspace = new OverexposureRecoveryWorkspace(
        m_overexposureEntryImage,
        m_overexposureGlareCanvas,
        m_overexposureBrightCanvas,
        ui->toolsDockContents
        );

    ui->toolsScrollArea->hide();
    ui->toolsDockLayout->addWidget(m_overexposureWorkspace);
    ui->toolsDock->show();

    m_overexposureBottomActions = new QWidget(ui->centralwidget);
    m_overexposureBottomActions->setObjectName("overexposureBottomActions");

    QHBoxLayout *actionLayout = new QHBoxLayout(m_overexposureBottomActions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(6);

    m_overexposureCancelButton = new QPushButton(
        "Cancel",
        m_overexposureBottomActions
        );
    m_overexposureApplyButton = new QPushButton(
        "Apply Region",
        m_overexposureBottomActions
        );
    m_overexposureFinishButton = new QPushButton(
        "Finish",
        m_overexposureBottomActions
        );

    m_overexposureCancelButton->setObjectName("overexposureCancelButton");
    m_overexposureApplyButton->setObjectName("overexposureApplyButton");
    m_overexposureFinishButton->setObjectName("overexposureFinishButton");

    m_overexposureCancelButton->setToolTip(
        "Discard the entire Overexposure Recovery session"
        );
    m_overexposureApplyButton->setToolTip(
        "Commit the current selected region and remain in the tool"
        );
    m_overexposureFinishButton->setToolTip(
        "Apply the active selection if needed, commit the session, and return to the editor"
        );

    actionLayout->addWidget(m_overexposureCancelButton);
    actionLayout->addWidget(m_overexposureApplyButton);
    actionLayout->addWidget(m_overexposureFinishButton);

    ui->showOriginalBarLayout->insertWidget(
        1,
        m_overexposureBottomActions
        );

    connect(
        m_overexposureCancelButton,
        &QPushButton::clicked,
        this,
        &MainWindow::cancelOverexposureWorkspace,
        Qt::QueuedConnection
        );

    connect(
        m_overexposureApplyButton,
        &QPushButton::clicked,
        this,
        &MainWindow::applyOverexposureWorkspaceRegion
        );

    connect(
        m_overexposureFinishButton,
        &QPushButton::clicked,
        this,
        &MainWindow::finishOverexposureWorkspace,
        Qt::QueuedConnection
        );

    connect(
        m_overexposureWorkspace,
        &OverexposureRecoveryWorkspace::canvasModeChanged,
        this,
        &MainWindow::setOverexposureCanvasMode
        );

    connect(
        m_overexposureWorkspace,
        &OverexposureRecoveryWorkspace::applyButtonTextChanged,
        this,
        [this](const QString &text) {
            if (m_overexposureApplyButton) {
                m_overexposureApplyButton->setText(text);
            }
        }
        );

    connect(
        m_overexposureWorkspace,
        &OverexposureRecoveryWorkspace::statusMessage,
        this,
        [this](const QString &message) {
            statusBar()->showMessage(message);
        }
        );

    disableActionsForOverexposureWorkspace();

    ui->showOriginalButton->setEnabled(true);
    ui->showOriginalButton->setToolTip(
        "Hold to compare with the image as it entered Overexposure Recovery. Shortcut: Spacebar."
        );

    setOverexposureCanvasMode(0);
    updateOverexposureCanvasGeometry();

    statusBar()->showMessage(
        "Overexposure Recovery workspace — edit directly on the main image"
        );
}

void MainWindow::applyOverexposureWorkspaceRegion()
{
    if (!m_overexposureWorkspace) {
        return;
    }

    m_overexposureWorkspace->applyCurrentRegion();
}

void MainWindow::finishOverexposureWorkspace()
{
    if (!m_overexposureWorkspace) {
        return;
    }

    QImage finishedImage;

    if (!m_overexposureWorkspace->finish(finishedImage)) {
        return;
    }

    pushUndoState("Overexposure Recovery");

    m_currentImage = finishedImage;
    commitCurrentImageAsAdjustmentBase();
    m_hasUnsavedChanges = true;

    closeOverexposureWorkspace();
    updatePreview();

    statusBar()->showMessage("Overexposure recovery applied");
}

void MainWindow::cancelOverexposureWorkspace()
{
    if (!m_overexposureWorkspace) {
        return;
    }

    closeOverexposureWorkspace();
    updatePreview();

    statusBar()->showMessage("Overexposure recovery canceled");
}

void MainWindow::closeOverexposureWorkspace()
{
    if (!m_overexposureWorkspace) {
        return;
    }

    m_showOriginalTemporarily = false;

    restoreActionsAfterOverexposureWorkspace();

    ui->toolsDockLayout->removeWidget(m_overexposureWorkspace);
    delete m_overexposureWorkspace;
    m_overexposureWorkspace = nullptr;

    if (m_overexposureGlareCanvas) {
        ui->previewLayout->removeWidget(m_overexposureGlareCanvas);
        delete m_overexposureGlareCanvas;
        m_overexposureGlareCanvas = nullptr;
    }

    if (m_overexposureBrightCanvas) {
        ui->previewLayout->removeWidget(m_overexposureBrightCanvas);
        delete m_overexposureBrightCanvas;
        m_overexposureBrightCanvas = nullptr;
    }

    if (m_overexposureBottomActions) {
        ui->showOriginalBarLayout->removeWidget(m_overexposureBottomActions);
        delete m_overexposureBottomActions;
        m_overexposureBottomActions = nullptr;
    }

    m_overexposureCancelButton = nullptr;
    m_overexposureApplyButton = nullptr;
    m_overexposureFinishButton = nullptr;

    ui->toolsScrollArea->show();
    ui->imagePreviewLabel->show();
    ui->previewLayout->setAlignment(ui->imagePreviewLabel, Qt::AlignCenter);

    ui->showOriginalButton->setToolTip(
        "Hold to compare with the unedited image. Shortcut: Spacebar."
        );

    m_overexposureEntryImage = QImage();

    if (m_toolsPanelWasVisibleBeforeOverexposure) {
        ui->toolsDock->show();
    } else {
        ui->toolsDock->hide();
    }
}

void MainWindow::setOverexposureCanvasMode(int tabIndex)
{
    if (!m_overexposureWorkspace ||
        !m_overexposureGlareCanvas ||
        !m_overexposureBrightCanvas) {
        return;
    }

    const bool glareActive = tabIndex == 0;

    m_overexposureGlareCanvas->setVisible(glareActive);
    m_overexposureBrightCanvas->setVisible(!glareActive);

    if (m_overexposureApplyButton) {
        m_overexposureApplyButton->setText(
            glareActive ? "Apply Region" : "Apply Area"
            );
    }

    updateOverexposureCanvasGeometry();
}

void MainWindow::updateOverexposureCanvasGeometry()
{
    if (!m_overexposureWorkspace || m_overexposureEntryImage.isNull()) {
        return;
    }

    const QSize viewportSize = ui->previewScrollArea->viewport()->size();
    QSize canvasSize;

    if (m_fitToWindow) {
        canvasSize = m_overexposureEntryImage.size();
        canvasSize.scale(viewportSize, Qt::KeepAspectRatio);
    } else {
        canvasSize = QSize(
            std::max(1, static_cast<int>(m_overexposureEntryImage.width() * m_zoomFactor)),
            std::max(1, static_cast<int>(m_overexposureEntryImage.height() * m_zoomFactor))
            );
    }

    if (canvasSize.isEmpty()) {
        return;
    }

    if (m_overexposureGlareCanvas) {
        m_overexposureGlareCanvas->setFixedSize(canvasSize);
    }

    if (m_overexposureBrightCanvas) {
        m_overexposureBrightCanvas->setFixedSize(canvasSize);
    }

    const int contentWidth = std::max(canvasSize.width(), viewportSize.width());
    const int contentHeight = std::max(canvasSize.height(), viewportSize.height());
    ui->previewScrollAreaContents->resize(contentWidth, contentHeight);
}

void MainWindow::disableActionsForOverexposureWorkspace()
{
    m_overexposurePreviouslyEnabledActions.clear();

    const QList<QAction *> actions = findChildren<QAction *>();

    for (QAction *action : actions) {
        if (!action || !action->isEnabled()) {
            continue;
        }

        if (action == ui->actionZoomIn ||
            action == ui->actionZoomOut ||
            action == ui->actionFitToWindow ||
            action == ui->actionActualSize) {
            continue;
        }

        m_overexposurePreviouslyEnabledActions.append(action);
        action->setEnabled(false);
    }
}

void MainWindow::restoreActionsAfterOverexposureWorkspace()
{
    // History menu entries are rebuilt whenever an undo state is added.
    // Guarded pointers automatically become null if one of those temporary
    // QAction objects is deleted while Overexposure Recovery is open.
    for (const QPointer<QAction> &actionPointer :
         m_overexposurePreviouslyEnabledActions) {
        if (QAction *action = actionPointer.data()) {
            action->setEnabled(true);
        }
    }

    m_overexposurePreviouslyEnabledActions.clear();
}

void MainWindow::showWhiteBalanceDialog()
{
    if (m_currentImage.isNull() ||
        m_adjustmentBaseImage.isNull()) {
        statusBar()->showMessage(
            "Open an image before using White Balance"
            );

        return;
    }

    WhiteBalanceDialog dialog(
        m_adjustmentBaseImage,
        [this](const QImage &correctedBaseImage) {
            return buildAdjustedImage(
                correctedBaseImage
                );
        },
        this
        );

    if (dialog.exec() !=
        QDialog::Accepted) {
        statusBar()->showMessage(
            "White Balance canceled"
            );

        return;
    }

    if (!dialog.hasAdjustment()) {
        statusBar()->showMessage(
            "White Balance unchanged"
            );

        return;
    }

    pushUndoState(
        "White Balance"
        );

    // The dialog returns the corrected adjustment base.
    // Commit then reapplies the still-live Basic, Saturation,
    // and HMS values exactly once.
    m_currentImage =
        dialog.resultImage();

    commitCurrentImageAsAdjustmentBase();

    m_hasUnsavedChanges = true;

    updatePreview();

    statusBar()->showMessage(
        "White Balance finished"
        );
}

void MainWindow::showHueSaturationDialog()
{
    if (m_currentImage.isNull() ||
        m_adjustmentBaseImage.isNull()) {
        statusBar()->showMessage(
            "Open an image before using Hue/Tone"
            );
        return;
    }

    // Keep both layers:
    //
    // dialogStartBaseImage:
    // The underlying image before live Basic/HMS adjustments.
    //
    // dialogStartDisplayImage:
    // The exact image the user was viewing when the dialog opened.
    const QImage dialogStartBaseImage = m_adjustmentBaseImage;
    const QImage dialogStartDisplayImage = m_currentImage;

    ImageProcessingRuntime &processingRuntime =
        ImageProcessingRuntime::instance();

    // Slider movement uses the optimizer-selected preview dimension.
    // The full-resolution image is rendered after the control settles and
    // again when Finish commits the tool.
    const QImage dialogInteractiveBaseImage =
        processingRuntime.makeInteractivePreview(
            dialogStartBaseImage
            );

    const QImage dialogInteractiveDisplayImage =
        processingRuntime.makeInteractivePreview(
            dialogStartDisplayImage
            );

    QDialog dialog(this);
    dialog.setWindowTitle("Hue / Tone");
    // Keep the Hue/Tone window open while allowing the user to click
    // the large main image for color sampling. A local event loop below
    // preserves the existing synchronous Finish/Cancel workflow.
    dialog.setModal(false);
    dialog.setWindowModality(Qt::NonModal);
    dialog.resize(700, 390);
    dialog.setMinimumSize(620, 360);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(10);

    QLabel *titleLabel = new QLabel(
        "Adjust individual color tones. "
        "Move left to reduce a tone or right to add it."
        );

    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);

    struct HueControlInfo
    {
        const char *name;
        const char *color;
    };

    static const std::array<HueControlInfo, 8> hueControls = {{
        { "Red",     "#c0392b" },
        { "Orange",  "#e67e22" },
        { "Yellow",  "#f1c40f" },
        { "Green",   "#27ae60" },
        { "Cyan",    "#16a5a5" },
        { "Blue",    "#2980b9" },
        { "Purple",  "#8e44ad" },
        { "Magenta", "#c2185b" }
    }};

    QGridLayout *controlLayout = new QGridLayout();
    controlLayout->setHorizontalSpacing(10);
    controlLayout->setVerticalSpacing(8);
    controlLayout->setColumnStretch(1, 1);

    QVector<QLabel *> hueSwatches;
    QVector<QSlider *> hueSliders;
    QVector<QSpinBox *> hueSpinBoxes;

    hueSwatches.reserve(8);
    hueSliders.reserve(8);
    hueSpinBoxes.reserve(8);

    for (int index = 0; index < 8; ++index) {
        QLabel *colorSwatch = new QLabel(&dialog);
        colorSwatch->setFixedSize(26, 26);
        colorSwatch->setToolTip(hueControls[index].name);
        colorSwatch->setAccessibleName(
            QString("%1 tone").arg(hueControls[index].name)
            );

        colorSwatch->setStyleSheet(
            QString(
                "QLabel {"
                "background-color: %1;"
                "border: 1px solid #777777;"
                "border-radius: 3px;"
                "}"
                )
                .arg(hueControls[index].color)
            );

        QSlider *slider = new QSlider(
            Qt::Horizontal,
            &dialog
            );

        slider->setRange(-100, 100);
        slider->setValue(0);

        slider->setProperty(
            "resetOnDoubleClick",
            true
            );

        slider->setProperty(
            "resetLabel",
            QString("%1 tone")
                .arg(hueControls[index].name)
            );

        slider->installEventFilter(this);

        slider->setToolTip(
            QString("Reduce or add %1 tone")
                .arg(hueControls[index].name)
            );

        QSpinBox *spinBox = new QSpinBox(&dialog);
        spinBox->setRange(-100, 100);
        spinBox->setValue(0);
        spinBox->setFixedWidth(68);
        spinBox->setToolTip(
            QString("%1 tone amount")
                .arg(hueControls[index].name)
            );

        hueSwatches.append(colorSwatch);
        hueSliders.append(slider);
        hueSpinBoxes.append(spinBox);

        controlLayout->addWidget(
            colorSwatch,
            index,
            0,
            Qt::AlignCenter
            );

        controlLayout->addWidget(
            slider,
            index,
            1
            );

        controlLayout->addWidget(
            spinBox,
            index,
            2
            );
    }

    QWidget *controlsWidget = new QWidget(&dialog);
    controlsWidget->setLayout(controlLayout);
    controlsWidget->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Maximum
        );

    mainLayout->addWidget(controlsWidget);

    QPushButton *eyedropperButton =
        new QPushButton(
            "Eyedropper",
            &dialog
            );

    eyedropperButton->setCheckable(true);

    eyedropperButton->setToolTip(
        "Pick a color from the large main image"
        );

    QLabel *sampledColorSwatch =
        new QLabel(&dialog);

    sampledColorSwatch->setFixedSize(26, 26);
    sampledColorSwatch->setToolTip(
        "Sampled photo color"
        );

    sampledColorSwatch->setStyleSheet(
        "QLabel {"
        "background-color: #303030;"
        "border: 1px solid #777777;"
        "border-radius: 3px;"
        "}"
        );

    QLabel *sampledColorLabel =
        new QLabel(
            "No color sampled",
            &dialog
            );

    ToolPreviewWidget *pickerPreview =
        new ToolPreviewWidget(&dialog);

    pickerPreview->setMinimumWidth(560);
    pickerPreview->setFixedHeight(260);
    pickerPreview->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    pickerPreview->setBackgroundColor(
        QColor("#191919")
        );

    pickerPreview->setToolCursor(
        Qt::CrossCursor
        );

    pickerPreview->setToolModeIcon(
        makeHueEyedropperIcon()
        );

    pickerPreview->setToolModeToolTip(
        "Eyedropper: click a color in the photograph"
        );

    pickerPreview->setPanEnabled(true);
    pickerPreview->setLeftButtonPans(false);
    pickerPreview->setToolTip(
        "Use the eyedropper or hand tool. "
        "The mouse wheel zooms under the pointer."
        );

    pickerPreview->setVisible(false);

    QWidget *pickerZoomBar =
        pickerPreview->createZoomControlBar(
            &dialog
            );

    pickerZoomBar->setVisible(false);

    QHBoxLayout *eyedropperLayout =
        new QHBoxLayout();

    eyedropperLayout->addWidget(
        eyedropperButton
        );

    eyedropperLayout->addWidget(
        pickerZoomBar
        );

    eyedropperLayout->addWidget(
        sampledColorSwatch
        );

    eyedropperLayout->addWidget(
        sampledColorLabel
        );

    eyedropperLayout->addStretch(1);

    mainLayout->addLayout(
        eyedropperLayout
        );

    QLabel *sampledAmountLabel =
        new QLabel(
            "Selected Color",
            &dialog
            );

    sampledAmountLabel->setToolTip(
        "Add or remove the exact color sampled with the eyedropper"
        );

    QSlider *sampledHueSlider =
        new QSlider(
            Qt::Horizontal,
            &dialog
            );

    sampledHueSlider->setRange(-100, 100);
    sampledHueSlider->setValue(0);

    sampledHueSlider->setProperty(
        "resetOnDoubleClick",
        true
        );

    sampledHueSlider->setProperty(
        "resetLabel",
        "Selected Color"
        );

    sampledHueSlider->installEventFilter(this);

    sampledHueSlider->setEnabled(false);
    sampledHueSlider->setToolTip(
        "Move left to remove the sampled tone or right to add it"
        );

    QSpinBox *sampledHueSpinBox =
        new QSpinBox(&dialog);

    sampledHueSpinBox->setRange(-100, 100);
    sampledHueSpinBox->setValue(0);
    sampledHueSpinBox->setFixedWidth(68);
    sampledHueSpinBox->setEnabled(false);
    sampledHueSpinBox->setToolTip(
        "Sampled color amount"
        );

    QGridLayout *sampledAmountLayout =
        new QGridLayout();

    sampledAmountLayout->setHorizontalSpacing(10);
    sampledAmountLayout->setColumnStretch(1, 1);

    sampledAmountLayout->addWidget(
        sampledAmountLabel,
        0,
        0
        );

    sampledAmountLayout->addWidget(
        sampledHueSlider,
        0,
        1
        );

    sampledAmountLayout->addWidget(
        sampledHueSpinBox,
        0,
        2
        );

    mainLayout->addLayout(
        sampledAmountLayout
        );

    mainLayout->addWidget(
        pickerPreview
        );

    QPushButton *showOriginalButton =
        new QPushButton(
            "Hold to Show Original",
            &dialog
            );

    QPushButton *resetButton =
        new QPushButton(
            "Reset",
            &dialog
            );

    QPushButton *cancelButton =
        new QPushButton(
            "Cancel",
            &dialog
            );

    QPushButton *finishButton =
        new QPushButton(
            "Finish",
            &dialog
            );

    finishButton->setDefault(true);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(showOriginalButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(resetButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(finishButton);

    mainLayout->addLayout(buttonLayout);

    bool showingOriginal = false;
    bool hueSliderDragging = false;

    QTimer fullResolutionPreviewTimer(&dialog);
    fullResolutionPreviewTimer.setSingleShot(true);
    fullResolutionPreviewTimer.setInterval(220);

    int matchedHueIndex = -1;
    int sampledHueValue = -1;

    // Centers of the eight visible Hue/Tone controls.
    // This must appear before readHueAmounts.
    static const std::array<int, 8> hueCenters = {{
        0,   // Red
        30,  // Orange
        60,  // Yellow
        120, // Green
        180, // Cyan
        240, // Blue
        275, // Purple
        320  // Magenta
    }};

    auto setMatchedHueIndex =
        [&](int selectedIndex) {
            matchedHueIndex = selectedIndex;

            for (int index = 0;
                 index < hueSwatches.size();
                 ++index) {
                const QString border =
                    index == matchedHueIndex
                        ? "3px solid #f2f2f2"
                        : "1px solid #777777";

                hueSwatches[index]->setStyleSheet(
                    QString(
                        "QLabel {"
                        "background-color: %1;"
                        "border: %2;"
                        "border-radius: 3px;"
                        "}"
                        )
                        .arg(hueControls[index].color)
                        .arg(border)
                    );
            }
        };

    bool mainImageEyedropperActive = false;

    const QCursor normalImageCursor =
        ui->imagePreviewLabel->cursor();

    auto closeEyedropper = [&]() {
        mainImageEyedropperActive = false;

        // The old compact picker stays hidden. Hue/Tone now samples
        // directly from the main canvas.
        pickerPreview->setLeftButtonPans(false);
        pickerPreview->setVisible(false);
        pickerZoomBar->setVisible(false);

        eyedropperButton->setChecked(false);
        eyedropperButton->setText(
            "Eyedropper"
            );

        ui->imagePreviewLabel->setCursor(
            normalImageCursor
            );

        dialog.adjustSize();
    };

    auto clearSampledColor = [&]() {
        sampledHueValue = -1;
        setMatchedHueIndex(-1);

        {
            QSignalBlocker sliderBlocker(
                sampledHueSlider
                );

            QSignalBlocker spinBoxBlocker(
                sampledHueSpinBox
                );

            sampledHueSlider->setValue(0);
            sampledHueSpinBox->setValue(0);
        }

        sampledHueSlider->setEnabled(false);
        sampledHueSpinBox->setEnabled(false);

        sampledColorSwatch->setStyleSheet(
            "QLabel {"
            "background-color: #303030;"
            "border: 1px solid #777777;"
            "border-radius: 3px;"
            "}"
            );

        sampledColorSwatch->setToolTip(
            "Sampled photo color"
            );

        sampledColorLabel->setText(
            "No color sampled"
            );
    };

    auto readHueAmounts = [&]() {
        std::array<int, 8> amounts {};

        // Begin with the eight visible fixed-color controls.
        for (int index = 0; index < 8; ++index) {
            amounts[
                static_cast<std::size_t>(index)
            ] = hueSliders[index]->value();
        }

        const int sampledAmount =
            sampledHueSlider->value();

        if (sampledHueValue < 0 ||
            sampledAmount == 0) {
            return amounts;
        }

        int lowerIndex = 7;
        int upperIndex = 0;

        double lowerHue = 320.0;
        double upperHue = 360.0;
        double workingHue =
            static_cast<double>(
                sampledHueValue
                );

        // Locate the sampled hue between two neighboring controls.
        // The final Magenta-to-Red interval wraps through 360 degrees.
        if (sampledHueValue < hueCenters[7]) {
            for (int index = 0; index < 7; ++index) {
                if (sampledHueValue >= hueCenters[index] &&
                    sampledHueValue <= hueCenters[index + 1]) {
                    lowerIndex = index;
                    upperIndex = index + 1;

                    lowerHue =
                        hueCenters[index];

                    upperHue =
                        hueCenters[index + 1];

                    break;
                }
            }
        }

        const double interval =
            std::max(
                1.0,
                upperHue - lowerHue
                );

        const double upperWeight =
            std::clamp(
                (workingHue - lowerHue) /
                    interval,
                0.0,
                1.0
                );

        const double lowerWeight =
            1.0 - upperWeight;

        amounts[
            static_cast<std::size_t>(
                lowerIndex
                )
        ] += static_cast<int>(
            std::round(
                sampledAmount *
                lowerWeight
                )
            );

        amounts[
            static_cast<std::size_t>(
                upperIndex
                )
        ] += static_cast<int>(
            std::round(
                sampledAmount *
                upperWeight
                )
            );

        // A fixed slider and the sampled slider may affect the same hue.
        // Keep the combined value inside the processing range.
        for (int &amount : amounts) {
            amount = std::clamp(
                amount,
                -100,
                100
                );
        }

        return amounts;
    };

    auto hasHueAdjustment =
        [](const std::array<int, 8> &amounts) {
            return std::any_of(
                amounts.begin(),
                amounts.end(),
                [](int amount) {
                    return amount != 0;
                }
                );
        };

    auto renderHuePreview =
        [&](bool fullResolution) {
            if (showingOriginal) {
                return;
            }

            const std::array<int, 8> amounts =
                readHueAmounts();

            const QImage &previewBaseSource =
                fullResolution
                    ? dialogStartBaseImage
                    : dialogInteractiveBaseImage;

            const QImage &unchangedDisplaySource =
                fullResolution
                    ? dialogStartDisplayImage
                    : dialogInteractiveDisplayImage;

            if (!hasHueAdjustment(amounts)) {
                m_currentImage =
                    unchangedDisplaySource;

                this->updatePreview();
                return;
            }

            const QImage previewBaseImage =
                ImageTools::applyHueToneMix(
                    previewBaseSource,
                    amounts
                    );

            // Reapply the current live Basic, Saturation, and HMS values
            // to the temporarily Hue/Tone-adjusted base image.
            m_currentImage =
                buildAdjustedImage(previewBaseImage);

            this->updatePreview();
        };

    auto updateHuePreview = [&]() {
        if (showingOriginal) {
            return;
        }

        // Every control movement gets a quick optimizer-sized preview.
        // A single newest full-resolution pass follows after keyboard/spin
        // input settles. Mouse drags render full resolution on release.
        renderHuePreview(false);

        if (!hueSliderDragging) {
            fullResolutionPreviewTimer.start();
        }
    };

    connect(
        &fullResolutionPreviewTimer,
        &QTimer::timeout,
        &dialog,
        [&]() {
            if (!showingOriginal &&
                dialog.isVisible()) {
                renderHuePreview(true);
            }
        }
        );

    connect(
        sampledHueSlider,
        &QSlider::sliderPressed,
        &dialog,
        [&]() {
            hueSliderDragging = true;
            fullResolutionPreviewTimer.stop();
        }
        );

    connect(
        sampledHueSlider,
        &QSlider::sliderReleased,
        &dialog,
        [&]() {
            hueSliderDragging = false;
            renderHuePreview(true);
        }
        );

    connect(
        sampledHueSlider,
        &QSlider::valueChanged,
        &dialog,
        [sampledHueSpinBox,
         &updateHuePreview](int value) {
            QSignalBlocker blocker(
                sampledHueSpinBox
                );

            sampledHueSpinBox->setValue(
                value
                );

            updateHuePreview();
        }
        );

    connect(
        sampledHueSpinBox,
        QOverload<int>::of(
            &QSpinBox::valueChanged
            ),
        &dialog,
        [sampledHueSlider,
         &updateHuePreview](int value) {
            QSignalBlocker blocker(
                sampledHueSlider
                );

            sampledHueSlider->setValue(
                value
                );

            updateHuePreview();
        }
        );

    connect(
        eyedropperButton,
        &QPushButton::clicked,
        &dialog,
        [&]() {
            if (mainImageEyedropperActive) {
                closeEyedropper();

                sampledColorLabel->setText(
                    matchedHueIndex >= 0
                        ? QString("Matched to %1")
                              .arg(
                                  hueControls[
                                      matchedHueIndex
                ].name
                                  )
                        : QString("No color sampled")
                    );

                statusBar()->showMessage(
                    "Hue/Tone eyedropper canceled"
                    );
                return;
            }

            mainImageEyedropperActive = true;
            eyedropperButton->setChecked(true);
            eyedropperButton->setText(
                "Sampling Main Image"
                );

            sampledColorLabel->setText(
                "Click a color on the large main image"
                );

            const QPixmap cursorPixmap =
                makeHueEyedropperIcon().pixmap(24, 24);

            ui->imagePreviewLabel->setCursor(
                QCursor(cursorPixmap, 5, 20)
                );

            statusBar()->showMessage(
                "Hue/Tone eyedropper active — click a color on the main image"
                );
        }
        );

    auto processHueSample =
        [&](const QPoint &imagePoint) {
            if (dialogStartDisplayImage.isNull()) {
                return;
            }

            const int imageX =
                std::clamp(
                    imagePoint.x(),
                    0,
                    dialogStartDisplayImage.width() - 1
                    );

            const int imageY =
                std::clamp(
                    imagePoint.y(),
                    0,
                    dialogStartDisplayImage.height() - 1
                    );

            // Sample the stable image that existed when Hue/Tone opened.
            // Live Hue/Tone changes remain visible on the main canvas but
            // cannot feed back into the color being identified.
            const QColor sampledColor =
                dialogStartDisplayImage.pixelColor(
                    imageX,
                    imageY
                    );

            sampledColorSwatch->setStyleSheet(
                QString(
                    "QLabel {"
                    "background-color: rgb(%1, %2, %3);"
                    "border: 1px solid #aaaaaa;"
                    "border-radius: 3px;"
                    "}"
                    )
                    .arg(sampledColor.red())
                    .arg(sampledColor.green())
                    .arg(sampledColor.blue())
                );

            sampledColorSwatch->setToolTip(
                QString(
                    "RGB %1, %2, %3"
                    )
                    .arg(sampledColor.red())
                    .arg(sampledColor.green())
                    .arg(sampledColor.blue())
                );

            const int sampledHue =
                sampledColor.hsvHue();

            const int sampledSaturation =
                sampledColor.hsvSaturation();

            // Gray, white, and nearly neutral colors do not have a
            // dependable hue to match to one of the eight sliders. Keep
            // sampling active so the user can immediately try another spot.
            if (sampledHue < 0 ||
                sampledSaturation < 24) {
                sampledHueValue = -1;
                setMatchedHueIndex(-1);

                {
                    QSignalBlocker sliderBlocker(
                        sampledHueSlider
                        );

                    QSignalBlocker spinBoxBlocker(
                        sampledHueSpinBox
                        );

                    sampledHueSlider->setValue(0);
                    sampledHueSpinBox->setValue(0);
                }

                sampledHueSlider->setEnabled(false);
                sampledHueSpinBox->setEnabled(false);

                sampledColorLabel->setText(
                    "Low color saturation — click a stronger color"
                    );

                statusBar()->showMessage(
                    "That point has too little color — click another point on the main image"
                    );

                updateHuePreview();
                return;
            }

            sampledHueValue = sampledHue;

            // A newly sampled color begins neutral. Do not carry an amount
            // from the previously sampled color into the new one.
            {
                QSignalBlocker sliderBlocker(
                    sampledHueSlider
                    );

                QSignalBlocker spinBoxBlocker(
                    sampledHueSpinBox
                    );

                sampledHueSlider->setValue(0);
                sampledHueSpinBox->setValue(0);
            }

            sampledHueSlider->setEnabled(true);
            sampledHueSpinBox->setEnabled(true);

            updateHuePreview();

            int closestIndex = 0;
            int closestDistance = 361;

            for (int index = 0;
                 index < static_cast<int>(
                     hueCenters.size()
                     );
                 ++index) {
                int distance =
                    std::abs(
                        sampledHue -
                        hueCenters[
                            static_cast<std::size_t>(
                                index
                                )
                ]
                        );

                distance =
                    std::min(
                        distance,
                        360 - distance
                        );

                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestIndex = index;
                }
            }

            setMatchedHueIndex(
                closestIndex
                );

            sampledColorLabel->setText(
                QString(
                    "Matched near %1 — adjust Selected Color below"
                    )
                    .arg(
                        hueControls[
                            closestIndex
            ].name
                        )
                );

            closeEyedropper();

            sampledHueSlider->setFocus(
                Qt::OtherFocusReason
                );

            dialog.raise();
            dialog.activateWindow();

            statusBar()->showMessage(
                QString("Hue/Tone sampled near %1")
                    .arg(hueControls[closestIndex].name)
                );
        };

    // Retain the hidden preview connection as a harmless compatibility
    // fallback, although the button no longer opens that preview.
    connect(
        pickerPreview,
        &ToolPreviewWidget::imageMousePressed,
        &dialog,
        [&](const QPointF &imagePoint,
            Qt::MouseButton button,
            Qt::KeyboardModifiers,
            bool insideImage) {
            if (button != Qt::LeftButton ||
                !insideImage) {
                return;
            }

            processHueSample(
                QPoint(
                    static_cast<int>(std::floor(imagePoint.x())),
                    static_cast<int>(std::floor(imagePoint.y()))
                    )
                );
        }
        );

    auto labelPointToHueSourcePoint =
        [&](const QPoint &labelPoint,
            QPoint &imagePoint) {
            if (dialogStartDisplayImage.isNull() ||
                m_lastPreviewPixmapSize.isEmpty()) {
                return false;
            }

            const QSize labelSize =
                ui->imagePreviewLabel->size();

            const int pixmapX =
                (labelSize.width() -
                 m_lastPreviewPixmapSize.width()) /
                2;

            const int pixmapY =
                (labelSize.height() -
                 m_lastPreviewPixmapSize.height()) /
                2;

            const QRect pixmapRect(
                pixmapX,
                pixmapY,
                m_lastPreviewPixmapSize.width(),
                m_lastPreviewPixmapSize.height()
                );

            if (!pixmapRect.contains(labelPoint)) {
                return false;
            }

            const int localX =
                labelPoint.x() - pixmapRect.x();

            const int localY =
                labelPoint.y() - pixmapRect.y();

            const double scaleX =
                static_cast<double>(
                    dialogStartDisplayImage.width()
                    ) /
                static_cast<double>(
                    m_lastPreviewPixmapSize.width()
                    );

            const double scaleY =
                static_cast<double>(
                    dialogStartDisplayImage.height()
                    ) /
                static_cast<double>(
                    m_lastPreviewPixmapSize.height()
                    );

            const int imageX =
                std::clamp(
                    static_cast<int>(
                        localX * scaleX
                        ),
                    0,
                    dialogStartDisplayImage.width() - 1
                    );

            const int imageY =
                std::clamp(
                    static_cast<int>(
                        localY * scaleY
                        ),
                    0,
                    dialogStartDisplayImage.height() - 1
                    );

            imagePoint = QPoint(
                imageX,
                imageY
                );

            return true;
        };

    MainImageSampleFilter mainImageSampleFilter(
        [&](const QPoint &labelPoint, Qt::MouseButton button) {
            if (!mainImageEyedropperActive ||
                button != Qt::LeftButton) {
                return false;
            }

            QPoint imagePoint;

            if (!labelPointToHueSourcePoint(
                    labelPoint,
                    imagePoint
                    )) {
                return false;
            }

            processHueSample(imagePoint);
            return true;
        },
        &dialog
        );

    ui->imagePreviewLabel->installEventFilter(
        &mainImageSampleFilter
        );

    for (int index = 0; index < 8; ++index) {
        QSlider *slider = hueSliders[index];
        QSpinBox *spinBox = hueSpinBoxes[index];

        connect(
            slider,
            &QSlider::sliderPressed,
            &dialog,
            [&]() {
                hueSliderDragging = true;
                fullResolutionPreviewTimer.stop();
            }
            );

        connect(
            slider,
            &QSlider::sliderReleased,
            &dialog,
            [&]() {
                hueSliderDragging = false;
                renderHuePreview(true);
            }
            );

        connect(
            slider,
            &QSlider::valueChanged,
            &dialog,
            [spinBox, &updateHuePreview](int value) {
                QSignalBlocker blocker(spinBox);
                spinBox->setValue(value);
                updateHuePreview();
            }
            );

        connect(
            spinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            &dialog,
            [slider, &updateHuePreview](int value) {
                QSignalBlocker blocker(slider);
                slider->setValue(value);
                updateHuePreview();
            }
            );
    }

    connect(
        showOriginalButton,
        &QPushButton::pressed,
        &dialog,
        [&]() {
            showingOriginal = true;
            fullResolutionPreviewTimer.stop();
            m_currentImage = dialogStartDisplayImage;
            this->updatePreview();
            statusBar()->showMessage(
                "Hue/Tone: showing image before this tool"
                );
        }
        );

    connect(
        showOriginalButton,
        &QPushButton::released,
        &dialog,
        [&]() {
            showingOriginal = false;
            updateHuePreview();
            statusBar()->showMessage(
                "Hue/Tone preview"
                );
        }
        );

    connect(
        resetButton,
        &QPushButton::clicked,
        &dialog,
        [&]() {
            fullResolutionPreviewTimer.stop();

            for (int index = 0; index < 8; ++index) {
                QSignalBlocker sliderBlocker(
                    hueSliders[index]
                    );

                QSignalBlocker spinBoxBlocker(
                    hueSpinBoxes[index]
                    );

                hueSliders[index]->setValue(0);
                hueSpinBoxes[index]->setValue(0);
            }

            showingOriginal = false;

            closeEyedropper();
            clearSampledColor();

            m_currentImage = dialogStartDisplayImage;
            this->updatePreview();

            statusBar()->showMessage(
                "Hue/Tone controls reset"
                );
        }
        );

    connect(
        cancelButton,
        &QPushButton::clicked,
        &dialog,
        [&]() {
            fullResolutionPreviewTimer.stop();
            m_currentImage = dialogStartDisplayImage;
            this->updatePreview();
            dialog.reject();
        }
        );

    connect(
        finishButton,
        &QPushButton::clicked,
        &dialog,
        [&]() {
            fullResolutionPreviewTimer.stop();
            dialog.accept();
        }
        );

    QEventLoop dialogLoop;

    connect(
        &dialog,
        &QDialog::finished,
        &dialogLoop,
        &QEventLoop::quit
        );

    statusBar()->showMessage(
        QString(
            "Hue/Tone optimized preview — %1 CPU workers, %2 px; GPU lookup %3"
            )
            .arg(processingRuntime.workerThreads())
            .arg(
                processingRuntime
                    .interactivePreviewMaxDimension()
                )
            .arg(
                processingRuntime.gpuBackendAvailable()
                    ? "ready"
                    : "unavailable"
                )
        );

    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    dialogLoop.exec();

    const int result = dialog.result();

    closeEyedropper();

    if (result != QDialog::Accepted) {
        // This also handles closing the dialog with its window X button.
        m_currentImage = dialogStartDisplayImage;
        this->updatePreview();

        statusBar()->showMessage(
            "Hue/Tone canceled"
            );
        return;
    }

    const std::array<int, 8> finalAmounts =
        readHueAmounts();

    if (!hasHueAdjustment(finalAmounts)) {
        m_currentImage = dialogStartDisplayImage;
        this->updatePreview();

        statusBar()->showMessage(
            "Hue/Tone unchanged"
            );
        return;
    }

    const QImage finalBaseImage =
        ImageTools::applyHueToneMix(
            dialogStartBaseImage,
            finalAmounts
            );

    // Capture exactly what existed before the dialog for Undo.
    m_currentImage = dialogStartDisplayImage;
    pushUndoState("Hue/Tone");

    // Commit the newly corrected base. The commit function then reapplies
    // the still-live Basic, Saturation, and HMS values.
    m_currentImage = finalBaseImage;
    commitCurrentImageAsAdjustmentBase();

    m_hasUnsavedChanges = true;
    this->updatePreview();

    statusBar()->showMessage(
        "Hue/Tone finished"
        );
}
