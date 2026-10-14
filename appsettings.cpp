#include "appsettings.h"

#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QtGlobal>

namespace
{

constexpr const char *organizationName =
    "OnPoint";

constexpr const char *applicationName =
    "OnPointPhotoEditor";

constexpr const char *filenameTagKey =
    "file/filenameTag";

constexpr const char *defaultSaveAsFormatKey =
    "file/defaultSaveAsFormat";

constexpr const char *defaultExportFormatKey =
    "file/defaultExportFormat";

constexpr const char *lastSaveAsDirectoryKey =
    "file/lastSaveAsDirectory";

constexpr const char *lastExportDirectoryKey =
    "file/lastExportDirectory";

constexpr const char *exportJpegQualityKey =
    "export/jpegQuality";

constexpr const char *exportWebpQualityKey =
    "export/webpQuality";

constexpr const char *exportPngCompressionKey =
    "export/pngCompression";

constexpr const char *exportWebpLosslessKey =
    "export/webpLossless";

constexpr const char *exportProgressiveJpegKey =
    "export/progressiveJpeg";

constexpr const char *exportPreserveTransparencyKey =
    "export/preserveTransparency";

constexpr const char *exportFlattenColorKey =
    "export/flattenColor";

constexpr const char *exportCopyAvailableMetadataKey =
    "export/copyAvailableMetadata";

constexpr const char *themeKey =
    "appearance/theme";

constexpr const char *toolsPanelSideKey =
    "appearance/toolsPanelSide";

constexpr const char *performanceModeKey =
    "performance/mode";

constexpr const char *showSystemOptimizerAtStartupKey =
    "performance/showSystemOptimizerAtStartup";

constexpr const char *optimizeSystemOnNextLaunchKey =
    "performance/optimizeSystemOnNextLaunch";

QSettings makeSettings()
{
    return QSettings(
        organizationName,
        applicationName
        );
}

QString lightThemeStyleSheet()
{
    // A complete companion theme rather than an inverted dark-theme patch.
    // The image workspace remains black so photographs are judged against
    // a neutral viewing area in either theme.
    return QString::fromUtf8(
        R"QSS(
QWidget {
    background-color: #E8E4DC;
    color: #242628;
    selection-background-color: #A58B69;
    selection-color: #171717;
}

QMainWindow,
QDialog {
    background-color: #E8E4DC;
}

QMenuBar {
    background-color: #DDD7CD;
    color: #242628;
    border-bottom: 1px solid #B8AEA0;
}

QMenuBar::item {
    background: transparent;
    padding: 5px 9px;
}

QMenuBar::item:selected {
    background-color: #CFC6B9;
}

QMenu {
    background-color: #EEEAE3;
    color: #242628;
    border: 1px solid #AFA69A;
}

QMenu::item {
    padding: 6px 28px 6px 22px;
}

QMenu::item:selected {
    background-color: #D7CEC1;
}

QToolBar {
    background-color: #D9D3C9;
    border: 0;
    border-bottom: 1px solid #B8AEA0;
    spacing: 4px;
    padding: 3px;
}

QToolBar::separator {
    background-color: #B6AB9C;
    width: 1px;
    margin: 4px 6px;
}

QDockWidget {
    background-color: #E5E0D7;
    color: #242628;
}

QGroupBox {
    background-color: #E1DCD3;
    border: 1px solid #B5AA9C;
    border-radius: 5px;
    margin-top: 12px;
    padding-top: 8px;
    font-weight: bold;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 9px;
    padding: 0 5px;
    background-color: #D4CDC2;
    color: #3A3027;
}

QLabel {
    background-color: transparent;
}

QPushButton,
QToolButton {
    background-color: #D8D1C6;
    color: #242628;
    border: 1px solid #AFA496;
    border-radius: 4px;
    padding: 5px 9px;
}

QPushButton:hover,
QToolButton:hover {
    background-color: #E3DDD4;
    border-color: #A58B69;
}

QPushButton:pressed,
QToolButton:pressed,
QPushButton:checked,
QToolButton:checked {
    background-color: #C9BEAF;
    border-color: #765638;
}

QPushButton:disabled,
QToolButton:disabled {
    background-color: #DDD8D0;
    color: #99948D;
    border-color: #C4BDB4;
}

QLineEdit,
QSpinBox,
QDoubleSpinBox,
QComboBox,
QTextEdit,
QPlainTextEdit {
    background-color: #F7F5F1;
    color: #202224;
    border: 1px solid #AFA69A;
    border-radius: 3px;
    padding: 4px;
}

QLineEdit:focus,
QSpinBox:focus,
QDoubleSpinBox:focus,
QComboBox:focus,
QTextEdit:focus,
QPlainTextEdit:focus {
    border-color: #765638;
}

QComboBox::drop-down {
    border: 0;
    width: 22px;
}

QComboBox QAbstractItemView {
    background-color: #F4F1EC;
    color: #242628;
    selection-background-color: #CBBEAC;
    selection-color: #242628;
}

QTabWidget::pane {
    background-color: #E2DDD4;
    border: 1px solid #B5AA9C;
    border-radius: 4px;
    top: -1px;
}

QTabBar::tab {
    background-color: #D2CBC0;
    color: #4A4743;
    border: 1px solid #B5AA9C;
    border-bottom: 0;
    padding: 7px 16px;
    margin-right: 2px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
}

QTabBar::tab:selected {
    background-color: #E8E4DC;
    color: #3A2B20;
    border-color: #765638;
}

QTabBar::tab:hover:!selected {
    background-color: #DDD6CB;
}

QSlider::groove:horizontal {
    height: 4px;
    background-color: #BEB6AB;
    border-radius: 2px;
}

QSlider::sub-page:horizontal {
    background-color: #765638;
    border-radius: 2px;
}

QSlider::add-page:horizontal {
    background-color: #BEB6AB;
    border-radius: 2px;
}

QSlider::handle:horizontal {
    background-color: #A58B69;
    border: 1px solid #765638;
    width: 13px;
    margin: -5px 0;
    border-radius: 6px;
}

QScrollArea,
QAbstractScrollArea {
    border: 0;
}

QScrollBar:vertical {
    background-color: #D5CFC6;
    width: 12px;
    margin: 0;
}

QScrollBar::handle:vertical {
    background-color: #A99F92;
    min-height: 24px;
    border-radius: 5px;
}

QScrollBar:horizontal {
    background-color: #D5CFC6;
    height: 12px;
    margin: 0;
}

QScrollBar::handle:horizontal {
    background-color: #A99F92;
    min-width: 24px;
    border-radius: 5px;
}

QScrollBar::add-line,
QScrollBar::sub-line {
    width: 0;
    height: 0;
}

QStatusBar {
    background-color: #D8D2C8;
    color: #343638;
    border-top: 1px solid #B8AEA0;
}

QToolTip {
    background-color: #FFFDF7;
    color: #242628;
    border: 1px solid #765638;
    padding: 4px;
}

#previewScrollArea,
#previewScrollAreaContents,
#imagePreviewLabel {
    background-color: #000000;
}
)QSS"
        );
}

QString themeToString(
    AppSettings::Theme theme
    )
{
    return theme ==
                   AppSettings::Theme::Light
        ? "light"
        : "dark";
}

AppSettings::Theme themeFromString(
    const QString &value
    )
{
    return value.compare(
               "light",
               Qt::CaseInsensitive
               ) == 0
        ? AppSettings::Theme::Light
        : AppSettings::Theme::Dark;
}

QString sideToString(
    AppSettings::ToolsPanelSide side
    )
{
    return side ==
                   AppSettings::ToolsPanelSide::Left
        ? "left"
        : "right";
}

AppSettings::ToolsPanelSide sideFromString(
    const QString &value
    )
{
    return value.compare(
               "left",
               Qt::CaseInsensitive
               ) == 0
        ? AppSettings::ToolsPanelSide::Left
        : AppSettings::ToolsPanelSide::Right;
}

QString performanceToString(
    AppSettings::PerformanceMode mode
    )
{
    switch (mode) {
    case AppSettings::PerformanceMode::Compatibility:
        return "compatibility";

    case AppSettings::PerformanceMode::Performance:
        return "performance";

    case AppSettings::PerformanceMode::MaximumPerformance:
        return "maximum";

    case AppSettings::PerformanceMode::Balanced:
    default:
        return "balanced";
    }
}

AppSettings::PerformanceMode performanceFromString(
    const QString &value
    )
{
    if (value.compare(
            "compatibility",
            Qt::CaseInsensitive
            ) == 0) {
        return AppSettings::
            PerformanceMode::Compatibility;
    }

    if (value.compare(
            "performance",
            Qt::CaseInsensitive
            ) == 0) {
        return AppSettings::
            PerformanceMode::Performance;
    }

    if (value.compare(
            "maximum",
            Qt::CaseInsensitive
            ) == 0) {
        return AppSettings::
            PerformanceMode::MaximumPerformance;
    }

    return AppSettings::
        PerformanceMode::Balanced;
}

} // namespace

namespace AppSettings
{

QString filenameTag()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        filenameTagKey,
        "_OP"
        ).toString();
}

void setFilenameTag(
    const QString &tag
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        filenameTagKey,
        tag
        );

    settings.sync();
}

QString defaultSaveAsFormat()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        defaultSaveAsFormatKey,
        "png"
        ).toString();
}

void setDefaultSaveAsFormat(
    const QString &formatId
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        defaultSaveAsFormatKey,
        formatId.trimmed().toLower()
        );

    settings.sync();
}

QString defaultExportFormat()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        defaultExportFormatKey,
        "png"
        ).toString();
}

void setDefaultExportFormat(
    const QString &formatId
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        defaultExportFormatKey,
        formatId.trimmed().toLower()
        );

    settings.sync();
}

QString lastSaveAsDirectory()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        lastSaveAsDirectoryKey
        ).toString();
}

void setLastSaveAsDirectory(
    const QString &directory
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        lastSaveAsDirectoryKey,
        directory
        );

    settings.sync();
}

QString lastExportDirectory()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        lastExportDirectoryKey
        ).toString();
}

void setLastExportDirectory(
    const QString &directory
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        lastExportDirectoryKey,
        directory
        );

    settings.sync();
}

int exportJpegQuality()
{
    QSettings settings =
        makeSettings();

    return qBound(
        1,
        settings.value(
            exportJpegQualityKey,
            95
            ).toInt(),
        100
        );
}

void setExportJpegQuality(
    int quality
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportJpegQualityKey,
        qBound(1, quality, 100)
        );

    settings.sync();
}

int exportWebpQuality()
{
    QSettings settings =
        makeSettings();

    return qBound(
        1,
        settings.value(
            exportWebpQualityKey,
            90
            ).toInt(),
        100
        );
}

void setExportWebpQuality(
    int quality
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportWebpQualityKey,
        qBound(1, quality, 100)
        );

    settings.sync();
}

int exportPngCompression()
{
    QSettings settings =
        makeSettings();

    return qBound(
        0,
        settings.value(
            exportPngCompressionKey,
            6
            ).toInt(),
        9
        );
}

void setExportPngCompression(
    int compression
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportPngCompressionKey,
        qBound(0, compression, 9)
        );

    settings.sync();
}

bool exportWebpLossless()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        exportWebpLosslessKey,
        false
        ).toBool();
}

void setExportWebpLossless(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportWebpLosslessKey,
        enabled
        );

    settings.sync();
}

bool exportProgressiveJpeg()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        exportProgressiveJpegKey,
        false
        ).toBool();
}

void setExportProgressiveJpeg(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportProgressiveJpegKey,
        enabled
        );

    settings.sync();
}

bool exportPreserveTransparency()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        exportPreserveTransparencyKey,
        true
        ).toBool();
}

void setExportPreserveTransparency(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportPreserveTransparencyKey,
        enabled
        );

    settings.sync();
}

QColor exportFlattenColor()
{
    QSettings settings =
        makeSettings();

    const QColor color(
        settings.value(
            exportFlattenColorKey,
            "#FFFFFFFF"
            ).toString()
        );

    return color.isValid()
        ? color
        : QColor(Qt::white);
}

void setExportFlattenColor(
    const QColor &color
    )
{
    QSettings settings =
        makeSettings();

    QColor opaqueColor = color;
    opaqueColor.setAlpha(255);

    settings.setValue(
        exportFlattenColorKey,
        opaqueColor.name(
            QColor::HexArgb
            )
        );

    settings.sync();
}

bool exportCopyAvailableMetadata()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        exportCopyAvailableMetadataKey,
        true
        ).toBool();
}

void setExportCopyAvailableMetadata(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        exportCopyAvailableMetadataKey,
        enabled
        );

    settings.sync();
}

Theme theme()
{
    QSettings settings =
        makeSettings();

    return themeFromString(
        settings.value(
            themeKey,
            "dark"
            ).toString()
        );
}

void setTheme(
    Theme selectedTheme
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        themeKey,
        themeToString(
            selectedTheme
            )
        );

    settings.sync();
}

ToolsPanelSide toolsPanelSide()
{
    QSettings settings =
        makeSettings();

    return sideFromString(
        settings.value(
            toolsPanelSideKey,
            "right"
            ).toString()
        );
}

void setToolsPanelSide(
    ToolsPanelSide side
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        toolsPanelSideKey,
        sideToString(
            side
            )
        );

    settings.sync();
}

PerformanceMode performanceMode()
{
    QSettings settings =
        makeSettings();

    return performanceFromString(
        settings.value(
            performanceModeKey,
            "balanced"
            ).toString()
        );
}

void setPerformanceMode(
    PerformanceMode mode
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        performanceModeKey,
        performanceToString(
            mode
            )
        );

    settings.sync();
}

bool showSystemOptimizerAtStartup()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        showSystemOptimizerAtStartupKey,
        true
        ).toBool();
}

void setShowSystemOptimizerAtStartup(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        showSystemOptimizerAtStartupKey,
        enabled
        );

    settings.sync();
}

bool optimizeSystemOnNextLaunch()
{
    QSettings settings =
        makeSettings();

    return settings.value(
        optimizeSystemOnNextLaunchKey,
        false
        ).toBool();
}

void setOptimizeSystemOnNextLaunch(
    bool enabled
    )
{
    QSettings settings =
        makeSettings();

    settings.setValue(
        optimizeSystemOnNextLaunchKey,
        enabled
        );

    settings.sync();
}

void applyTheme()
{
    if (!qApp) {
        return;
    }

    if (theme() ==
        Theme::Light) {
        qApp->setStyleSheet(
            lightThemeStyleSheet()
            );

        return;
    }

    QFile styleFile(
        ":/styles/onpoint.qss"
        );

    if (!styleFile.open(
            QIODevice::ReadOnly |
            QIODevice::Text
            )) {
        qApp->setStyleSheet(
            QString()
            );

        return;
    }

    qApp->setStyleSheet(
        QString::fromUtf8(
            styleFile.readAll()
            )
        );
}

} // namespace AppSettings
