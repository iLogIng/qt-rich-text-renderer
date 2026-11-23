#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

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

    const ThemeSpec dark = ThemeManager::loadTheme(QStringLiteral("dark"), &error);
    expect(error.isEmpty(), QStringLiteral("dark theme loads without error"));
    expect(dark.id == QStringLiteral("dark"), QStringLiteral("dark theme id"));
    expect(dark.dark, QStringLiteral("dark theme is dark"));
    expect(dark.text == QColor(QStringLiteral("#e6e6e6")),
           QStringLiteral("dark theme preserves the text color"));

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
        "metrics": {
            "radius": 10,
            "spacing": 7
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

    const QString style = ThemeManager::buildStyleSheet(custom, &error);
    expect(error.isEmpty(), QStringLiteral("custom QSS builds without error"));
    expect(!style.isEmpty(), QStringLiteral("custom QSS is not empty"));
    expect(!style.contains(QLatin1Char('@')), QStringLiteral("all QSS tokens are replaced"));
    expect(style.contains(QStringLiteral("#e07a5f")),
           QStringLiteral("QSS contains the custom accent"));

    ThemeManager::apply(custom);
    expect(qApp->styleSheet() == style, QStringLiteral("custom QSS is applied"));
    expect(QApplication::palette().color(QPalette::Window) == custom.window,
           QStringLiteral("custom palette is applied"));

    ThemeManager::apply(light);
    expect(qApp->styleSheet().isEmpty(), QStringLiteral("built-in theme clears QSS"));

    const QVector<ThemeInfo> themes = ThemeManager::availableThemes();
    const bool hasCustom = std::any_of(
        themes.cbegin(),
        themes.cend(),
        [](const ThemeInfo &info) { return info.id == QStringLiteral("test-custom"); });
    expect(hasCustom, QStringLiteral("available themes include custom theme"));

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
    QDir(invalidDirectory).removeRecursively();

    if (failures == 0) {
        QTextStream(stdout) << "theme tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
