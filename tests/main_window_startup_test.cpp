#include "MainWindow.h"

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QPlainTextEdit>
#include <QSettings>
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

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary directory available"));
    if (!directory.isValid()) {
        return 1;
    }

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    MainWindow window(false, settingsPath);
    auto *source = window.findChild<QPlainTextEdit *>();
    auto *history = window.findChild<QComboBox *>(QStringLiteral("historyCombo"));
    auto *back = window.findChild<QToolButton *>(QStringLiteral("historyBackButton"));
    auto *forward = window.findChild<QToolButton *>(QStringLiteral("historyForwardButton"));
    auto *outline = window.findChild<QTreeWidget *>(QStringLiteral("outlineTree"));
    auto *outlineToggle =
        window.findChild<QToolButton *>(QStringLiteral("outlineToggleButton"));
    auto *outlineDock = window.findChild<QDockWidget *>(QStringLiteral("outlineDock"));

    expect(source && history && back && forward && outline && outlineToggle && outlineDock,
           QStringLiteral("main window widgets found"));
    if (!source || !history || !back || !forward || !outline || !outlineToggle || !outlineDock) {
        return 1;
    }

    expect(outlineToggle->isChecked(), QStringLiteral("outline toggle starts checked"));
    expect(!outlineDock->isHidden(), QStringLiteral("outline starts visible"));
    outlineToggle->click();
    expect(outlineDock->isHidden() && !outlineToggle->isChecked(),
           QStringLiteral("outline toggle hides the fixed outline"));
    outlineToggle->click();
    expect(!outlineDock->isHidden() && outlineToggle->isChecked(),
           QStringLiteral("outline toggle restores the fixed outline"));

    expect(source->isReadOnly(), QStringLiteral("source view is read-only"));
    expect(source->toPlainText().isEmpty(), QStringLiteral("startup document is empty"));
    expect(history->count() == 0, QStringLiteral("startup history is empty"));
    expect(outline->topLevelItemCount() == 0, QStringLiteral("startup outline is empty"));
    expect(!back->isEnabled(), QStringLiteral("startup Back is disabled"));
    expect(!forward->isEnabled(), QStringLiteral("startup Forward is disabled"));

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

    const QString customSettingsPath = directory.filePath(QStringLiteral("custom-settings.ini"));
    {
        QSettings customSettings(customSettingsPath, QSettings::IniFormat);
        customSettings.setValue(QStringLiteral("appearance/uiFontPointSize"), 12);
        customSettings.setValue(QStringLiteral("appearance/sourceFontPointSize"), 14);
        customSettings.setValue(QStringLiteral("view/outlineVisible"), false);
        customSettings.setValue(QStringLiteral("view/sourceVisible"), false);
        customSettings.sync();
    }

    MainWindow customized(false, customSettingsPath);
    auto *customSource = customized.findChild<QPlainTextEdit *>();
    auto *customTabs = customized.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
    auto *customOutline = customized.findChild<QDockWidget *>(QStringLiteral("outlineDock"));
    auto *customOutlineToggle =
        customized.findChild<QToolButton *>(QStringLiteral("outlineToggleButton"));
    expect(customSource && customTabs && customOutline && customOutlineToggle,
           QStringLiteral("customized window widgets found"));
    if (customSource && customTabs && customOutline && customOutlineToggle) {
        expect(QApplication::font().pointSize() == 12,
               QStringLiteral("UI font size setting is applied"));
        expect(customSource->font().pointSize() == 14,
               QStringLiteral("source font size setting is applied"));
        expect(customOutline->isHidden(), QStringLiteral("outline visibility setting is applied"));
        expect(!customOutlineToggle->isChecked(),
               QStringLiteral("outline toggle reflects hidden outline setting"));
        const int sourceIndex = customTabs->indexOf(customSource);
        expect(sourceIndex >= 0 && !customTabs->isTabVisible(sourceIndex),
               QStringLiteral("source visibility setting is applied"));
    }

    if (failures == 0) {
        QTextStream(stdout) << "main window startup tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
