#include "CentralUiLayout.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLayout>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextStream>
#include <QWidget>

#include <memory>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << "\n";
        ++failures;
    }
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

QString validLayoutUi(const QString &markerText = QStringLiteral("Custom layout"))
{
    QString ui = QStringLiteral(R"UI(<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>CentralWidget</class>
 <widget class="QWidget" name="central.root">
  <layout class="QVBoxLayout" name="centralLayout">
   <property name="spacing"><number>6</number></property>
   <item>
    <widget class="QWidget" name="central.toolbar.leading">
     <layout class="QHBoxLayout" name="leadingLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QWidget" name="central.toolbar.trailing">
     <layout class="QHBoxLayout" name="trailingLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QWidget" name="central.content">
     <layout class="QVBoxLayout" name="contentLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QWidget" name="central.top">
     <layout class="QHBoxLayout" name="topLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QLabel" name="customMarker">
     <property name="text"><string>__MARKER__</string></property>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
)UI");
    ui.replace(QStringLiteral("__MARKER__"), markerText);
    return ui;
}

ThemeSpec themeForLayout(const QString &packageDirectory, const QString &layoutName)
{
    ThemeSpec theme;
    theme.id = QStringLiteral("layout-test");
    theme.name = QStringLiteral("Layout Test");
    theme.packageDirectory = packageDirectory;
    theme.layoutFile = QFileInfo(QDir(packageDirectory).filePath(layoutName)).canonicalFilePath();
    theme.layoutApiVersion = 1;
    return theme;
}

bool rejected(const ThemeSpec &theme, const QString &expectedFragment)
{
    QWidget parent;
    QString error;
    const CentralUiLayout layout = CentralUiBuilder::loadTheme(theme, &parent, &error);
    if (layout.isValid()) {
        delete layout.root;
        QTextStream(stderr) << "FAIL: layout unexpectedly loaded for " << expectedFragment << "\n";
        ++failures;
        return false;
    }
    if (!error.contains(expectedFragment, Qt::CaseInsensitive)) {
        QTextStream(stderr) << "FAIL: expected error containing '" << expectedFragment
                            << "', got '" << error << "'\n";
        ++failures;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rich-text-renderer-ui-layout-test"));
    app.setOrganizationName(QStringLiteral("rich-text-renderer-ui-layout-test"));

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary directory available"));
    if (!directory.isValid()) {
        return 1;
    }

    QWidget defaultParent;
    const CentralUiLayout defaults = CentralUiBuilder::createDefault(&defaultParent);
    expect(defaults.isValid(), QStringLiteral("default central layout is valid"));
    expect(defaults.root->objectName() == QStringLiteral("central.root"),
           QStringLiteral("default root slot name"));
    expect(defaults.toolbarLeading && defaults.toolbarTrailing && defaults.content,
           QStringLiteral("default required slots exist"));
    expect(defaults.top && defaults.bottom && defaults.empty,
           QStringLiteral("default optional slots exist"));
    expect(defaults.top->isHidden() && defaults.top->maximumSize() == QSize(0, 0),
           QStringLiteral("default optional slots are zero-size"));
    delete defaults.root;

    const QString layoutPath = directory.filePath(QStringLiteral("layout.ui"));
    expect(writeFile(layoutPath, validLayoutUi().toUtf8()),
           QStringLiteral("write valid layout"));
    QWidget validParent;
    QString error;
    const ThemeSpec validTheme = themeForLayout(directory.path(), QStringLiteral("layout.ui"));
    const CentralUiLayout valid = CentralUiBuilder::loadTheme(validTheme, &validParent, &error);
    expect(valid.isValid(), QStringLiteral("valid theme layout loads"));
    expect(error.isEmpty(), QStringLiteral("valid theme layout has no error"));
    expect(valid.fromTheme, QStringLiteral("valid layout is marked as theme-provided"));
    expect(valid.root->objectName() == QStringLiteral("central.root"),
           QStringLiteral("theme layout keeps the central root name"));
    expect(valid.root->findChild<QWidget *>(QStringLiteral("customMarker")) != nullptr,
           QStringLiteral("theme layout content is created"));
    expect(valid.toolbarLeading && valid.toolbarTrailing && valid.content,
           QStringLiteral("theme layout required slots resolve"));
    expect(valid.top && valid.top->isHidden(),
           QStringLiteral("empty optional theme slot is hidden"));
    expect(valid.top && valid.top->property("centralSlot").toString() == QStringLiteral("top")
               && valid.content->property("centralSlot").toString()
                      == QStringLiteral("content")
               && valid.toolbarLeading->property("centralSlot").toString()
                      == QStringLiteral("toolbarLeading"),
           QStringLiteral("theme layout slots expose their QSS style property"));
    delete valid.root;

    {
        const QString path = directory.filePath(QStringLiteral("text-edit.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("class=\"QLabel\" name=\"customMarker\""),
                   QStringLiteral("class=\"QTextEdit\" name=\"customMarker\""));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write text-edit layout"));
        QWidget textEditParent;
        const CentralUiLayout textEditLayout =
            CentralUiBuilder::loadTheme(themeForLayout(directory.path(),
                                                       QStringLiteral("text-edit.ui")),
                                        &textEditParent,
                                        &error);
        expect(textEditLayout.isValid(),
               QStringLiteral("empty QTextEdit layout loads without generated-style false positives"));
        delete textEditLayout.root;
    }

    {
        const QString path = directory.filePath(QStringLiteral("missing-slot.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("central.toolbar.trailing"),
                   QStringLiteral("unusedTrailing"));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write missing-slot layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("missing-slot.ui")),
                 QStringLiteral("required layout slot is missing"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("unknown-widget.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("class=\"QLabel\" name=\"customMarker\""),
                   QStringLiteral("class=\"QWebEngineView\" name=\"customMarker\""));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write unknown-widget layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("unknown-widget.ui")),
                 QStringLiteral("widget class is not allowed"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("reserved-slot.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("name=\"customMarker\""),
                   QStringLiteral("name=\"central.preview\""));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write reserved-slot layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("reserved-slot.ui")),
                 QStringLiteral("reserved layout slot is not allowed"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("toolbar-layout.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("class=\"QHBoxLayout\" name=\"leadingLayout\""),
                   QStringLiteral("class=\"QVBoxLayout\" name=\"leadingLayout\""));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write toolbar-layout layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("toolbar-layout.ui")),
                 QStringLiteral("toolbar layout slot must use QHBoxLayout"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("font-point-size.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("<widget class=\"QLabel\" name=\"customMarker\">"),
                   QStringLiteral("<widget class=\"QLabel\" name=\"customMarker\">"
                                  "<property name=\"font\"><font><pointsize>14</pointsize>"
                                  "</font></property>"));
        expect(writeFile(path, ui.toUtf8()),
               QStringLiteral("write font-point-size layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("font-point-size.ui")),
                 QStringLiteral("layout element is not allowed: pointsize"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("stylesheet.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("<widget class=\"QWidget\" name=\"central.content\">"),
                   QStringLiteral("<widget class=\"QWidget\" name=\"central.content\">"
                                  "<property name=\"styleSheet\"><string>color: red</string></property>"));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write stylesheet layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("stylesheet.ui")),
                 QStringLiteral("property is not allowed"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("external-url.ui"));
        expect(writeFile(path, validLayoutUi(QStringLiteral("https://example.com")).toUtf8()),
               QStringLiteral("write external-url layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("external-url.ui")),
                 QStringLiteral("forbidden URL"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("relative-resource.ui"));
        expect(writeFile(path,
                         validLayoutUi(
                             QStringLiteral("&lt;img src=\"../../secret.png\"&gt;"))
                             .toUtf8()),
               QStringLiteral("write relative-resource layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("relative-resource.ui")),
                 QStringLiteral("forbidden URL"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("invalid.ui"));
        expect(writeFile(path, QByteArray("<ui version=\"4.0\">")),
               QStringLiteral("write invalid layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("invalid.ui")),
                 QStringLiteral("invalid layout XML"));
    }

    {
        const QString path = directory.filePath(QStringLiteral("too-large.ui"));
        expect(writeFile(path, QByteArray(513 * 1024, ' ')),
               QStringLiteral("write oversized layout"));
        rejected(themeForLayout(directory.path(), QStringLiteral("too-large.ui")),
                 QStringLiteral("layout file is too large"));
    }

    {
        QTemporaryDir other;
        expect(other.isValid(), QStringLiteral("second temporary directory available"));
        const QString outsidePath = other.filePath(QStringLiteral("layout.ui"));
        expect(writeFile(outsidePath, validLayoutUi().toUtf8()),
               QStringLiteral("write outside layout"));
        ThemeSpec theme = themeForLayout(directory.path(), QStringLiteral("layout.ui"));
        theme.layoutFile = QFileInfo(outsidePath).canonicalFilePath();
        rejected(theme, QStringLiteral("outside the theme package"));
    }

    {
        ThemeSpec theme = themeForLayout(directory.path(), QStringLiteral("layout.ui"));
        theme.layoutApiVersion = 2;
        rejected(theme, QStringLiteral("unsupported layout API version"));
    }

    const ThemeSpec builtIn = ThemeManager::loadTheme(QStringLiteral("light"), &error);
    expect(error.isEmpty() && builtIn.layoutFile.isEmpty() && builtIn.layoutError.isEmpty(),
           QStringLiteral("built-in themes use the default central layout"));

    const QString themeRoot = ThemeManager::themeDirectory();
    const QString typographyId = QStringLiteral("typography-test-theme");
    const QString typographyDirectory = QDir(themeRoot).filePath(typographyId);
    QDir(typographyDirectory).removeRecursively();
    const QByteArray typographyJson = R"JSON({
        "schemaVersion": 1,
        "id": "typography-test-theme",
        "name": "Typography Test Theme",
        "dark": false,
        "typography": {
            "uiFontFamily": "Test UI Sans",
            "previewFontFamily": "Test Preview Serif",
            "codeFontFamily": "Test Code Mono"
        },
        "metrics": {
            "spacing": 9,
            "documentMargin": 18
        }
    })JSON";
    expect(writeFile(QDir(typographyDirectory).filePath(QStringLiteral("theme.json")),
                     typographyJson),
           QStringLiteral("write typography theme"));
    const QString typographySettingsPath =
        directory.filePath(QStringLiteral("typography-settings.ini"));
    {
        QSettings settings(typographySettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), typographyId);
        settings.sync();
    }
    {
        MainWindow window(false, typographySettingsPath);
        auto *preview = window.findChild<QTextBrowser *>(QStringLiteral("previewView"));
        auto *source = window.findChild<QPlainTextEdit *>(QStringLiteral("sourceView"));
        QWidget *central = window.centralWidget();
        expect(QApplication::font().family() == QStringLiteral("Test UI Sans"),
               QStringLiteral("theme UI font family is applied"));
        expect(preview && preview->font().family() == QStringLiteral("Test Preview Serif"),
               QStringLiteral("theme preview font family is applied"));
        expect(source && source->font().family() == QStringLiteral("Test Code Mono"),
               QStringLiteral("theme source font family is applied"));
        expect(preview && preview->document()->documentMargin() == 18,
               QStringLiteral("theme document margin is applied"));
        expect(central && central->layout() && central->layout()->spacing() == 9,
               QStringLiteral("theme default layout spacing is applied"));

        window.setDarkMode(true);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        expect(QApplication::font().family() != QStringLiteral("Test UI Sans"),
               QStringLiteral("theme switch resets the UI font family"));
        expect(!source || source->font().family() != QStringLiteral("Test Code Mono"),
               QStringLiteral("theme switch resets the code font family"));
        expect(!preview || preview->font().family() != QStringLiteral("Test Preview Serif"),
               QStringLiteral("theme switch resets the preview font family"));
    }

    const QString layoutThemeId = QStringLiteral("layout-test-theme");
    const QString layoutThemeDirectory = QDir(themeRoot).filePath(layoutThemeId);
    QDir(layoutThemeDirectory).removeRecursively();
    expect(writeFile(QDir(layoutThemeDirectory).filePath(QStringLiteral("layout.ui")),
                     validLayoutUi(QStringLiteral("Metadata layout")).toUtf8()),
           QStringLiteral("write theme package layout"));
    const QByteArray layoutThemeJson = R"JSON({
        "schemaVersion": 1,
        "id": "layout-test-theme",
        "name": "Layout Test Theme",
        "dark": false,
        "metrics": { "spacing": 9 },
        "layout": { "apiVersion": 1, "file": "layout.ui" }
    })JSON";
    expect(writeFile(QDir(layoutThemeDirectory).filePath(QStringLiteral("theme.json")),
                     layoutThemeJson),
           QStringLiteral("write theme package manifest"));

    const ThemeSpec packaged =
        ThemeManager::loadTheme(layoutThemeId, &error);
    expect(error.isEmpty(), QStringLiteral("layout theme package loads"));
    expect(packaged.layoutApiVersion == 1 && !packaged.layoutFile.isEmpty()
               && packaged.layoutError.isEmpty(),
           QStringLiteral("layout theme package metadata resolves"));
    if (!packaged.layoutFile.isEmpty()) {
        expect(QFileInfo(packaged.layoutFile).canonicalFilePath()
                   == QFileInfo(QDir(layoutThemeDirectory).filePath(QStringLiteral("layout.ui")))
                          .canonicalFilePath(),
               QStringLiteral("layout theme file is resolved inside the package"));
    }

    const QString traversalId = QStringLiteral("layout-traversal-theme");
    const QString traversalDirectory = QDir(themeRoot).filePath(traversalId);
    QDir(traversalDirectory).removeRecursively();
    const QByteArray traversalJson = R"JSON({
        "schemaVersion": 1,
        "id": "layout-traversal-theme",
        "name": "Layout Traversal Theme",
        "layout": { "apiVersion": 1, "file": "../outside.ui" }
    })JSON";
    expect(writeFile(QDir(traversalDirectory).filePath(QStringLiteral("theme.json")),
                     traversalJson),
           QStringLiteral("write traversal theme"));
    const ThemeSpec traversal = ThemeManager::loadTheme(traversalId, &error);
    expect(error.isEmpty() && traversal.id == traversalId,
           QStringLiteral("invalid layout metadata does not discard theme colors"));
    expect(traversal.layoutFile.isEmpty() && !traversal.layoutError.isEmpty(),
           QStringLiteral("layout traversal is rejected in metadata"));

    const QString missingId = QStringLiteral("layout-missing-theme");
    const QString missingDirectory = QDir(themeRoot).filePath(missingId);
    QDir(missingDirectory).removeRecursively();
    const QByteArray missingJson = R"JSON({
        "schemaVersion": 1,
        "id": "layout-missing-theme",
        "name": "Layout Missing Theme",
        "layout": { "apiVersion": 1, "file": "missing.ui" }
    })JSON";
    expect(writeFile(QDir(missingDirectory).filePath(QStringLiteral("theme.json")),
                     missingJson),
           QStringLiteral("write missing-layout theme"));
    const ThemeSpec missing = ThemeManager::loadTheme(missingId, &error);
    expect(missing.layoutFile.isEmpty() && !missing.layoutError.isEmpty(),
           QStringLiteral("missing layout file is rejected in metadata"));

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), layoutThemeId);
        settings.sync();
    }
    {
        MainWindow window(false, settingsPath);
        expect(window.findChild<QWidget *>(QStringLiteral("central.root")) != nullptr,
               QStringLiteral("main window exposes the central root"));
        expect(window.findChild<QWidget *>(QStringLiteral("customMarker")) != nullptr,
               QStringLiteral("main window loads the theme central layout"));
        expect(window.findChild<QWidget *>(QStringLiteral("central.content")) != nullptr,
               QStringLiteral("main window keeps the central content slot"));
        expect(window.findChild<QDockWidget *>(QStringLiteral("outlineDock")) != nullptr,
               QStringLiteral("theme layout does not replace the outline dock"));
        expect(window.findChild<QTabWidget *>(QStringLiteral("documentTabs")) != nullptr,
               QStringLiteral("theme layout does not replace the document tabs"));
        expect(window.centralWidget() && window.centralWidget()->layout()
                   && window.centralWidget()->layout()->spacing() == 6,
               QStringLiteral("theme layout keeps its explicit spacing"));

        window.setDarkMode(true);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        expect(window.findChild<QWidget *>(QStringLiteral("customMarker")) == nullptr,
               QStringLiteral("switching to a built-in theme rebuilds the default layout"));
        expect(window.findChild<QWidget *>(QStringLiteral("central.content")) != nullptr,
               QStringLiteral("default layout remains available after a theme switch"));
        expect(window.findChild<QDockWidget *>(QStringLiteral("outlineDock")) != nullptr,
               QStringLiteral("theme switch keeps the outline dock"));
    }

    const QString badId = QStringLiteral("layout-bad-theme");
    const QString badDirectory = QDir(themeRoot).filePath(badId);
    QDir(badDirectory).removeRecursively();
    QString badUi = validLayoutUi();
    badUi.replace(QStringLiteral("central.toolbar.trailing"),
                  QStringLiteral("unusedTrailing"));
    expect(writeFile(QDir(badDirectory).filePath(QStringLiteral("layout.ui")),
                     badUi.toUtf8()),
           QStringLiteral("write bad theme layout"));
    const QByteArray badJson = R"JSON({
        "schemaVersion": 1,
        "id": "layout-bad-theme",
        "name": "Layout Bad Theme",
        "layout": { "apiVersion": 1, "file": "layout.ui" }
    })JSON";
    expect(writeFile(QDir(badDirectory).filePath(QStringLiteral("theme.json")),
                     badJson),
           QStringLiteral("write bad theme manifest"));
    const QString badSettingsPath = directory.filePath(QStringLiteral("bad-settings.ini"));
    {
        QSettings settings(badSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), badId);
        settings.sync();
    }
    {
        MainWindow window(false, badSettingsPath);
        expect(window.findChild<QWidget *>(QStringLiteral("customMarker")) == nullptr,
               QStringLiteral("invalid theme layout falls back to the default"));
        expect(window.findChild<QWidget *>(QStringLiteral("central.content")) != nullptr,
               QStringLiteral("fallback keeps the default central skeleton"));
        expect(window.findChild<QDockWidget *>(QStringLiteral("outlineDock")) != nullptr,
               QStringLiteral("fallback keeps the outline dock"));
    }

    QDir(typographyDirectory).removeRecursively();
    QDir(layoutThemeDirectory).removeRecursively();
    QDir(traversalDirectory).removeRecursively();
    QDir(missingDirectory).removeRecursively();
    QDir(badDirectory).removeRecursively();

    if (failures == 0) {
        QTextStream(stdout) << "ui layout tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
