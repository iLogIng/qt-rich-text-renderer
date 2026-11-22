#include "MarkdownOutline.h"

#include <QTextBlock>
#include <QTextDocument>

QVector<OutlineEntry> MarkdownOutline::fromDocument(
    const QTextDocument &document,
    const QHash<QString, QString> &tokenLabels)
{
    QVector<OutlineEntry> entries;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const int level = block.blockFormat().headingLevel();
        if (level <= 0) {
            continue;
        }

        QString title = block.text().trimmed();
        for (auto it = tokenLabels.cbegin(); it != tokenLabels.cend(); ++it) {
            title.replace(it.key(), it.value());
        }
        if (title.isEmpty()) {
            title = QStringLiteral("(untitled)");
        }

        OutlineEntry entry;
        entry.level = qBound(1, level, 6);
        entry.title = title;
        entry.blockNumber = block.blockNumber();
        entries.push_back(entry);
    }
    return entries;
}
