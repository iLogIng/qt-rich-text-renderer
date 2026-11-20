#pragma once

#include "NavigationHistory.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QUrl>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QToolButton;
class CodeHighlighter;
class MarkdownLinkHandler;
class RendererService;
class SvgObjectHandler;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(bool dark = false, QWidget *parent = nullptr);
    ~MainWindow() override;
    void loadFile(const QString &path);
    void setDarkMode(bool dark);

private slots:
    void loadSample();
    void renderNow();
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

    void configureRenderer();
    void updateThemeButton();
    void updateHistoryUi();
    bool openLocalFile(const QString &path, const QString &fragment, HistoryMode mode);
    void navigateHistory(HistoryDirection direction);
    void selectHistoryIndex(int index);
    void showLoadError(const QString &path, const QString &reason);
    void applyPendingAnchor();

    QPlainTextEdit *m_source = nullptr;
    QTextBrowser *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QComboBox *m_historyCombo = nullptr;
    RendererService *m_renderer = nullptr;
    SvgObjectHandler *m_svgHandler = nullptr;
    CodeHighlighter *m_codeHighlighter = nullptr;
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
