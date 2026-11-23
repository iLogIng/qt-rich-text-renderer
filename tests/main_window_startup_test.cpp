#include "MainWindow.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>

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
    auto *centralRoot = window.findChild<QWidget *>(QStringLiteral("central.root"));
    auto *toolbarLeading =
        window.findChild<QWidget *>(QStringLiteral("central.toolbar.leading"));
    auto *toolbarTrailing =
        window.findChild<QWidget *>(QStringLiteral("central.toolbar.trailing"));
    auto *centralContent = window.findChild<QWidget *>(QStringLiteral("central.content"));
    auto *previewPage = window.findChild<QWidget *>(QStringLiteral("central.preview"));
    auto *sourcePage = window.findChild<QWidget *>(QStringLiteral("central.source"));
    auto *centralTop = window.findChild<QWidget *>(QStringLiteral("central.top"));
    auto *centralBottom = window.findChild<QWidget *>(QStringLiteral("central.bottom"));
    auto *centralEmpty = window.findChild<QWidget *>(QStringLiteral("central.empty"));
    auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
    auto *loadButton = window.findChild<QPushButton *>(QStringLiteral("loadButton"));
    auto *renderButton = window.findChild<QPushButton *>(QStringLiteral("renderButton"));
    auto *themeButton = window.findChild<QPushButton *>(QStringLiteral("themeButton"));
    auto *settingsButton = window.findChild<QPushButton *>(QStringLiteral("settingsButton"));

    expect(source && history && back && forward && outline && outlineToggle && outlineDock,
           QStringLiteral("main window widgets found"));
    expect(centralRoot && toolbarLeading && toolbarTrailing && centralContent && previewPage
               && sourcePage && centralTop && centralBottom && centralEmpty && tabs,
           QStringLiteral("central layout slots found"));
    expect(loadButton && renderButton && themeButton && settingsButton,
           QStringLiteral("toolbar buttons found"));
    if (!source || !history || !back || !forward || !outline || !outlineToggle || !outlineDock
        || !centralRoot || !toolbarLeading || !toolbarTrailing || !centralContent
        || !previewPage || !sourcePage || !centralTop || !centralBottom || !centralEmpty
        || !tabs || !loadButton || !renderButton || !themeButton || !settingsButton) {
        return 1;
    }

    expect(source->parentWidget() == sourcePage,
           QStringLiteral("source view is injected into central.source"));
    expect(tabs->indexOf(previewPage) >= 0 && tabs->indexOf(sourcePage) >= 0,
           QStringLiteral("preview and source pages are owned by the document tabs"));
    expect(centralContent->isAncestorOf(tabs),
           QStringLiteral("document tabs are injected into central.content"));
    expect(!toolbarLeading->isHidden() && !toolbarTrailing->isHidden(),
           QStringLiteral("toolbar slots are visible when populated"));
    expect(toolbarLeading->isAncestorOf(loadButton)
               && toolbarLeading->isAncestorOf(renderButton)
               && toolbarLeading->isAncestorOf(themeButton)
               && toolbarLeading->isAncestorOf(settingsButton),
           QStringLiteral("toolbar buttons are injected into the leading slot"));
    auto *statusLabel = window.findChild<QLabel *>(QStringLiteral("statusLabel"));
    expect(statusLabel && toolbarTrailing->isAncestorOf(statusLabel),
           QStringLiteral("status is injected into the trailing slot"));
    expect(centralTop->property("centralSlot").toString() == QStringLiteral("top")
               && centralBottom->property("centralSlot").toString() == QStringLiteral("bottom")
               && centralEmpty->property("centralSlot").toString() == QStringLiteral("empty")
               && toolbarLeading->property("centralSlot").toString()
                      == QStringLiteral("toolbarLeading")
               && toolbarTrailing->property("centralSlot").toString()
                      == QStringLiteral("toolbarTrailing")
               && centralContent->property("centralSlot").toString()
                      == QStringLiteral("content")
               && previewPage->property("centralSlot").toString() == QStringLiteral("preview")
               && sourcePage->property("centralSlot").toString() == QStringLiteral("source"),
           QStringLiteral("central slots expose their QSS style property"));
    expect(window.centralWidget() && window.centralWidget()->layout()
               && window.centralWidget()->layout()->spacing() == 6,
           QStringLiteral("default central layout keeps the theme spacing"));
    expect(centralTop->isHidden() && centralBottom->isHidden() && centralEmpty->isHidden(),
           QStringLiteral("optional empty slots are hidden"));
    expect(centralTop->maximumSize() == QSize(0, 0)
               && centralBottom->maximumSize() == QSize(0, 0)
               && centralEmpty->maximumSize() == QSize(0, 0),
           QStringLiteral("optional empty slots have zero size"));

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

    window.show();
    QCoreApplication::processEvents();

    const QString visiblePath = directory.filePath(QStringLiteral("visible-load.md"));
    QString visibleContent =
        QStringLiteral("# Visible load\n\n"
                       "Intro paragraph.\n\n"
                       "| A | B |\n"
                       "|---|---|\n"
                       "| 1 | 2 |\n\n"
                       "## First section\n\n"
                       "Body text.\n\n"
                       "```cpp\n"
                       "int main() { return 0; }\n"
                       "```\n\n"
                       "## Second section\n\n"
                       "Final paragraph.\n");
    for (int i = 0; i < 80; ++i) {
        visibleContent += QStringLiteral("\nline %1\n").arg(i);
    }
    QFile visibleFile(visiblePath);
    expect(visibleFile.open(QIODevice::WriteOnly), QStringLiteral("write visible-load fixture"));
    visibleFile.write(visibleContent.toUtf8());
    visibleFile.close();

    window.loadFile(visiblePath);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    auto *preview = window.findChild<QTextBrowser *>(QStringLiteral("previewView"));
    expect(preview != nullptr, QStringLiteral("preview view found"));
    expect(source->toPlainText() == visibleContent,
           QStringLiteral("visible-window load keeps the complete source"));
    expect(outline->topLevelItemCount() == 1,
           QStringLiteral("visible-window load updates the outline root"));
    if (outline->topLevelItemCount() == 1) {
        expect(outline->topLevelItem(0)->childCount() == 2,
               QStringLiteral("visible-window load updates the outline children"));
    }
    if (preview) {
        expect(preview->document()->documentMargin() == 12,
               QStringLiteral("default preview document margin is preserved"));
        expect(preview->document()->toPlainText().contains(QStringLiteral("Second section")),
               QStringLiteral("visible-window load keeps all preview sections"));
        expect(preview->verticalScrollBar()->value() == 0,
               QStringLiteral("visible-window load returns the preview to the top"));
        expect(preview->textCursor().position() == 0,
               QStringLiteral("visible-window load resets the preview cursor"));

        const int maximumScroll = preview->verticalScrollBar()->maximum();
        expect(maximumScroll > 0, QStringLiteral("visible-window fixture is scrollable"));
        if (maximumScroll > 0) {
            preview->verticalScrollBar()->setValue(maximumScroll);
            QTextCursor endCursor(preview->document());
            endCursor.movePosition(QTextCursor::End);
            preview->setTextCursor(endCursor);
            expect(preview->verticalScrollBar()->value() > 0,
                   QStringLiteral("fixture is scrolled before reload"));

            window.loadFile(visiblePath);
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            expect(preview->verticalScrollBar()->value() == 0,
                   QStringLiteral("reload resets a previously scrolled preview"));
            expect(preview->textCursor().position() == 0,
                   QStringLiteral("reload resets a previously moved cursor"));
        }
    }

    const QString targetPath = directory.filePath(QStringLiteral("target.md"));
    QString targetContent = QStringLiteral("# Target document\n\n");
    for (int i = 0; i < 60; ++i) {
        targetContent += QStringLiteral("target line %1\n\n").arg(i);
    }
    targetContent += QStringLiteral("## Details\n\nTarget body.\n");
    QFile targetFile(targetPath);
    expect(targetFile.open(QIODevice::WriteOnly), QStringLiteral("write anchor target"));
    targetFile.write(targetContent.toUtf8());
    targetFile.close();

    const QString linkSourcePath = directory.filePath(QStringLiteral("link-source.md"));
    QFile linkSourceFile(linkSourcePath);
    expect(linkSourceFile.open(QIODevice::WriteOnly), QStringLiteral("write link source"));
    linkSourceFile.write("# Link source\n\n[Open target](target.md#details)\n");
    linkSourceFile.close();

    window.loadFile(linkSourcePath);
    QCoreApplication::processEvents();
    if (preview) {
        preview->anchorClicked(QUrl(QStringLiteral("target.md#details")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        expect(preview->textCursor().block().text() == QStringLiteral("Details"),
               QStringLiteral("anchor link opens the target heading"));
        expect(preview->verticalScrollBar()->value() > 0,
               QStringLiteral("anchor link remains positioned after load"));

        preview->anchorClicked(QUrl(QStringLiteral("target.md#missing")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        expect(preview->verticalScrollBar()->value() == 0,
               QStringLiteral("unresolved anchor returns the preview to the top"));
        expect(preview->textCursor().position() == 0,
               QStringLiteral("unresolved anchor resets the preview cursor"));
    }

    window.loadFile(directory.filePath(QStringLiteral("missing.md")), false);
    QCoreApplication::processEvents();
    auto *status = window.findChild<QLabel *>(QStringLiteral("statusLabel"));
    expect(status && status->text().contains(QStringLiteral("Cannot open")),
           QStringLiteral("status-only load failure is reported without a dialog"));

    const QString readmePath =
        QStringLiteral(TEST_SOURCE_DIR) + QStringLiteral("/README.md");
    window.loadFile(readmePath);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    if (preview) {
        QTextBlock lastHeading;
        for (int i = preview->document()->blockCount() - 1; i >= 0; --i) {
            const QTextBlock block = preview->document()->findBlockByNumber(i);
            if (block.blockFormat().headingLevel() > 0) {
                lastHeading = block;
                break;
            }
        }
        expect(lastHeading.isValid(), QStringLiteral("README last heading exists"));
        if (lastHeading.isValid()) {
            const QRectF rect =
                preview->document()->documentLayout()->blockBoundingRect(lastHeading);
            expect(rect.height() > 0,
                   QStringLiteral("README last heading receives layout height"));
        }
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
        const int sourceIndex = customTabs->indexOf(customSource->parentWidget());
        expect(sourceIndex >= 0 && !customTabs->isTabVisible(sourceIndex),
               QStringLiteral("source visibility setting is applied"));
    }

    if (failures == 0) {
        QTextStream(stdout) << "main window startup tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
