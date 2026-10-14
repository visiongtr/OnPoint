#include "optimizersplashwindow.h"

#include "appsettings.h"

#include <QApplication>
#include <QCheckBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QPixmap>
#include <QResizeEvent>
#include <QFontDatabase>


namespace
{

class SplashImageLabel : public QLabel
{
public:
    explicit SplashImageLabel(
        QWidget *parent = nullptr
        )
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
    }

    void setSourcePixmap(
        const QPixmap &pixmap
        )
    {
        m_sourcePixmap = pixmap;
        updateScaledPixmap();
    }

protected:
    void resizeEvent(
        QResizeEvent *event
        ) override
    {
        QLabel::resizeEvent(event);
        updateScaledPixmap();
    }

private:
    void updateScaledPixmap()
    {
        if (m_sourcePixmap.isNull() ||
            size().isEmpty()) {
            return;
        }

        QLabel::setPixmap(
            m_sourcePixmap.scaled(
                size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                )
            );
    }

    QPixmap m_sourcePixmap;
};

} // namespace

OptimizerSplashWindow::OptimizerSplashWindow(
    QWidget *parent
    )
    : QDialog(parent)
{
    setObjectName("optimizerSplashWindow");
    setWindowTitle("On Point Photo Editor");
    setModal(false);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowMinimizeButtonHint, false);

    setStyleSheet(
        R"QSS(
QDialog#optimizerSplashWindow {
    background-color: #0F1316;
    color: #E5E9EB;
}
QWidget#optimizerSystemPanel {
    background-color: #151A1E;
    border: 1px solid #3E474D;
    border-radius: 10px;
}
QLabel#optimizerHeading,
QLabel#optimizerOptionsHeading {
    color: #E9ECEE;
    font-size: 20px;
    font-weight: 600;
}
QLabel#optimizerExplanation,
QLabel#optimizerStatusLabel,
QLabel#optimizerCountdownLabel {
    color: #B9C0C5;
}
QPlainTextEdit#optimizerSystemReadout {
    background-color: #0D1114;
    color: #CFE8EC;
    border: 1px solid #30383D;
    border-radius: 6px;
    padding: 8px;
    selection-background-color: #6A4D31;
}
QProgressBar#optimizerProgressBar {
    min-height: 20px;
    background-color: #171C20;
    color: #E5E9EB;
    border: 1px solid #424A4F;
    border-radius: 7px;
    text-align: center;
}
QProgressBar#optimizerProgressBar::chunk {
    background-color: #B7813E;
    border-radius: 6px;
}
QPushButton {
    background-color: #2A3034;
    color: #E5E9EB;
    border: 1px solid #4C555B;
    border-radius: 5px;
    padding: 7px 14px;
}
QPushButton:hover {
    background-color: #343B40;
    border-color: #9A6D38;
}
QPushButton:default {
    border-color: #B7813E;
}
QPushButton:disabled {
    color: #777F84;
    background-color: #202529;
    border-color: #343B3F;
}
        )QSS"
        );

    QScreen *screen =
        QGuiApplication::primaryScreen();

    const QSize availableSize = screen
        ? screen->availableGeometry().size()
        : QSize(1280, 800);

    const int targetWidth =
        qBound(920, availableSize.width() - 120, 1320);

    const int targetHeight =
        qBound(650, availableSize.height() - 100, 860);

    resize(targetWidth, targetHeight);
    setMinimumSize(860, 610);

    QVBoxLayout *mainLayout =
        new QVBoxLayout(this);

    mainLayout->setContentsMargins(18, 18, 18, 16);
    mainLayout->setSpacing(12);

    QHBoxLayout *contentLayout =
        new QHBoxLayout();

    contentLayout->setSpacing(18);

    SplashImageLabel *splashImageLabel =
        new SplashImageLabel(this);

    m_splashImageLabel =
        splashImageLabel;

    m_splashImageLabel->setObjectName("optimizerSplashImage");
    m_splashImageLabel->setAlignment(Qt::AlignCenter);
    m_splashImageLabel->setMinimumSize(430, 430);
    m_splashImageLabel->setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding
        );

    const QPixmap splashPixmap(
        ":/styles/OnPoint_Splash.png"
        );

    if (!splashPixmap.isNull()) {
        splashImageLabel->setSourcePixmap(
            splashPixmap
            );
    } else {
        m_splashImageLabel->setText(
            "On Point Photo Editor\nPrecision Photo Correction"
            );
    }

    contentLayout->addWidget(
        m_splashImageLabel,
        3
        );

    QWidget *systemPanel =
        new QWidget(this);

    systemPanel->setObjectName("optimizerSystemPanel");
    systemPanel->setMinimumWidth(340);
    systemPanel->setMaximumWidth(480);

    QVBoxLayout *systemLayout =
        new QVBoxLayout(systemPanel);

    systemLayout->setContentsMargins(16, 16, 16, 16);
    systemLayout->setSpacing(10);

    m_headingLabel =
        new QLabel(
            "System Optimizer",
            systemPanel
            );

    m_headingLabel->setObjectName("optimizerHeading");

    systemLayout->addWidget(m_headingLabel);

    QLabel *explanationLabel =
        new QLabel(
            "On Point is reading this computer and preparing a safe "
            "performance profile. The saved profile is verified at every launch.",
            systemPanel
            );

    explanationLabel->setObjectName("optimizerExplanation");
    explanationLabel->setWordWrap(true);

    systemLayout->addWidget(explanationLabel);

    m_systemTextEdit =
        new QPlainTextEdit(systemPanel);

    m_systemTextEdit->setObjectName("optimizerSystemReadout");
    m_systemTextEdit->setReadOnly(true);
    m_systemTextEdit->setLineWrapMode(
        QPlainTextEdit::WidgetWidth
        );
    m_systemTextEdit->setFont(
        QFontDatabase::systemFont(
            QFontDatabase::FixedFont
            )
        );
    m_systemTextEdit->setPlainText(
        "Reading system information..."
        );

    systemLayout->addWidget(
        m_systemTextEdit,
        1
        );

    contentLayout->addWidget(
        systemPanel,
        2
        );

    mainLayout->addLayout(
        contentLayout,
        1
        );

    m_statusLabel =
        new QLabel(
            "Starting system scan...",
            this
            );

    m_statusLabel->setObjectName("optimizerStatusLabel");
    m_statusLabel->setWordWrap(true);

    mainLayout->addWidget(m_statusLabel);

    m_progressBar =
        new QProgressBar(this);

    m_progressBar->setObjectName("optimizerProgressBar");
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    mainLayout->addWidget(m_progressBar);

    m_skipStartupCheckBox =
        new QCheckBox(
            "Do not show the System Optimizer at startup",
            this
            );

    m_skipStartupCheckBox->setChecked(
        !AppSettings::showSystemOptimizerAtStartup()
        );
    m_skipStartupCheckBox->setEnabled(false);
    m_skipStartupCheckBox->setToolTip(
        "A silent fingerprint check still runs. The optimizer returns "
        "automatically after relevant hardware, driver, or engine changes."
        );

    mainLayout->addWidget(m_skipStartupCheckBox);

    QHBoxLayout *buttonLayout =
        new QHBoxLayout();

    buttonLayout->setSpacing(10);

    m_countdownLabel =
        new QLabel(this);

    m_countdownLabel->setObjectName("optimizerCountdownLabel");

    buttonLayout->addWidget(
        m_countdownLabel,
        1
        );

    m_optionsButton =
        new QPushButton(
            "Options...",
            this
            );

    m_optionsButton->setEnabled(false);

    buttonLayout->addWidget(m_optionsButton);

    m_continueButton =
        new QPushButton(
            "Continue Now",
            this
            );

    m_continueButton->setDefault(true);
    m_continueButton->setEnabled(false);

    buttonLayout->addWidget(m_continueButton);

    mainLayout->addLayout(buttonLayout);

    m_countdownTimer =
        new QTimer(this);

    m_countdownTimer->setInterval(1000);

    connect(
        m_countdownTimer,
        &QTimer::timeout,
        this,
        [this]() {
            if (m_secondsRemaining > 0) {
                --m_secondsRemaining;
            }

            updateCountdownText();

            if (m_secondsRemaining <= 0) {
                m_countdownTimer->stop();
                emit continueRequested();
            }
        });

    connect(
        m_skipStartupCheckBox,
        &QCheckBox::toggled,
        this,
        [](bool skipAtStartup) {
            AppSettings::setShowSystemOptimizerAtStartup(
                !skipAtStartup
                );
        });

    connect(
        m_optionsButton,
        &QPushButton::clicked,
        this,
        [this]() {
            pauseLaunchCountdown();
            emit optionsRequested();
        });

    connect(
        m_continueButton,
        &QPushButton::clicked,
        this,
        [this]() {
            pauseLaunchCountdown();
            emit continueRequested();
        });
}

void OptimizerSplashWindow::setSystemText(
    const QString &text
    )
{
    m_systemTextEdit->setPlainText(text);
    m_systemTextEdit->verticalScrollBar()->setValue(0);
}

void OptimizerSplashWindow::setStatus(
    int progress,
    const QString &text
    )
{
    m_progressBar->setValue(
        qBound(0, progress, 100)
        );

    m_statusLabel->setText(text);
}

void OptimizerSplashWindow::setProfileReady(
    const QString &profileSummary
    )
{
    setSystemText(profileSummary);
    setStatus(100, "Performance profile ready.");

    m_optionsButton->setEnabled(true);
    m_continueButton->setEnabled(true);
    m_skipStartupCheckBox->setEnabled(true);
}

void OptimizerSplashWindow::startLaunchCountdown(
    int seconds
    )
{
    m_secondsRemaining = qMax(1, seconds);
    updateCountdownText();
    m_countdownTimer->start();
}

void OptimizerSplashWindow::pauseLaunchCountdown()
{
    if (m_countdownTimer) {
        m_countdownTimer->stop();
    }

    if (m_secondsRemaining > 0) {
        m_countdownLabel->setText(
            "Automatic launch paused while Options is open."
            );
    }
}

void OptimizerSplashWindow::updateCountdownText()
{
    m_countdownLabel->setText(
        QString("Launching On Point in %1 second%2...")
            .arg(m_secondsRemaining)
            .arg(m_secondsRemaining == 1 ? "" : "s")
        );
}
