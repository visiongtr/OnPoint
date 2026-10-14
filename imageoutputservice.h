#ifndef IMAGEOUTPUTSERVICE_H
#define IMAGEOUTPUTSERVICE_H

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

namespace ImageOutput
{

enum class Format
{
    Png,
    Jpeg,
    WebP,
    Tiff,
    Bmp
};

enum class MetadataMode
{
    CopyAvailableText,
    Remove
};

struct Options
{
    Format format = Format::Png;

    int quality = 95;
    int pngCompression = 6;

    bool webpLossless = false;
    bool progressiveJpeg = false;

    bool preserveTransparency = true;
    QColor flattenColor = Qt::white;

    MetadataMode metadataMode =
        MetadataMode::CopyAvailableText;
};

struct WriteResult
{
    bool success = false;
    bool transparencyFlattened = false;

    QString filePath;
    QString errorMessage;
};

QList<Format> allFormats();
QList<Format> supportedFormats();

bool isFormatSupported(
    Format format
    );

Format bestSupportedFormat(
    Format preferred
    );

QString formatId(
    Format format
    );

Format formatFromId(
    const QString &id,
    bool *recognized = nullptr
    );

QString displayName(
    Format format
    );

QString shortDescription(
    Format format
    );

QString extension(
    Format format
    );

Format formatFromExtension(
    const QString &extension,
    bool *recognized = nullptr
    );

bool isRecognizedExtension(
    const QString &extension
    );

QString normalizeFilePath(
    const QString &filePath,
    Format format
    );

QString suggestedFileName(
    const QString &sourceFilePath,
    const QString &filenameTag,
    Format format
    );

bool formatSupportsTransparency(
    Format format
    );

bool imageHasTransparency(
    const QImage &image
    );

Options defaultOptions(
    Format format
    );

WriteResult writeImage(
    const QImage &image,
    const QString &filePath,
    const Options &options,
    const QString &sourceMetadataPath = QString()
    );

} // namespace ImageOutput

#endif // IMAGEOUTPUTSERVICE_H
