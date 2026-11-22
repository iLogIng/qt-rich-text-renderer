#pragma once

#include "AppSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const AppSettingsData &current,
                   const AppSettingsData &defaults,
                   QWidget *parent = nullptr);

    AppSettingsData values() const;

private:
    void loadValues(const AppSettingsData &data);

    AppSettingsData m_current;
    AppSettingsData m_defaults;
    QComboBox *m_theme = nullptr;
    QSpinBox *m_uiFontPointSize = nullptr;
    QSpinBox *m_sourceFontPointSize = nullptr;
    QCheckBox *m_sourceHighlighting = nullptr;
    QCheckBox *m_outlineVisible = nullptr;
    QCheckBox *m_sourceVisible = nullptr;
    QSpinBox *m_maxSvgWidth = nullptr;
    QSpinBox *m_idleTimeoutMs = nullptr;
    QSpinBox *m_mathRequestTimeoutMs = nullptr;
    QSpinBox *m_mermaidRequestTimeoutMs = nullptr;
};
