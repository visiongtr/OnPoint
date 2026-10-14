#ifndef IMAGEOUTPUTDIALOG_H
#define IMAGEOUTPUTDIALOG_H

#include "imageoutputservice.h"

#include <QColor>
#include <QDialog>
#include <QImage>
#include <QMap>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;
class QWidget;

class ImageOutputDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode
    {
        SaveAs,
        Export
    };

    explicit ImageOutputDialog(
        Mode mode,
        const QImage &image,
        const QString &sourceFilePath,
        QWidget *parent = nullptr
        );

    QString selectedFilePath() const;
    ImageOutput::Options outputOptions() const;

private:
    QWidget *createGeneralTab();
    QWidget *createAdvancedTab();

    QPushButton *createFormatButton(
        ImageOutput::Format format,
        QWidget *parent
        );

    void initializeValues();

    void setFormat(
        ImageOutput::Format format,
        bool updateFilename
        );

    void captureFormatSpecificValues();
    void loadFormatSpecificValues();

    void updateAdvancedControls();
    void updateSummary();
    void updateFlattenColorButton();

    void syncFormatFromFilename();
    void browseForFolder();
    void chooseFlattenColor();
    void validateAndAccept();

    bool outputWillFlattenTransparency() const;
    QString currentFullPath() const;

    Mode m_mode = Mode::SaveAs;

    QImage m_image;
    bool m_imageHasTransparency = false;

    QString m_sourceFilePath;
    QString m_selectedFilePath;

    ImageOutput::Format m_format =
        ImageOutput::Format::Png;

    QTabWidget *m_tabWidget = nullptr;

    QLineEdit *m_folderEdit = nullptr;
    QLineEdit *m_filenameEdit = nullptr;

    QMap<ImageOutput::Format, QPushButton *>
        m_formatButtons;

    QLabel *m_formatDescriptionLabel = nullptr;

    QLabel *m_summaryNameValue = nullptr;
    QLabel *m_summaryFormatValue = nullptr;
    QLabel *m_summaryDimensionsValue = nullptr;
    QLabel *m_summaryTransparencyValue = nullptr;

    QWidget *m_qualityRow = nullptr;
    QLabel *m_qualityLabel = nullptr;
    QSlider *m_qualitySlider = nullptr;
    QSpinBox *m_qualitySpinBox = nullptr;

    QWidget *m_compressionRow = nullptr;
    QSlider *m_compressionSlider = nullptr;
    QSpinBox *m_compressionSpinBox = nullptr;

    QCheckBox *m_webpLosslessCheckBox = nullptr;
    QCheckBox *m_progressiveJpegCheckBox = nullptr;

    QLabel *m_advancedFormatHintLabel = nullptr;

    QCheckBox *m_preserveTransparencyCheckBox = nullptr;
    QPushButton *m_flattenColorButton = nullptr;

    QComboBox *m_metadataComboBox = nullptr;

    int m_jpegQuality = 95;
    int m_webpQuality = 90;
    int m_pngCompression = 6;

    bool m_webpLossless = false;
    bool m_progressiveJpeg = false;

    QColor m_flattenColor = Qt::white;

    bool m_updatingControls = false;
};

#endif // IMAGEOUTPUTDIALOG_H
