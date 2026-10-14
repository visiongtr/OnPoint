#ifndef IMAGEIMPORTSERVICE_H
#define IMAGEIMPORTSERVICE_H

#include <QByteArray>
#include <QImage>
#include <QMap>
#include <QSize>
#include <QString>
#include <QStringList>

namespace ImageImport
{

enum class SourceKind
{
    ImportedFile,
    NewCanvas
};

struct SourceInfo
{
    SourceKind kind =
        SourceKind::ImportedFile;

    QString sourceFilePath;
    QString sourceFileName;
    QString sourceBaseName;

    QByteArray sourceFormat;

    qint64 sourceFileSize = 0;

    QSize encodedPixelSize;
    QSize decodedPixelSize;

    // QImageReader does not consistently expose the original file's
    // channel bit depth. Keep an explicit field ready for future RAW
    // and high-bit-depth decoders rather than pretending it is known.
    int sourceBitDepth = 0;
    bool sourceBitDepthKnown = false;

    int decodedDepth = 0;

    bool hasAlpha = false;
    bool autoTransformApplied = false;
    int sourceTransformation = 0;

    int dotsPerMeterX = 0;
    int dotsPerMeterY = 0;

    QString colorSpaceDescription;
    QMap<QString, QString> textMetadata;

    bool isImportedFile() const;
    bool isValid() const;
};

struct ImportResult
{
    bool success = false;

    QImage image;
    SourceInfo sourceInfo;

    QString errorMessage;
};

QString openFileFilter();
QStringList supportedExtensions();

bool canImportFile(
    const QString &filePath
    );

ImportResult importImage(
    const QString &filePath
    );

QString formatDisplayName(
    const QByteArray &format
    );

} // namespace ImageImport

#endif // IMAGEIMPORTSERVICE_H
