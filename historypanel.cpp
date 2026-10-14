#include "historypanel.h"

#include <QAbstractItemView>
#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QPalette>
#include <QSignalBlocker>
#include <QStringList>
#include <QVBoxLayout>

HistoryPanel::HistoryPanel(
    QWidget *parent
    )
    : QWidget(parent)
{
    setObjectName(
        "historyPanel"
        );

    QVBoxLayout *layout =
        new QVBoxLayout(this);

    layout->setContentsMargins(
        8,
        8,
        8,
        8
        );

    layout->setSpacing(
        8
        );

    QLabel *instructionLabel =
        new QLabel(
            "Click a committed edit to return to that point.",
            this
            );

    instructionLabel->setWordWrap(
        true
        );

    layout->addWidget(
        instructionLabel
        );

    m_historyList =
        new QListWidget(this);

    m_historyList->setObjectName(
        "historyListWidget"
        );

    m_historyList->setAlternatingRowColors(
        false
        );

    m_historyList->setSelectionMode(
        QAbstractItemView::SingleSelection
        );

    m_historyList->setUniformItemSizes(
        true
        );

    m_historyList->setSpacing(
        1
        );

    layout->addWidget(
        m_historyList,
        1
        );

    m_limitLabel =
        new QLabel(this);

    m_limitLabel->setWordWrap(
        true
        );

    layout->addWidget(
        m_limitLabel
        );

    connect(
        m_historyList,
        &QListWidget::currentRowChanged,
        this,
        [this](int row) {
            if (row < 0) {
                return;
            }

            emit historyIndexRequested(
                row
                );
        });
}

void HistoryPanel::setHistory(
    const QStringList &labels,
    int currentIndex,
    bool hasDocument,
    bool olderHistoryDiscarded,
    int maximumUndoLevels
    )
{
    QSignalBlocker blocker(
        m_historyList
        );

    m_historyList->clear();

    if (!hasDocument) {
        QListWidgetItem *emptyItem =
            new QListWidgetItem(
                "No image open",
                m_historyList
                );

        emptyItem->setFlags(
            Qt::NoItemFlags
            );

        m_historyList->setEnabled(
            false
            );

        m_limitLabel->setText(
            QString(
                "History stores up to %1 committed edits."
                )
                .arg(
                    maximumUndoLevels
                    )
            );

        return;
    }

    m_historyList->setEnabled(
        true
        );

    const QPalette listPalette =
        m_historyList->palette();

    for (int index = 0;
         index < labels.size();
         ++index) {
        QListWidgetItem *item =
            new QListWidgetItem(
                labels.at(index),
                m_historyList
                );

        item->setToolTip(
            index == currentIndex
                ? "Current image state"
                : index < currentIndex
                    ? "Click to undo back to this state"
                    : "Click to redo forward to this state"
            );

        QFont itemFont =
            item->font();

        if (index == currentIndex) {
            itemFont.setBold(
                true
                );
        } else if (index > currentIndex) {
            itemFont.setItalic(
                true
                );

            item->setForeground(
                listPalette.brush(
                    QPalette::Disabled,
                    QPalette::Text
                    )
                );
        }

        item->setFont(
            itemFont
            );
    }

    if (currentIndex >= 0 &&
        currentIndex < m_historyList->count()) {
        m_historyList->setCurrentRow(
            currentIndex
            );

        m_historyList->scrollToItem(
            m_historyList->item(
                currentIndex
                ),
            QAbstractItemView::PositionAtCenter
            );
    }

    if (olderHistoryDiscarded) {
        m_limitLabel->setText(
            QString(
                "The oldest edits were discarded after the %1-level limit was reached."
                )
                .arg(
                    maximumUndoLevels
                    )
            );
    } else {
        m_limitLabel->setText(
            QString(
                "Stores up to %1 committed edits. Live previews are not added."
                )
                .arg(
                    maximumUndoLevels
                    )
            );
    }
}
