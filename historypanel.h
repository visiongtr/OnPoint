#ifndef HISTORYPANEL_H
#define HISTORYPANEL_H

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QListWidgetItem;

class HistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPanel(
        QWidget *parent = nullptr
        );

    void setHistory(
        const QStringList &labels,
        int currentIndex,
        bool hasDocument,
        bool olderHistoryDiscarded,
        int maximumUndoLevels
        );

signals:
    void historyIndexRequested(
        int index
        );

private:
    QListWidget *m_historyList = nullptr;
    QLabel *m_limitLabel = nullptr;
};

#endif // HISTORYPANEL_H
