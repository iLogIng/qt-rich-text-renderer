#include "SettingsDialog.h"

#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const AppSettingsData &current,
                               const AppSettingsData &defaults,
                               QWidget *parent)
    : QDialog(parent)
    , m_current(current)
    , m_defaults(defaults)
{
    setWindowTitle(tr("Settings"));
    setModal(true);

    auto *appearanceGroup = new QGroupBox(tr("Appearance"), this);
    auto *appearanceForm = new QFormLayout(appearanceGroup);

    m_theme = new QComboBox(appearanceGroup);
    m_theme->setObjectName(QStringLiteral("themeCombo"));
    for (const ThemeInfo &theme : ThemeManager::availableThemes()) {
        m_theme->addItem(theme.name, theme.id);
    }
    appearanceForm->addRow(tr("Theme"), m_theme);

    m_layout = new QComboBox(appearanceGroup);
    m_layout->setObjectName(QStringLiteral("layoutCombo"));
    m_layout->setToolTip(tr("Named central layout provided by the selected theme"));
    appearanceForm->addRow(tr("Layout"), m_layout);
    connect(m_theme,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { populateLayouts(); });

    auto *openThemes = new QPushButton(tr("Open themes folder"), appearanceGroup);
    openThemes->setObjectName(QStringLiteral("openThemesButton"));
    connect(openThemes, &QPushButton::clicked, this, []() {
        const QString directory = ThemeManager::themeDirectory();
        QDir().mkpath(directory);
        QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
    });
    appearanceForm->addRow(openThemes);

    m_uiFontPointSize = new QSpinBox(appearanceGroup);
    m_uiFontPointSize->setObjectName(QStringLiteral("uiFontPointSizeSpin"));
    m_uiFontPointSize->setRange(8, 18);
    m_uiFontPointSize->setSuffix(tr(" pt"));
    appearanceForm->addRow(tr("Application and preview font size"), m_uiFontPointSize);

    m_sourceFontPointSize = new QSpinBox(appearanceGroup);
    m_sourceFontPointSize->setObjectName(QStringLiteral("sourceFontPointSizeSpin"));
    m_sourceFontPointSize->setRange(8, 24);
    m_sourceFontPointSize->setSuffix(tr(" pt"));
    appearanceForm->addRow(tr("Source font size"), m_sourceFontPointSize);

    m_sourceHighlighting = new QCheckBox(tr("Highlight Markdown in Source"), appearanceGroup);
    m_sourceHighlighting->setObjectName(QStringLiteral("sourceHighlightingCheck"));
    appearanceForm->addRow(m_sourceHighlighting);

    auto *viewGroup = new QGroupBox(tr("View"), this);
    auto *viewForm = new QFormLayout(viewGroup);
    m_outlineVisible = new QCheckBox(tr("Show document outline"), viewGroup);
    m_outlineVisible->setObjectName(QStringLiteral("outlineVisibleCheck"));
    viewForm->addRow(m_outlineVisible);
    m_sourceVisible = new QCheckBox(tr("Show Source tab"), viewGroup);
    m_sourceVisible->setObjectName(QStringLiteral("sourceVisibleCheck"));
    viewForm->addRow(m_sourceVisible);

    auto *previewGroup = new QGroupBox(tr("Preview"), this);
    auto *previewForm = new QFormLayout(previewGroup);
    m_maxSvgWidth = new QSpinBox(previewGroup);
    m_maxSvgWidth->setObjectName(QStringLiteral("maxSvgWidthSpin"));
    m_maxSvgWidth->setRange(320, 2000);
    m_maxSvgWidth->setSuffix(tr(" px"));
    previewForm->addRow(tr("Maximum SVG width"), m_maxSvgWidth);

    auto *advancedGroup = new QGroupBox(tr("Advanced"), this);
    auto *advancedForm = new QFormLayout(advancedGroup);
    m_idleTimeoutMs = new QSpinBox(advancedGroup);
    m_idleTimeoutMs->setObjectName(QStringLiteral("idleTimeoutSpin"));
    m_idleTimeoutMs->setRange(1000, 60 * 60 * 1000);
    m_idleTimeoutMs->setSuffix(tr(" ms"));
    advancedForm->addRow(tr("Renderer idle timeout"), m_idleTimeoutMs);

    m_mathRequestTimeoutMs = new QSpinBox(advancedGroup);
    m_mathRequestTimeoutMs->setObjectName(QStringLiteral("mathTimeoutSpin"));
    m_mathRequestTimeoutMs->setRange(1000, 5 * 60 * 1000);
    m_mathRequestTimeoutMs->setSuffix(tr(" ms"));
    advancedForm->addRow(tr("Math request timeout"), m_mathRequestTimeoutMs);

    m_mermaidRequestTimeoutMs = new QSpinBox(advancedGroup);
    m_mermaidRequestTimeoutMs->setObjectName(QStringLiteral("mermaidTimeoutSpin"));
    m_mermaidRequestTimeoutMs->setRange(1000, 5 * 60 * 1000);
    m_mermaidRequestTimeoutMs->setSuffix(tr(" ms"));
    advancedForm->addRow(tr("Mermaid request timeout"), m_mermaidRequestTimeoutMs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults),
            &QPushButton::clicked,
            this,
            [this]() { loadValues(m_defaults); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(appearanceGroup);
    layout->addWidget(viewGroup);
    layout->addWidget(previewGroup);
    layout->addWidget(advancedGroup);
    layout->addWidget(buttons);

    loadValues(m_current);
}

void SettingsDialog::populateLayouts(const QString &preferredId)
{
    if (!m_layout) {
        return;
    }

    const QString previous = preferredId.isEmpty()
                                 ? m_layout->currentData().toString()
                                 : preferredId;
    m_layout->clear();
    QString error;
    const ThemeSpec theme = ThemeManager::loadTheme(m_theme->currentData().toString(), &error);
    if (error.isEmpty() && theme.layoutError.isEmpty() && !theme.layoutVariants.isEmpty()) {
        for (const ThemeLayout &variant : theme.layoutVariants) {
            m_layout->addItem(variant.id, variant.id);
        }
    } else {
        m_layout->addItem(tr("Default layout"), QString());
    }
    const int preferredIndex = m_layout->findData(previous);
    m_layout->setCurrentIndex(preferredIndex >= 0 ? preferredIndex : 0);
    m_layout->setEnabled(!theme.layoutVariants.isEmpty());
}

void SettingsDialog::loadValues(const AppSettingsData &data)
{
    const int themeIndex = m_theme->findData(data.themeId);
    m_theme->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    populateLayouts(data.layoutId);
    m_uiFontPointSize->setValue(data.uiFontPointSize);
    m_sourceFontPointSize->setValue(data.sourceFontPointSize);
    m_sourceHighlighting->setChecked(data.sourceHighlighting);
    m_outlineVisible->setChecked(data.outlineVisible);
    m_sourceVisible->setChecked(data.sourceVisible);
    m_maxSvgWidth->setValue(data.maxSvgWidth);
    m_idleTimeoutMs->setValue(data.rendererIdleTimeoutMs);
    m_mathRequestTimeoutMs->setValue(data.mathRequestTimeoutMs);
    m_mermaidRequestTimeoutMs->setValue(data.mermaidRequestTimeoutMs);
}

AppSettingsData SettingsDialog::values() const
{
    AppSettingsData data = m_current;
    data.themeId = m_theme->currentData().toString();
    data.layoutId = m_layout->currentData().toString();
    data.uiFontPointSize = m_uiFontPointSize->value();
    data.sourceFontPointSize = m_sourceFontPointSize->value();
    data.sourceHighlighting = m_sourceHighlighting->isChecked();
    data.outlineVisible = m_outlineVisible->isChecked();
    data.sourceVisible = m_sourceVisible->isChecked();
    data.maxSvgWidth = m_maxSvgWidth->value();
    data.rendererIdleTimeoutMs = m_idleTimeoutMs->value();
    data.mathRequestTimeoutMs = m_mathRequestTimeoutMs->value();
    data.mermaidRequestTimeoutMs = m_mermaidRequestTimeoutMs->value();
    return data;
}
