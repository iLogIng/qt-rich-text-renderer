#include "AppSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
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

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rich-text-renderer-test"));
    app.setOrganizationName(QStringLiteral("rich-text-renderer-test"));

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("temporary directory available"));
    if (!directory.isValid()) {
        return 1;
    }

    const QString templatePath = directory.filePath(QStringLiteral("template.ini"));
    const QString userPath = directory.filePath(QStringLiteral("user/settings.ini"));
    const QByteArray templateData(
        "[appearance]\n"
        "theme=dark\n"
        "uiFontPointSize=12\n"
        "sourceFontPointSize=14\n"
        "sourceHighlighting=true\n"
        "[view]\n"
        "outlineVisible=true\n"
        "sourceVisible=false\n"
        "[preview]\n"
        "maxSvgWidth=900\n"
        "[renderer]\n"
        "idleTimeoutMs=5000\n"
        "mathRequestTimeoutMs=2000\n"
        "mermaidRequestTimeoutMs=3000\n");
    expect(writeFile(templatePath, templateData), QStringLiteral("write settings template"));

    AppSettings settings(userPath, templatePath);
    AppSettingsData data = settings.defaults();
    expect(data.theme == AppSettingsData::Theme::Dark, QStringLiteral("template theme is default"));
    expect(data.uiFontPointSize == 12, QStringLiteral("template UI font size is default"));
    expect(data.sourceFontPointSize == 14, QStringLiteral("template source font size is default"));
    expect(data.sourceVisible == false, QStringLiteral("template source visibility is default"));
    expect(data.maxSvgWidth == 900, QStringLiteral("template preview width is default"));
    expect(data.rendererIdleTimeoutMs == 5000, QStringLiteral("template idle timeout is default"));

    {
        QSettings user(userPath, QSettings::IniFormat);
        user.setValue(QStringLiteral("appearance/theme"), QStringLiteral("light"));
        user.setValue(QStringLiteral("appearance/sourceFontPointSize"), 30);
        user.setValue(QStringLiteral("appearance/uiFontPointSize"), 9);
        user.setValue(QStringLiteral("view/outlineVisible"), false);
        user.sync();
    }

    data = settings.load();
    expect(data.theme == AppSettingsData::Theme::Light, QStringLiteral("user theme overrides template"));
    expect(data.uiFontPointSize == 9, QStringLiteral("user UI font size overrides template"));
    expect(data.sourceFontPointSize == 24, QStringLiteral("source font size is clamped"));
    expect(!data.outlineVisible, QStringLiteral("user outline visibility overrides template"));
    expect(data.mathRequestTimeoutMs == 2000, QStringLiteral("missing user value falls back to template"));

    data.lastDirectory = directory.path();
    data.windowGeometry = QByteArray("geometry");
    settings.save(data);

    const AppSettings reloaded(userPath, templatePath);
    const AppSettingsData saved = reloaded.load();
    expect(saved.lastDirectory == directory.path(), QStringLiteral("last directory round-trips"));
    expect(saved.windowGeometry == QByteArray("geometry"), QStringLiteral("geometry round-trips"));
    expect(saved.rendererIdleTimeoutMs == 5000, QStringLiteral("saved data preserves renderer defaults"));
    expect(AppSettings::defaultUserPath().endsWith(QStringLiteral("settings.ini")),
           QStringLiteral("default user settings path ends with settings.ini"));

    if (failures == 0) {
        QTextStream(stdout) << "application settings tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
