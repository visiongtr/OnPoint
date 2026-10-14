#include "imageoutputdialog.h"

#include "appsettings.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{

QWidget *makeSliderRow(
    const QString &labelText,
    int minimum,
    int maximum,
    int value,
    QLabel **labelOut,
    QSlider **sliderOut,
    QSpinBox **spinBoxOut,
    QWidget *parent
    )
{
    QWidget *row =
        new QWidget(parent);

    QHBoxLayout *layout =
        new QHBoxLayout(row);

    layout->setContentsMargins(
        0,
        0,
        0,
        0
        );

    layout->setSpacing(8);

    QLabel *label =
        new QLabel(
            labelText,
            row
            );

    label->setMinimumWidth(120);

    QSlider *slider =
        new QSlider(
            Qt::Horizontal,
            row
            );

    slider->setRange(
        minimum,
        maximum
        );

    slider->setValue(value);

    QSpinBox *spinBox =
        new QSpinBox(row);

    spinBox->setRange(
        minimum,
        maximum
        );

    spinBox->setValue(value);
    spinBox->setMinimumWidth(72);

    layout->addWidget(label);
    layout->addWidget(slider, 1);
    layout->addWidget(spinBox);

    QObject::connect(
        slider,
        &QSlider::valueChanged,
        spinBox,
        &QSpinBox::setValue
        );

    QObject::connect(
        spinBox,
        QOverload<int>::of(
            &QSpinBox::valueChanged
            ),
        slider,
        &QSlider::setValue
        );

    if (labelOut) {
        *labelOut = label;
    }

    *sliderOut = slider;
    *spinBoxOut = spinBox;

    return row;
}

QString preferredStartingFolder(
    ImageOutputDialog::Mode mode,
    const QString &sourceFilePath
    )
{
    const QFileInfo sourceInfo(
        sourceFilePath
        );

    const QString sourceFolder =
        sourceInfo.absolutePath();

    QString candidate;

    if (mode ==
        ImageOutputDialog::Mode::Export) {
        candidate =
            AppSettings::lastExportDirectory();

        if (!QDir(candidate).exists()) {
            candidate = sourceFolder;
        }
    } else {
        candidate = sourceFolder;

        if (!QDir(candidate).exists()) {
            candidate =
                AppSettings::lastSaveAsDirectory();
        }
    }

    if (!QDir(candidate).exists()) {
        candidate =
            QStandardPaths::writableLocation(
                QStandardPaths::PicturesLocation
                );
    }

    if (!QDir(candidate).exists()) {
        candidate =
            QDir::homePath();
    }

    return candidate;
}

} // namespace

ImageOutputDialog::ImageOutputDialog(
    Mode mode,
    const QImage &image,
    const QString &sourceFilePath,
    QWidget *parent
    )
    : QDialog(parent),
      m_mode(mode),
      m_image(image),
      m_imageHasTransparency(
          ImageOutput::imageHasTransparency(image)
          ),
      m_sourceFilePath(sourceFilePath)
{
    setObjectName(
        "imageOutputDialog"
        );

    setWindowTitle(
        m_mode == Mode::SaveAs
            ? "Save Image As"
            : "Export Image"
        );

    setModal(true);

    resize(
        790,
        m_mode == Mode::Export
            ? 620
            : 640
        );

    setMinimumSize(
        680,
        m_mode == Mode::Export
            ? 540
            : 580
        );

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(
        14,
        14,
        14,
        14
        );

    mainLayout->setSpacing(12);

    QLabel *introLabel =
        new QLabel(
            m_mode == Mode::SaveAs
                ? "Create a new working image file. The saved file becomes the current document."
                : "Create a finished output copy without changing the current document path.",
            this
            );

    introLabel->setWordWrap(true);
    mainLayout->addWidget(introLabel);

    m_tabWidget =
        new QTabWidget(this);

    m_tabWidget->setTabPosition(
        QTabWidget::North
        );

    m_tabWidget->addTab(
        createGeneralTab(),
        "General"
        );

    if (m_mode == Mode::Export) {
        m_tabWidget->addTab(
            createAdvancedTab(),
            "Advanced"
            );
    }

    mainLayout->addWidget(
        m_tabWidget,
        1
        );

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(
            QDialogButtonBox::Cancel,
            this
            );

    QPushButton *acceptButton =
        buttonBox->addButton(
            m_mode == Mode::SaveAs
                ? "Save As"
                : "Export",
            QDialogButtonBox::AcceptRole
            );

    acceptButton->setDefault(true);

    mainLayout->addWidget(buttonBox);

    connect(
        acceptButton,
        &QPushButton::clicked,
        this,
        &ImageOutputDialog::validateAndAccept
        );

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    initializeValues();
}

QString ImageOutputDialog::selectedFilePath() const
{
    return m_selectedFilePath;
}

ImageOutput::Options ImageOutputDialog::outputOptions() const
{
    ImageOutput::Options options =
        ImageOutput::defaultOptions(
            m_format
            );

    options.quality =
        m_format == ImageOutput::Format::Jpeg
            ? m_jpegQuality
            : m_webpQuality;

    options.pngCompression =
        m_pngCompression;

    options.webpLossless =
        m_webpLossless;

    options.progressiveJpeg =
        m_progressiveJpeg;

    options.preserveTransparency =
        m_preserveTransparencyCheckBox
            ? m_preserveTransparencyCheckBox->isChecked()
            : true;

    options.flattenColor =
        m_flattenColor;

    if (m_metadataComboBox &&
        m_metadataComboBox->currentData().toInt() ==
            static_cast<int>(
                ImageOutput::MetadataMode::Remove
                )) {
        options.metadataMode =
            ImageOutput::MetadataMode::Remove;
    }

    return options;
}

QWidget *ImageOutputDialog::createGeneralTab()
{
    QWidget *tab =
        new QWidget(this);

    QVBoxLayout *layout =
        new QVBoxLayout(tab);

    layout->setContentsMargins(
        16,
        16,
        16,
        16
        );

    layout->setSpacing(14);

    QGroupBox *destinationGroup =
        new QGroupBox(
            "Destination",
            tab
            );

    QGridLayout *destinationLayout =
        new QGridLayout(
            destinationGroup
            );

    destinationLayout->setHorizontalSpacing(10);
    destinationLayout->setVerticalSpacing(10);

    QLabel *folderLabel =
        new QLabel(
            "Folder",
            destinationGroup
            );

    m_folderEdit =
        new QLineEdit(
            destinationGroup
            );

    QPushButton *browseButton =
        new QPushButton(
            "Browse…",
            destinationGroup
            );

    QLabel *filenameLabel =
        new QLabel(
            "File name",
            destinationGroup
            );

    m_filenameEdit =
        new QLineEdit(
            destinationGroup
            );

    m_filenameEdit->setMaxLength(220);

    m_filenameEdit->setValidator(
        new QRegularExpressionValidator(
            QRegularExpression(
                R"([^\\/:*?\"<>|]*)"
                ),
            m_filenameEdit
            )
        );

    destinationLayout->addWidget(
        folderLabel,
        0,
        0
        );

    destinationLayout->addWidget(
        m_folderEdit,
        0,
        1
        );

    destinationLayout->addWidget(
        browseButton,
        0,
        2
        );

    destinationLayout->addWidget(
        filenameLabel,
        1,
        0
        );

    destinationLayout->addWidget(
        m_filenameEdit,
        1,
        1,
        1,
        2
        );

    layout->addWidget(destinationGroup);

    QGroupBox *formatGroup =
        new QGroupBox(
            "Format",
            tab
            );

    QVBoxLayout *formatLayout =
        new QVBoxLayout(
            formatGroup
            );

    QHBoxLayout *formatButtonLayout =
        new QHBoxLayout();

    formatButtonLayout->setSpacing(8);

    for (ImageOutput::Format format :
         ImageOutput::allFormats()) {
        QPushButton *button =
            createFormatButton(
                format,
                formatGroup
                );

        formatButtonLayout->addWidget(button);
    }

    formatButtonLayout->addStretch(1);

    formatLayout->addLayout(
        formatButtonLayout
        );

    m_formatDescriptionLabel =
        new QLabel(formatGroup);

    m_formatDescriptionLabel->setWordWrap(true);

    formatLayout->addWidget(
        m_formatDescriptionLabel
        );

    layout->addWidget(formatGroup);

    QGroupBox *summaryGroup =
        new QGroupBox(
            "Output Summary",
            tab
            );

    QFormLayout *summaryLayout =
        new QFormLayout(
            summaryGroup
            );

    summaryLayout->setHorizontalSpacing(18);
    summaryLayout->setVerticalSpacing(7);

    m_summaryNameValue =
        new QLabel(summaryGroup);

    m_summaryFormatValue =
        new QLabel(summaryGroup);

    m_summaryDimensionsValue =
        new QLabel(summaryGroup);

    m_summaryTransparencyValue =
        new QLabel(summaryGroup);

    for (QLabel *label : {
             m_summaryNameValue,
             m_summaryFormatValue,
             m_summaryDimensionsValue,
             m_summaryTransparencyValue
         }) {
        label->setTextInteractionFlags(
            Qt::TextSelectableByMouse
            );
        label->setWordWrap(true);
    }

    summaryLayout->addRow(
        "Output:",
        m_summaryNameValue
        );

    summaryLayout->addRow(
        "Format:",
        m_summaryFormatValue
        );

    summaryLayout->addRow(
        "Dimensions:",
        m_summaryDimensionsValue
        );

    summaryLayout->addRow(
        "Transparency:",
        m_summaryTransparencyValue
        );

    layout->addWidget(summaryGroup);
    layout->addStretch(1);

    connect(
        browseButton,
        &QPushButton::clicked,
        this,
        &ImageOutputDialog::browseForFolder
        );

    connect(
        m_filenameEdit,
        &QLineEdit::editingFinished,
        this,
        &ImageOutputDialog::syncFormatFromFilename
        );

    connect(
        m_filenameEdit,
        &QLineEdit::textChanged,
        this,
        &ImageOutputDialog::updateSummary
        );

    connect(
        m_folderEdit,
        &QLineEdit::textChanged,
        this,
        &ImageOutputDialog::updateSummary
        );

    return tab;
}

QWidget *ImageOutputDialog::createAdvancedTab()
{
    QWidget *tab =
        new QWidget(this);

    QVBoxLayout *layout =
        new QVBoxLayout(tab);

    layout->setContentsMargins(
        16,
        16,
        16,
        16
        );

    layout->setSpacing(14);

    QGroupBox *formatOptionsGroup =
        new QGroupBox(
            "Format Options",
            tab
            );

    QVBoxLayout *formatOptionsLayout =
        new QVBoxLayout(
            formatOptionsGroup
            );

    m_qualityRow =
        makeSliderRow(
            "Quality",
            1,
            100,
            95,
            &m_qualityLabel,
            &m_qualitySlider,
            &m_qualitySpinBox,
            formatOptionsGroup
            );

    formatOptionsLayout->addWidget(
        m_qualityRow
        );

    m_compressionRow =
        makeSliderRow(
            "PNG Compression",
            0,
            9,
            6,
            nullptr,
            &m_compressionSlider,
            &m_compressionSpinBox,
            formatOptionsGroup
            );

    formatOptionsLayout->addWidget(
        m_compressionRow
        );

    m_webpLosslessCheckBox =
        new QCheckBox(
            "Lossless WebP",
            formatOptionsGroup
            );

    formatOptionsLayout->addWidget(
        m_webpLosslessCheckBox
        );

    m_progressiveJpegCheckBox =
        new QCheckBox(
            "Progressive JPEG",
            formatOptionsGroup
            );

    formatOptionsLayout->addWidget(
        m_progressiveJpegCheckBox
        );

    m_advancedFormatHintLabel =
        new QLabel(
            formatOptionsGroup
            );

    m_advancedFormatHintLabel->setWordWrap(true);

    formatOptionsLayout->addWidget(
        m_advancedFormatHintLabel
        );

    layout->addWidget(formatOptionsGroup);

    QGroupBox *transparencyGroup =
        new QGroupBox(
            "Transparency",
            tab
            );

    QVBoxLayout *transparencyLayout =
        new QVBoxLayout(
            transparencyGroup
            );

    m_preserveTransparencyCheckBox =
        new QCheckBox(
            "Preserve transparent pixels when the format supports them",
            transparencyGroup
            );

    transparencyLayout->addWidget(
        m_preserveTransparencyCheckBox
        );

    QHBoxLayout *flattenLayout =
        new QHBoxLayout();

    flattenLayout->addWidget(
        new QLabel(
            "Flatten color",
            transparencyGroup
            )
        );

    m_flattenColorButton =
        new QPushButton(
            transparencyGroup
            );

    m_flattenColorButton->setMinimumWidth(150);

    flattenLayout->addWidget(
        m_flattenColorButton
        );

    flattenLayout->addStretch(1);

    transparencyLayout->addLayout(
        flattenLayout
        );

    QLabel *transparencyNote =
        new QLabel(
            "JPEG and BMP cannot keep transparent pixels. On Point will flatten them onto the selected color and warn before writing.",
            transparencyGroup
            );

    transparencyNote->setWordWrap(true);

    transparencyLayout->addWidget(
        transparencyNote
        );

    layout->addWidget(transparencyGroup);

    QGroupBox *metadataGroup =
        new QGroupBox(
            "Metadata",
            tab
            );

    QVBoxLayout *metadataLayout =
        new QVBoxLayout(
            metadataGroup
            );

    m_metadataComboBox =
        new QComboBox(
            metadataGroup
            );

    m_metadataComboBox->addItem(
        "Copy available text metadata",
        static_cast<int>(
            ImageOutput::MetadataMode::CopyAvailableText
            )
        );

    m_metadataComboBox->addItem(
        "Remove metadata",
        static_cast<int>(
            ImageOutput::MetadataMode::Remove
            )
        );

    metadataLayout->addWidget(
        m_metadataComboBox
        );

    QLabel *metadataNote =
        new QLabel(
            "This version copies metadata that Qt exposes as text. Full camera and EXIF preservation will expand when the Image Import Service records it directly.",
            metadataGroup
            );

    metadataNote->setWordWrap(true);

    metadataLayout->addWidget(
        metadataNote
        );

    layout->addWidget(metadataGroup);
    layout->addStretch(1);

    connect(
        m_qualitySlider,
        &QSlider::valueChanged,
        this,
        [this](int value) {
            if (m_updatingControls) {
                return;
            }

            if (m_format ==
                ImageOutput::Format::Jpeg) {
                m_jpegQuality = value;
            } else if (m_format ==
                       ImageOutput::Format::WebP) {
                m_webpQuality = value;
            }
        });

    connect(
        m_compressionSlider,
        &QSlider::valueChanged,
        this,
        [this](int value) {
            if (!m_updatingControls) {
                m_pngCompression = value;
            }
        });

    connect(
        m_webpLosslessCheckBox,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            if (!m_updatingControls) {
                m_webpLossless = checked;
            }

            if (m_qualityRow &&
                m_format ==
                    ImageOutput::Format::WebP) {
                m_qualityRow->setEnabled(
                    !checked
                    );
            }
        });

    connect(
        m_progressiveJpegCheckBox,
        &QCheckBox::toggled,
        this,
        [this](bool checked) {
            if (!m_updatingControls) {
                m_progressiveJpeg = checked;
            }
        });

    connect(
        m_preserveTransparencyCheckBox,
        &QCheckBox::toggled,
        this,
        &ImageOutputDialog::updateSummary
        );

    connect(
        m_flattenColorButton,
        &QPushButton::clicked,
        this,
        &ImageOutputDialog::chooseFlattenColor
        );

    return tab;
}

QPushButton *ImageOutputDialog::createFormatButton(
    ImageOutput::Format format,
    QWidget *parent
    )
{
    QString buttonText =
        ImageOutput::displayName(
            format
            );

    // Keep the format choices compact and immediately readable.
    // The descriptive sentence below the buttons explains the
    // currently selected format.
    if (format ==
        ImageOutput::Format::Jpeg) {
        buttonText = "JPG";
    }

    QPushButton *button =
        new QPushButton(
            buttonText,
            parent
            );

    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setMinimumSize(112, 42);

    const bool supported =
        ImageOutput::isFormatSupported(
            format
            );

    button->setEnabled(supported);

    if (!supported) {
        button->setToolTip(
            QString("The %1 writer is not installed on this system.")
                .arg(
                    ImageOutput::displayName(format)
                    )
            );
    }

    connect(
        button,
        &QPushButton::clicked,
        this,
        [this, format]() {
            setFormat(
                format,
                true
                );
        });

    m_formatButtons.insert(
        format,
        button
        );

    return button;
}

void ImageOutputDialog::initializeValues()
{
    m_jpegQuality =
        AppSettings::exportJpegQuality();

    m_webpQuality =
        AppSettings::exportWebpQuality();

    m_pngCompression =
        AppSettings::exportPngCompression();

    m_webpLossless =
        AppSettings::exportWebpLossless();

    m_progressiveJpeg =
        AppSettings::exportProgressiveJpeg();

    m_flattenColor =
        AppSettings::exportFlattenColor();

    if (m_folderEdit) {
        m_folderEdit->setText(
            preferredStartingFolder(
                m_mode,
                m_sourceFilePath
                )
            );
    }

    const QString preferredFormatId =
        m_mode == Mode::SaveAs
            ? AppSettings::defaultSaveAsFormat()
            : AppSettings::defaultExportFormat();

    bool recognized = false;

    ImageOutput::Format preferredFormat =
        ImageOutput::formatFromId(
            preferredFormatId,
            &recognized
            );

    if (!recognized) {
        preferredFormat =
            ImageOutput::Format::Png;
    }

    m_format =
        ImageOutput::bestSupportedFormat(
            preferredFormat
            );

    if (m_filenameEdit) {
        m_filenameEdit->setText(
            ImageOutput::suggestedFileName(
                m_sourceFilePath,
                AppSettings::filenameTag(),
                m_format
                )
            );
    }

    if (m_preserveTransparencyCheckBox) {
        m_preserveTransparencyCheckBox->setChecked(
            AppSettings::exportPreserveTransparency()
            );
    }

    if (m_metadataComboBox) {
        const int metadataValue =
            AppSettings::exportCopyAvailableMetadata()
                ? static_cast<int>(
                      ImageOutput::MetadataMode::CopyAvailableText
                      )
                : static_cast<int>(
                      ImageOutput::MetadataMode::Remove
                      );

        const int index =
            m_metadataComboBox->findData(
                metadataValue
                );

        if (index >= 0) {
            m_metadataComboBox->setCurrentIndex(
                index
                );
        }
    }

    setFormat(
        m_format,
        false
        );

    updateFlattenColorButton();
    updateSummary();
}

void ImageOutputDialog::setFormat(
    ImageOutput::Format format,
    bool updateFilename
    )
{
    if (!ImageOutput::isFormatSupported(
            format
            )) {
        return;
    }

    captureFormatSpecificValues();

    m_format = format;

    for (auto iterator =
             m_formatButtons.begin();
         iterator !=
             m_formatButtons.end();
         ++iterator) {
        QSignalBlocker blocker(
            iterator.value()
            );

        iterator.value()->setChecked(
            iterator.key() == m_format
            );
    }

    if (updateFilename &&
        m_filenameEdit) {
        const QString normalizedName =
            QFileInfo(
                ImageOutput::normalizeFilePath(
                    m_filenameEdit->text(),
                    m_format
                    )
                ).fileName();

        QSignalBlocker blocker(
            m_filenameEdit
            );

        m_filenameEdit->setText(
            normalizedName
            );
    }

    loadFormatSpecificValues();
    updateAdvancedControls();
    updateSummary();
}

void ImageOutputDialog::captureFormatSpecificValues()
{
    if (!m_qualitySlider ||
        m_updatingControls) {
        return;
    }

    if (m_format ==
        ImageOutput::Format::Jpeg) {
        m_jpegQuality =
            m_qualitySlider->value();

        m_progressiveJpeg =
            m_progressiveJpegCheckBox &&
            m_progressiveJpegCheckBox->isChecked();
    } else if (m_format ==
               ImageOutput::Format::WebP) {
        m_webpQuality =
            m_qualitySlider->value();

        m_webpLossless =
            m_webpLosslessCheckBox &&
            m_webpLosslessCheckBox->isChecked();
    } else if (m_format ==
               ImageOutput::Format::Png) {
        m_pngCompression =
            m_compressionSlider->value();
    }
}

void ImageOutputDialog::loadFormatSpecificValues()
{
    if (!m_qualitySlider) {
        return;
    }

    m_updatingControls = true;

    if (m_format ==
        ImageOutput::Format::Jpeg) {
        m_qualitySlider->setValue(
            m_jpegQuality
            );

        m_progressiveJpegCheckBox->setChecked(
            m_progressiveJpeg
            );
    } else if (m_format ==
               ImageOutput::Format::WebP) {
        m_qualitySlider->setValue(
            m_webpQuality
            );

        m_webpLosslessCheckBox->setChecked(
            m_webpLossless
            );
    } else if (m_format ==
               ImageOutput::Format::Png) {
        m_compressionSlider->setValue(
            m_pngCompression
            );
    }

    m_updatingControls = false;
}

void ImageOutputDialog::updateAdvancedControls()
{
    if (!m_qualityRow) {
        return;
    }

    const bool jpeg =
        m_format ==
        ImageOutput::Format::Jpeg;

    const bool webp =
        m_format ==
        ImageOutput::Format::WebP;

    const bool png =
        m_format ==
        ImageOutput::Format::Png;

    m_qualityRow->setVisible(
        jpeg || webp
        );

    m_qualityRow->setEnabled(
        !webp ||
        !m_webpLossless
        );

    m_qualityLabel->setText(
        jpeg
            ? "JPEG Quality"
            : "WebP Quality"
        );

    m_compressionRow->setVisible(png);
    m_webpLosslessCheckBox->setVisible(webp);
    m_progressiveJpegCheckBox->setVisible(jpeg);

    QString hint;

    if (png) {
        hint =
            "PNG is lossless. Compression changes file size and writing time, not image quality.";
    } else if (jpeg) {
        hint =
            "JPEG is designed for photographs and does not support transparency.";
    } else if (webp) {
        hint =
            "WebP can use lossy quality or lossless output and can preserve transparency.";
    } else if (m_format ==
               ImageOutput::Format::Tiff) {
        hint =
            "TIFF uses the installed Qt writer's high-quality defaults.";
    } else {
        hint =
            "BMP is a basic compatibility format with few adjustable options.";
    }

    m_advancedFormatHintLabel->setText(hint);

    const bool canPreserve =
        ImageOutput::formatSupportsTransparency(
            m_format
            );

    m_preserveTransparencyCheckBox->setEnabled(
        canPreserve
        );

    const bool flattenControlsNeeded =
        !canPreserve ||
        !m_preserveTransparencyCheckBox->isChecked();

    m_flattenColorButton->setEnabled(
        flattenControlsNeeded
        );
}

void ImageOutputDialog::updateSummary()
{
    if (!m_summaryNameValue) {
        return;
    }

    const QString normalizedPath =
        currentFullPath();

    m_summaryNameValue->setText(
        normalizedPath
        );

    m_summaryFormatValue->setText(
        QString("%1 (.%2)")
            .arg(
                ImageOutput::displayName(
                    m_format
                    ),
                ImageOutput::extension(
                    m_format
                    )
                )
        );

    m_summaryDimensionsValue->setText(
        QString("%1 × %2 pixels")
            .arg(m_image.width())
            .arg(m_image.height())
        );

    QString transparencyText;

    if (!m_imageHasTransparency) {
        transparencyText =
            "No transparent pixels";
    } else if (!outputWillFlattenTransparency()) {
        transparencyText =
            "Preserved";
    } else {
        transparencyText =
            QString("Flattened onto %1")
                .arg(
                    m_flattenColor.name(
                        QColor::HexRgb
                        ).toUpper()
                    );
    }

    m_summaryTransparencyValue->setText(
        transparencyText
        );

    if (m_formatDescriptionLabel) {
        m_formatDescriptionLabel->setText(
            QString("%1 — %2")
                .arg(
                    ImageOutput::displayName(
                        m_format
                        ),
                    ImageOutput::shortDescription(
                        m_format
                        )
                    )
            );
    }

    if (m_preserveTransparencyCheckBox) {
        updateAdvancedControls();
    }
}

void ImageOutputDialog::updateFlattenColorButton()
{
    if (!m_flattenColorButton) {
        return;
    }

    const QString colorName =
        m_flattenColor.name(
            QColor::HexRgb
            ).toUpper();

    const int brightness =
        qGray(
            m_flattenColor.rgb()
            );

    const QString textColor =
        brightness < 125
            ? "#FFFFFF"
            : "#000000";

    m_flattenColorButton->setText(
        colorName
        );

    m_flattenColorButton->setStyleSheet(
        QString(
            "QPushButton { background-color: %1; color: %2; }"
            )
            .arg(
                colorName,
                textColor
                )
        );
}

void ImageOutputDialog::syncFormatFromFilename()
{
    if (!m_filenameEdit) {
        return;
    }

    const QString suffix =
        QFileInfo(
            m_filenameEdit->text()
            ).suffix();

    bool recognized = false;

    const ImageOutput::Format typedFormat =
        ImageOutput::formatFromExtension(
            suffix,
            &recognized
            );

    if (recognized &&
        ImageOutput::isFormatSupported(
            typedFormat
            )) {
        setFormat(
            typedFormat,
            false
            );
    }

    const QString normalizedName =
        QFileInfo(
            ImageOutput::normalizeFilePath(
                m_filenameEdit->text(),
                m_format
                )
            ).fileName();

    QSignalBlocker blocker(
        m_filenameEdit
        );

    m_filenameEdit->setText(
        normalizedName
        );

    updateSummary();
}

void ImageOutputDialog::browseForFolder()
{
    const QString folder =
        QFileDialog::getExistingDirectory(
            this,
            "Choose Output Folder",
            m_folderEdit
                ? m_folderEdit->text()
                : QDir::homePath()
            );

    if (!folder.isEmpty() &&
        m_folderEdit) {
        m_folderEdit->setText(folder);
    }
}

void ImageOutputDialog::chooseFlattenColor()
{
    const QColor color =
        QColorDialog::getColor(
            m_flattenColor,
            this,
            "Choose Transparency Flatten Color",
            QColorDialog::ShowAlphaChannel
            );

    if (!color.isValid()) {
        return;
    }

    m_flattenColor = color;
    m_flattenColor.setAlpha(255);

    updateFlattenColorButton();
    updateSummary();
}

void ImageOutputDialog::validateAndAccept()
{
    syncFormatFromFilename();
    captureFormatSpecificValues();

    const QString folder =
        m_folderEdit
            ? m_folderEdit->text().trimmed()
            : QString();

    const QString fileName =
        m_filenameEdit
            ? m_filenameEdit->text().trimmed()
            : QString();

    if (folder.isEmpty() ||
        !QDir(folder).exists()) {
        QMessageBox::warning(
            this,
            windowTitle(),
            "Choose an existing output folder."
            );
        return;
    }

    if (fileName.isEmpty() ||
        fileName == "." ||
        fileName == "..") {
        QMessageBox::warning(
            this,
            windowTitle(),
            "Enter a valid file name."
            );
        return;
    }

    m_selectedFilePath =
        ImageOutput::normalizeFilePath(
            QDir(folder).filePath(
                fileName
                ),
            m_format
            );

    if (QFileInfo::exists(
            m_selectedFilePath
            )) {
        const QMessageBox::StandardButton replace =
            QMessageBox::question(
                this,
                "Replace Existing File?",
                QString(
                    "A file named %1 already exists.\n\nReplace it?"
                    )
                    .arg(
                        QFileInfo(
                            m_selectedFilePath
                            ).fileName()
                        ),
                QMessageBox::Yes |
                    QMessageBox::No,
                QMessageBox::No
                );

        if (replace != QMessageBox::Yes) {
            return;
        }
    }

    if (m_imageHasTransparency &&
        outputWillFlattenTransparency()) {
        const QMessageBox::StandardButton flatten =
            QMessageBox::warning(
                this,
                "Transparency Will Be Flattened",
                QString(
                    "%1 cannot preserve the current transparency settings.\n\n"
                    "Transparent pixels will be flattened onto %2. Continue?"
                    )
                    .arg(
                        ImageOutput::displayName(
                            m_format
                            ),
                        m_flattenColor.name(
                            QColor::HexRgb
                            ).toUpper()
                        ),
                QMessageBox::Yes |
                    QMessageBox::No,
                QMessageBox::No
                );

        if (flatten != QMessageBox::Yes) {
            return;
        }
    }

    if (m_mode == Mode::SaveAs) {
        AppSettings::setLastSaveAsDirectory(
            folder
            );
    } else {
        AppSettings::setLastExportDirectory(
            folder
            );

        AppSettings::setExportJpegQuality(
            m_jpegQuality
            );

        AppSettings::setExportWebpQuality(
            m_webpQuality
            );

        AppSettings::setExportPngCompression(
            m_pngCompression
            );

        AppSettings::setExportWebpLossless(
            m_webpLossless
            );

        AppSettings::setExportProgressiveJpeg(
            m_progressiveJpeg
            );

        AppSettings::setExportPreserveTransparency(
            m_preserveTransparencyCheckBox->isChecked()
            );

        AppSettings::setExportFlattenColor(
            m_flattenColor
            );

        AppSettings::setExportCopyAvailableMetadata(
            m_metadataComboBox->currentData().toInt() ==
                static_cast<int>(
                    ImageOutput::MetadataMode::CopyAvailableText
                    )
            );
    }

    accept();
}

bool ImageOutputDialog::outputWillFlattenTransparency() const
{
    if (!ImageOutput::formatSupportsTransparency(
            m_format
            )) {
        return true;
    }

    if (!m_preserveTransparencyCheckBox) {
        return false;
    }

    return !m_preserveTransparencyCheckBox->isChecked();
}

QString ImageOutputDialog::currentFullPath() const
{
    if (!m_folderEdit ||
        !m_filenameEdit) {
        return QString();
    }

    return ImageOutput::normalizeFilePath(
        QDir(
            m_folderEdit->text().trimmed()
            ).filePath(
                m_filenameEdit->text().trimmed()
                ),
        m_format
        );
}
