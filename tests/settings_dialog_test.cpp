#include "SettingsDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSpinBox>
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
    QApplication app(argc, argv);

    AppSettingsData current;
    current.theme = AppSettingsData::Theme::Light;
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
    defaults.theme = AppSettingsData::Theme::Dark;
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
    auto *uiFont = dialog.findChild<QSpinBox *>(QStringLiteral("uiFontPointSizeSpin"));
    auto *sourceFont =
        dialog.findChild<QSpinBox *>(QStringLiteral("sourceFontPointSizeSpin"));
    auto *outline = dialog.findChild<QCheckBox *>(QStringLiteral("outlineVisibleCheck"));
    auto *maxWidth = dialog.findChild<QSpinBox *>(QStringLiteral("maxSvgWidthSpin"));
    auto *buttons = dialog.findChild<QDialogButtonBox *>();

    expect(theme && uiFont && sourceFont && outline && maxWidth && buttons,
           QStringLiteral("settings controls found"));
    if (!theme || !uiFont || !sourceFont || !outline || !maxWidth || !buttons) {
        return 1;
    }

    expect(uiFont->value() == 11, QStringLiteral("UI font size is loaded"));
    expect(sourceFont->value() == 12, QStringLiteral("source font size is loaded"));
    expect(maxWidth->value() == 900, QStringLiteral("preview width is loaded"));
    expect(buttons->button(QDialogButtonBox::RestoreDefaults) != nullptr,
           QStringLiteral("restore defaults button exists"));

    uiFont->setValue(13);
    sourceFont->setValue(18);
    theme->setCurrentIndex(1);
    outline->setChecked(false);
    maxWidth->setValue(1200);

    AppSettingsData edited = dialog.values();
    expect(edited.uiFontPointSize == 13, QStringLiteral("UI font size is read back"));
    expect(edited.sourceFontPointSize == 18, QStringLiteral("source font size is read back"));
    expect(edited.theme == AppSettingsData::Theme::Dark, QStringLiteral("theme is read back"));
    expect(!edited.outlineVisible, QStringLiteral("outline visibility is read back"));
    expect(edited.maxSvgWidth == 1200, QStringLiteral("preview width is read back"));
    expect(edited.lastDirectory == QStringLiteral("/tmp"),
           QStringLiteral("last directory is preserved"));
    expect(edited.windowGeometry == QByteArray("geometry"),
           QStringLiteral("window geometry is preserved"));

    buttons->button(QDialogButtonBox::RestoreDefaults)->click();
    const AppSettingsData restored = dialog.values();
    expect(restored.theme == defaults.theme, QStringLiteral("restore resets theme"));
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

    if (failures == 0) {
        QTextStream(stdout) << "settings dialog tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
