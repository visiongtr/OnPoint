#include "imageimportservice.h"

#include <QColorSpace>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QSet>
#include <QtGlobal>

#include <algorithm>

namespace
{

QString normalizedExtension(
    const QByteArray &format
    )
{
    QString extension =
        QString::fromLatin1(
            format
            ).trimmed().toLower();

    if (extension ==
        "jpeg") {
        extension = "jpg";
    } else if (extension ==
               "tiff") {
        extension = "tif";
    }

    return extension;
}

QString colorSpaceDescriptionForImage(
    const QImage &image
    )
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QColorSpace colorSpace =
        image.colorSpace();

    if (!colorSpace.isValid()) {
        return "Unspecified";
    }

    const QString description =
        colorSpace.description().trimmed();

    if (!description.isEmpty()) {
        return description;
    }

    return "Embedded color profile";
#else
    Q_UNUSED(image);
    return "Unavailable in this Qt build";
#endif
}

QMap<QString, QString> readTextMetadata(
    QImageReader &reader
    )
{
    QMap<QString, QString> metadata;

    const QStringList keys =
        reader.textKeys();

    for (const QString &key : keys) {
        const QString value =
            reader.text(key);

        if (!value.isEmpty()) {
            metadata.insert(
                key,
                value
                );
        }
    }

    return metadata;
}

} // namespace

namespace ImageImport
{

bool SourceInfo::isImportedFile() const
{
    return kind ==
           SourceKind::ImportedFile;
}

bool SourceInfo::isValid() const
{
    if (kind ==
        SourceKind::NewCanvas) {
        return decodedPixelSize.isValid();
    }

    return !sourceFilePath.isEmpty() &&
           decodedPixelSize.isValid();
}

QStringList supportedExtensions()
{
    const QList<QByteArray> formats =
        QImageReader::supportedImageFormats();

    QSet<QString> uniqueExtensions;

    for (const QByteArray &format : formats) {
        const QString extension =
            normalizedExtension(
                format
                );

        if (!extension.isEmpty()) {
            uniqueExtensions.insert(
                extension
                );
        }
    }

    // Keep familiar alternate extensions available when their
    // corresponding Qt reader is installed.
    if (uniqueExtensions.contains(
            "jpg"
            )) {
        uniqueExtensions.insert(
            "jpeg"
            );
    }

    if (uniqueExtensions.contains(
            "tif"
            )) {
        uniqueExtensions.insert(
            "tiff"
            );
    }

    QStringList extensions =
        uniqueExtensions.values();

    std::sort(
        extensions.begin(),
        extensions.end()
        );

    return extensions;
}

QString openFileFilter()
{
    const QStringList extensions =
        supportedExtensions();

    QStringList wildcardParts;

    for (const QString &extension : extensions) {
        wildcardParts.append(
            "*." + extension
            );
    }

    if (wildcardParts.isEmpty()) {
        return "All Files (*)";
    }

    return QString(
        "Supported Images (%1);;All Files (*)"
        )
        .arg(
            wildcardParts.join(
                ' '
                )
            );
}

bool canImportFile(
    const QString &filePath
    )
{
    const QFileInfo fileInfo(
        filePath
        );

    if (!fileInfo.exists() ||
        !fileInfo.isFile()) {
        return false;
    }

    QImageReader reader(
        filePath
        );

    reader.setDecideFormatFromContent(
        true
        );

    return reader.canRead();
}

ImportResult importImage(
    const QString &filePath
    )
{
    ImportResult result;

    const QFileInfo fileInfo(
        filePath
        );

    if (!fileInfo.exists() ||
        !fileInfo.isFile()) {
        result.errorMessage =
            "The selected file does not exist or is not a regular file.";

        return result;
    }

    QImageReader reader(
        filePath
        );

    reader.setDecideFormatFromContent(
        true
        );

    reader.setAutoTransform(
        true
        );

    if (!reader.canRead()) {
        result.errorMessage =
            reader.errorString();

        if (result.errorMessage.isEmpty()) {
            result.errorMessage =
                "No installed image decoder could read this file.";
        }

        return result;
    }

    SourceInfo sourceInfo;

    sourceInfo.kind =
        SourceKind::ImportedFile;

    sourceInfo.sourceFilePath =
        fileInfo.absoluteFilePath();

    sourceInfo.sourceFileName =
        fileInfo.fileName();

    sourceInfo.sourceBaseName =
        fileInfo.completeBaseName();

    sourceInfo.sourceFileSize =
        fileInfo.size();

    sourceInfo.encodedPixelSize =
        reader.size();

    const QImageIOHandler::Transformations transformation =
        reader.transformation();

    sourceInfo.sourceTransformation =
        static_cast<int>(
            transformation
            );

    sourceInfo.autoTransformApplied =
        transformation !=
        QImageIOHandler::TransformationNone;

    // Read optional text metadata through a separate reader. The primary
    // reader is consumed by reader.read() below; asking that same PNG reader
    // for text afterward can make libpng attempt another read at end-of-file
    // and print a misleading "libpng error: Read Error" even though the image
    // decoded successfully.
    QImageReader metadataReader(
        filePath
        );

    metadataReader.setDecideFormatFromContent(
        true
        );

    sourceInfo.textMetadata =
        readTextMetadata(
            metadataReader
            );

    QByteArray sourceFormat =
        reader.format().toLower();

    if (sourceFormat.isEmpty()) {
        sourceFormat =
            QImageReader::imageFormat(
                filePath
                ).toLower();
    }

    QImage loadedImage =
        reader.read();

    if (loadedImage.isNull()) {
        result.errorMessage =
            reader.errorString();

        if (result.errorMessage.isEmpty()) {
            result.errorMessage =
                "The decoder returned an empty image.";
        }

        return result;
    }

    if (sourceFormat.isEmpty()) {
        sourceFormat =
            reader.format().toLower();
    }

    sourceInfo.sourceFormat =
        sourceFormat;

    sourceInfo.decodedPixelSize =
        loadedImage.size();

    sourceInfo.decodedDepth =
        loadedImage.depth();

    sourceInfo.hasAlpha =
        loadedImage.hasAlphaChannel();

    sourceInfo.dotsPerMeterX =
        loadedImage.dotsPerMeterX();

    sourceInfo.dotsPerMeterY =
        loadedImage.dotsPerMeterY();

    sourceInfo.colorSpaceDescription =
        colorSpaceDescriptionForImage(
            loadedImage
            );

    result.success = true;
    result.image =
        std::move(
            loadedImage
            );

    result.sourceInfo =
        std::move(
            sourceInfo
            );

    return result;
}

QString formatDisplayName(
    const QByteArray &format
    )
{
    const QByteArray normalized =
        format.trimmed().toLower();

    if (normalized == "jpg" ||
        normalized == "jpeg") {
        return "JPEG";
    }

    if (normalized == "png") {
        return "PNG";
    }

    if (normalized == "webp") {
        return "WebP";
    }

    if (normalized == "tif" ||
        normalized == "tiff") {
        return "TIFF";
    }

    if (normalized == "bmp") {
        return "BMP";
    }

    if (normalized == "gif") {
        return "GIF";
    }

    if (normalized.isEmpty()) {
        return "Image";
    }

    return QString::fromLatin1(
        normalized
        ).toUpper();
}

} // namespace ImageImport
