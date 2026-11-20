#include "MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextStream>
#include <QToolButton>

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

    expect(source && history && back && forward, QStringLiteral("main window widgets found"));
    if (!source || !history || !back || !forward) {
        return 1;
    }

    expect(source->toPlainText().isEmpty(), QStringLiteral("startup document is empty"));
    expect(history->count() == 0, QStringLiteral("startup history is empty"));
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

    if (failures == 0) {
        QTextStream(stdout) << "main window startup tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
