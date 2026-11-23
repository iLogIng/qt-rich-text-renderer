#include "CentralUiLayout.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPalette>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QWidget>

#include <algorithm>

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

bool copyDirectory(const QString &sourcePath, const QString &targetPath)
{
    const QDir source(sourcePath);
    if (!source.exists() || !QDir().mkpath(targetPath)) {
        return false;
    }

    bool copied = true;
    const QFileInfoList entries =
        source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(targetPath).filePath(entry.fileName());
        if (entry.isDir()) {
            copied = copyDirectory(entry.absoluteFilePath(), target) && copied;
        } else {
            copied = QFile::copy(entry.absoluteFilePath(), target) && copied;
        }
    }
    return copied;
}

} // namespace

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rich-text-renderer-theme-test"));
    app.setOrganizationName(QStringLiteral("rich-text-renderer-theme-test"));

    QString error;
    const ThemeSpec light = ThemeManager::loadTheme(QStringLiteral("light"), &error);
    expect(error.isEmpty(), QStringLiteral("light theme loads without error"));
    expect(light.id == QStringLiteral("light"), QStringLiteral("light theme id"));
    expect(light.name == QStringLiteral("Light"), QStringLiteral("light theme name"));
    expect(!light.dark, QStringLiteral("light theme is not dark"));
    expect(!light.useQss, QStringLiteral("built-in light theme does not use QSS"));
    expect(light.window == QColor(QStringLiteral("#ffffff")),
           QStringLiteral("light theme preserves the window color"));
    expect(light.documentMargin == 12, QStringLiteral("light theme keeps the document margin"));
    expect(light.spacing == 6, QStringLiteral("light theme keeps the layout spacing"));
    expect(light.codeSyntax.keyword == QColor(QStringLiteral("#0033b3")),
           QStringLiteral("light theme keeps the code syntax defaults"));
    expect(light.sourceSyntax.heading == QColor(QStringLiteral("#0b57d0")),
           QStringLiteral("light theme keeps the source syntax defaults"));
    expect(light.mermaid.name == QStringLiteral("default"),
           QStringLiteral("light theme keeps the default Mermaid theme"));
    const QString lightStyle = ThemeManager::buildStyleSheet(light, &error);
    expect(error.isEmpty() && !lightStyle.contains(QStringLiteral("centralSlot")),
           QStringLiteral("theme without slot styles emits no slot QSS"));

    const ThemeSpec dark = ThemeManager::loadTheme(QStringLiteral("dark"), &error);
    expect(error.isEmpty(), QStringLiteral("dark theme loads without error"));
    expect(dark.id == QStringLiteral("dark"), QStringLiteral("dark theme id"));
    expect(dark.dark, QStringLiteral("dark theme is dark"));
    expect(dark.text == QColor(QStringLiteral("#e6e6e6")),
           QStringLiteral("dark theme preserves the text color"));
    expect(dark.codeSyntax.keyword == QColor(QStringLiteral("#cf8e6d")),
           QStringLiteral("dark theme keeps the code syntax defaults"));
    expect(dark.mermaid.name == QStringLiteral("dark"),
           QStringLiteral("dark theme keeps the dark Mermaid theme"));

    const QString userDirectory = ThemeManager::themeDirectory();
    const QString customDirectory = QDir(userDirectory).filePath(QStringLiteral("test-custom"));
    const QByteArray customJson = R"JSON({
        "schemaVersion": 1,
        "id": "test-custom",
        "name": "Test Custom",
        "dark": true,
        "colors": {
            "window": "#202124",
            "base": "#18191c",
            "text": "#f0f0f0",
            "highlight": "#e07a5f",
            "border": "#3c4043"
        },
        "typography": {
            "uiFontFamily": "Test UI Sans",
            "previewFontFamily": "Test Preview Serif",
            "codeFontFamily": "Test Code Mono"
        },
        "metrics": {
            "radius": 10,
            "spacing": 7,
            "documentMargin": 18
        },
        "syntax": {
            "code": {
                "keyword": "#112233"
            },
            "source": {
                "heading": "#445566"
            }
        },
        "preview": {
            "maxTextWidth": 720,
            "codeBackground": "#202020",
            "codeBlockMargin": 4
        },
        "mermaid": {
            "theme": "forest",
            "variables": {
                "primaryColor": "#123456",
                "textColor": "#abcdef"
            }
        },
        "slots": {
            "top": {
                "background": "#101010",
                "borderColor": "#202020",
                "borderWidth": 2,
                "radius": 4,
                "padding": 3
            }
        }
    })JSON";
    expect(writeFile(QDir(customDirectory).filePath(QStringLiteral("theme.json")), customJson),
           QStringLiteral("write custom theme"));

    const ThemeSpec custom = ThemeManager::loadTheme(QStringLiteral("test-custom"), &error);
    expect(error.isEmpty(), QStringLiteral("custom theme loads without error"));
    expect(custom.dark, QStringLiteral("custom theme dark flag"));
    expect(custom.useQss, QStringLiteral("custom theme enables QSS by default"));
    expect(custom.highlight == QColor(QStringLiteral("#e07a5f")),
           QStringLiteral("custom theme color loads"));
    expect(custom.radius == 10, QStringLiteral("custom theme radius loads"));
    expect(custom.documentMargin == 18, QStringLiteral("custom theme document margin loads"));
    expect(custom.uiFontFamily == QStringLiteral("Test UI Sans"),
           QStringLiteral("custom theme UI font family loads"));
    expect(custom.previewFontFamily == QStringLiteral("Test Preview Serif"),
           QStringLiteral("custom theme preview font family loads"));
    expect(custom.codeFontFamily == QStringLiteral("Test Code Mono"),
           QStringLiteral("custom theme code font family loads"));
    expect(custom.codeSyntax.keyword == QColor(QStringLiteral("#112233")),
           QStringLiteral("custom theme code syntax color loads"));
    expect(custom.sourceSyntax.heading == QColor(QStringLiteral("#445566")),
           QStringLiteral("custom theme source syntax color loads"));
    expect(custom.slotStyles.contains(QStringLiteral("top")),
           QStringLiteral("custom theme slot style loads"));
    expect(custom.preview.maxTextWidth == 720,
           QStringLiteral("custom theme preview width loads"));
    expect(custom.preview.codeBackground == QColor(QStringLiteral("#202020")),
           QStringLiteral("custom theme code background loads"));
    expect(custom.preview.codeBlockMargin == 4,
           QStringLiteral("custom theme code block margin loads"));
    expect(custom.mermaid.name == QStringLiteral("forest"),
           QStringLiteral("custom Mermaid theme loads"));
    expect(custom.mermaid.variables.value(QStringLiteral("primaryColor"))
               == QStringLiteral("#123456"),
           QStringLiteral("custom Mermaid color variable loads"));

    const QString style = ThemeManager::buildStyleSheet(custom, &error);
    expect(error.isEmpty(), QStringLiteral("custom QSS builds without error"));
    expect(!style.isEmpty(), QStringLiteral("custom QSS is not empty"));
    expect(!style.contains(QLatin1Char('@')), QStringLiteral("all QSS tokens are replaced"));
    expect(style.contains(QStringLiteral("#e07a5f")),
           QStringLiteral("QSS contains the custom accent"));
    expect(style.contains(QStringLiteral("QWidget[centralSlot=\"top\"]")),
           QStringLiteral("QSS contains the fixed slot selector"));
    expect(style.contains(QStringLiteral("background-color: #101010")),
           QStringLiteral("QSS contains the slot background"));
    expect(style.contains(QStringLiteral("border: 2px solid #202020")),
           QStringLiteral("QSS contains the slot border"));
    expect(style.contains(QStringLiteral("border-radius: 4px")),
           QStringLiteral("QSS contains the slot radius"));
    expect(style.contains(QStringLiteral("padding: 3px")),
           QStringLiteral("QSS contains the slot padding"));

    ThemeManager::apply(custom);
    expect(qApp->styleSheet() == style, QStringLiteral("custom QSS is applied"));
    expect(QApplication::palette().color(QPalette::Window) == custom.window,
           QStringLiteral("custom palette is applied"));
    expect(QApplication::palette().color(QPalette::Light).lightness() < 128,
           QStringLiteral("dark palette sets dark bevel roles"));
    expect(QApplication::palette().color(QPalette::Disabled, QPalette::Text)
               == custom.mutedText,
           QStringLiteral("dark palette sets disabled text"));

    ThemeManager::apply(light);
    expect(qApp->styleSheet().isEmpty(), QStringLiteral("built-in theme clears QSS"));
    expect(QApplication::palette().color(QPalette::Light).lightness() > 128,
           QStringLiteral("light palette restores light bevel roles"));

    const QVector<ThemeInfo> themes = ThemeManager::availableThemes();
    const bool hasCustom = std::any_of(
        themes.cbegin(),
        themes.cend(),
        [](const ThemeInfo &info) { return info.id == QStringLiteral("test-custom"); });
    expect(hasCustom, QStringLiteral("available themes include custom theme"));

    const QString assetThemeId = QStringLiteral("asset-test-theme");
    const QString assetThemeDirectory = QDir(userDirectory).filePath(assetThemeId);
    QDir(assetThemeDirectory).removeRecursively();
    const QString assetImagePath =
        QDir(assetThemeDirectory).filePath(QStringLiteral("assets/logo.png"));
    QDir().mkpath(QFileInfo(assetImagePath).absolutePath());
    QImage assetImage(8, 8, QImage::Format_ARGB32);
    assetImage.fill(Qt::blue);
    expect(assetImage.save(assetImagePath, "PNG"), QStringLiteral("write theme asset image"));
    const QByteArray assetThemeJson = R"JSON({
        "schemaVersion": 1,
        "id": "asset-test-theme",
        "name": "Asset Test Theme",
        "assets": {
            "logo": "assets/logo.png"
        }
    })JSON";
    expect(writeFile(QDir(assetThemeDirectory).filePath(QStringLiteral("theme.json")),
                     assetThemeJson),
           QStringLiteral("write asset theme"));
    error.clear();
    const ThemeSpec assetTheme = ThemeManager::loadTheme(assetThemeId, &error);
    expect(error.isEmpty() && assetTheme.assetError.isEmpty(),
           QStringLiteral("asset theme loads without error"));
    expect(assetTheme.assets.contains(QStringLiteral("logo")),
           QStringLiteral("asset manifest is parsed"));
    if (assetTheme.assets.contains(QStringLiteral("logo"))) {
        expect(assetTheme.assets.value(QStringLiteral("logo")).format
                   == QStringLiteral("png"),
               QStringLiteral("asset format is detected"));
        expect(assetTheme.assets.value(QStringLiteral("logo")).size == QSize(8, 8),
               QStringLiteral("asset dimensions are recorded"));
    }

    const QString invalidAssetThemeId = QStringLiteral("invalid-asset-theme");
    const QString invalidAssetThemeDirectory =
        QDir(userDirectory).filePath(invalidAssetThemeId);
    QDir(invalidAssetThemeDirectory).removeRecursively();
    const QByteArray invalidAssetJson = R"JSON({
        "schemaVersion": 1,
        "id": "invalid-asset-theme",
        "name": "Invalid Asset Theme",
        "assets": {
            "logo": "../escape.png"
        }
    })JSON";
    expect(writeFile(QDir(invalidAssetThemeDirectory).filePath(QStringLiteral("theme.json")),
                     invalidAssetJson),
           QStringLiteral("write invalid asset theme"));
    error.clear();
    const ThemeSpec invalidAsset = ThemeManager::loadTheme(invalidAssetThemeId, &error);
    expect(error.isEmpty() && !invalidAsset.assetError.isEmpty(),
           QStringLiteral("asset path error keeps theme colors available"));
    expect(invalidAsset.assets.isEmpty(),
           QStringLiteral("invalid asset is not exposed"));

    const QString invalidSlotDirectory =
        QDir(userDirectory).filePath(QStringLiteral("invalid-slot-theme"));
    const QByteArray invalidSlotJson = R"JSON({
        "schemaVersion": 1,
        "id": "invalid-slot-theme",
        "name": "Invalid Slot Theme",
        "slots": {
            "unknown": {
                "background": "#000000"
            }
        }
    })JSON";
    expect(writeFile(QDir(invalidSlotDirectory).filePath(QStringLiteral("theme.json")),
                     invalidSlotJson),
           QStringLiteral("write invalid slot theme"));
    error.clear();
    const ThemeSpec invalidSlot =
        ThemeManager::loadTheme(QStringLiteral("invalid-slot-theme"), &error);
    expect(!error.isEmpty(), QStringLiteral("unknown slot style reports an error"));
    expect(invalidSlot.id == ThemeManager::fallbackThemeId(),
           QStringLiteral("unknown slot style falls back to the default"));

    const QString invalidSyntaxDirectory =
        QDir(userDirectory).filePath(QStringLiteral("invalid-syntax-theme"));
    const QByteArray invalidSyntaxJson = R"JSON({
        "schemaVersion": 1,
        "id": "invalid-syntax-theme",
        "name": "Invalid Syntax Theme",
        "syntax": {
            "code": {
                "keyword": "not-a-color"
            }
        }
    })JSON";
    expect(writeFile(QDir(invalidSyntaxDirectory).filePath(QStringLiteral("theme.json")),
                     invalidSyntaxJson),
           QStringLiteral("write invalid syntax theme"));
    error.clear();
    const ThemeSpec invalidSyntax =
        ThemeManager::loadTheme(QStringLiteral("invalid-syntax-theme"), &error);
    expect(!error.isEmpty(), QStringLiteral("invalid syntax color reports an error"));
    expect(invalidSyntax.id == ThemeManager::fallbackThemeId(),
           QStringLiteral("invalid syntax color falls back to the default"));

    const QString sampleSourceDirectory =
        QStringLiteral(TEST_SOURCE_DIR) + QStringLiteral("/samples/themes/nixie-crt");
    const QString sampleThemeId = QStringLiteral("nixie-crt");
    const QString sampleThemeDirectory = QDir(userDirectory).filePath(sampleThemeId);
    QDir(sampleThemeDirectory).removeRecursively();
    expect(copyDirectory(sampleSourceDirectory, sampleThemeDirectory),
           QStringLiteral("copy the nixie sample theme"));
    error.clear();
    const ThemeSpec nixie = ThemeManager::loadTheme(sampleThemeId, &error);
    expect(error.isEmpty() && nixie.id == sampleThemeId,
           QStringLiteral("nixie sample theme loads"));
    expect(nixie.dark && nixie.useQss,
           QStringLiteral("nixie sample theme is dark and uses QSS"));
    expect(nixie.assetError.isEmpty() && nixie.layoutError.isEmpty(),
           QStringLiteral("nixie sample theme accepts its asset and layouts"));
    expect(nixie.uiFontFamily == QStringLiteral("Noto Sans Mono")
               && nixie.previewFontFamily == QStringLiteral("Noto Sans Mono")
               && nixie.codeFontFamily == QStringLiteral("Noto Sans Mono"),
           QStringLiteral("nixie sample theme sets the mono families"));
    expect(nixie.preview.maxTextWidth == 0
               && nixie.preview.codeBackground == QColor(QStringLiteral("#160d07"))
               && nixie.preview.codeBlockMargin == 6,
           QStringLiteral("nixie sample theme sets the preview layout"));
    expect(nixie.mermaid.name == QStringLiteral("base")
               && nixie.mermaid.variables.size() == 8,
           QStringLiteral("nixie sample theme sets all Mermaid variables"));
    expect(nixie.assets.contains(QStringLiteral("nixie")),
           QStringLiteral("nixie sample theme exposes its PNG asset"));
    if (nixie.assets.contains(QStringLiteral("nixie"))) {
        const ThemeAsset asset = nixie.assets.value(QStringLiteral("nixie"));
        expect(asset.format == QStringLiteral("png") && asset.size == QSize(96, 34),
               QStringLiteral("nixie sample asset is a 96x34 PNG"));
    }
    expect(nixie.slotStyles.size() == 9
               && nixie.slotStyles.contains(QStringLiteral("top"))
               && nixie.slotStyles.contains(QStringLiteral("bottom"))
               && nixie.slotStyles.contains(QStringLiteral("empty"))
               && nixie.slotStyles.contains(QStringLiteral("toolbarLeading"))
               && nixie.slotStyles.contains(QStringLiteral("toolbarTrailing"))
               && nixie.slotStyles.contains(QStringLiteral("content"))
               && nixie.slotStyles.contains(QStringLiteral("preview"))
               && nixie.slotStyles.contains(QStringLiteral("source"))
               && nixie.slotStyles.contains(QStringLiteral("outline")),
           QStringLiteral("nixie sample theme styles every layout slot"));
    error.clear();
    const QString nixieStyle = ThemeManager::buildStyleSheet(nixie, &error);
    expect(error.isEmpty()
               && nixieStyle.contains(QStringLiteral("QWidget[centralSlot=\"preview\"]"))
               && nixieStyle.contains(QStringLiteral("QWidget[centralSlot=\"source\"]")),
           QStringLiteral("nixie sample theme emits slot QSS"));
    expect(nixie.layoutVariants.size() == 2,
           QStringLiteral("nixie sample theme provides two layout variants"));
    if (nixie.layoutVariants.size() == 2) {
        const ThemeLayout &console = nixie.layoutVariants.at(0);
        const ThemeLayout &dual = nixie.layoutVariants.at(1);
        expect(console.id == QStringLiteral("console")
                   && console.contentMode == QStringLiteral("tabs"),
               QStringLiteral("nixie console variant uses tabs"));
        expect(dual.id == QStringLiteral("dual")
                   && dual.contentMode == QStringLiteral("slots"),
               QStringLiteral("nixie dual variant uses slots"));

        for (const ThemeLayout &variant : nixie.layoutVariants) {
            ThemeSpec layoutTheme = nixie;
            layoutTheme.layoutFile = variant.file;
            layoutTheme.contentMode = variant.contentMode;
            QWidget parent;
            QString layoutError;
            const CentralUiLayout layout =
                CentralUiBuilder::loadTheme(layoutTheme, &parent, &layoutError);
            expect(layout.isValid(),
                   QStringLiteral("nixie %1 layout loads").arg(variant.id));
            if (!layout.isValid()) {
                continue;
            }
            if (variant.id == QStringLiteral("console")) {
                expect(!layout.slotsMode && layout.content,
                       QStringLiteral("nixie console layout uses the content slot"));
            } else {
                expect(layout.slotsMode && layout.preview && layout.source
                           && layout.outline && !layout.content,
                       QStringLiteral("nixie dual layout exposes preview, source and outline"));
            }
            delete layout.root;
        }
    }

    const QString invalidDirectory = QDir(userDirectory).filePath(QStringLiteral("invalid-theme"));
    expect(writeFile(QDir(invalidDirectory).filePath(QStringLiteral("theme.json")),
                     QByteArray("{ invalid json")),
           QStringLiteral("write invalid theme"));
    error.clear();
    const ThemeSpec fallback = ThemeManager::loadTheme(QStringLiteral("invalid-theme"), &error);
    expect(!error.isEmpty(), QStringLiteral("invalid theme reports an error"));
    expect(fallback.id == ThemeManager::fallbackThemeId(),
           QStringLiteral("invalid theme falls back to the default"));

    QDir(customDirectory).removeRecursively();
    QDir(assetThemeDirectory).removeRecursively();
    QDir(invalidAssetThemeDirectory).removeRecursively();
    QDir(invalidSlotDirectory).removeRecursively();
    QDir(invalidSyntaxDirectory).removeRecursively();
    QDir(invalidDirectory).removeRecursively();
    QDir(sampleThemeDirectory).removeRecursively();

    if (failures == 0) {
        QTextStream(stdout) << "theme tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
