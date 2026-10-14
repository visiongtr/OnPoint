#ifndef OPTIMIZERSPLASHWINDOW_H
#define OPTIMIZERSPLASHWINDOW_H

#include <QDialog>

class QCheckBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTimer;

class OptimizerSplashWindow : public QDialog
{
    Q_OBJECT

public:
    explicit OptimizerSplashWindow(
        QWidget *parent = nullptr
        );

    void setSystemText(
        const QString &text
        );

    void setStatus(
        int progress,
        const QString &text
        );

    void setProfileReady(
        const QString &profileSummary
        );

    void startLaunchCountdown(
        int seconds = 10
        );

    void pauseLaunchCountdown();

signals:
    void optionsRequested();
    void continueRequested();

private:
    void updateCountdownText();

    QLabel *m_splashImageLabel = nullptr;
    QLabel *m_headingLabel = nullptr;
    QPlainTextEdit *m_systemTextEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_countdownLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QCheckBox *m_skipStartupCheckBox = nullptr;
    QPushButton *m_optionsButton = nullptr;
    QPushButton *m_continueButton = nullptr;
    QTimer *m_countdownTimer = nullptr;
    int m_secondsRemaining = 0;
};

#endif // OPTIMIZERSPLASHWINDOW_H
