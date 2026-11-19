#pragma once

#include <QElapsedTimer>
#include <QMainWindow>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
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
    void configureRenderer();
    void updateThemeButton();

    QPlainTextEdit *m_source = nullptr;
    QTextBrowser *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_renderButton = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_themeButton = nullptr;
    RendererService *m_renderer = nullptr;
    SvgObjectHandler *m_svgHandler = nullptr;
    bool m_dark = false;
    QElapsedTimer m_renderTimer;
    int m_generation = 0;
    int m_pending = 0;
    int m_cacheHits = 0;
    int m_blockCount = 0;
};
