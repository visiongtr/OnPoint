#include "optimizeroptionsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QFileInfo>

namespace
{

class AdvancedPerformanceDialog : public QDialog
{
public:
    explicit AdvancedPerformanceDialog(
        PerformanceProfile *profile,
        QWidget *parent = nullptr
        )
        : QDialog(parent)
        , m_profile(profile)
    {
        setWindowTitle("Advanced Performance Controls");
        setModal(true);
        resize(560, 400);

        QVBoxLayout *mainLayout =
            new QVBoxLayout(this);

        QLabel *warningLabel =
            new QLabel(
                "These controls override On Point's automatic recommendations. "
                "Incorrect values can reduce performance, increase memory use, "
                "or make the desktop less responsive.",
                this
                );

        warningLabel->setWordWrap(true);
        warningLabel->setObjectName("advancedPerformanceWarning");
        mainLayout->addWidget(warningLabel);

        QGroupBox *settingsBox =
            new QGroupBox(
                "Manual performance profile",
                this
                );

        QFormLayout *formLayout =
            new QFormLayout(settingsBox);

        m_backendCombo =
            new QComboBox(settingsBox);

        m_backendCombo->addItem(
            "Automatic GPU / CPU selection",
            "auto"
            );
        m_backendCombo->addItem(
            "CPU only",
            "cpu"
            );
        m_backendCombo->addItem(
            "Prefer GPU",
            "gpu"
            );

        const int backendIndex =
            m_backendCombo->findData(
                m_profile->settings.preferredBackend
                );

        if (backendIndex >= 0) {
            m_backendCombo->setCurrentIndex(backendIndex);
        }

        formLayout->addRow(
            "Preferred backend:",
            m_backendCombo
            );

        m_workerSpinBox =
            new QSpinBox(settingsBox);

        m_workerSpinBox->setRange(
            1,
            qMax(1, m_profile->system.logicalThreads)
            );
        m_workerSpinBox->setValue(
            qBound(
                1,
                m_profile->settings.workerThreads,
                m_workerSpinBox->maximum()
                )
            );

        formLayout->addRow(
            "CPU worker threads:",
            m_workerSpinBox
            );

        m_previewSpinBox =
            new QSpinBox(settingsBox);

        m_previewSpinBox->setRange(800, 4096);
        m_previewSpinBox->setSingleStep(100);
        m_previewSpinBox->setSuffix(" px");
        m_previewSpinBox->setValue(
            m_profile->settings.interactivePreviewMaxDimension
            );

        formLayout->addRow(
            "Interactive preview maximum:",
            m_previewSpinBox
            );

        m_tileHeightCombo =
            new QComboBox(settingsBox);

        for (int tileHeight : {64, 128, 192, 256, 384, 512}) {
            m_tileHeightCombo->addItem(
                QString("%1 rows").arg(tileHeight),
                tileHeight
                );
        }

        const int tileIndex =
            m_tileHeightCombo->findData(
                m_profile->settings.tileHeight
                );

        if (tileIndex >= 0) {
            m_tileHeightCombo->setCurrentIndex(tileIndex);
        }

        formLayout->addRow(
            "Processing tile height:",
            m_tileHeightCombo
            );

        m_cpuFallbackCheckBox =
            new QCheckBox(
                "Always keep dependable CPU fallback enabled",
                settingsBox
                );

        m_cpuFallbackCheckBox->setChecked(
            m_profile->settings.cpuFallback
            );

        formLayout->addRow(
            QString(),
            m_cpuFallbackCheckBox
            );

        mainLayout->addWidget(settingsBox);

        QLabel *foundationLabel =
            new QLabel(
                "These values configure On Point's shared CPU thread pool, "
                "interactive preview scheduler, OpenCL GPU backend, and "
                "dependable CPU fallback.",
                this
                );

        foundationLabel->setWordWrap(true);
        mainLayout->addWidget(foundationLabel);
        mainLayout->addStretch(1);

        QDialogButtonBox *buttonBox =
            new QDialogButtonBox(
                QDialogButtonBox::Save |
                    QDialogButtonBox::Cancel,
                this
                );

        mainLayout->addWidget(buttonBox);

        connect(
            buttonBox,
            &QDialogButtonBox::accepted,
            this,
            [this]() {
                m_profile->settings.preferredBackend =
                    m_backendCombo->currentData().toString();
                m_profile->settings.workerThreads =
                    m_workerSpinBox->value();
                m_profile->settings.interactivePreviewMaxDimension =
                    m_previewSpinBox->value();
                m_profile->settings.tileHeight =
                    m_tileHeightCombo->currentData().toInt();
                m_profile->settings.cpuFallback =
                    m_cpuFallbackCheckBox->isChecked();
                m_profile->settings.manualOverride = true;
                accept();
            });

        connect(
            buttonBox,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject
            );
    }

private:
    PerformanceProfile *m_profile = nullptr;
    QComboBox *m_backendCombo = nullptr;
    QSpinBox *m_workerSpinBox = nullptr;
    QSpinBox *m_previewSpinBox = nullptr;
    QComboBox *m_tileHeightCombo = nullptr;
    QCheckBox *m_cpuFallbackCheckBox = nullptr;
};

} // namespace

OptimizerOptionsDialog::OptimizerOptionsDialog(
    PerformanceProfile *profile,
    QWidget *parent
    )
    : QDialog(parent)
    , m_profile(profile)
{
    setObjectName("optimizerOptionsDialog");
    setWindowTitle("On Point System Optimizer Options");
    setModal(true);
    resize(760, 590);
    setMinimumSize(660, 500);

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *headingLabel =
        new QLabel(
            "System profile and diagnostics",
            this
            );

    headingLabel->setObjectName("optimizerOptionsHeading");
    mainLayout->addWidget(headingLabel);

    QLabel *explanationLabel =
        new QLabel(
            "The automatic profile is the recommended setting. You can save a "
            "copy of its JSON data, export a readable Linux system report, or "
            "rerun the optimizer after changing hardware or drivers.",
            this
            );

    explanationLabel->setWordWrap(true);
    mainLayout->addWidget(explanationLabel);

    m_summaryEdit =
        new QPlainTextEdit(this);

    m_summaryEdit->setReadOnly(true);
    mainLayout->addWidget(m_summaryEdit, 1);

    m_profilePathLabel =
        new QLabel(this);

    m_profilePathLabel->setWordWrap(true);
    m_profilePathLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse
        );

    mainLayout->addWidget(m_profilePathLabel);

    QHBoxLayout *fileButtonLayout =
        new QHBoxLayout();

    QPushButton *saveJsonButton =
        new QPushButton(
            "Save Profile JSON Copy...",
            this
            );

    QPushButton *saveReportButton =
        new QPushButton(
            "Export System Report...",
            this
            );

    fileButtonLayout->addWidget(saveJsonButton);
    fileButtonLayout->addWidget(saveReportButton);

    mainLayout->addLayout(fileButtonLayout);

    QHBoxLayout *actionButtonLayout =
        new QHBoxLayout();

    QPushButton *advancedButton =
        new QPushButton(
            "Advanced Manual Controls...",
            this
            );

    QPushButton *rerunButton =
        new QPushButton(
            "Run System Optimizer Again",
            this
            );

    actionButtonLayout->addWidget(advancedButton);
    actionButtonLayout->addWidget(rerunButton);

    mainLayout->addLayout(actionButtonLayout);

    QDialogButtonBox *buttonBox =
        new QDialogButtonBox(
            QDialogButtonBox::Close,
            this
            );

    mainLayout->addWidget(buttonBox);

    connect(
        saveJsonButton,
        &QPushButton::clicked,
        this,
        &OptimizerOptionsDialog::saveProfileCopy
        );

    connect(
        saveReportButton,
        &QPushButton::clicked,
        this,
        &OptimizerOptionsDialog::saveSystemReport
        );

    connect(
        advancedButton,
        &QPushButton::clicked,
        this,
        &OptimizerOptionsDialog::openAdvancedSettings
        );

    connect(
        rerunButton,
        &QPushButton::clicked,
        this,
        [this]() {
            m_rerunRequested = true;
            accept();
        });

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
        );

    refreshSummary();
}

bool OptimizerOptionsDialog::rerunRequested() const
{
    return m_rerunRequested;
}

bool OptimizerOptionsDialog::profileChanged() const
{
    return m_profileChanged;
}

void OptimizerOptionsDialog::saveProfileCopy()
{
    if (!m_profile) {
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

    QString errorMessage;

    if (!SystemProfiler::saveProfileCopy(
            *m_profile,
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

void OptimizerOptionsDialog::saveSystemReport()
{
    if (!m_profile) {
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

    QString errorMessage;

    if (!SystemProfiler::saveSystemReport(
            *m_profile,
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

void OptimizerOptionsDialog::openAdvancedSettings()
{
    if (!m_profile) {
        return;
    }

    const QMessageBox::StandardButton choice =
        QMessageBox::warning(
            this,
            "Advanced Performance Controls",
            "Manual settings override the automatic system recommendation. "
            "Incorrect values can reduce performance, increase memory use, "
            "or make the desktop less responsive.\n\n"
            "Open the advanced controls anyway?",
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
            );

    if (choice != QMessageBox::Yes) {
        return;
    }

    AdvancedPerformanceDialog dialog(
        m_profile,
        this
        );

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString errorMessage;

    if (!SystemProfiler::saveProfile(
            *m_profile,
            &errorMessage
            )) {
        QMessageBox::critical(
            this,
            "Could Not Save Profile",
            errorMessage
            );
        return;
    }

    m_profileChanged = true;
    refreshSummary();
}

void OptimizerOptionsDialog::refreshSummary()
{
    if (!m_profile) {
        m_summaryEdit->setPlainText(
            "No performance profile is available."
            );
        return;
    }

    m_summaryEdit->setPlainText(
        m_profile->system.displayText() +
        "\n\n" +
        m_profile->summaryText()
        );

    m_profilePathLabel->setText(
        "Active profile: " +
        SystemProfiler::activeProfilePath()
        );
}
