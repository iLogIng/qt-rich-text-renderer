#include "MarkdownOutline.h"

#include <QGuiApplication>
#include <QTextDocument>
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

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QTextDocument document;
    document.setMarkdown(QStringLiteral(
        "# Title\n\n"
        "intro\n\n"
        "## Section A\n\n"
        "text\n\n"
        "### Subsection\n\n"
        "text\n\n"
        "## Section B\n\n"
        "text\n\n"
        "#### Deep\n\n"
        "body\n"));

    QVector<OutlineEntry> entries = MarkdownOutline::fromDocument(document);
    expect(entries.size() == 5, QStringLiteral("all headings are collected"));
    if (entries.size() == 5) {
        expect(entries.at(0).level == 1 && entries.at(0).title == QStringLiteral("Title"),
               QStringLiteral("level one heading"));
        expect(entries.at(1).level == 2 && entries.at(1).title == QStringLiteral("Section A"),
               QStringLiteral("level two heading"));
        expect(entries.at(2).level == 3 && entries.at(2).title == QStringLiteral("Subsection"),
               QStringLiteral("level three heading"));
        expect(entries.at(3).level == 2 && entries.at(3).title == QStringLiteral("Section B"),
               QStringLiteral("second level two heading"));
        expect(entries.at(4).level == 4 && entries.at(4).title == QStringLiteral("Deep"),
               QStringLiteral("level four heading"));
        expect(entries.at(0).blockNumber < entries.at(1).blockNumber,
               QStringLiteral("block numbers increase in document order"));
    }

    QHash<QString, QString> labels;
    labels.insert(QStringLiteral("FORMULATOKEN"), QStringLiteral("a+b"));
    QTextDocument tokenDocument;
    tokenDocument.setMarkdown(QStringLiteral("# Formula FORMULATOKEN\n\nbody\n"));
    entries = MarkdownOutline::fromDocument(tokenDocument, labels);
    expect(entries.size() == 1, QStringLiteral("token heading collected"));
    if (entries.size() == 1) {
        expect(entries.first().title == QStringLiteral("Formula a+b"),
               QStringLiteral("token label replaces internal placeholder"));
    }

    QTextDocument emptyDocument;
    emptyDocument.setMarkdown(QStringLiteral("plain text\n\nanother paragraph\n"));
    expect(MarkdownOutline::fromDocument(emptyDocument).isEmpty(),
           QStringLiteral("documents without headings have no outline"));

    if (failures == 0) {
        QTextStream(stdout) << "Markdown outline tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
