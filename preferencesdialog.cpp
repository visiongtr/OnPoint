#include "preferencesdialog.h"

#include "appsettings.h"
#include "imageoutputservice.h"
#include "imageprocessingruntime.h"
#include "systemprofile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{

template<typename EnumType>
void selectEnumValue(
    QComboBox *comboBox,
    EnumType value
    )
{
    if (!comboBox) {
        return;
    }

    const int index =
        comboBox->findData(
            static_cast<int>(
                value
                )
            );

    if (index >= 0) {
        comboBox->setCurrentIndex(
            index
            );
    }
}

template<typename EnumType>
EnumType selectedEnumValue(
    const QComboBox *comboBox,
    EnumType fallback
    )
{
    if (!comboBox ||
        comboBox->currentIndex() < 0) {
        return fallback;
    }

    return static_cast<EnumType>(
        comboBox->currentData().toInt()
        );
}

QLabel *makeExplanationLabel(
    const QString &text,
    QWidget *parent
    )
{
    QLabel *label =
        new QLabel(
            text,
            parent
            );

    label->setWordWrap(true);
    return label;
}

QString optimizerScheduledStatus()
{
    return AppSettings::optimizeSystemOnNextLaunch()
        ? "Recalibration is scheduled for the next launch."
        : "The saved performance profile is active.";
}

} // namespace

PreferencesDialog::PreferencesDialog(
    QWidget *parent,
    bool showPerformancePage
    )
    : QDialog(parent)
{
    setObjectName(
        "preferencesDialog"
        );

    setWindowTitle(
        "Preferences"
        );

    setModal(true);

    resize(
        860,
        680
        );

    setMinimumSize(
        720,
        560
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

    m_tabWidget =
        new QTabWidget(this);

    m_tabWidget->setTabPosition(
        QTabWidget::North
        );

    m_tabWidget->addTab(
        createAppearanceTab(),
        "Appearance"
        );

    m_tabWidget->addTab(
        createFileTab(),
        "File"
        );

    m_tabWidget->addTab(
        createPerformanceTab(),
        "Performance"
        );

    mainLayout->addWidget(
        m_tabWidget,
        1
        );

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(
            QDialogButtonBox::RestoreDefaults |
                QDialogButtonBox::Ok |
                QDialogButtonBox::Cancel |
                QDialogButtonBox::Apply,
            this
            );

    mainLayout->addWidget(buttonBox);

    connect(
        buttonBox,
        &QDialogButtonBox::accepted,
        this,
        [this]() {
            applySettings();
            accept();
        });

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    connect(
        buttonBox->button(
            QDialogButtonBox::Apply
            ),
        &QPushButton::clicked,
        this,
        &PreferencesDialog::applySettings
        );

    connect(
        buttonBox->button(
            QDialogButtonBox::RestoreDefaults
            ),
        &QPushButton::clicked,
        this,
        &PreferencesDialog::loadDefaultSettings
        );

    loadCurrentSettings();

    if (showPerformancePage) {
        m_tabWidget->setCurrentIndex(2);

        if (m_performanceTabWidget) {
            m_performanceTabWidget->setCurrentIndex(0);
        }
    }
}

QWidget *PreferencesDialog::createAppearanceTab()
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

    layout->addWidget(
        makeExplanationLabel(
            "Choose how On Point looks and where the Tools panel sits. "
            "The dark theme and right-side Tools panel remain the defaults.",
            tab
            )
        );

    QFormLayout *formLayout =
        new QFormLayout();

    formLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow
        );

    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(12);

    m_themeComboBox =
        new QComboBox(tab);

    m_themeComboBox->addItem(
        "Dark (Default)",
        static_cast<int>(
            AppSettings::Theme::Dark
            )
        );

    m_themeComboBox->addItem(
        "Light",
        static_cast<int>(
            AppSettings::Theme::Light
            )
        );

    formLayout->addRow(
        "Theme:",
        m_themeComboBox
        );

    m_toolsSideComboBox =
        new QComboBox(tab);

    m_toolsSideComboBox->addItem(
        "Right (Default)",
        static_cast<int>(
            AppSettings::ToolsPanelSide::Right
            )
        );

    m_toolsSideComboBox->addItem(
        "Left",
        static_cast<int>(
            AppSettings::ToolsPanelSide::Left
            )
        );

    formLayout->addRow(
        "Tools panel:",
        m_toolsSideComboBox
        );

    layout->addLayout(formLayout);
    layout->addStretch(1);

    return tab;
}

QWidget *PreferencesDialog::createFileTab()
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

    layout->addWidget(
        makeExplanationLabel(
            "Choose how On Point builds suggested filenames and which formats "
            "Save As and Export select when their dialogs first open.",
            tab
            )
        );

    QFormLayout *formLayout =
        new QFormLayout();

    formLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow
        );

    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(12);

    m_filenameTagEdit =
        new QLineEdit(tab);

    m_filenameTagEdit->setPlaceholderText(
        "_OP"
        );

    m_filenameTagEdit->setMaxLength(40);

    m_filenameTagEdit->setValidator(
        new QRegularExpressionValidator(
            QRegularExpression(
                R"([^\\/:*?\"<>|]*)"
                ),
            m_filenameTagEdit
            )
        );

    formLayout->addRow(
        "Filename tag:",
        m_filenameTagEdit
        );

    m_saveAsFormatComboBox =
        new QComboBox(tab);

    populateFormatComboBox(
        m_saveAsFormatComboBox
        );

    formLayout->addRow(
        "Default Save As format:",
        m_saveAsFormatComboBox
        );

    m_exportFormatComboBox =
        new QComboBox(tab);

    populateFormatComboBox(
        m_exportFormatComboBox
        );

    formLayout->addRow(
        "Default Export format:",
        m_exportFormatComboBox
        );

    m_filenamePreviewLabel =
        new QLabel(tab);

    formLayout->addRow(
        "Suggested name:",
        m_filenamePreviewLabel
        );

    layout->addLayout(formLayout);

    layout->addWidget(
        makeExplanationLabel(
            "The Export dialog remembers its last quality, compression, "
            "transparency, flatten-color, and metadata settings.",
            tab
            )
        );

    layout->addStretch(1);

    connect(
        m_filenameTagEdit,
        &QLineEdit::textChanged,
        this,
        &PreferencesDialog::updateFilenamePreview
        );

    connect(
        m_saveAsFormatComboBox,
        QOverload<int>::of(
            &QComboBox::currentIndexChanged
            ),
        this,
        &PreferencesDialog::updateFilenamePreview
        );

    return tab;
}

QWidget *PreferencesDialog::createPerformanceTab()
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

    layout->setSpacing(12);

    m_showOptimizerAtStartupCheckBox =
        new QCheckBox(
            "Show the System Optimizer at startup",
            tab
            );

    m_showOptimizerAtStartupCheckBox->setToolTip(
        "When disabled, On Point performs a silent fingerprint check and "
        "opens the optimizer automatically only when recalibration is needed."
        );

    layout->addWidget(m_showOptimizerAtStartupCheckBox);

    layout->addWidget(
        makeExplanationLabel(
            "The startup splash is optional after calibration. On Point still "
            "verifies the saved hardware fingerprint silently and restores the "
            "full optimizer after relevant hardware, driver, or engine changes.",
            tab
            )
        );

    m_performanceTabWidget =
        new QTabWidget(tab);

    // Overview ------------------------------------------------------------
    QWidget *overviewPage =
        new QWidget(m_performanceTabWidget);

    QVBoxLayout *overviewLayout =
        new QVBoxLayout(overviewPage);

    overviewLayout->setContentsMargins(12, 12, 12, 12);
    overviewLayout->setSpacing(10);

    overviewLayout->addWidget(
        makeExplanationLabel(
            "Live system, profile, and active processing information. This is "
            "the permanent control-room view of the same information shown on "
            "the System Optimizer splash.",
            overviewPage
            )
        );

    m_performanceOverviewEdit =
        new QPlainTextEdit(overviewPage);

    m_performanceOverviewEdit->setReadOnly(true);
    m_performanceOverviewEdit->setLineWrapMode(
        QPlainTextEdit::NoWrap
        );
    m_performanceOverviewEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );

    overviewLayout->addWidget(
        m_performanceOverviewEdit,
        1
        );

    m_performanceProfilePathLabel =
        new QLabel(overviewPage);

    m_performanceProfilePathLabel->setWordWrap(true);
    m_performanceProfilePathLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        );

    overviewLayout->addWidget(
        m_performanceProfilePathLabel
        );

    QPushButton *refreshStatusButton =
        new QPushButton(
            "Refresh Live Status",
            overviewPage
            );

    overviewLayout->addWidget(
        refreshStatusButton,
        0,
        Qt::AlignRight
        );

    connect(
        refreshStatusButton,
        &QPushButton::clicked,
        this,
        &PreferencesDialog::refreshPerformanceProfileSummary
        );

    m_performanceTabWidget->addTab(
        overviewPage,
        "Overview"
        );

    // Controls ------------------------------------------------------------
    QWidget *controlsPage =
        new QWidget(m_performanceTabWidget);

    QVBoxLayout *controlsLayout =
        new QVBoxLayout(controlsPage);

    controlsLayout->setContentsMargins(12, 12, 12, 12);
    controlsLayout->setSpacing(12);

    QFormLayout *modeFormLayout =
        new QFormLayout();

    modeFormLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow
        );
    modeFormLayout->setHorizontalSpacing(18);
    modeFormLayout->setVerticalSpacing(12);

    m_performanceComboBox =
        new QComboBox(controlsPage);

    m_performanceComboBox->addItem(
        "Compatibility",
        static_cast<int>(
            AppSettings::PerformanceMode::Compatibility
            )
        );

    m_performanceComboBox->addItem(
        "Balanced (Default)",
        static_cast<int>(
            AppSettings::PerformanceMode::Balanced
            )
        );

    m_performanceComboBox->addItem(
        "Performance",
        static_cast<int>(
            AppSettings::PerformanceMode::Performance
            )
        );

    m_performanceComboBox->addItem(
        "Maximum Performance",
        static_cast<int>(
            AppSettings::PerformanceMode::MaximumPerformance
            )
        );

    modeFormLayout->addRow(
        "Processing mode:",
        m_performanceComboBox
        );

    controlsLayout->addLayout(modeFormLayout);

    controlsLayout->addWidget(
        makeExplanationLabel(
            "The System Optimizer creates a hardware fingerprint and saved "
            "profile. The actions below remain available even when the startup "
            "splash is disabled.",
            controlsPage
            )
        );

    QGridLayout *buttonGrid =
        new QGridLayout();

    QPushButton *saveJsonButton =
        new QPushButton(
            "Save Profile JSON Copy...",
            controlsPage
            );

    QPushButton *saveReportButton =
        new QPushButton(
            "Export System Report...",
            controlsPage
            );

    QPushButton *recalibrateButton =
        new QPushButton(
            "Run System Optimizer Again",
            controlsPage
            );

    QPushButton *showStartupButton =
        new QPushButton(
            "Show Optimizer on Next Launch",
            controlsPage
            );

    buttonGrid->addWidget(saveJsonButton, 0, 0);
    buttonGrid->addWidget(saveReportButton, 0, 1);
    buttonGrid->addWidget(recalibrateButton, 1, 0);
    buttonGrid->addWidget(showStartupButton, 1, 1);

    controlsLayout->addLayout(buttonGrid);

    m_performanceControlStatusLabel =
        new QLabel(controlsPage);

    m_performanceControlStatusLabel->setWordWrap(true);
    controlsLayout->addWidget(m_performanceControlStatusLabel);
    controlsLayout->addStretch(1);

    connect(
        saveJsonButton,
        &QPushButton::clicked,
        this,
        &PreferencesDialog::savePerformanceProfileCopy
        );

    connect(
        saveReportButton,
        &QPushButton::clicked,
        this,
        &PreferencesDialog::savePerformanceSystemReport
        );

    connect(
        recalibrateButton,
        &QPushButton::clicked,
        this,
        &PreferencesDialog::scheduleSystemOptimizer
        );

    connect(
        showStartupButton,
        &QPushButton::clicked,
        this,
        [this]() {
            m_showOptimizerAtStartupCheckBox->setChecked(true);
            AppSettings::setShowSystemOptimizerAtStartup(true);
            refreshPerformanceProfileSummary();

            QMessageBox::information(
                this,
                "Startup Optimizer Enabled",
                "The full System Optimizer will be shown on the next launch."
                );
        });

    m_performanceTabWidget->addTab(
        controlsPage,
        "Controls"
        );

    // Advanced ------------------------------------------------------------
    QWidget *advancedPage =
        new QWidget(m_performanceTabWidget);

    QVBoxLayout *advancedLayout =
        new QVBoxLayout(advancedPage);

    advancedLayout->setContentsMargins(12, 12, 12, 12);
    advancedLayout->setSpacing(12);

    QLabel *warningLabel =
        makeExplanationLabel(
            "Advanced settings override On Point's automatic recommendation. "
            "Incorrect values can reduce performance, increase memory use, or "
            "make the desktop less responsive.",
            advancedPage
            );

    warningLabel->setObjectName(
        "advancedPerformanceWarning"
        );

    advancedLayout->addWidget(warningLabel);

    QGroupBox *advancedGroup =
        new QGroupBox(
            "Manual performance profile",
            advancedPage
            );

    QFormLayout *advancedForm =
        new QFormLayout(advancedGroup);

    advancedForm->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow
        );
    advancedForm->setHorizontalSpacing(18);
    advancedForm->setVerticalSpacing(10);

    m_backendComboBox =
        new QComboBox(advancedGroup);

    m_backendComboBox->addItem(
        "Automatic GPU / CPU selection",
        "auto"
        );
    m_backendComboBox->addItem(
        "CPU only",
        "cpu"
        );
    m_backendComboBox->addItem(
        "Prefer GPU",
        "gpu"
        );

    advancedForm->addRow(
        "Preferred backend:",
        m_backendComboBox
        );

    m_workerThreadsSpinBox =
        new QSpinBox(advancedGroup);

    m_workerThreadsSpinBox->setRange(1, 256);

    advancedForm->addRow(
        "CPU worker threads:",
        m_workerThreadsSpinBox
        );

    m_previewDimensionSpinBox =
        new QSpinBox(advancedGroup);

    m_previewDimensionSpinBox->setRange(800, 4096);
    m_previewDimensionSpinBox->setSingleStep(100);
    m_previewDimensionSpinBox->setSuffix(" px");

    advancedForm->addRow(
        "Interactive preview maximum:",
        m_previewDimensionSpinBox
        );

    m_tileHeightComboBox =
        new QComboBox(advancedGroup);

    for (int tileHeight : {64, 128, 192, 256, 384, 512}) {
        m_tileHeightComboBox->addItem(
            QString("%1 rows").arg(tileHeight),
            tileHeight
            );
    }

    advancedForm->addRow(
        "Processing tile height:",
        m_tileHeightComboBox
        );

    m_cpuFallbackCheckBox =
        new QCheckBox(
            "Always keep dependable CPU fallback enabled",
            advancedGroup
            );

    advancedForm->addRow(
        QString(),
        m_cpuFallbackCheckBox
        );

    advancedLayout->addWidget(advancedGroup);

    QHBoxLayout *advancedButtonLayout =
        new QHBoxLayout();

    QPushButton *applyAdvancedButton =
        new QPushButton(
            "Apply Advanced Profile Now",
            advancedPage
            );

    QPushButton *restoreAutomaticButton =
        new QPushButton(
            "Restore Automatic Recommendations",
            advancedPage
            );

    advancedButtonLayout->addWidget(applyAdvancedButton);
    advancedButtonLayout->addWidget(restoreAutomaticButton);

    advancedLayout->addLayout(advancedButtonLayout);

    m_advancedProfileStatusLabel =
        new QLabel(advancedPage);

    m_advancedProfileStatusLabel->setWordWrap(true);
    advancedLayout->addWidget(m_advancedProfileStatusLabel);
    advancedLayout->addStretch(1);

    connect(
        applyAdvancedButton,
        &QPushButton::clicked,
        this,
        [this]() {
            applyAdvancedPerformanceSettings(true);
        });

    connect(
        restoreAutomaticButton,
        &QPushButton::clicked,
        this,
        &PreferencesDialog::restoreAutomaticPerformanceRecommendations
        );

    m_performanceTabWidget->addTab(
        advancedPage,
        "Advanced"
        );

    layout->addWidget(
        m_performanceTabWidget,
        1
        );

    return tab;
}

void PreferencesDialog::populateFormatComboBox(
    QComboBox *comboBox
    )
{
    if (!comboBox) {
        return;
    }

    for (ImageOutput::Format format :
         ImageOutput::supportedFormats()) {
        comboBox->addItem(
            QString("%1 (.%2)")
                .arg(
                    ImageOutput::displayName(format),
                    ImageOutput::extension(format)
                    ),
            ImageOutput::formatId(format)
            );
    }

    if (comboBox->count() == 0) {
        comboBox->addItem(
            "PNG (.png)",
            "png"
            );
    }
}

void PreferencesDialog::selectFormatId(
    QComboBox *comboBox,
    const QString &formatId
    )
{
    if (!comboBox) {
        return;
    }

    int index =
        comboBox->findData(
            formatId.trimmed().toLower()
            );

    if (index < 0) {
        index =
            comboBox->findData(
                "png"
                );
    }

    if (index < 0 &&
        comboBox->count() > 0) {
        index = 0;
    }

    if (index >= 0) {
        comboBox->setCurrentIndex(index);
    }
}

QString PreferencesDialog::selectedFormatId(
    const QComboBox *comboBox
    ) const
{
    if (!comboBox ||
        comboBox->currentIndex() < 0) {
        return "png";
    }

    return comboBox->currentData().toString();
}

void PreferencesDialog::loadCurrentSettings()
{
    selectEnumValue(
        m_themeComboBox,
        AppSettings::theme()
        );

    selectEnumValue(
        m_toolsSideComboBox,
        AppSettings::toolsPanelSide()
        );

    m_filenameTagEdit->setText(
        AppSettings::filenameTag()
        );

    selectFormatId(
        m_saveAsFormatComboBox,
        AppSettings::defaultSaveAsFormat()
        );

    selectFormatId(
        m_exportFormatComboBox,
        AppSettings::defaultExportFormat()
        );

    selectEnumValue(
        m_performanceComboBox,
        AppSettings::performanceMode()
        );

    m_showOptimizerAtStartupCheckBox->setChecked(
        AppSettings::showSystemOptimizerAtStartup()
        );

    updateFilenamePreview();
    refreshPerformanceProfileSummary();
}

void PreferencesDialog::loadDefaultSettings()
{
    selectEnumValue(
        m_themeComboBox,
        AppSettings::Theme::Dark
        );

    selectEnumValue(
        m_toolsSideComboBox,
        AppSettings::ToolsPanelSide::Right
        );

    m_filenameTagEdit->setText(
        "_OP"
        );

    selectFormatId(
        m_saveAsFormatComboBox,
        "png"
        );

    selectFormatId(
        m_exportFormatComboBox,
        "png"
        );

    selectEnumValue(
        m_performanceComboBox,
        AppSettings::PerformanceMode::Balanced
        );

    m_showOptimizerAtStartupCheckBox->setChecked(true);

    updateFilenamePreview();
}

void PreferencesDialog::refreshPerformanceProfileSummary()
{
    if (!m_performanceOverviewEdit) {
        return;
    }

    PerformanceProfile profile;
    QString errorMessage;

    if (!SystemProfiler::loadProfile(
            &profile,
            &errorMessage
            )) {
        m_performanceOverviewEdit->setPlainText(
            "SYSTEM OPTIMIZER\n"
            "STATUS      No saved performance profile\n\n"
            "The System Optimizer will run before the editor opens on the "
            "next launch."
            );

        m_performanceProfilePathLabel->setText(
            "Profile path: " +
            SystemProfiler::activeProfilePath()
            );

        m_performanceControlStatusLabel->setText(
            "No profile is available yet. Recalibration is required."
            );

        m_advancedProfileStatusLabel->setText(
            "Advanced controls become available after the first successful "
            "calibration."
            );

        m_backendComboBox->setEnabled(false);
        m_workerThreadsSpinBox->setEnabled(false);
        m_previewDimensionSpinBox->setEnabled(false);
        m_tileHeightComboBox->setEnabled(false);
        m_cpuFallbackCheckBox->setEnabled(false);
        return;
    }

    const PerformanceSettings runtimeSettings =
        ImageProcessingRuntime::instance().settings();

    const QString liveGpu =
        (ImageProcessingRuntime::instance().gpuBackendAvailable() ||
         ImageProcessingRuntime::instance().gpuSpatialPipelineAvailable())
        ? ImageProcessingRuntime::instance().gpuBackendName()
        : "Not active";

    m_performanceOverviewEdit->setPlainText(
        profile.system.displayText() +
        "\n\n" +
        profile.summaryText() +
        "\n\nLIVE RUNTIME\n" +
        QString(
            "ACTIVE      %1\n"
            "GPU DEVICE  %2\n"
            "GPU COLOR   %3\n"
            "GPU SPATIAL %4\n"
            "CPU WORKERS %5\n"
            "TILE HEIGHT %6 rows\n"
            "PREVIEW     %7 px maximum dimension\n"
            "FALLBACK    %8\n"
            "STARTUP UI  %9"
            )
            .arg(
                ImageProcessingRuntime::instance()
                    .processingBackendSummary(),
                liveGpu,
                ImageProcessingRuntime::instance()
                    .gpuAdjustmentPipelineAvailable()
                    ? "Stage 2 fused pipeline ready"
                    : "Optimized CPU path",
                ImageProcessingRuntime::instance()
                    .gpuSpatialPipelineAvailable()
                    ? "Stage 3 heavy-tool pipelines ready"
                    : "Optimized CPU path",
                QString::number(runtimeSettings.workerThreads),
                QString::number(runtimeSettings.tileHeight),
                QString::number(
                    runtimeSettings.interactivePreviewMaxDimension
                    ),
                runtimeSettings.cpuFallback
                    ? "Enabled"
                    : "Disabled",
                AppSettings::showSystemOptimizerAtStartup()
                    ? "Shown"
                    : "Hidden after silent validation"
                )
        );

    m_performanceProfilePathLabel->setText(
        "Active profile: " +
        SystemProfiler::activeProfilePath()
        );

    m_performanceControlStatusLabel->setText(
        optimizerScheduledStatus()
        );

    m_backendComboBox->setEnabled(true);
    m_workerThreadsSpinBox->setEnabled(true);
    m_previewDimensionSpinBox->setEnabled(true);
    m_tileHeightComboBox->setEnabled(true);
    m_cpuFallbackCheckBox->setEnabled(true);

    {
        const QSignalBlocker backendBlocker(m_backendComboBox);
        const int backendIndex =
            m_backendComboBox->findData(
                profile.settings.preferredBackend
                );

        if (backendIndex >= 0) {
            m_backendComboBox->setCurrentIndex(backendIndex);
        }
    }

    {
        const QSignalBlocker workerBlocker(m_workerThreadsSpinBox);
        m_workerThreadsSpinBox->setMaximum(
            qMax(1, profile.system.logicalThreads)
            );
        m_workerThreadsSpinBox->setValue(
            qBound(
                1,
                profile.settings.workerThreads,
                m_workerThreadsSpinBox->maximum()
                )
            );
    }

    {
        const QSignalBlocker previewBlocker(m_previewDimensionSpinBox);
        m_previewDimensionSpinBox->setValue(
            qBound(
                m_previewDimensionSpinBox->minimum(),
                profile.settings.interactivePreviewMaxDimension,
                m_previewDimensionSpinBox->maximum()
                )
            );
    }

    {
        const QSignalBlocker tileBlocker(m_tileHeightComboBox);
        const int tileIndex =
            m_tileHeightComboBox->findData(
                profile.settings.tileHeight
                );

        if (tileIndex >= 0) {
            m_tileHeightComboBox->setCurrentIndex(tileIndex);
        }
    }

    {
        const QSignalBlocker fallbackBlocker(m_cpuFallbackCheckBox);
        m_cpuFallbackCheckBox->setChecked(
            profile.settings.cpuFallback
            );
    }

    m_advancedProfileStatusLabel->setText(
        profile.settings.manualOverride
        ? "This profile currently uses a manual override."
        : "This profile currently uses automatic optimizer recommendations."
        );
}

void PreferencesDialog::savePerformanceProfileCopy()
{
    PerformanceProfile profile;
    QString errorMessage;

    if (!SystemProfiler::loadProfile(
            &profile,
            &errorMessage
            )) {
        QMessageBox::information(
            this,
            "No System Profile Yet",
            "The System Optimizer has not created a profile yet."
            );
        return;
    }

    const QString filePath =
        QFileDialog::getSaveFileName(
            this,
            "Save Performance Profile JSON",
            "OnPoint-performance-profile.json",
            "JSON files (*.json)"
            );

    if (filePath.isEmpty()) {
        return;
    }

    if (!SystemProfiler::saveProfileCopy(
            profile,
            filePath,
            &errorMessage
            )) {
        QMessageBox::critical(
            this,
            "Could Not Save Profile",
            errorMessage
            );
        return;
    }

    QMessageBox::information(
        this,
        "Profile Saved",
        "The performance-profile JSON copy was saved successfully."
        );
}

void PreferencesDialog::savePerformanceSystemReport()
{
    PerformanceProfile profile;
    QString errorMessage;

    if (!SystemProfiler::loadProfile(
            &profile,
            &errorMessage
            )) {
        QMessageBox::information(
            this,
            "No System Profile Yet",
            "The System Optimizer has not created a profile yet."
            );
        return;
    }

    const QString filePath =
        QFileDialog::getSaveFileName(
            this,
            "Export On Point System Report",
            "onpoint-system-report.txt",
            "Text files (*.txt)"
            );

    if (filePath.isEmpty()) {
        return;
    }

    if (!SystemProfiler::saveSystemReport(
            profile,
            filePath,
            &errorMessage
            )) {
        QMessageBox::critical(
            this,
            "Could Not Save Report",
            errorMessage
            );
        return;
    }

    QMessageBox::information(
        this,
        "System Report Saved",
        "The readable diagnostic report was saved successfully."
        );
}

void PreferencesDialog::scheduleSystemOptimizer()
{
    AppSettings::setOptimizeSystemOnNextLaunch(true);
    refreshPerformanceProfileSummary();

    QMessageBox::information(
        this,
        "Optimizer Scheduled",
        "On Point will show and rerun the System Optimizer before the editor "
        "opens on the next launch."
        );
}

void PreferencesDialog::restoreAutomaticPerformanceRecommendations()
{
    AppSettings::setOptimizeSystemOnNextLaunch(true);
    refreshPerformanceProfileSummary();

    QMessageBox::information(
        this,
        "Automatic Recalibration Scheduled",
        "On Point will rerun the System Optimizer on the next launch and "
        "replace the manual profile with fresh automatic recommendations."
        );
}

bool PreferencesDialog::applyAdvancedPerformanceSettings(
    bool showConfirmation
    )
{
    PerformanceProfile profile;
    QString errorMessage;

    if (!SystemProfiler::loadProfile(
            &profile,
            &errorMessage
            )) {
        if (showConfirmation) {
            QMessageBox::information(
                this,
                "No System Profile Yet",
                "Advanced settings are available after the first successful "
                "System Optimizer calibration."
                );
        }
        return false;
    }

    PerformanceSettings updated =
        profile.settings;

    updated.preferredBackend =
        m_backendComboBox->currentData().toString();
    updated.workerThreads =
        m_workerThreadsSpinBox->value();
    updated.interactivePreviewMaxDimension =
        m_previewDimensionSpinBox->value();
    updated.tileHeight =
        m_tileHeightComboBox->currentData().toInt();
    updated.cpuFallback =
        m_cpuFallbackCheckBox->isChecked();

    const bool changed =
        updated.preferredBackend != profile.settings.preferredBackend ||
        updated.workerThreads != profile.settings.workerThreads ||
        updated.interactivePreviewMaxDimension !=
            profile.settings.interactivePreviewMaxDimension ||
        updated.tileHeight != profile.settings.tileHeight ||
        updated.cpuFallback != profile.settings.cpuFallback;

    if (!changed) {
        if (showConfirmation) {
            QMessageBox::information(
                this,
                "Advanced Profile Unchanged",
                "The active performance profile already uses these values."
                );
        }
        return true;
    }

    updated.manualOverride = true;
    profile.settings = updated;

    if (!SystemProfiler::saveProfile(
            profile,
            &errorMessage
            )) {
        QMessageBox::critical(
            this,
            "Could Not Save Profile",
            errorMessage
            );
        return false;
    }

    ImageProcessingRuntime::instance().configure(
        profile.settings
        );

    AppSettings::setOptimizeSystemOnNextLaunch(false);
    refreshPerformanceProfileSummary();

    if (showConfirmation) {
        QMessageBox::information(
            this,
            "Advanced Profile Applied",
            "The manual performance profile was saved and applied to the "
            "current On Point session. CPU fallback remains governed by the "
            "selected profile."
            );
    }

    emit settingsApplied();
    return true;
}

void PreferencesDialog::applySettings()
{
    AppSettings::setTheme(
        selectedEnumValue(
            m_themeComboBox,
            AppSettings::Theme::Dark
            )
        );

    AppSettings::setToolsPanelSide(
        selectedEnumValue(
            m_toolsSideComboBox,
            AppSettings::ToolsPanelSide::Right
            )
        );

    AppSettings::setFilenameTag(
        m_filenameTagEdit->text()
        );

    AppSettings::setDefaultSaveAsFormat(
        selectedFormatId(
            m_saveAsFormatComboBox
            )
        );

    AppSettings::setDefaultExportFormat(
        selectedFormatId(
            m_exportFormatComboBox
            )
        );

    AppSettings::setPerformanceMode(
        selectedEnumValue(
            m_performanceComboBox,
            AppSettings::PerformanceMode::Balanced
            )
        );

    AppSettings::setShowSystemOptimizerAtStartup(
        m_showOptimizerAtStartupCheckBox->isChecked()
        );

    applyAdvancedPerformanceSettings(false);
    refreshPerformanceProfileSummary();

    emit settingsApplied();
}

void PreferencesDialog::updateFilenamePreview()
{
    bool recognized = false;

    const ImageOutput::Format format =
        ImageOutput::formatFromId(
            selectedFormatId(
                m_saveAsFormatComboBox
                ),
            &recognized
            );

    const ImageOutput::Format safeFormat =
        recognized
            ? format
            : ImageOutput::Format::Png;

    m_filenamePreviewLabel->setText(
        ImageOutput::suggestedFileName(
            "vacation.jpg",
            m_filenameTagEdit->text(),
            safeFormat
            )
        );
}
