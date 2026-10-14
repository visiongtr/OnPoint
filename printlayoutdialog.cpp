#include "printlayoutdialog.h"

#include "printpreviewwidget.h"

#include <QAbstractPrintDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSetupDialog>
#include <QPageSize>
#include <QPrintDialog>
#include <QPrinterInfo>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr double MillimetersPerInch = 25.4;
constexpr int CustomPaperSizeIndex = 8;

QList<QPageSize> commonPaperSizes()
{
    return {
        QPageSize(QPageSize::Letter),
        QPageSize(
            QSizeF(4.0, 6.0),
            QPageSize::Inch,
            "4 x 6 in Photo"
            ),
        QPageSize(
            QSizeF(5.0, 7.0),
            QPageSize::Inch,
            "5 x 7 in Photo"
            ),
        QPageSize(QPageSize::Imperial8x10),
        QPageSize(QPageSize::Legal),
        QPageSize(QPageSize::Tabloid),
        QPageSize(QPageSize::A4),
        QPageSize(QPageSize::A5)
    };
}

int matchingCommonPaperSizeIndex(const QPageSize &pageSize)
{
    const QList<QPageSize> sizes = commonPaperSizes();

    for (int index = 0; index < sizes.size(); ++index) {
        if (pageSize.isEquivalentTo(sizes.at(index))) {
            return index;
        }
    }

    return CustomPaperSizeIndex;
}

class WheelToControlsScrollFilter final : public QObject
{
public:
    explicit WheelToControlsScrollFilter(
        QScrollArea *scrollArea,
        QObject *parent = nullptr
        )
        : QObject(parent)
        , m_scrollArea(scrollArea)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::Wheel || !m_scrollArea) {
            return QObject::eventFilter(watched, event);
        }

        QWheelEvent *wheelEvent =
            static_cast<QWheelEvent *>(event);

        QScrollBar *scrollBar =
            m_scrollArea->verticalScrollBar();

        if (!scrollBar) {
            wheelEvent->accept();
            return true;
        }

        int scrollAmount = 0;

        if (!wheelEvent->pixelDelta().isNull()) {
            scrollAmount = wheelEvent->pixelDelta().y();
        } else if (!wheelEvent->angleDelta().isNull()) {
            int wheelDelta = wheelEvent->angleDelta().y();

            if (wheelDelta == 0) {
                wheelDelta = wheelEvent->angleDelta().x();
            }

            const int direction =
                wheelDelta > 0 ? 1 : (wheelDelta < 0 ? -1 : 0);

            scrollAmount =
                direction * scrollBar->singleStep() * 3;
        }

        if (scrollAmount != 0) {
            scrollBar->setValue(
                scrollBar->value() - scrollAmount
                );
        }

        wheelEvent->accept();
        return true;
    }

private:
    QScrollArea *m_scrollArea = nullptr;
};

QString pageSizeName(const QPrinter &printer)
{
    const QPageLayout layout = printer.pageLayout();
    const QPageSize pageSize = layout.pageSize();

    QSizeF sizeInches = pageSize.size(QPageSize::Inch);

    if (layout.orientation() == QPageLayout::Landscape) {
        sizeInches.transpose();
    }

    QString name = pageSize.name().trimmed();

    if (name.isEmpty()) {
        name = "Custom";
    }

    return QString("%1 — %2 x %3 in")
        .arg(name)
        .arg(sizeInches.width(), 0, 'f', 2)
        .arg(sizeInches.height(), 0, 'f', 2);
}

QString printerDisplayName(const QPrinter &printer)
{
    if (!printer.printerName().trimmed().isEmpty()) {
        return printer.printerName();
    }

    return "Default system printer";
}

} // namespace

PrintLayoutDialog::PrintLayoutDialog(
    const QImage &image,
    int sourceDotsPerMeterX,
    int sourceDotsPerMeterY,
    const QString &documentName,
    QWidget *parent
    )
    : QDialog(parent)
    , m_image(image)
    , m_sourceDotsPerMeterX(sourceDotsPerMeterX)
    , m_sourceDotsPerMeterY(sourceDotsPerMeterY)
    , m_documentName(documentName)
    , m_printer(QPrinter::HighResolution)
{
    setWindowTitle("Print Layout & Preview");
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setModal(true);
    setSizeGripEnabled(true);

    // Keep the first opening size inside the available desktop, including
    // systems using larger display scaling. The dialog can still be resized.
    QSize initialSize(1120, 760);
    QSize minimumSize(720, 500);

    QScreen *targetScreen = nullptr;

    if (parentWidget()) {
        targetScreen = parentWidget()->screen();
    }

    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (targetScreen) {
        const QSize availableSize =
            targetScreen->availableGeometry().size();

        initialSize = initialSize.boundedTo(
            QSize(
                std::max(640, availableSize.width() - 48),
                std::max(460, availableSize.height() - 48)
                )
            );

        minimumSize = minimumSize.boundedTo(
            QSize(
                std::max(640, availableSize.width() - 48),
                std::max(460, availableSize.height() - 48)
                )
            );
    }

    setMinimumSize(minimumSize);
    resize(initialSize);

    m_printer.setFullPage(true);

    // On Point is currently a US-focused Linux application, so start with
    // US Letter instead of inheriting Qt/CUPS' A4 fallback when no system
    // default printer has been configured. Match the initial orientation to
    // the image so a landscape photo opens as 11 x 8.5 inches.
    QPageLayout initialLayout = m_printer.pageLayout();
    initialLayout.setPageSize(QPageSize(QPageSize::Letter));
    initialLayout.setOrientation(
        m_image.width() > m_image.height()
            ? QPageLayout::Landscape
            : QPageLayout::Portrait
        );
    m_printer.setPageLayout(initialLayout);

    m_printer.setDocName(
        m_documentName.isEmpty()
            ? QString("On Point Photo")
            : m_documentName
        );

    buildUi();
    setControlsFromSettings();
    updatePrinterSummary();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::focusPrintButton()
{
    if (m_printNowButton) {
        m_printNowButton->setFocus();
    }
}

QDoubleSpinBox *PrintLayoutDialog::createMarginSpinBox()
{
    QDoubleSpinBox *spinBox = new QDoubleSpinBox(this);
    spinBox->setRange(0.0, 4.0);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.05);
    spinBox->setSuffix(" in");
    return spinBox;
}

void PrintLayoutDialog::buildUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(14);

    QScrollArea *controlsScroll = new QScrollArea(this);
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlsScroll->setMinimumWidth(285);
    controlsScroll->setMaximumWidth(380);
    controlsScroll->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Expanding
        );

    QWidget *controlsWidget = new QWidget;
    controlsWidget->setMinimumWidth(270);
    controlsWidget->setSizePolicy(
        QSizePolicy::Preferred,
        QSizePolicy::Preferred
        );

    QVBoxLayout *controlsLayout =
        new QVBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(10);

    QGroupBox *printerGroup =
        new QGroupBox("Printer and Paper", controlsWidget);
    QVBoxLayout *printerLayout =
        new QVBoxLayout(printerGroup);

    m_printerLabel = new QLabel(printerGroup);
    m_printerLabel->setWordWrap(true);

    m_paperLabel = new QLabel(printerGroup);
    m_paperLabel->setWordWrap(true);

    m_paperSizeCombo = new QComboBox(printerGroup);
    m_paperSizeCombo->addItem("US Letter — 8.5 x 11 in");
    m_paperSizeCombo->addItem("Photo — 4 x 6 in");
    m_paperSizeCombo->addItem("Photo — 5 x 7 in");
    m_paperSizeCombo->addItem("Photo — 8 x 10 in");
    m_paperSizeCombo->addItem("US Legal — 8.5 x 14 in");
    m_paperSizeCombo->addItem("Tabloid — 11 x 17 in");
    m_paperSizeCombo->addItem("A4 — 210 x 297 mm");
    m_paperSizeCombo->addItem("A5 — 148 x 210 mm");
    m_paperSizeCombo->addItem("Other / Custom via Page Setup...");

    m_orientationCombo = new QComboBox(printerGroup);
    m_orientationCombo->addItem("Portrait");
    m_orientationCombo->addItem("Landscape");

    m_colorModeCombo = new QComboBox(printerGroup);
    m_colorModeCombo->addItem("Color");
    m_colorModeCombo->addItem("Grayscale / black and white");

    QPushButton *printerOptionsButton =
        new QPushButton("Choose Printer / Properties...", printerGroup);

    QPushButton *pageSetupButton =
        new QPushButton("Page Setup...", printerGroup);

    QLabel *driverHint = new QLabel(
        "Paper type, tray, print quality, borderless mode, and Canon-specific color controls are supplied by the selected printer driver. Open Properties in the printer dialog when those options are available.",
        printerGroup
        );
    driverHint->setWordWrap(true);

    printerLayout->addWidget(m_printerLabel);
    printerLayout->addWidget(m_paperLabel);
    printerLayout->addWidget(new QLabel("Paper size", printerGroup));
    printerLayout->addWidget(m_paperSizeCombo);
    printerLayout->addWidget(new QLabel("Orientation", printerGroup));
    printerLayout->addWidget(m_orientationCombo);
    printerLayout->addWidget(new QLabel("Output", printerGroup));
    printerLayout->addWidget(m_colorModeCombo);
    printerLayout->addWidget(pageSetupButton);
    printerLayout->addWidget(printerOptionsButton);
    printerLayout->addWidget(driverHint);

    QGroupBox *sizeGroup =
        new QGroupBox("Image Size and Placement", controlsWidget);
    QVBoxLayout *sizeLayout =
        new QVBoxLayout(sizeGroup);

    m_scaleModeCombo = new QComboBox(sizeGroup);
    m_scaleModeCombo->addItem("Fit to printable area");
    m_scaleModeCombo->addItem("Actual size");

    m_preserveAspectCheck =
        new QCheckBox("Preserve aspect ratio", sizeGroup);

    m_centerImageCheck =
        new QCheckBox("Center image", sizeGroup);

    m_centerButton =
        new QPushButton("Center Now", sizeGroup);

    QLabel *dragHint = new QLabel(
        "Drag the image directly on the page to place it at the top, bottom, side, or any custom position.",
        sizeGroup
        );
    dragHint->setWordWrap(true);

    sizeLayout->addWidget(new QLabel("Sizing mode", sizeGroup));
    sizeLayout->addWidget(m_scaleModeCombo);
    sizeLayout->addWidget(m_preserveAspectCheck);
    sizeLayout->addWidget(m_centerImageCheck);
    sizeLayout->addWidget(m_centerButton);
    sizeLayout->addWidget(dragHint);

    QGroupBox *marginGroup =
        new QGroupBox("Margins", controlsWidget);
    QVBoxLayout *marginOuterLayout =
        new QVBoxLayout(marginGroup);

    m_useMarginsCheck =
        new QCheckBox("Use margins", marginGroup);

    m_showMarginGuideCheck =
        new QCheckBox("Show margin guide", marginGroup);

    QFormLayout *marginForm = new QFormLayout;

    m_leftMarginSpin = createMarginSpinBox();
    m_topMarginSpin = createMarginSpinBox();
    m_rightMarginSpin = createMarginSpinBox();
    m_bottomMarginSpin = createMarginSpinBox();

    marginForm->addRow("Left", m_leftMarginSpin);
    marginForm->addRow("Top", m_topMarginSpin);
    marginForm->addRow("Right", m_rightMarginSpin);
    marginForm->addRow("Bottom", m_bottomMarginSpin);

    QLabel *marginNote = new QLabel(
        "Turning margins off uses the printer driver's full reported printable area. Hardware-unprintable edges are still respected unless the driver enables borderless printing.",
        marginGroup
        );
    marginNote->setWordWrap(true);

    marginOuterLayout->addWidget(m_useMarginsCheck);
    marginOuterLayout->addWidget(m_showMarginGuideCheck);
    marginOuterLayout->addLayout(marginForm);
    marginOuterLayout->addWidget(marginNote);

    m_statusLabel = new QLabel(controlsWidget);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumHeight(44);

    controlsLayout->addWidget(printerGroup);
    controlsLayout->addWidget(sizeGroup);
    controlsLayout->addWidget(marginGroup);
    controlsLayout->addWidget(m_statusLabel);
    controlsLayout->addStretch(1);

    QGridLayout *outputButtons = new QGridLayout;
    outputButtons->setContentsMargins(0, 0, 0, 0);
    outputButtons->setHorizontalSpacing(8);
    outputButtons->setVerticalSpacing(6);

    QPushButton *savePdfButton =
        new QPushButton("Save PDF...", controlsWidget);

    m_printNowButton =
        new QPushButton("Print Now", controlsWidget);
    m_printNowButton->setDefault(true);

    QPushButton *closeButton =
        new QPushButton("Close", controlsWidget);

    savePdfButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );
    m_printNowButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );
    closeButton->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Fixed
        );

    outputButtons->addWidget(savePdfButton, 0, 0);
    outputButtons->addWidget(m_printNowButton, 0, 1);
    outputButtons->addWidget(closeButton, 1, 0, 1, 2);

    controlsLayout->addLayout(outputButtons);

    m_previewWidget = new PrintPreviewWidget(this);
    m_previewWidget->setPrinter(&m_printer);
    m_previewWidget->setImage(
        m_image,
        m_sourceDotsPerMeterX,
        m_sourceDotsPerMeterY
        );

    controlsScroll->setWidget(controlsWidget);

    mainLayout->addWidget(controlsScroll);
    mainLayout->addWidget(m_previewWidget, 1);

    connect(m_scaleModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &PrintLayoutDialog::controlsChanged);

    connect(m_paperSizeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &PrintLayoutDialog::changePaperSize);

    connect(m_orientationCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &PrintLayoutDialog::changeOrientation);

    connect(m_colorModeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &PrintLayoutDialog::changeColorMode);

    connect(m_preserveAspectCheck,
            &QCheckBox::toggled,
            this,
            &PrintLayoutDialog::controlsChanged);

    connect(m_centerImageCheck,
            &QCheckBox::toggled,
            this,
            &PrintLayoutDialog::controlsChanged);

    connect(m_useMarginsCheck,
            &QCheckBox::toggled,
            this,
            &PrintLayoutDialog::controlsChanged);

    connect(m_showMarginGuideCheck,
            &QCheckBox::toggled,
            this,
            &PrintLayoutDialog::controlsChanged);

    const QList<QDoubleSpinBox *> marginSpins = {
        m_leftMarginSpin,
        m_topMarginSpin,
        m_rightMarginSpin,
        m_bottomMarginSpin
    };

    for (QDoubleSpinBox *spinBox : marginSpins) {
        connect(spinBox,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this,
                &PrintLayoutDialog::controlsChanged);
    }

    // The mouse wheel belongs to the controls panel's scrollbar. Prevent
    // wheel motion over spin boxes or combo boxes from silently changing
    // print settings while the user is only trying to scroll the panel.
    WheelToControlsScrollFilter *wheelFilter =
        new WheelToControlsScrollFilter(controlsScroll, this);

    const QList<QWidget *> wheelProtectedControls = {
        m_paperSizeCombo,
        m_orientationCombo,
        m_colorModeCombo,
        m_scaleModeCombo,
        m_leftMarginSpin,
        m_topMarginSpin,
        m_rightMarginSpin,
        m_bottomMarginSpin
    };

    for (QWidget *control : wheelProtectedControls) {
        control->installEventFilter(wheelFilter);
    }

    connect(m_centerButton,
            &QPushButton::clicked,
            this,
            &PrintLayoutDialog::centerImage);

    connect(pageSetupButton,
            &QPushButton::clicked,
            this,
            &PrintLayoutDialog::openPageSetup);

    connect(printerOptionsButton,
            &QPushButton::clicked,
            this,
            &PrintLayoutDialog::choosePrinterAndOptions);

    connect(savePdfButton,
            &QPushButton::clicked,
            this,
            &PrintLayoutDialog::savePdf);

    connect(m_printNowButton,
            &QPushButton::clicked,
            this,
            &PrintLayoutDialog::printNow);

    connect(closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::reject);

    connect(m_previewWidget,
            &PrintPreviewWidget::layoutSettingsChanged,
            this,
            &PrintLayoutDialog::previewSettingsChanged);
}

void PrintLayoutDialog::setControlsFromSettings()
{
    const QSignalBlocker blockScale(m_scaleModeCombo);
    const QSignalBlocker blockAspect(m_preserveAspectCheck);
    const QSignalBlocker blockCenter(m_centerImageCheck);
    const QSignalBlocker blockUseMargins(m_useMarginsCheck);
    const QSignalBlocker blockGuide(m_showMarginGuideCheck);
    const QSignalBlocker blockLeft(m_leftMarginSpin);
    const QSignalBlocker blockTop(m_topMarginSpin);
    const QSignalBlocker blockRight(m_rightMarginSpin);
    const QSignalBlocker blockBottom(m_bottomMarginSpin);
    const QSignalBlocker blockPaperSize(m_paperSizeCombo);
    const QSignalBlocker blockOrientation(m_orientationCombo);
    const QSignalBlocker blockColorMode(m_colorModeCombo);

    m_scaleModeCombo->setCurrentIndex(
        m_settings.scaleMode ==
                OnPointPrint::ScaleMode::FitToPrintableArea
            ? 0
            : 1
        );

    m_preserveAspectCheck->setChecked(
        m_settings.preserveAspectRatio
        );

    m_centerImageCheck->setChecked(
        m_settings.centerImage
        );

    m_useMarginsCheck->setChecked(
        m_settings.useMargins
        );

    m_showMarginGuideCheck->setChecked(
        m_settings.showMarginGuide
        );

    m_leftMarginSpin->setValue(
        m_settings.marginsMillimeters.left() /
        MillimetersPerInch
        );

    m_topMarginSpin->setValue(
        m_settings.marginsMillimeters.top() /
        MillimetersPerInch
        );

    m_rightMarginSpin->setValue(
        m_settings.marginsMillimeters.right() /
        MillimetersPerInch
        );

    m_bottomMarginSpin->setValue(
        m_settings.marginsMillimeters.bottom() /
        MillimetersPerInch
        );

    m_paperSizeCombo->setCurrentIndex(
        matchingCommonPaperSizeIndex(
            m_printer.pageLayout().pageSize()
            )
        );

    m_orientationCombo->setCurrentIndex(
        m_printer.pageLayout().orientation() ==
                QPageLayout::Portrait
            ? 0
            : 1
        );

    m_colorModeCombo->setCurrentIndex(
        m_printer.colorMode() == QPrinter::GrayScale
            ? 1
            : 0
        );

    const bool fitMode =
        m_settings.scaleMode ==
        OnPointPrint::ScaleMode::FitToPrintableArea;

    m_preserveAspectCheck->setEnabled(fitMode);

    if (!fitMode) {
        m_preserveAspectCheck->setChecked(true);
    }

    const bool marginsEnabled =
        m_settings.useMargins;

    m_leftMarginSpin->setEnabled(marginsEnabled);
    m_topMarginSpin->setEnabled(marginsEnabled);
    m_rightMarginSpin->setEnabled(marginsEnabled);
    m_bottomMarginSpin->setEnabled(marginsEnabled);
}

void PrintLayoutDialog::updateSettingsFromControls()
{
    m_settings.scaleMode =
        m_scaleModeCombo->currentIndex() == 0
            ? OnPointPrint::ScaleMode::FitToPrintableArea
            : OnPointPrint::ScaleMode::ActualSize;

    m_settings.preserveAspectRatio =
        m_scaleModeCombo->currentIndex() == 0
            ? m_preserveAspectCheck->isChecked()
            : true;

    m_settings.centerImage =
        m_centerImageCheck->isChecked();

    m_settings.useMargins =
        m_useMarginsCheck->isChecked();

    m_settings.showMarginGuide =
        m_showMarginGuideCheck->isChecked();

    m_settings.marginsMillimeters =
        QMarginsF(
            m_leftMarginSpin->value() * MillimetersPerInch,
            m_topMarginSpin->value() * MillimetersPerInch,
            m_rightMarginSpin->value() * MillimetersPerInch,
            m_bottomMarginSpin->value() * MillimetersPerInch
            );
}

void PrintLayoutDialog::controlsChanged()
{
    updateSettingsFromControls();
    setControlsFromSettings();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::previewSettingsChanged(
    const OnPointPrint::LayoutSettings &settings
    )
{
    m_settings = settings;
    setControlsFromSettings();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::centerImage()
{
    m_settings.centerImage = true;
    m_settings.horizontalPosition = 0.5;
    m_settings.verticalPosition = 0.5;

    setControlsFromSettings();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::changePaperSize(int index)
{
    if (index == CustomPaperSizeIndex) {
        openPageSetup();
        return;
    }

    const QList<QPageSize> sizes = commonPaperSizes();

    if (index < 0 || index >= sizes.size()) {
        setControlsFromSettings();
        return;
    }

    QPageLayout layout = m_printer.pageLayout();
    layout.setPageSize(sizes.at(index));
    m_printer.setPageLayout(layout);

    updatePrinterSummary();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::changeOrientation(int index)
{
    QPageLayout layout = m_printer.pageLayout();
    layout.setOrientation(
        index == 0
            ? QPageLayout::Portrait
            : QPageLayout::Landscape
        );

    m_printer.setPageLayout(layout);
    updatePrinterSummary();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::changeColorMode(int index)
{
    m_printer.setColorMode(
        index == 1
            ? QPrinter::GrayScale
            : QPrinter::Color
        );

    updatePrinterSummary();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::openPageSetup()
{
    QPageSetupDialog dialog(&m_printer, this);
    dialog.setWindowTitle("Page Setup");

    if (dialog.exec() != QDialog::Accepted) {
        setControlsFromSettings();
        return;
    }

    QPageLayout layout = m_printer.pageLayout();
    layout.setUnits(QPageLayout::Millimeter);

    m_settings.marginsMillimeters =
        layout.margins();

    setControlsFromSettings();
    updatePrinterSummary();
    updatePreviewAndStatus();
}

void PrintLayoutDialog::choosePrinterAndOptions()
{
    QPrintDialog dialog(&m_printer, this);
    dialog.setWindowTitle("Choose Printer and Driver Options");

    // On Linux/X11 with CUPS this exposes the page-size/margins pane, and
    // the selected printer's Properties button can expose driver or IPP
    // capabilities such as media type, tray, quality, borderless, and color.
    dialog.setOption(
        QAbstractPrintDialog::PrintShowPageSize,
        true
        );

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_printer.setFullPage(true);

    setControlsFromSettings();
    updatePrinterSummary();
    updatePreviewAndStatus();

    m_statusLabel->setText(
        "Printer settings updated. Review the refreshed preview, then choose Print Now."
        );
}

void PrintLayoutDialog::savePdf()
{
    QString suggestedName =
        m_documentName.trimmed();

    if (suggestedName.isEmpty()) {
        suggestedName = "OnPoint-Print";
    }

    if (!suggestedName.endsWith(".pdf", Qt::CaseInsensitive)) {
        suggestedName += ".pdf";
    }

    const QString filePath =
        QFileDialog::getSaveFileName(
            this,
            "Save Print as PDF",
            suggestedName,
            "PDF Files (*.pdf)"
            );

    if (filePath.isEmpty()) {
        return;
    }

    QString finalPath = filePath;

    if (!finalPath.endsWith(".pdf", Qt::CaseInsensitive)) {
        finalPath += ".pdf";
    }

    QPrinter pdfPrinter(QPrinter::HighResolution);
    pdfPrinter.setOutputFormat(QPrinter::PdfFormat);
    pdfPrinter.setOutputFileName(finalPath);
    pdfPrinter.setDocName(m_printer.docName());
    pdfPrinter.setFullPage(true);
    pdfPrinter.setPageLayout(
        m_printer.pageLayout()
        );

    renderToPrinter(
        pdfPrinter,
        "PDF saved successfully."
        );
}

void PrintLayoutDialog::printNow()
{
    if (m_printer.printerName().trimmed().isEmpty()) {
        QMessageBox::information(
            this,
            "Choose a Printer",
            "Choose Printer & Driver Options before printing to a physical printer."
            );
        return;
    }

    renderToPrinter(
        m_printer,
        "Print job sent to the printer."
        );
}

void PrintLayoutDialog::updatePrinterSummary()
{
    m_printerLabel->setText(
        "Printer: " + printerDisplayName(m_printer)
        );

    const QString orientation =
        m_printer.pageLayout().orientation() ==
                QPageLayout::Portrait
            ? "Portrait"
            : "Landscape";

    const QString colorMode =
        m_printer.colorMode() == QPrinter::GrayScale
            ? "Grayscale"
            : "Color";

    m_paperLabel->setText(
        "Paper: " + pageSizeName(m_printer) +
        " — " + orientation +
        " — " + colorMode
        );
}

void PrintLayoutDialog::updatePreviewAndStatus()
{
    m_previewWidget->setLayoutSettings(
        m_settings
        );

    const OnPointPrint::LayoutResult result =
        OnPointPrint::calculateLayout(
            m_printer,
            m_image,
            m_sourceDotsPerMeterX,
            m_sourceDotsPerMeterY,
            m_settings
            );

    QStringList messages;

    if (m_settings.scaleMode ==
        OnPointPrint::ScaleMode::ActualSize) {
        messages << QString("Actual size: %1 DPI")
                        .arg(result.actualSizeDpi, 0, 'f', 1);

        if (result.usedFallbackDpi) {
            messages <<
                "No reliable embedded resolution was found, so Actual Size is using the 300 DPI fallback.";
        }
    }

    if (m_settings.scaleMode ==
            OnPointPrint::ScaleMode::FitToPrintableArea &&
        !m_settings.preserveAspectRatio) {
        messages <<
            "Preserve Aspect Ratio is off, so the image will stretch to the entire active print area without cropping.";
    }

    if (result.imageWillBeClipped) {
        messages <<
            "Warning: part of the image lies outside the printer's printable area and will be clipped. Choose Fit to printable area or reposition the image.";
    }

    if (messages.isEmpty()) {
        messages <<
            "Preview and output use the same full-resolution layout calculation.";
    }

    m_statusLabel->setText(
        messages.join(" ")
        );
}

bool PrintLayoutDialog::renderToPrinter(
    QPrinter &printer,
    const QString &successMessage
    )
{
    QString errorMessage;

    const OnPointPrint::LayoutResult previewLayout =
        OnPointPrint::calculateLayout(
            m_printer,
            m_image,
            m_sourceDotsPerMeterX,
            m_sourceDotsPerMeterY,
            m_settings
            );

    if (!OnPointPrint::renderImage(
            printer,
            m_image,
            m_sourceDotsPerMeterX,
            m_sourceDotsPerMeterY,
            m_settings,
            &errorMessage,
            &previewLayout
            )) {
        QMessageBox::warning(
            this,
            "Print Failed",
            errorMessage
            );
        return false;
    }

    m_statusLabel->setText(successMessage);
    return true;
}
