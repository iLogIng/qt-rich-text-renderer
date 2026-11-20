#include "MainWindow.h"

#include "CodeHighlighter.h"
#include "LocalFileLoader.h"
#include "MarkdownDocumentBuilder.h"
#include "MarkdownLinkHandler.h"
#include "MarkdownPreprocessor.h"
#include "MarkdownSourceHighlighter.h"
#include "RendererService.h"
#include "SvgObjectHandler.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDebug>
#include <cstdio>

namespace {

QString runtimeRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appImageRoot = QDir(appDir).absoluteFilePath(QStringLiteral("../share/rich-text-render"));
    if (QFileInfo::exists(appImageRoot + QStringLiteral("/renderer/dist/mathjax-worker.mjs"))) {
        return QDir(appImageRoot).absolutePath();
    }
    return QStringLiteral(RENDERER_ROOT);
}

QString qjsPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundled = QDir(appDir).absoluteFilePath(QStringLiteral("qjs"));
    if (QFileInfo::exists(bundled)) {
        return bundled;
    }
    return runtimeRoot() + QStringLiteral("/third_party/quickjs-ng/build/qjs");
}

} // namespace

MainWindow::MainWindow(bool dark, QWidget *parent)
    : QMainWindow(parent)
    , m_dark(dark)
{
    applyTheme(m_dark);

    setWindowTitle(QStringLiteral("rich-text-render spike"));
    resize(1280, 860);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout;
    m_loadButton = new QPushButton(QStringLiteral("Load"), central);
    m_loadButton->setToolTip(QStringLiteral("Open a local Markdown or text file"));
    m_renderButton = new QPushButton(QStringLiteral("Render (Ctrl+Enter)"), central);
    m_themeButton = new QPushButton(central);
    updateThemeButton();
    m_status = new QLabel(QStringLiteral("No document open"), central);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backButton = new QToolButton(central);
    m_backButton->setObjectName(QStringLiteral("historyBackButton"));
    m_backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_backButton->setToolTip(QStringLiteral("Back"));
    m_backButton->setAutoRaise(true);
    m_forwardButton = new QToolButton(central);
    m_forwardButton->setObjectName(QStringLiteral("historyForwardButton"));
    m_forwardButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_forwardButton->setToolTip(QStringLiteral("Forward"));
    m_forwardButton->setAutoRaise(true);
    m_historyCombo = new QComboBox(central);
    m_historyCombo->setObjectName(QStringLiteral("historyCombo"));
    m_historyCombo->setPlaceholderText(QStringLiteral("History"));
    m_historyCombo->setMinimumContentsLength(18);
    m_historyCombo->setMaximumWidth(280);
    m_historyCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_historyCombo->setMaxVisibleItems(16);
    m_historyCombo->setToolTip(QStringLiteral("Local file history (newest first, max 64)"));
    const int toolbarHeight = m_renderButton->sizeHint().height();
    m_backButton->setFixedHeight(toolbarHeight);
    m_forwardButton->setFixedHeight(toolbarHeight);
    m_historyCombo->setFixedHeight(toolbarHeight);

    toolbar->addWidget(m_loadButton);
    toolbar->addWidget(m_renderButton);
    toolbar->addWidget(m_themeButton);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_status, 1);
    toolbar->addWidget(m_backButton);
    toolbar->addWidget(m_forwardButton);
    toolbar->addWidget(m_historyCombo);
    layout->addLayout(toolbar);

    m_tabs = new QTabWidget(central);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));

    m_source = new QPlainTextEdit(m_tabs);
    m_source->setObjectName(QStringLiteral("sourceView"));
    m_source->setReadOnly(true);
    m_source->setPlaceholderText(QStringLiteral("Markdown source"));
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixedFont.setPointSize(10);
    m_source->setFont(fixedFont);
    m_sourceHighlighter = new MarkdownSourceHighlighter(m_source->document(), m_dark, this);

    m_preview = new QTextBrowser(m_tabs);
    m_preview->setObjectName(QStringLiteral("previewView"));
    m_preview->setOpenExternalLinks(false);
    m_preview->setReadOnly(true);
    m_preview->document()->setDocumentMargin(12);
    m_svgHandler = new SvgObjectHandler(this);
    m_preview->document()->documentLayout()->registerHandler(SvgObjectHandler::Type, m_svgHandler);
    m_codeHighlighter = new CodeHighlighter(m_preview->document(), m_dark, this);
    m_linkHandler = new MarkdownLinkHandler(m_preview, this, this);

    m_tabs->addTab(m_preview, tr("Preview"));
    m_tabs->addTab(m_source, tr("Source"));
    m_tabs->setCurrentWidget(m_preview);
    layout->addWidget(m_tabs, 1);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Spike: QuickJS + MathJax + Mermaid -> SVG -> QtSvg"));

    m_renderer = new RendererService(this);
    configureRenderer();

    connect(m_renderButton, &QPushButton::clicked, this, &MainWindow::renderNow);
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(m_backButton, &QToolButton::clicked, this, [this]() {
        navigateHistory(HistoryDirection::Back);
    });
    connect(m_forwardButton, &QToolButton::clicked, this, [this]() {
        navigateHistory(HistoryDirection::Forward);
    });
    connect(m_historyCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::selectHistoryIndex);
    connect(m_linkHandler, &MarkdownLinkHandler::localFileRequested,
            this, [this](const QString &path, const QString &fragment) {
                openLocalFile(path,
                              fragment,
                              HistoryMode::Record,
                              LoadErrorMode::Dialog);
            });

    auto *renderShortcut = new QAction(this);
    renderShortcut->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));
    renderShortcut->setShortcutContext(Qt::WindowShortcut);
    connect(renderShortcut, &QAction::triggered, this, &MainWindow::renderNow);
    addAction(renderShortcut);

    updateHistoryUi();
}

MainWindow::~MainWindow()
{
    if (m_renderer) {
        m_renderer->shutdown();
    }
}

void MainWindow::configureRenderer()
{
    const QString root = runtimeRoot();
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/rich-text-render-spike");
    bool idleOk = false;
    const int idleEnv = qEnvironmentVariableIntValue("RICH_TEXT_RENDER_IDLE_MS", &idleOk);
    const int idleMs = idleOk && idleEnv > 0 ? idleEnv : 60000;
    m_renderer->configure(qjsPath(),
                          root + QStringLiteral("/renderer/dist/mathjax-worker.mjs"),
                          root + QStringLiteral("/renderer/dist/mermaid-full-worker.mjs"),
                          cacheDir,
                          QStringLiteral("mathjax-3.2.2;mermaid-13.0.0-alpha.3;qjs-0.18.0;service-v3-full"),
                          idleMs);
}

void MainWindow::loadFile(const QString &path)
{
    openLocalFile(path,
                  QString(),
                  HistoryMode::Record,
                  LoadErrorMode::StatusOnly);
}

void MainWindow::chooseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open file"),
        QString(),
        tr("Markdown files (*.md *.markdown);;Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    openLocalFile(path,
                  QString(),
                  HistoryMode::Record,
                  LoadErrorMode::Dialog);
}

bool MainWindow::openLocalFile(const QString &path,
                               const QString &fragment,
                               HistoryMode mode,
                               LoadErrorMode errorMode)
{
    const LocalFileContent loaded = LocalFileLoader::load(path);
    if (!loaded.ok) {
        showLoadError(path, loaded.error, errorMode);
        return false;
    }

    const QFileInfo info(loaded.path);
    const QString displayName = info.fileName().isEmpty() ? loaded.path : info.fileName();

    QString directory = info.absolutePath();
    if (!directory.endsWith(QLatin1Char('/'))) {
        directory += QLatin1Char('/');
    }
    m_baseUrl = QUrl::fromLocalFile(directory);
    if (m_linkHandler) {
        m_linkHandler->setBaseUrl(m_baseUrl);
    }

    m_source->setPlainText(loaded.text);
    m_pendingAnchor = fragment;

    if (mode == HistoryMode::Record) {
        m_history.visit(loaded.path, displayName);
    }

    renderNow();
    updateHistoryUi();
    return true;
}

void MainWindow::updateHistoryUi()
{
    if (!m_historyCombo) {
        return;
    }

    const QVector<NavigationEntry> entries = m_history.entriesNewestFirst();
    const QSignalBlocker blocker(m_historyCombo);
    m_historyCombo->clear();
    for (const NavigationEntry &entry : entries) {
        m_historyCombo->addItem(entry.displayName, entry.path);
        m_historyCombo->setItemData(m_historyCombo->count() - 1,
                                    entry.path,
                                    Qt::ToolTipRole);
    }
    m_historyCombo->setCurrentIndex(m_history.currentIndexNewestFirst());
    m_historyCombo->setEnabled(!entries.isEmpty());

    if (m_backButton) {
        m_backButton->setEnabled(m_history.canBack());
    }
    if (m_forwardButton) {
        m_forwardButton->setEnabled(m_history.canForward());
    }
}

void MainWindow::navigateHistory(HistoryDirection direction)
{
    const int previousIndex = m_history.currentIndexNewestFirst();
    const QString path = direction == HistoryDirection::Back ? m_history.back()
                                                              : m_history.forward();
    if (path.isEmpty()) {
        updateHistoryUi();
        return;
    }

    if (!openLocalFile(path,
                       QString(),
                       HistoryMode::Restore,
                       LoadErrorMode::Dialog)) {
        m_history.setCurrentIndexNewestFirst(previousIndex);
        updateHistoryUi();
    }
}

void MainWindow::selectHistoryIndex(int index)
{
    const QVector<NavigationEntry> entries = m_history.entriesNewestFirst();
    if (index < 0 || index >= entries.size()) {
        return;
    }

    const int previousIndex = m_history.currentIndexNewestFirst();
    if (index == previousIndex) {
        return;
    }

    if (!m_history.setCurrentIndexNewestFirst(index)) {
        updateHistoryUi();
        return;
    }

    if (!openLocalFile(entries.at(index).path,
                       QString(),
                       HistoryMode::Restore,
                       LoadErrorMode::Dialog)) {
        m_history.setCurrentIndexNewestFirst(previousIndex);
        updateHistoryUi();
    }
}

void MainWindow::showLoadError(const QString &path,
                               const QString &reason,
                               LoadErrorMode errorMode)
{
    const QFileInfo info(path);
    const QString displayName = info.fileName().isEmpty() ? path : info.fileName();
    const QString detail = QStringLiteral("%1\n\n%2").arg(displayName, reason);
    m_status->setText(QStringLiteral("Cannot open %1: %2").arg(displayName, reason));

    if (errorMode == LoadErrorMode::StatusOnly) {
        return;
    }

    QMessageBox::warning(this,
                         tr("Cannot open file"),
                         tr("This file cannot be displayed in the window:\n\n%1")
                             .arg(detail));
}

void MainWindow::applyPendingAnchor()
{
    if (m_pendingAnchor.isEmpty()) {
        return;
    }

    const QString anchor = m_pendingAnchor;
    m_pendingAnchor.clear();
    MarkdownLinkHandler::scrollToAnchor(m_preview, anchor);
}

void MainWindow::updateThemeButton()
{
    if (m_themeButton) {
        m_themeButton->setText(m_dark ? QStringLiteral("Light mode") : QStringLiteral("Dark mode"));
    }
}

void MainWindow::toggleTheme()
{
    setDarkMode(!m_dark);
}

void MainWindow::setDarkMode(bool dark)
{
    if (m_dark == dark) {
        updateThemeButton();
        return;
    }
    m_dark = dark;
    applyTheme(dark);
    updateThemeButton();
    if (m_codeHighlighter) {
        m_codeHighlighter->setDark(dark);
    }
    if (m_sourceHighlighter) {
        m_sourceHighlighter->setDark(dark);
    }
    if (!m_source->toPlainText().isEmpty()) {
        renderNow();
    }
}

void MainWindow::renderNow()
{
    m_renderer->cancelAll();
    ++m_generation;
    const int generation = m_generation;
    const QString source = m_source->toPlainText();
    const PreparedMarkdown prepared = MarkdownPreprocessor::prepare(source);
    const bool debug = qEnvironmentVariableIsSet("RICH_TEXT_RENDER_DEBUG");
    if (debug) {
        fprintf(stderr, "PREPARED MARKDOWN:\n%s\n", prepared.markdown.toUtf8().constData());
    }

    m_preview->setMarkdown(prepared.markdown);
    if (m_preview->document()->baseUrl() != m_baseUrl) {
        m_preview->document()->setBaseUrl(m_baseUrl);
    }
    if (m_codeHighlighter) {
        m_codeHighlighter->rehighlight();
    }
    m_blockCount = prepared.blocks.size();
    m_pending = m_blockCount;
    m_cacheHits = 0;
    m_renderTimer.start();

    if (prepared.blocks.isEmpty()) {
        m_status->setText(QStringLiteral("Markdown rendered; no math/mermaid blocks found."));
        applyPendingAnchor();
        return;
    }

    m_status->setText(QStringLiteral("Rendering %1 blocks...").arg(m_blockCount));

    for (const RenderBlock &block : prepared.blocks) {
        auto callback = [this, generation, block](const RenderResult &result) {
            if (generation != m_generation) {
                return;
            }
            if (result.fromCache) {
                ++m_cacheHits;
            }
            QString error;
            const bool ok = MarkdownDocumentBuilder::embedSvg(
                m_preview->document(),
                block.token,
                result.svg,
                result.error,
                block.kind == RenderBlock::Kind::Mermaid,
                m_dark,
                m_preview->palette().color(QPalette::Text),
                860,
                &error);
            if (!ok) {
                qWarning() << "render failed for" << block.token << error;
            }
            if (qEnvironmentVariableIsSet("RICH_TEXT_RENDER_DEBUG")) {
                fprintf(stderr,
                        "BLOCK kind=%s cache=%d ms=%lld svgBytes=%lld ok=%d error=%s\n",
                        block.kind == RenderBlock::Kind::Math ? "math" : "mermaid",
                        result.fromCache ? 1 : 0,
                        static_cast<long long>(result.elapsedMs),
                        static_cast<long long>(result.svg.toUtf8().size()),
                        ok ? 1 : 0,
                        error.toUtf8().constData());
            }
            --m_pending;
            if (m_pending == 0) {
                m_status->setText(QStringLiteral("Rendered %1 blocks in %2 ms; cache hits: %3")
                                      .arg(m_blockCount)
                                      .arg(m_renderTimer.elapsed())
                                      .arg(m_cacheHits));
                if (qEnvironmentVariableIsSet("RICH_TEXT_RENDER_DEBUG")) {
                    fprintf(stderr, "SUMMARY blocks=%d elapsedMs=%lld cacheHits=%d\n",
                            m_blockCount,
                            static_cast<long long>(m_renderTimer.elapsed()),
                            m_cacheHits);
                }
                applyPendingAnchor();
            }
        };

        if (block.kind == RenderBlock::Kind::Math) {
            m_renderer->renderMath(block.source, block.display, callback);
        } else {
            m_renderer->renderMermaid(block.source, m_dark, callback);
        }
    }
}
