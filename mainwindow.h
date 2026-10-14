#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "imageoutputservice.h"
#include "imageimportservice.h"

#include <QImage>
#include <QList>
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QResizeEvent;
class QAction;
class QToolButton;
class QCloseEvent;
class QMenu;
class QPushButton;
class QLabel;
class QWidget;
class BrightAreaSelectionWidget;
class OverexposureSelectionWidget;
class OverexposureRecoveryWorkspace;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:

    void undoEdit();
    void redoEdit();

    void openImage();
    void closeImage();
    void saveImage();
    void saveImageAs();
    void exportImageAs();
    void showPrintPreviewDialog();
    void showPrintDialog();

    void fitToWindow();
    void actualSize();
    void zoomIn();
    void zoomOut();

    void showOriginalPressed();
    void showOriginalReleased();
    void toggleSideBySideCompare(bool checked);

    void rotateLeft();
    void rotateRight();
    void flipHorizontal();
    void flipVertical();

    void selectCropArea();
    void applyCrop();
    void cancelCrop();
    void cropControlsChanged();
    void openResizeDialog();

    void brightnessControlsChanged(int value);
    void contrastControlsChanged(int value);
    void gammaControlsChanged(int value);

    void baseHighlightControlsChanged(int value);
    void baseMidtoneControlsChanged(int value);
    void baseShadowControlsChanged(int value);
    void resetBaseHmsAdjustments();
    void resetCyanRedHmsAdjustments();
    void resetMagentaGreenHmsAdjustments();
    void resetYellowBlueHmsAdjustments();

    void resetBasicAdjustments();

    void setToolsPanelVisible(bool visible);

    void showRedEyeDialog();
    void showOverexposureRecoveryDialog();
    void showWhiteBalanceDialog();
    void showHueSaturationDialog();
    void showHmsColorBalanceDialog();
    void saturationControlsChanged(int value);

private:

    struct ImageState
    {
        QImage originalImage;
        QImage adjustmentBaseImage;
        QImage currentImage;

        int brightnessAmount = 0;
        int contrastAmount = 0;
        int gammaAmount = 0;
        int saturationAmount = 0;

        int baseHighlightAmount = 0;
        int baseMidtoneAmount = 0;
        int baseShadowAmount = 0;

        int cyanRedHighlightAmount = 0;
        int cyanRedMidtoneAmount = 0;
        int cyanRedShadowAmount = 0;

        int magentaGreenHighlightAmount = 0;
        int magentaGreenMidtoneAmount = 0;
        int magentaGreenShadowAmount = 0;

        int yellowBlueHighlightAmount = 0;
        int yellowBlueMidtoneAmount = 0;
        int yellowBlueShadowAmount = 0;

        QString filePath;
        QString label;
    };

    bool loadImageFile(const QString &filePath);

    bool writeImageToFile(
        const QString &filePath,
        const ImageOutput::Options &options,
        const QString &operationTitle
        );

    void updatePreview();
    void openPreferences(
        bool showPerformancePage
        );
    void openPrintLayoutDialog(bool focusPrintButton);

    QImage makeSideBySideImage() const;

    void resetCropControlsToImage();
    void setCropControlsFromRect(const QRect &rect);
    void pushUndoState(const QString &label);
    void restoreImageState(const ImageState &state);
    void updateUndoRedoActions();
    void clearHistory();
    void setupHistoryMenu();
    void updateHistoryMenu();
    void goToHistoryIndex(int targetIndex);
    QStringList historyActionLabels() const;

    QImage buildAdjustedImage(const QImage &baseImage) const;
    void applyBasicAdjustments();
    void setBasicAdjustmentControls(int brightness, int contrast, int gamma, int saturation);
    void setBaseHmsControls(int highlight, int midtone, int shadow);
    void setCyanRedHmsControls(int highlight, int midtone, int shadow);
    void setMagentaGreenHmsControls(int highlight, int midtone, int shadow);
    void setYellowBlueHmsControls(int highlight, int midtone, int shadow);

    void hmsColorControlChanged(int &storedAmount, int value, const QString &label);
    void commitCurrentImageAsAdjustmentBase();

    void setupQuickToolsPanel();
    void setQuickToolsEnabled(bool enabled);
    void updateOpenFileToolbar();

    void startOverexposureWorkspace();
    void applyOverexposureWorkspaceRegion();
    void finishOverexposureWorkspace();
    void cancelOverexposureWorkspace();
    void closeOverexposureWorkspace();
    void setOverexposureCanvasMode(int tabIndex);
    void updateOverexposureCanvasGeometry();
    void disableActionsForOverexposureWorkspace();
    void restoreActionsAfterOverexposureWorkspace();

    QRect currentCropRectFromControls() const;

    bool labelPointToImagePoint(const QPoint &labelPoint, QPoint &imagePoint) const;
    void setCropRectFromImagePoints(const QPoint &startPoint, const QPoint &endPoint);
    QRect scaledCropRectForPreview(const QSize &previewSize) const;

    QRect cropRectInLabelCoordinates() const;
    void updateCropCursor(const QPoint &labelPoint);

    enum class CropDragMode
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

    CropDragMode hitTestCropHandle(const QPoint &labelPoint) const;
    QRect adjustedCropRectForDrag(const QPoint &currentImagePoint) const;

    Ui::MainWindow *ui;

    QImage m_originalImage;
    QImage m_adjustmentBaseImage;
    QImage m_currentImage;

    QString m_currentFilePath;
    QString m_lastSavePath;

    // Import/source identity stays separate from the working Save As path.
    // A future New Canvas document can populate the same source record
    // without pretending that it came from a decoded image file.
    ImageImport::SourceInfo m_sourceInfo;

    bool m_hasUnsavedChanges = false;
    int m_brightnessAmount = 0;
    int m_contrastAmount = 0;
    int m_gammaAmount = 0;
    int m_saturationAmount = 0;

    int m_baseHighlightAmount = 0;
    int m_baseMidtoneAmount = 0;
    int m_baseShadowAmount = 0;

    int m_cyanRedHighlightAmount = 0;
    int m_cyanRedMidtoneAmount = 0;
    int m_cyanRedShadowAmount = 0;

    int m_magentaGreenHighlightAmount = 0;
    int m_magentaGreenMidtoneAmount = 0;
    int m_magentaGreenShadowAmount = 0;

    int m_yellowBlueHighlightAmount = 0;
    int m_yellowBlueMidtoneAmount = 0;
    int m_yellowBlueShadowAmount = 0;

    bool m_basicAdjustmentUndoCaptured = false;

    bool confirmUnsavedChanges();

    QVector<ImageState> m_undoStack;
    QVector<ImageState> m_redoStack;
    int m_maxUndoStates = 10;
    bool m_historyWasTrimmed = false;

    QMenu *m_historyMenu = nullptr;

    bool m_fitToWindow = true;
    double m_zoomFactor = 1.0;

    bool m_showOriginalTemporarily = false;
    bool m_sideBySideCompare = false;

    bool m_cropMode = false;
    bool m_cropDragActive = false;
    bool m_cropSelectionVisible = false;

    CropDragMode m_cropDragMode = CropDragMode::None;
    QPoint m_cropDragStartImagePoint;
    QRect m_cropDragStartRect;
    QRect m_cropRect;

    QSize m_lastPreviewPixmapSize;

    QAction *m_viewToolsPanelAction = nullptr;
    QAction *m_toolbarShowToolsAction = nullptr;

    QWidget *m_openFileToolbarWidget = nullptr;
    QLabel *m_openFileNameLabel = nullptr;
    QToolButton *m_closeFileToolbarButton = nullptr;

    OverexposureRecoveryWorkspace *m_overexposureWorkspace = nullptr;
    OverexposureSelectionWidget *m_overexposureGlareCanvas = nullptr;
    BrightAreaSelectionWidget *m_overexposureBrightCanvas = nullptr;

    QWidget *m_overexposureBottomActions = nullptr;
    QPushButton *m_overexposureCancelButton = nullptr;
    QPushButton *m_overexposureApplyButton = nullptr;
    QPushButton *m_overexposureFinishButton = nullptr;

    QImage m_overexposureEntryImage;
    QList<QPointer<QAction>> m_overexposurePreviouslyEnabledActions;
    bool m_toolsPanelWasVisibleBeforeOverexposure = true;

    QToolButton *m_redEyeToolButton = nullptr;
    QToolButton *m_overexposureToolButton = nullptr;
    QToolButton *m_whiteBalanceToolButton = nullptr;
    QToolButton *m_hueSaturationToolButton = nullptr;
    QToolButton *m_hmsColorBalanceToolButton = nullptr;

};

#endif // MAINWINDOW_H
