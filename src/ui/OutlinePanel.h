#pragma once

#include "MarkdownOutline.h"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

class OutlinePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    void setEntries(const QVector<OutlineEntry> &entries);

signals:
    void blockRequested(int blockNumber);

private:
    void requestBlock(QTreeWidgetItem *item);

    QTreeWidget *m_tree = nullptr;
};
