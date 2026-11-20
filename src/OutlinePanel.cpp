#include "OutlinePanel.h"

#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
    , m_tree(new QTreeWidget(this))
{
    m_tree->setObjectName(QStringLiteral("outlineTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setToolTip(tr("Document headings"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        requestBlock(item);
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        requestBlock(item);
    });
}

void OutlinePanel::setEntries(const QVector<OutlineEntry> &entries)
{
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    QVector<QTreeWidgetItem *> lastItemByLevel(7, nullptr);
    for (const OutlineEntry &entry : entries) {
        const int level = qBound(1, entry.level, 6);
        QTreeWidgetItem *parent = nullptr;
        for (int parentLevel = level - 1; parentLevel >= 1; --parentLevel) {
            if (lastItemByLevel.at(parentLevel)) {
                parent = lastItemByLevel.at(parentLevel);
                break;
            }
        }

        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, entry.title);
        item->setData(0, Qt::UserRole, entry.blockNumber);
        item->setToolTip(0, entry.title);

        lastItemByLevel[level] = item;
        for (int childLevel = level + 1; childLevel <= 6; ++childLevel) {
            lastItemByLevel[childLevel] = nullptr;
        }
    }

    m_tree->expandAll();
}

void OutlinePanel::requestBlock(QTreeWidgetItem *item)
{
    if (!item) {
        return;
    }
    emit blockRequested(item->data(0, Qt::UserRole).toInt());
}
