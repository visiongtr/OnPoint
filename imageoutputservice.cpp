#include "imageoutputservice.h"

#include <QDir>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>
#include <QtGlobal>

namespace
{

QByteArray writerFormat(
    ImageOutput::Format format
    )
{
    switch (format) {
    case ImageOutput::Format::Jpeg:
        return "jpg";

    case ImageOutput::Format::WebP:
        return "webp";

    case ImageOutput::Format::Tiff:
        return "tiff";

    case ImageOutput::Format::Bmp:
        return "bmp";

    case ImageOutput::Format::Png:
    default:
        return "png";
    }
}

bool supportedFormatListContains(
    const QList<QByteArray> &formats,
    const QByteArray &candidate
    )
{
    for (const QByteArray &format : formats) {
        if (format.compare(
                candidate,
                Qt::CaseInsensitive
                ) == 0) {
            return true;
        }
    }

    return false;
}

QImage flattenedImage(
    const QImage &source,
    const QColor &backgroundColor
    )
{
    QImage result(
        source.size(),
        QImage::Format_RGB32
        );

    result.fill(
        backgroundColor.rgb()
        );

    result.setDotsPerMeterX(
        source.dotsPerMeterX()
        );

    result.setDotsPerMeterY(
        source.dotsPerMeterY()
        );

    QPainter painter(&result);
    painter.setCompositionMode(
        QPainter::CompositionMode_SourceOver
        );
    painter.drawImage(
        0,
        0,
        source
        );
    painter.end();

    return result;
}

void copyAvailableTextMetadata(
    QImageWriter &writer,
    const QString &sourceFilePath
    )
{
    if (sourceFilePath.isEmpty()) {
        return;
    }

    QImageReader reader(
        sourceFilePath
        );

    const QStringList keys =
        reader.textKeys();

    for (const QString &key : keys) {
        const QString value =
            reader.text(key);

        if (!value.isEmpty()) {
            writer.setText(
                key,
                value
                );
        }
    }
}

} // namespace

namespace ImageOutput
{

QList<Format> allFormats()
{
    return {
        Format::Png,
        Format::Jpeg,
        Format::WebP,
        Format::Tiff,
        Format::Bmp
    };
}

QList<Format> supportedFormats()
{
    const QList<QByteArray> writerFormats =
        QImageWriter::supportedImageFormats();

    QList<Format> result;

    for (Format format : allFormats()) {
        bool supported = false;

        switch (format) {
        case Format::Jpeg:
            supported =
                supportedFormatListContains(
                    writerFormats,
                    "jpg"
                    ) ||
                supportedFormatListContains(
                    writerFormats,
                    "jpeg"
                    );
            break;

        case Format::Tiff:
            supported =
                supportedFormatListContains(
                    writerFormats,
                    "tif"
                    ) ||
                supportedFormatListContains(
                    writerFormats,
                    "tiff"
                    );
            break;

        default:
            supported =
                supportedFormatListContains(
                    writerFormats,
                    writerFormat(format)
                    );
            break;
        }

        if (supported) {
            result.append(format);
        }
    }

    return result;
}

bool isFormatSupported(
    Format format
    )
{
    return supportedFormats().contains(
        format
        );
}

Format bestSupportedFormat(
    Format preferred
    )
{
    const QList<Format> formats =
        supportedFormats();

    if (formats.contains(preferred)) {
        return preferred;
    }

    if (formats.contains(Format::Png)) {
        return Format::Png;
    }

    if (!formats.isEmpty()) {
        return formats.first();
    }

    return Format::Png;
}

QString formatId(
    Format format
    )
{
    switch (format) {
    case Format::Jpeg:
        return "jpeg";

    case Format::WebP:
        return "webp";

    case Format::Tiff:
        return "tiff";

    case Format::Bmp:
        return "bmp";

    case Format::Png:
    default:
        return "png";
    }
}

Format formatFromId(
    const QString &id,
    bool *recognized
    )
{
    const QString normalized =
        id.trimmed().toLower();

    if (recognized) {
        *recognized = true;
    }

    if (normalized == "jpg" ||
        normalized == "jpeg") {
        return Format::Jpeg;
    }

    if (normalized == "webp") {
        return Format::WebP;
    }

    if (normalized == "tif" ||
        normalized == "tiff") {
        return Format::Tiff;
    }

    if (normalized == "bmp") {
        return Format::Bmp;
    }

    if (normalized == "png") {
        return Format::Png;
    }

    if (recognized) {
        *recognized = false;
    }

    return Format::Png;
}

QString displayName(
    Format format
    )
{
    switch (format) {
    case Format::Jpeg:
        return "JPEG";

    case Format::WebP:
        return "WebP";

    case Format::Tiff:
        return "TIFF";

    case Format::Bmp:
        return "BMP";

    case Format::Png:
    default:
        return "PNG";
    }
}

QString shortDescription(
    Format format
    )
{
    switch (format) {
    case Format::Jpeg:
        return "Photos and smaller files";

    case Format::WebP:
        return "Modern compact output";

    case Format::Tiff:
        return "High-quality interchange";

    case Format::Bmp:
        return "Basic compatibility";

    case Format::Png:
    default:
        return "Lossless with transparency";
    }
}

QString extension(
    Format format
    )
{
    switch (format) {
    case Format::Jpeg:
        return "jpg";

    case Format::WebP:
        return "webp";

    case Format::Tiff:
        return "tif";

    case Format::Bmp:
        return "bmp";

    case Format::Png:
    default:
        return "png";
    }
}

Format formatFromExtension(
    const QString &fileExtension,
    bool *recognized
    )
{
    QString normalized =
        fileExtension.trimmed().toLower();

    if (normalized.startsWith('.')) {
        normalized.remove(
            0,
            1
            );
    }

    return formatFromId(
        normalized,
        recognized
        );
}

bool isRecognizedExtension(
    const QString &fileExtension
    )
{
    bool recognized = false;

    formatFromExtension(
        fileExtension,
        &recognized
        );

    return recognized;
}

QString normalizeFilePath(
    const QString &filePath,
    Format format
    )
{
    const QFileInfo info(
        filePath
        );

    QString fileName =
        info.fileName();

    const QString suffix =
        info.suffix();

    if (fileName.isEmpty()) {
        fileName =
            QString("Untitled.%1")
                .arg(extension(format));
    } else if (suffix.isEmpty()) {
        fileName +=
            "." + extension(format);
    } else if (isRecognizedExtension(suffix)) {
        fileName =
            info.completeBaseName() +
            "." + extension(format);
    } else {
        fileName +=
            "." + extension(format);
    }

    if (info.path().isEmpty() ||
        info.path() == ".") {
        return fileName;
    }

    return QDir(info.path()).filePath(
        fileName
        );
}

QString suggestedFileName(
    const QString &sourceFilePath,
    const QString &filenameTag,
    Format format
    )
{
    QString baseName =
        sourceFilePath.isEmpty()
            ? QString("Untitled")
            : QFileInfo(
                  sourceFilePath
                  ).completeBaseName();

    if (baseName.isEmpty()) {
        baseName = "Untitled";
    }

    if (!filenameTag.isEmpty() &&
        !baseName.endsWith(
            filenameTag,
            Qt::CaseSensitive
            )) {
        baseName += filenameTag;
    }

    return QString("%1.%2")
        .arg(
            baseName,
            extension(format)
            );
}

bool formatSupportsTransparency(
    Format format
    )
{
    return format == Format::Png ||
           format == Format::WebP ||
           format == Format::Tiff;
}

bool imageHasTransparency(
    const QImage &image
    )
{
    if (image.isNull() ||
        !image.hasAlphaChannel()) {
        return false;
    }

    const QImage argbImage =
        image.convertToFormat(
            QImage::Format_ARGB32
            );

    for (int y = 0;
         y < argbImage.height();
         ++y) {
        const QRgb *line =
            reinterpret_cast<const QRgb *>(
                argbImage.constScanLine(y)
                );

        for (int x = 0;
             x < argbImage.width();
             ++x) {
            if (qAlpha(line[x]) < 255) {
                return true;
            }
        }
    }

    return false;
}

Options defaultOptions(
    Format format
    )
{
    Options options;
    options.format = format;
    return options;
}

WriteResult writeImage(
    const QImage &image,
    const QString &filePath,
    const Options &options,
    const QString &sourceMetadataPath
    )
{
    WriteResult result;
    result.filePath =
        normalizeFilePath(
            filePath,
            options.format
            );

    if (image.isNull()) {
        result.errorMessage =
            "There is no image data to write.";
        return result;
    }

    if (!isFormatSupported(
            options.format
            )) {
        result.errorMessage =
            QString("The %1 writer is not available on this system.")
                .arg(
                    displayName(options.format)
                    );
        return result;
    }

    const QFileInfo outputInfo(
        result.filePath
        );

    if (!QDir().mkpath(
            outputInfo.absolutePath()
            )) {
        result.errorMessage =
            "Could not create the selected output folder.";
        return result;
    }

    const bool hasTransparency =
        imageHasTransparency(image);

    const bool canPreserveTransparency =
        formatSupportsTransparency(
            options.format
            );

    const bool preserveTransparency =
        options.preserveTransparency &&
        canPreserveTransparency;

    QImage imageToWrite = image;

    if (hasTransparency &&
        !preserveTransparency) {
        imageToWrite =
            flattenedImage(
                image,
                options.flattenColor
                );

        result.transparencyFlattened = true;
    }

    QSaveFile saveFile(
        result.filePath
        );

    if (!saveFile.open(
            QIODevice::WriteOnly
            )) {
        result.errorMessage =
            saveFile.errorString();
        return result;
    }

    {
        QImageWriter writer(
            &saveFile,
            writerFormat(options.format)
            );

        if (writer.supportsOption(
                QImageIOHandler::OptimizedWrite
                )) {
            writer.setOptimizedWrite(true);
        }

        if (options.format == Format::Jpeg ||
            options.format == Format::WebP) {
            const int quality =
                options.format == Format::WebP &&
                        options.webpLossless
                    ? 100
                    : qBound(
                          1,
                          options.quality,
                          100
                          );

            writer.setQuality(quality);
        }

        if (options.format == Format::Png) {
            writer.setCompression(
                qBound(
                    0,
                    options.pngCompression,
                    9
                    )
                );
        }

        if (options.format == Format::Jpeg &&
            writer.supportsOption(
                QImageIOHandler::ProgressiveScanWrite
                )) {
            writer.setProgressiveScanWrite(
                options.progressiveJpeg
                );
        }

        if (options.metadataMode ==
            MetadataMode::CopyAvailableText) {
            copyAvailableTextMetadata(
                writer,
                sourceMetadataPath
                );
        }

        if (!writer.write(
                imageToWrite
                )) {
            result.errorMessage =
                writer.errorString();

            saveFile.cancelWriting();
            return result;
        }
    }

    if (!saveFile.commit()) {
        result.errorMessage =
            saveFile.errorString();
        return result;
    }

    result.success = true;
    return result;
}

} // namespace ImageOutput
