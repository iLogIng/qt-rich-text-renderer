#include "CentralUiLayout.h"
#include "MainWindow.h"
#include "PreviewBrowser.h"
#include "Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTextBlock>
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

QString slotsLayoutUi()
{
    return QStringLiteral(R"UI(<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>CentralWidget</class>
 <widget class="QWidget" name="central.root">
  <layout class="QVBoxLayout" name="centralLayout">
   <item>
    <layout class="QHBoxLayout" name="toolbarRow">
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
    </layout>
   </item>
   <item>
    <widget class="QWidget" name="central.outline">
     <layout class="QHBoxLayout" name="outlineLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QWidget" name="central.preview">
     <layout class="QVBoxLayout" name="previewLayout"/>
    </widget>
   </item>
   <item>
    <widget class="QWidget" name="central.source">
     <layout class="QVBoxLayout" name="sourceLayout"/>
    </widget>
   </item>
  </layout>
 </widget>
</ui>
)UI");
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
        const QString path = directory.filePath(QStringLiteral("slots-layout.ui"));
        expect(writeFile(path, slotsLayoutUi().toUtf8()),
               QStringLiteral("write slots layout"));
        ThemeSpec theme = themeForLayout(directory.path(), QStringLiteral("slots-layout.ui"));
        theme.contentMode = QStringLiteral("slots");
        QWidget slotsParent;
        const CentralUiLayout slotsLayout =
            CentralUiBuilder::loadTheme(theme, &slotsParent, &error);
        expect(slotsLayout.isValid(), QStringLiteral("slots layout loads"));
        expect(slotsLayout.slotsMode && slotsLayout.preview && slotsLayout.source
                   && slotsLayout.outline,
               QStringLiteral("slots layout exposes preview, source and outline slots"));
        expect(!slotsLayout.content, QStringLiteral("slots layout has no content slot"));
        expect(slotsLayout.preview->property("centralSlot").toString()
                       == QStringLiteral("preview")
                   && slotsLayout.source->property("centralSlot").toString()
                          == QStringLiteral("source"),
               QStringLiteral("slots layout exposes preview/source QSS properties"));
        delete slotsLayout.root;

        ThemeSpec contentInSlots = theme;
        QString invalidUi = slotsLayoutUi();
        invalidUi.replace(QStringLiteral("<widget class=\"QWidget\" name=\"central.outline\">"),
                          QStringLiteral("<widget class=\"QWidget\" name=\"central.content\">"));
        invalidUi.replace(QStringLiteral("name=\"outlineLayout\""),
                          QStringLiteral("name=\"contentLayout\""));
        expect(writeFile(directory.filePath(QStringLiteral("slots-content.ui")),
                         invalidUi.toUtf8()),
               QStringLiteral("write slots-content layout"));
        contentInSlots.layoutFile =
            QFileInfo(directory.filePath(QStringLiteral("slots-content.ui")))
                .canonicalFilePath();
        rejected(contentInSlots, QStringLiteral("central.content is not allowed"));
    }

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
        const QString path = directory.filePath(QStringLiteral("asset-layout.ui"));
        QString ui = validLayoutUi();
        ui.replace(QStringLiteral("name=\"customMarker\""),
                   QStringLiteral("name=\"themeLogo\""));
        ui.replace(QStringLiteral("<property name=\"text\"><string>Custom layout</string></property>"),
                   QStringLiteral("<property name=\"themeAsset\" stdset=\"0\">"
                                  "<string>logo</string></property>"));
        expect(writeFile(path, ui.toUtf8()), QStringLiteral("write asset layout"));
        const QString imagePath = directory.filePath(QStringLiteral("assets/logo.png"));
        QDir().mkpath(QFileInfo(imagePath).absolutePath());
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(Qt::red);
        expect(image.save(imagePath, "PNG"), QStringLiteral("write asset image"));

        ThemeSpec theme = themeForLayout(directory.path(), QStringLiteral("asset-layout.ui"));
        ThemeAsset asset;
        asset.id = QStringLiteral("logo");
        asset.filePath = QFileInfo(imagePath).canonicalFilePath();
        asset.format = QStringLiteral("png");
        asset.size = image.size();
        theme.assets.insert(asset.id, asset);
        QWidget assetParent;
        const CentralUiLayout assetLayout =
            CentralUiBuilder::loadTheme(theme, &assetParent, &error);
        expect(assetLayout.isValid(), QStringLiteral("asset layout loads"));
        auto *logo = assetLayout.isValid()
                         ? assetLayout.root->findChild<QLabel *>(QStringLiteral("themeLogo"))
                         : nullptr;
        expect(logo && !logo->pixmap().isNull(), QStringLiteral("theme asset is injected"));
        expect(logo && !logo->hasScaledContents(),
               QStringLiteral("theme asset is not stretched"));
        expect(logo && (logo->alignment() & Qt::AlignCenter),
               QStringLiteral("theme asset is centered"));
        delete assetLayout.root;

        ThemeSpec missingTheme = theme;
        missingTheme.assets.clear();
        rejected(missingTheme, QStringLiteral("unknown theme asset"));
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
                   QStringLiteral("name=\"central.forbidden\""));
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

    const QString previewThemeId = QStringLiteral("preview-style-theme");
    const QString previewThemeDirectory = QDir(themeRoot).filePath(previewThemeId);
    QDir(previewThemeDirectory).removeRecursively();
    const QByteArray previewThemeJson = R"JSON({
        "schemaVersion": 1,
        "id": "preview-style-theme",
        "name": "Preview Style Theme",
        "dark": false,
        "preview": {
            "maxTextWidth": 400,
            "codeBackground": "#202020",
            "codeBlockMargin": 4
        }
    })JSON";
    expect(writeFile(QDir(previewThemeDirectory).filePath(QStringLiteral("theme.json")),
                     previewThemeJson),
           QStringLiteral("write preview style theme"));
    const QString previewSettingsPath =
        directory.filePath(QStringLiteral("preview-settings.ini"));
    {
        QSettings settings(previewSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), previewThemeId);
        settings.sync();
    }
    const QString previewDocumentPath =
        directory.filePath(QStringLiteral("preview-style.md"));
    expect(writeFile(previewDocumentPath,
                     QByteArray("# Preview style\n\n```cpp\nint value = 1;\n```\n")),
           QStringLiteral("write preview style document"));
    {
        MainWindow window(false, previewSettingsPath);
        window.show();
        window.loadFile(previewDocumentPath);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        auto *preview = window.findChild<PreviewBrowser *>(QStringLiteral("previewView"));
        expect(preview && preview->document()->textWidth() == 400,
               QStringLiteral("preview max text width is applied"));
        expect(preview && preview->contentMargins().left() > 0
                   && preview->contentMargins().right() > 0,
               QStringLiteral("preview content is centered"));
        if (preview) {
            bool foundCodeBlock = false;
            for (QTextBlock block = preview->document()->begin(); block.isValid();
                 block = block.next()) {
                if (block.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage)
                    == QStringLiteral("cpp")) {
                    foundCodeBlock = true;
                    expect(block.blockFormat().background().color()
                               == QColor(QStringLiteral("#202020")),
                           QStringLiteral("preview code background is applied"));
                    break;
                }
            }
            expect(foundCodeBlock, QStringLiteral("preview code block exists"));
        }
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

    const QString slotsThemeId = QStringLiteral("slots-test-theme");
    const QString slotsThemeDirectory = QDir(themeRoot).filePath(slotsThemeId);
    QDir(slotsThemeDirectory).removeRecursively();
    expect(writeFile(QDir(slotsThemeDirectory).filePath(QStringLiteral("layout.ui")),
                     slotsLayoutUi().toUtf8()),
           QStringLiteral("write slots theme layout"));
    const QByteArray slotsThemeJson = R"JSON({
        "schemaVersion": 1,
        "id": "slots-test-theme",
        "name": "Slots Test Theme",
        "layout": {
            "apiVersion": 1,
            "file": "layout.ui",
            "contentMode": "slots"
        }
    })JSON";
    expect(writeFile(QDir(slotsThemeDirectory).filePath(QStringLiteral("theme.json")),
                     slotsThemeJson),
           QStringLiteral("write slots theme manifest"));
    const QString slotsSettingsPath =
        directory.filePath(QStringLiteral("slots-settings.ini"));
    {
        QSettings settings(slotsSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), slotsThemeId);
        settings.sync();
    }
    {
        MainWindow window(false, slotsSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        auto *previewSlot = window.findChild<QWidget *>(QStringLiteral("central.preview"));
        auto *sourceSlot = window.findChild<QWidget *>(QStringLiteral("central.source"));
        auto *outlineSlot = window.findChild<QWidget *>(QStringLiteral("central.outline"));
        auto *preview = window.findChild<PreviewBrowser *>(QStringLiteral("previewView"));
        auto *source = window.findChild<QPlainTextEdit *>(QStringLiteral("sourceView"));
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
        auto *outlineToggle =
            window.findChild<QToolButton *>(QStringLiteral("outlineToggleButton"));
        auto *outlineDock =
            window.findChild<QDockWidget *>(QStringLiteral("outlineDock"));
        expect(previewSlot && sourceSlot && outlineSlot && preview && source && tabs
                   && outlineDock,
               QStringLiteral("slots mode widgets found"));
        expect(previewSlot && previewSlot->isAncestorOf(preview),
               QStringLiteral("preview is injected into central.preview"));
        expect(sourceSlot && sourceSlot->isAncestorOf(source),
               QStringLiteral("source is injected into central.source"));
        expect(tabs && tabs->isHidden(),
               QStringLiteral("document tabs are hidden in slots mode"));
        expect(outlineToggle && outlineSlot && outlineSlot->isAncestorOf(outlineToggle),
               QStringLiteral("outline toggle uses the outline slot"));
        expect(sourceSlot && !sourceSlot->isHidden(),
               QStringLiteral("source slot is visible by default"));
        expect(outlineDock && outlineDock->width() >= outlineDock->minimumWidth()
                   && outlineDock->minimumWidth() >= 200,
               QStringLiteral("slots layout keeps a usable outline width"));

        window.resize(1024, 700);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        expect(outlineDock && outlineDock->width() >= outlineDock->minimumWidth(),
               QStringLiteral("narrow slots layout keeps the outline usable"));

        window.setDarkMode(true);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        expect(tabs && !tabs->isHidden(),
               QStringLiteral("theme switch restores the tabbed content"));
        expect(outlineDock && outlineDock->width() >= outlineDock->minimumWidth(),
               QStringLiteral("theme switch restores the outline width"));
        expect(preview && preview->parentWidget()
                   && preview->parentWidget()->objectName() == QStringLiteral("central.preview"),
               QStringLiteral("theme switch restores the preview page"));
        expect(source && source->parentWidget()
                   && source->parentWidget()->objectName() == QStringLiteral("central.source"),
               QStringLiteral("theme switch restores the source page"));
    }

    const QString slotsHiddenSettingsPath =
        directory.filePath(QStringLiteral("slots-hidden-settings.ini"));
    {
        QSettings settings(slotsHiddenSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), slotsThemeId);
        settings.setValue(QStringLiteral("view/sourceVisible"), false);
        settings.sync();
    }
    {
        MainWindow window(false, slotsHiddenSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        auto *sourceSlot = window.findChild<QWidget *>(QStringLiteral("central.source"));
        expect(sourceSlot && sourceSlot->isHidden(),
               QStringLiteral("source slot honors the source visibility setting"));
    }

    const QString variantsThemeId = QStringLiteral("variants-test-theme");
    const QString variantsThemeDirectory = QDir(themeRoot).filePath(variantsThemeId);
    QDir(variantsThemeDirectory).removeRecursively();
    expect(writeFile(QDir(variantsThemeDirectory).filePath(QStringLiteral("wide.ui")),
                     validLayoutUi().toUtf8()),
           QStringLiteral("write wide variant layout"));
    expect(writeFile(QDir(variantsThemeDirectory).filePath(QStringLiteral("compact.ui")),
                     slotsLayoutUi().toUtf8()),
           QStringLiteral("write compact variant layout"));
    expect(writeFile(QDir(variantsThemeDirectory).filePath(QStringLiteral("broken.ui")),
                     QByteArray("<ui version=\"4.0\">")),
           QStringLiteral("write broken variant layout"));
    const QByteArray variantsThemeJson = R"JSON({
        "schemaVersion": 1,
        "id": "variants-test-theme",
        "name": "Variants Test Theme",
        "layouts": {
            "apiVersion": 1,
            "variants": [
                { "id": "wide", "file": "wide.ui", "contentMode": "tabs" },
                { "id": "compact", "file": "compact.ui", "contentMode": "slots" },
                { "id": "broken", "file": "broken.ui", "contentMode": "tabs" }
            ]
        }
    })JSON";
    expect(writeFile(QDir(variantsThemeDirectory).filePath(QStringLiteral("theme.json")),
                     variantsThemeJson),
           QStringLiteral("write variants theme manifest"));
    const QString compactSettingsPath =
        directory.filePath(QStringLiteral("variants-compact.ini"));
    {
        QSettings settings(compactSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), variantsThemeId);
        settings.setValue(QStringLiteral("appearance/layoutId"), QStringLiteral("compact"));
        settings.sync();
    }
    {
        MainWindow window(false, compactSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
        auto *previewSlot = window.findChild<QWidget *>(QStringLiteral("central.preview"));
        auto *preview = window.findChild<PreviewBrowser *>(QStringLiteral("previewView"));
        expect(previewSlot && previewSlot->isAncestorOf(preview),
               QStringLiteral("manual layout selection uses the compact variant"));
        expect(tabs && tabs->isHidden(),
               QStringLiteral("manual slots variant hides the document tabs"));
    }
    const QString wideSettingsPath =
        directory.filePath(QStringLiteral("variants-wide.ini"));
    {
        QSettings settings(wideSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), variantsThemeId);
        settings.setValue(QStringLiteral("appearance/layoutId"), QStringLiteral("wide"));
        settings.sync();
    }
    {
        MainWindow window(false, wideSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
        auto *content = window.findChild<QWidget *>(QStringLiteral("central.content"));
        expect(content && tabs && !tabs->isHidden(),
               QStringLiteral("manual layout selection uses the wide variant"));
        expect(tabs && content && content->isAncestorOf(tabs),
               QStringLiteral("wide variant keeps the tabbed content slot"));
    }
    const QString fallbackSettingsPath =
        directory.filePath(QStringLiteral("variants-fallback.ini"));
    {
        QSettings settings(fallbackSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), variantsThemeId);
        settings.setValue(QStringLiteral("appearance/layoutId"), QStringLiteral("missing"));
        settings.sync();
    }
    {
        MainWindow window(false, fallbackSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        auto *tabs = window.findChild<QTabWidget *>(QStringLiteral("documentTabs"));
        auto *content = window.findChild<QWidget *>(QStringLiteral("central.content"));
        expect(content && tabs && !tabs->isHidden(),
               QStringLiteral("missing layout id falls back to the first variant"));
    }

    const QString brokenSettingsPath =
        directory.filePath(QStringLiteral("variants-broken.ini"));
    {
        QSettings settings(brokenSettingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/themeId"), variantsThemeId);
        settings.setValue(QStringLiteral("appearance/layoutId"), QStringLiteral("broken"));
        settings.sync();
    }
    {
        MainWindow window(false, brokenSettingsPath);
        window.show();
        QCoreApplication::processEvents();
        expect(window.findChild<QWidget *>(QStringLiteral("customMarker")) != nullptr,
               QStringLiteral("broken variant falls back to the first variant"));
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
    QDir(previewThemeDirectory).removeRecursively();
    QDir(slotsThemeDirectory).removeRecursively();
    QDir(variantsThemeDirectory).removeRecursively();
    QDir(layoutThemeDirectory).removeRecursively();
    QDir(traversalDirectory).removeRecursively();
    QDir(missingDirectory).removeRecursively();
    QDir(badDirectory).removeRecursively();

    if (failures == 0) {
        QTextStream(stdout) << "ui layout tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
