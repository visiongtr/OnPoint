#ifndef OPTIMIZEROPTIONSDIALOG_H
#define OPTIMIZEROPTIONSDIALOG_H

#include "systemprofile.h"

#include <QDialog>

class QLabel;
class QPlainTextEdit;

class OptimizerOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OptimizerOptionsDialog(
        PerformanceProfile *profile,
        QWidget *parent = nullptr
        );

    bool rerunRequested() const;
    bool profileChanged() const;

private:
    void saveProfileCopy();
    void saveSystemReport();
    void openAdvancedSettings();
    void refreshSummary();

    PerformanceProfile *m_profile = nullptr;
    QPlainTextEdit *m_summaryEdit = nullptr;
    QLabel *m_profilePathLabel = nullptr;
    bool m_rerunRequested = false;
    bool m_profileChanged = false;
};

#endif // OPTIMIZEROPTIONSDIALOG_H
