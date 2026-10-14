#ifndef PRINTLAYOUTDIALOG_H
#define PRINTLAYOUTDIALOG_H

#include "printservice.h"

#include <QDialog>
#include <QImage>
#include <QPrinter>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class PrintPreviewWidget;

class PrintLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrintLayoutDialog(
        const QImage &image,
        int sourceDotsPerMeterX,
        int sourceDotsPerMeterY,
        const QString &documentName,
        QWidget *parent = nullptr
        );

    void focusPrintButton();

private slots:
    void controlsChanged();
    void previewSettingsChanged(
        const OnPointPrint::LayoutSettings &settings
        );

    void centerImage();
    void changePaperSize(int index);
    void changeOrientation(int index);
    void changeColorMode(int index);
    void openPageSetup();
    void choosePrinterAndOptions();
    void savePdf();
    void printNow();

private:
    QDoubleSpinBox *createMarginSpinBox();

    void buildUi();
    void setControlsFromSettings();
    void updateSettingsFromControls();
    void updatePrinterSummary();
    void updatePreviewAndStatus();

    bool renderToPrinter(
        QPrinter &printer,
        const QString &successMessage
        );

    QImage m_image;
    int m_sourceDotsPerMeterX = 0;
    int m_sourceDotsPerMeterY = 0;
    QString m_documentName;

    QPrinter m_printer;
    OnPointPrint::LayoutSettings m_settings;

    PrintPreviewWidget *m_previewWidget = nullptr;

    QLabel *m_printerLabel = nullptr;
    QLabel *m_paperLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QComboBox *m_scaleModeCombo = nullptr;
    QComboBox *m_paperSizeCombo = nullptr;
    QComboBox *m_orientationCombo = nullptr;
    QComboBox *m_colorModeCombo = nullptr;

    QCheckBox *m_preserveAspectCheck = nullptr;
    QCheckBox *m_centerImageCheck = nullptr;
    QCheckBox *m_useMarginsCheck = nullptr;
    QCheckBox *m_showMarginGuideCheck = nullptr;

    QDoubleSpinBox *m_leftMarginSpin = nullptr;
    QDoubleSpinBox *m_topMarginSpin = nullptr;
    QDoubleSpinBox *m_rightMarginSpin = nullptr;
    QDoubleSpinBox *m_bottomMarginSpin = nullptr;

    QPushButton *m_centerButton = nullptr;
    QPushButton *m_printNowButton = nullptr;
};

#endif // PRINTLAYOUTDIALOG_H
