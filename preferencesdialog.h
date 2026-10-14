#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTabWidget;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(
        QWidget *parent = nullptr,
        bool showPerformancePage = false
        );

signals:
    void settingsApplied();

private:
    QWidget *createAppearanceTab();
    QWidget *createFileTab();
    QWidget *createPerformanceTab();

    void populateFormatComboBox(
        QComboBox *comboBox
        );

    void selectFormatId(
        QComboBox *comboBox,
        const QString &formatId
        );

    QString selectedFormatId(
        const QComboBox *comboBox
        ) const;

    void loadCurrentSettings();
    void loadDefaultSettings();

    void applySettings();
    void updateFilenamePreview();
    void refreshPerformanceProfileSummary();
    void savePerformanceProfileCopy();
    void savePerformanceSystemReport();
    void scheduleSystemOptimizer();
    void restoreAutomaticPerformanceRecommendations();
    bool applyAdvancedPerformanceSettings(
        bool showConfirmation
        );

    QTabWidget *m_tabWidget = nullptr;
    QTabWidget *m_performanceTabWidget = nullptr;

    QComboBox *m_themeComboBox = nullptr;
    QComboBox *m_toolsSideComboBox = nullptr;

    QLineEdit *m_filenameTagEdit = nullptr;
    QComboBox *m_saveAsFormatComboBox = nullptr;
    QComboBox *m_exportFormatComboBox = nullptr;
    QLabel *m_filenamePreviewLabel = nullptr;

    QCheckBox *m_showOptimizerAtStartupCheckBox = nullptr;
    QComboBox *m_performanceComboBox = nullptr;
    QPlainTextEdit *m_performanceOverviewEdit = nullptr;
    QLabel *m_performanceProfilePathLabel = nullptr;
    QLabel *m_performanceControlStatusLabel = nullptr;

    QComboBox *m_backendComboBox = nullptr;
    QSpinBox *m_workerThreadsSpinBox = nullptr;
    QSpinBox *m_previewDimensionSpinBox = nullptr;
    QComboBox *m_tileHeightComboBox = nullptr;
    QCheckBox *m_cpuFallbackCheckBox = nullptr;
    QLabel *m_advancedProfileStatusLabel = nullptr;
};

#endif // PREFERENCESDIALOG_H
