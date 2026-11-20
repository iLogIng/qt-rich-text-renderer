#pragma once

#include "AppSettings.h"
#include "NavigationHistory.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QUrl>

#include <memory>

class QCloseEvent;
class QComboBox;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QTextBrowser;
class QToolButton;
class CodeHighlighter;
class MarkdownLinkHandler;
class MarkdownSourceHighlighter;
class OutlinePanel;
class RendererService;
class SvgObjectHandler;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(bool dark = false,
                        const QString &settingsPath = QString(),
                        QWidget *parent = nullptr);
    ~MainWindow() override;
    void loadFile(const QString &path);
    void setDarkMode(bool dark);
    bool isDarkMode() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void chooseFile();
    void renderNow();
    void showSettings();
    void toggleTheme();

private:
    enum class HistoryMode {
        Record,
        Restore,
    };

    enum class HistoryDirection {
        Back,
        Forward,
    };

    enum class LoadErrorMode {
        StatusOnly,
        Dialog,
    };

    void configureRenderer();
    void updateThemeButton();
    void updateHistoryUi();
    bool openLocalFile(const QString &path,
                       const QString &fragment,
                       HistoryMode mode,
                       LoadErrorMode errorMode);
    void navigateHistory(HistoryDirection direction);
    void selectHistoryIndex(int index);
    void showLoadError(const QString &path,
                       const QString &reason,
                       LoadErrorMode errorMode);
    void applyPendingAnchor();
    void scrollPreviewToBlock(int blockNumber);
    void applySettings(const AppSettingsData &previous);
    void applyApplicationFont(int pointSize);
    void applySourceFont(int pointSize);
    void updateToolbarHeights();
    void syncViewSettingsFromUi();

    std::unique_ptr<AppSettings> m_settings;
    AppSettingsData m_settingsData;
    QPlainTextEdit *m_source = nullptr;
    QTextBrowser *m_preview = nullptr;
    QTabWidget *m_tabs = nullptr;
    QDockWidget *m_outlineDock = nullptr;
    OutlinePanel *m_outlinePanel = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QComboBox *m_historyCombo = nullptr;
    RendererService *m_renderer = nullptr;
    SvgObjectHandler *m_svgHandler = nullptr;
    CodeHighlighter *m_codeHighlighter = nullptr;
    MarkdownSourceHighlighter *m_sourceHighlighter = nullptr;
    MarkdownLinkHandler *m_linkHandler = nullptr;
    NavigationHistory m_history;
    QString m_pendingAnchor;
    QUrl m_baseUrl;
    bool m_dark = false;
    QElapsedTimer m_renderTimer;
    int m_generation = 0;
    int m_pending = 0;
    int m_cacheHits = 0;
    int m_blockCount = 0;
};
