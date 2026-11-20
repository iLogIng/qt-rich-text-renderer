#include "NavigationHistory.h"

#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << "\n";
        ++failures;
    }
}

} // namespace

int main()
{
    NavigationHistory history;
    expect(!history.canBack(), QStringLiteral("empty history has no back"));
    expect(!history.canForward(), QStringLiteral("empty history has no forward"));
    expect(history.currentIndexNewestFirst() == -1, QStringLiteral("empty history has no current index"));

    history.visit(QStringLiteral("/tmp/a.md"), QStringLiteral("a.md"));
    history.visit(QStringLiteral("/tmp/b.md"), QStringLiteral("b.md"));
    history.visit(QStringLiteral("/tmp/c.md"), QStringLiteral("c.md"));

    expect(history.size() == 3, QStringLiteral("three visits recorded"));
    expect(history.currentPath() == QStringLiteral("/tmp/c.md"), QStringLiteral("newest entry is current"));
    expect(history.currentIndexNewestFirst() == 0, QStringLiteral("current index is newest"));
    expect(history.canBack(), QStringLiteral("history can go back"));
    expect(!history.canForward(), QStringLiteral("newest history cannot go forward"));

    QVector<NavigationEntry> entries = history.entriesNewestFirst();
    expect(entries.size() == 3, QStringLiteral("newest-first list size"));
    expect(entries.at(0).path == QStringLiteral("/tmp/c.md"), QStringLiteral("newest entry listed first"));
    expect(entries.at(1).displayName == QStringLiteral("b.md"), QStringLiteral("display name preserved"));
    expect(entries.at(2).path == QStringLiteral("/tmp/a.md"), QStringLiteral("oldest entry listed last"));

    expect(history.back() == QStringLiteral("/tmp/b.md"), QStringLiteral("back moves to previous entry"));
    expect(history.currentIndexNewestFirst() == 1, QStringLiteral("back updates index"));
    expect(history.canForward(), QStringLiteral("back enables forward"));
    expect(history.forward() == QStringLiteral("/tmp/c.md"), QStringLiteral("forward restores newest entry"));

    history.back();
    history.visit(QStringLiteral("/tmp/d.md"), QStringLiteral("d.md"));
    entries = history.entriesNewestFirst();
    expect(history.size() == 3, QStringLiteral("new visit truncates forward history"));
    expect(!history.canForward(), QStringLiteral("forward history is discarded"));
    expect(entries.at(0).path == QStringLiteral("/tmp/d.md"), QStringLiteral("new entry becomes current"));
    expect(entries.at(1).path == QStringLiteral("/tmp/b.md"), QStringLiteral("history before current kept"));

    history.visit(QStringLiteral("/tmp/d.md"), QStringLiteral("d.md"));
    expect(history.size() == 3, QStringLiteral("revisiting current entry is ignored"));

    expect(history.setCurrentIndexNewestFirst(2), QStringLiteral("select oldest entry"));
    expect(history.currentPath() == QStringLiteral("/tmp/a.md"), QStringLiteral("selection updates current path"));
    expect(!history.canBack(), QStringLiteral("selection at oldest disables back"));
    expect(history.forward() == QStringLiteral("/tmp/b.md"), QStringLiteral("selection preserves forward movement"));
    expect(!history.setCurrentIndexNewestFirst(-1), QStringLiteral("negative selection rejected"));
    expect(!history.setCurrentIndexNewestFirst(3), QStringLiteral("out-of-range selection rejected"));

    history.clear();
    expect(history.size() == 0, QStringLiteral("clear removes entries"));
    expect(history.currentIndexNewestFirst() == -1, QStringLiteral("clear resets current"));

    for (int i = 0; i < 80; ++i) {
        history.visit(QStringLiteral("/tmp/file-%1.md").arg(i),
                      QStringLiteral("file-%1.md").arg(i));
    }
    expect(history.size() == NavigationHistory::kMaxEntries, QStringLiteral("history is capped at 64"));
    entries = history.entriesNewestFirst();
    expect(entries.first().path == QStringLiteral("/tmp/file-79.md"), QStringLiteral("cap keeps newest entry"));
    expect(entries.last().path == QStringLiteral("/tmp/file-16.md"), QStringLiteral("cap drops oldest entries"));
    expect(history.setCurrentIndexNewestFirst(63), QStringLiteral("oldest retained entry selectable"));
    expect(history.currentPath() == QStringLiteral("/tmp/file-16.md"), QStringLiteral("oldest retained entry current"));
    expect(!history.canBack(), QStringLiteral("oldest retained entry cannot go back"));

    if (failures == 0) {
        QTextStream(stdout) << "navigation history tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
