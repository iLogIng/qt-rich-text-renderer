#pragma once

#include "AppSettings.h"
#include "CentralUiLayout.h"
#include "NavigationHistory.h"
#include "Theme.h"

#include <QElapsedTimer>
#include <QFont>
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
class QToolButton;
class CodeHighlighter;
class MarkdownLinkHandler;
class MarkdownSourceHighlighter;
class OutlinePanel;
class PreviewBrowser;
class RendererService;
class SvgObjectHandler;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(bool dark = false,
                        const QString &settingsPath = QString(),
                        QWidget *parent = nullptr);
    ~MainWindow() override;
    void loadFile(const QString &path, bool reportErrors = true);
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
    void buildCentralShell();
    void createCentralUiWidgets();
    void populateCentralShell();
    void attachContentWidgets();
    void placeOutlineToggle();
    void setSourceVisibleInLayout(bool visible);
    void showPreviewContent();
    void rebuildCentralLayout();
    void setThemeId(const QString &id);
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
    void schedulePreviewRefresh();
    void finishPreviewLoad();
    void forcePreviewLayout();
    void resetPreviewToStart();
    void applyPendingAnchor();
    void scrollPreviewToBlock(int blockNumber);
    void applySettings(const AppSettingsData &previous);
    void applyApplicationFont(int pointSize);
    void applyPreviewFont(int pointSize);
    void applySourceFont(int pointSize);
    void applyOutlineTogglePalette();
    void applyOutlineChromePalette();
    void updateToolbarHeights();
    void syncViewSettingsFromUi();
    void previewImageUrl(const QUrl &url);
    void previewSvg(const QString &svg);

    std::unique_ptr<AppSettings> m_settings;
    AppSettingsData m_settingsData;
    QPlainTextEdit *m_source = nullptr;
    PreviewBrowser *m_preview = nullptr;
    QWidget *m_previewPage = nullptr;
    QWidget *m_sourcePage = nullptr;
    QTabWidget *m_tabs = nullptr;
    CentralUiLayout m_centralSlots;
    QString m_activeLayoutFile;
    QString m_activeLayoutId;
    QDockWidget *m_outlineDock = nullptr;
    OutlinePanel *m_outlinePanel = nullptr;
    QToolButton *m_outlineToggleButton = nullptr;
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
    QFont m_baseApplicationFont;
    QString m_pendingAnchor;
    bool m_loadAnchorRequested = false;
    QUrl m_baseUrl;
    ThemeSpec m_theme;
    QElapsedTimer m_renderTimer;
    int m_generation = 0;
    int m_loadGeneration = 0;
    int m_pending = 0;
    int m_cacheHits = 0;
    int m_blockCount = 0;
};
