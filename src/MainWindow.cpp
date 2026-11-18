#include "MainWindow.h"

#include "MarkdownRenderer.h"
#include "RendererClient.h"
#include "Theme.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolBar>
#include <QVBoxLayout>
#include <QDebug>
#include <cstdio>

namespace {

QString runtimeRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appImageRoot = QDir(appDir).absoluteFilePath(QStringLiteral("../share/rich-text-render"));
    if (QFileInfo::exists(appImageRoot + QStringLiteral("/renderer/dist/mathjax-qjs.mjs"))) {
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
    m_loadButton = new QPushButton(QStringLiteral("Load sample"), central);
    m_renderButton = new QPushButton(QStringLiteral("Render (Ctrl+Enter)"), central);
    m_themeButton = new QPushButton(central);
    updateThemeButton();
    m_status = new QLabel(QStringLiteral("Ready"), central);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    toolbar->addWidget(m_loadButton);
    toolbar->addWidget(m_renderButton);
    toolbar->addWidget(m_themeButton);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_status, 1);
    layout->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    m_source = new QPlainTextEdit(splitter);
    m_source->setPlaceholderText(QStringLiteral("Markdown source"));
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixedFont.setPointSize(10);
    m_source->setFont(fixedFont);

    m_preview = new QTextBrowser(splitter);
    m_preview->setOpenExternalLinks(false);
    m_preview->setReadOnly(true);
    m_preview->document()->setDocumentMargin(12);

    splitter->addWidget(m_source);
    splitter->addWidget(m_preview);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    layout->addWidget(splitter, 1);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Spike: QuickJS + MathJax + Mermaid -> SVG -> QtSvg"));

    m_renderer = new RendererClient(this);
    configureRenderer();

    connect(m_renderButton, &QPushButton::clicked, this, &MainWindow::renderNow);
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::loadSample);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);

    auto *renderShortcut = new QAction(this);
    renderShortcut->setShortcut(QKeySequence(QStringLiteral("Ctrl+Return")));
    renderShortcut->setShortcutContext(Qt::WindowShortcut);
    connect(renderShortcut, &QAction::triggered, this, &MainWindow::renderNow);
    addAction(renderShortcut);

    loadSample();
}

void MainWindow::configureRenderer()
{
    const QString root = runtimeRoot();
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/rich-text-render-spike");
    m_renderer->configure(qjsPath(),
                          root + QStringLiteral("/renderer/dist/mathjax-qjs.mjs"),
                          root + QStringLiteral("/renderer/dist/mermaid-qjs.mjs"),
                          cacheDir,
                          QStringLiteral("mathjax-3.2.2;mermaid-13.0.0-alpha.3;qjs-0.18.0;sanitize-3"));
}

void MainWindow::loadFile(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        m_source->setPlainText(QString::fromUtf8(file.readAll()));
    } else {
        m_source->setPlainText(QStringLiteral("# File not found\n\n%1").arg(path));
    }
    renderNow();
}

void MainWindow::loadSample()
{
    loadFile(runtimeRoot() + QStringLiteral("/samples/sample.md"));
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
    const PreparedMarkdown prepared = MarkdownRenderer::prepare(source);
    const bool debug = qEnvironmentVariableIsSet("RICH_TEXT_RENDER_DEBUG");
    if (debug) {
        fprintf(stderr, "PREPARED MARKDOWN:\n%s\n", prepared.markdown.toUtf8().constData());
    }

    m_preview->setMarkdown(prepared.markdown);
    m_blockCount = prepared.blocks.size();
    m_pending = m_blockCount;
    m_cacheHits = 0;
    m_renderTimer.start();

    if (prepared.blocks.isEmpty()) {
        m_status->setText(QStringLiteral("Markdown rendered; no math/mermaid blocks found."));
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
            const bool ok = MarkdownRenderer::embedSvg(m_preview->document(),
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
            }
        };

        if (block.kind == RenderBlock::Kind::Math) {
            m_renderer->renderMath(block.source, block.display, callback);
        } else {
            m_renderer->renderMermaid(block.source, m_dark, callback);
        }
    }
}
