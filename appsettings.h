#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QColor>
#include <QString>

namespace AppSettings
{

enum class Theme
{
    Dark,
    Light
};

enum class ToolsPanelSide
{
    Right,
    Left
};

enum class PerformanceMode
{
    Compatibility,
    Balanced,
    Performance,
    MaximumPerformance
};

QString filenameTag();
void setFilenameTag(
    const QString &tag
    );

QString defaultSaveAsFormat();
void setDefaultSaveAsFormat(
    const QString &formatId
    );

QString defaultExportFormat();
void setDefaultExportFormat(
    const QString &formatId
    );

QString lastSaveAsDirectory();
void setLastSaveAsDirectory(
    const QString &directory
    );

QString lastExportDirectory();
void setLastExportDirectory(
    const QString &directory
    );

int exportJpegQuality();
void setExportJpegQuality(
    int quality
    );

int exportWebpQuality();
void setExportWebpQuality(
    int quality
    );

int exportPngCompression();
void setExportPngCompression(
    int compression
    );

bool exportWebpLossless();
void setExportWebpLossless(
    bool enabled
    );

bool exportProgressiveJpeg();
void setExportProgressiveJpeg(
    bool enabled
    );

bool exportPreserveTransparency();
void setExportPreserveTransparency(
    bool enabled
    );

QColor exportFlattenColor();
void setExportFlattenColor(
    const QColor &color
    );

bool exportCopyAvailableMetadata();
void setExportCopyAvailableMetadata(
    bool enabled
    );

Theme theme();
void setTheme(
    Theme theme
    );

ToolsPanelSide toolsPanelSide();
void setToolsPanelSide(
    ToolsPanelSide side
    );

PerformanceMode performanceMode();
void setPerformanceMode(
    PerformanceMode mode
    );

bool showSystemOptimizerAtStartup();
void setShowSystemOptimizerAtStartup(
    bool enabled
    );

bool optimizeSystemOnNextLaunch();
void setOptimizeSystemOnNextLaunch(
    bool enabled
    );

void applyTheme();

} // namespace AppSettings

#endif // APPSETTINGS_H
