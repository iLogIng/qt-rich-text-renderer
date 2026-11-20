#include "MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>

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
    QApplication app(argc, argv);

    MainWindow window;
    auto *source = window.findChild<QPlainTextEdit *>();
    auto *history = window.findChild<QComboBox *>(QStringLiteral("historyCombo"));
    auto *back = window.findChild<QToolButton *>(QStringLiteral("historyBackButton"));
    auto *forward = window.findChild<QToolButton *>(QStringLiteral("historyForwardButton"));
    auto *outline = window.findChild<QTreeWidget *>(QStringLiteral("outlineTree"));

    expect(source && history && back && forward && outline,
           QStringLiteral("main window widgets found"));
    if (!source || !history || !back || !forward || !outline) {
        return 1;
    }

    expect(source->isReadOnly(), QStringLiteral("source view is read-only"));
    expect(source->toPlainText().isEmpty(), QStringLiteral("startup document is empty"));
    expect(history->count() == 0, QStringLiteral("startup history is empty"));
    expect(outline->topLevelItemCount() == 0, QStringLiteral("startup outline is empty"));
    expect(!back->isEnabled(), QStringLiteral("startup Back is disabled"));
    expect(!forward->isEnabled(), QStringLiteral("startup Forward is disabled"));

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary directory available"));
    if (!directory.isValid()) {
        return 1;
    }

    const QString path = directory.filePath(QStringLiteral("startup.md"));
    QFile file(path);
    expect(file.open(QIODevice::WriteOnly), QStringLiteral("write startup fixture"));
    file.write("# Startup heading\n");
    file.close();

    window.loadFile(path);
    QCoreApplication::processEvents();

    expect(source->toPlainText() == QStringLiteral("# Startup heading\n"),
           QStringLiteral("loaded file replaces the empty document"));
    expect(history->count() == 1, QStringLiteral("loaded file is recorded in history"));
    expect(history->currentText() == QStringLiteral("startup.md"),
           QStringLiteral("history shows the loaded file name"));
    expect(outline->topLevelItemCount() == 1, QStringLiteral("loaded heading appears in outline"));
    if (outline->topLevelItemCount() == 1) {
        expect(outline->topLevelItem(0)->text(0) == QStringLiteral("Startup heading"),
               QStringLiteral("outline shows the heading title"));
    }

    if (failures == 0) {
        QTextStream(stdout) << "main window startup tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
