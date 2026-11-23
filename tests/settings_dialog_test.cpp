#include "SettingsDialog.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
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
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("rich-text-renderer-settings-test"));
    app.setOrganizationName(QStringLiteral("rich-text-renderer-settings-test"));

    const QString themeRoot =
        QDir(ThemeManager::themeDirectory()).filePath(QStringLiteral("settings-layout-theme"));
    QDir(themeRoot).removeRecursively();
    QDir().mkpath(themeRoot);
    for (const QString &name : {QStringLiteral("wide.ui"), QStringLiteral("compact.ui")}) {
        QFile file(QDir(themeRoot).filePath(name));
        expect(file.open(QIODevice::WriteOnly), QStringLiteral("write dummy layout"));
        file.close();
    }
    const QByteArray themeJson = R"JSON({
        "schemaVersion": 1,
        "id": "settings-layout-theme",
        "name": "Settings Layout Theme",
        "layouts": {
            "apiVersion": 1,
            "variants": [
                { "id": "wide", "file": "wide.ui", "contentMode": "tabs" },
                { "id": "compact", "file": "compact.ui", "contentMode": "slots" }
            ]
        }
    })JSON";
    QFile themeFile(QDir(themeRoot).filePath(QStringLiteral("theme.json")));
    expect(themeFile.open(QIODevice::WriteOnly), QStringLiteral("write settings layout theme"));
    themeFile.write(themeJson);
    themeFile.close();

    AppSettingsData current;
    current.themeId = QStringLiteral("settings-layout-theme");
    current.layoutId = QStringLiteral("compact");
    current.uiFontPointSize = 11;
    current.sourceFontPointSize = 12;
    current.sourceHighlighting = true;
    current.outlineVisible = true;
    current.sourceVisible = true;
    current.maxSvgWidth = 900;
    current.rendererIdleTimeoutMs = 5000;
    current.mathRequestTimeoutMs = 2000;
    current.mermaidRequestTimeoutMs = 3000;
    current.lastDirectory = QStringLiteral("/tmp");
    current.windowGeometry = QByteArray("geometry");

    AppSettingsData defaults;
    defaults.themeId = QStringLiteral("dark");
    defaults.uiFontPointSize = 14;
    defaults.sourceFontPointSize = 16;
    defaults.sourceHighlighting = false;
    defaults.outlineVisible = false;
    defaults.sourceVisible = false;
    defaults.maxSvgWidth = 1000;
    defaults.rendererIdleTimeoutMs = 6000;
    defaults.mathRequestTimeoutMs = 4000;
    defaults.mermaidRequestTimeoutMs = 5000;

    SettingsDialog dialog(current, defaults);

    auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("themeCombo"));
    auto *layout = dialog.findChild<QComboBox *>(QStringLiteral("layoutCombo"));
    auto *openThemes = dialog.findChild<QPushButton *>(QStringLiteral("openThemesButton"));
    auto *uiFont = dialog.findChild<QSpinBox *>(QStringLiteral("uiFontPointSizeSpin"));
    auto *sourceFont =
        dialog.findChild<QSpinBox *>(QStringLiteral("sourceFontPointSizeSpin"));
    auto *outline = dialog.findChild<QCheckBox *>(QStringLiteral("outlineVisibleCheck"));
    auto *maxWidth = dialog.findChild<QSpinBox *>(QStringLiteral("maxSvgWidthSpin"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();

    expect(theme && layout && openThemes && uiFont && sourceFont && outline && maxWidth
               && buttons,
           QStringLiteral("settings controls found"));
    if (!theme || !layout || !openThemes || !uiFont || !sourceFont || !outline || !maxWidth
        || !buttons) {
        return 1;
    }

    expect(layout->findData(QStringLiteral("wide")) >= 0
               && layout->findData(QStringLiteral("compact")) >= 0,
           QStringLiteral("theme layout variants are listed"));
    expect(layout->currentData().toString() == QStringLiteral("compact"),
           QStringLiteral("selected layout variant is loaded"));

    expect(theme->findData(QStringLiteral("light")) >= 0,
           QStringLiteral("built-in light theme is listed"));
    expect(theme->findData(QStringLiteral("dark")) >= 0,
           QStringLiteral("built-in dark theme is listed"));

    expect(uiFont->value() == 11, QStringLiteral("UI font size is loaded"));
    expect(sourceFont->value() == 12, QStringLiteral("source font size is loaded"));
    expect(maxWidth->value() == 900, QStringLiteral("preview width is loaded"));
    expect(buttons->button(QDialogButtonBox::RestoreDefaults) != nullptr,
           QStringLiteral("restore defaults button exists"));

    uiFont->setValue(13);
    sourceFont->setValue(18);
    theme->setCurrentIndex(theme->findData(QStringLiteral("dark")));
    layout->setCurrentIndex(layout->findData(QStringLiteral("wide")));
    outline->setChecked(false);
    maxWidth->setValue(1200);

    AppSettingsData edited = dialog.values();
    expect(edited.uiFontPointSize == 13, QStringLiteral("UI font size is read back"));
    expect(edited.sourceFontPointSize == 18, QStringLiteral("source font size is read back"));
    expect(edited.themeId == QStringLiteral("dark"), QStringLiteral("theme is read back"));
    expect(edited.layoutId.isEmpty(),
           QStringLiteral("built-in theme resets the layout id"));
    expect(!edited.outlineVisible, QStringLiteral("outline visibility is read back"));
    expect(edited.maxSvgWidth == 1200, QStringLiteral("preview width is read back"));
    expect(edited.lastDirectory == QStringLiteral("/tmp"),
           QStringLiteral("last directory is preserved"));
    expect(edited.windowGeometry == QByteArray("geometry"),
           QStringLiteral("window geometry is preserved"));

    buttons->button(QDialogButtonBox::RestoreDefaults)->click();
    const AppSettingsData restored = dialog.values();
    expect(restored.themeId == defaults.themeId, QStringLiteral("restore resets theme"));
    expect(restored.layoutId == defaults.layoutId, QStringLiteral("restore resets layout id"));
    expect(restored.uiFontPointSize == defaults.uiFontPointSize,
           QStringLiteral("restore resets UI font size"));
    expect(restored.sourceFontPointSize == defaults.sourceFontPointSize,
           QStringLiteral("restore resets source font size"));
    expect(restored.sourceHighlighting == defaults.sourceHighlighting,
           QStringLiteral("restore resets source highlighting"));
    expect(restored.outlineVisible == defaults.outlineVisible,
           QStringLiteral("restore resets outline visibility"));
    expect(restored.sourceVisible == defaults.sourceVisible,
           QStringLiteral("restore resets source visibility"));
    expect(restored.maxSvgWidth == defaults.maxSvgWidth,
           QStringLiteral("restore resets preview width"));
    expect(restored.lastDirectory == QStringLiteral("/tmp"),
           QStringLiteral("restore preserves last directory"));
    expect(restored.windowGeometry == QByteArray("geometry"),
           QStringLiteral("restore preserves window geometry"));

    QDir(QStringLiteral("settings-layout-theme")).removeRecursively();
    QDir(themeRoot).removeRecursively();

    if (failures == 0) {
        QTextStream(stdout) << "settings dialog tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
