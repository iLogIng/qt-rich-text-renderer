#include "MainWindow.h"

#include "CodeHighlighter.h"
#include "LocalFileLoader.h"
#include "MarkdownDocumentBuilder.h"
#include "MarkdownLinkHandler.h"
#include "MarkdownOutline.h"
#include "MarkdownPreprocessor.h"
#include "MarkdownSourceHighlighter.h"
#include "MediaPreviewWindow.h"
#include "OutlinePanel.h"
#include "PreviewBrowser.h"
#include "RendererService.h"
#include "SettingsDialog.h"
#include "SvgObjectHandler.h"
#include "Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QDebug>
#include <cstdio>

namespace {

QString runtimeRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appImageRoot = QDir(appDir).absoluteFilePath(QStringLiteral("../share/rich-text-renderer"));
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

void logLoadedFile(const LocalFileContent &loaded)
{
    if (!qEnvironmentVariableIsSet("RICH_TEXT_RENDER_DEBUG_LOAD")) {
        return;
    }

    const QFileInfo info(loaded.path);
    fprintf(stderr,
            "[load] path=%s size=%lld mtime=%s\n",
            loaded.path.toUtf8().constData(),
            static_cast<long long>(info.size()),
            info.lastModified().toString(Qt::ISODateWithMs).toUtf8().constData());
}

} // namespace

MainWindow::MainWindow(bool dark, const QString &settingsPath, QWidget *parent)
    : QMainWindow(parent)
    , m_settings(std::make_unique<AppSettings>(
          settingsPath.isEmpty() ? AppSettings::defaultUserPath() : settingsPath,
          runtimeRoot() + QStringLiteral("/config/settings.ini")))
{
    static const QFont baseApplicationFont = QApplication::font();
    m_baseApplicationFont = baseApplicationFont;
    m_settingsData = m_settings->load();
    QString themeError;
    m_theme = ThemeManager::loadTheme(dark ? QStringLiteral("dark")
                                           : m_settingsData.themeId,
                                      &themeError);
    if (!themeError.isEmpty()) {
        qWarning("Theme load failed: %s", qPrintable(themeError));
    }

    ThemeManager::apply(m_theme);
    applyApplicationFont(m_settingsData.uiFontPointSize);

    setWindowTitle(QStringLiteral("rich-text-renderer"));
    resize(1280, 860);
    if (!m_settingsData.windowGeometry.isEmpty()) {
        restoreGeometry(m_settingsData.windowGeometry);
    }

    buildCentralShell();
    createCentralUiWidgets();
    populateCentralShell();
    setCentralWidget(m_centralSlots.root);
    if (!m_theme.previewFontFamily.isEmpty()) {
        applyPreviewFont(m_settingsData.uiFontPointSize);
    }
    applySourceFont(m_settingsData.sourceFontPointSize);

    m_outlinePanel = new OutlinePanel(this);
    m_outlineDock = new QDockWidget(tr("Outline"), this);
    m_outlineDock->setObjectName(QStringLiteral("outlineDock"));
    m_outlineDock->setWidget(m_outlinePanel);
    m_outlineDock->setAllowedAreas(Qt::LeftDockWidgetArea);
    m_outlineDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, m_outlineDock);
    resizeDocks({m_outlineDock}, {240}, Qt::Horizontal);
    connect(m_outlinePanel, &OutlinePanel::blockRequested,
            this, &MainWindow::scrollPreviewToBlock);
    connect(m_outlineToggleButton, &QToolButton::toggled, this, [this](bool visible) {
        m_outlineDock->setVisible(visible);
        m_settingsData.outlineVisible = visible;
        m_settings->save(m_settingsData);
    });
    m_outlineDock->setVisible(m_settingsData.outlineVisible);

    statusBar()->showMessage(QStringLiteral("rich-text-renderer: QuickJS + MathJax + Mermaid -> SVG -> QtSvg"));

    m_renderer = new RendererService(this);
    configureRenderer();

    connect(m_renderButton, &QPushButton::clicked, this, &MainWindow::renderNow);
    connect(m_loadButton, &QPushButton::clicked, this, &MainWindow::chooseFile);
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::toggleTheme);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);
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

void MainWindow::buildCentralShell()
{
    QString layoutError;
    CentralUiLayout shell;

    if (!m_theme.layoutError.isEmpty()) {
        qWarning("Theme layout rejected: %s", qPrintable(m_theme.layoutError));
    }

    if (!m_theme.layoutFile.isEmpty()) {
        shell = CentralUiBuilder::loadTheme(m_theme, this, &layoutError);
        if (!shell.isValid()) {
            qWarning("Theme layout load failed: %s", qPrintable(layoutError));
        }
    }

    if (!shell.isValid()) {
        shell = CentralUiBuilder::createDefault(this, m_theme.spacing);
        m_activeLayoutFile.clear();
    } else {
        m_activeLayoutFile = m_theme.layoutFile;
    }
    m_centralSlots = shell;
}

void MainWindow::createCentralUiWidgets()
{
    QWidget *shell = m_centralSlots.root ? m_centralSlots.root : this;

    m_loadButton = new QPushButton(QStringLiteral("Load"), shell);
    m_loadButton->setObjectName(QStringLiteral("loadButton"));
    m_loadButton->setToolTip(QStringLiteral("Open a local Markdown or text file"));
    m_renderButton = new QPushButton(QStringLiteral("Render (Ctrl+Enter)"), shell);
    m_renderButton->setObjectName(QStringLiteral("renderButton"));
    m_themeButton = new QPushButton(shell);
    m_themeButton->setObjectName(QStringLiteral("themeButton"));
    updateThemeButton();
    m_settingsButton = new QPushButton(QStringLiteral("Settings"), shell);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    m_status = new QLabel(QStringLiteral("No document open"), shell);
    m_status->setObjectName(QStringLiteral("statusLabel"));
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_backButton = new QToolButton(shell);
    m_backButton->setObjectName(QStringLiteral("historyBackButton"));
    m_backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_backButton->setToolTip(QStringLiteral("Back"));
    m_backButton->setAutoRaise(true);
    m_forwardButton = new QToolButton(shell);
    m_forwardButton->setObjectName(QStringLiteral("historyForwardButton"));
    m_forwardButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_forwardButton->setToolTip(QStringLiteral("Forward"));
    m_forwardButton->setAutoRaise(true);
    m_historyCombo = new QComboBox(shell);
    m_historyCombo->setObjectName(QStringLiteral("historyCombo"));
    m_historyCombo->setPlaceholderText(QStringLiteral("History"));
    m_historyCombo->setMinimumContentsLength(18);
    m_historyCombo->setMaximumWidth(280);
    m_historyCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_historyCombo->setMaxVisibleItems(16);
    m_historyCombo->setToolTip(QStringLiteral("Local file history (newest first, max 64)"));

    m_tabs = new QTabWidget(shell);
    m_tabs->setObjectName(QStringLiteral("documentTabs"));

    m_previewPage =
        CentralUiBuilder::createSlot(m_tabs, QString::fromLatin1(CentralUiSlotNames::Preview));
    m_previewPage->setProperty(
        ThemeSlotStyle::Property, QString::fromLatin1(ThemeSlotStyle::Preview));
    m_preview = new PreviewBrowser(m_previewPage);
    m_preview->setObjectName(QStringLiteral("previewView"));
    m_preview->setOpenExternalLinks(false);
    m_preview->setReadOnly(true);
    m_preview->document()->setDocumentMargin(m_theme.documentMargin);
    m_svgHandler = new SvgObjectHandler(this);
    m_preview->document()->documentLayout()->registerHandler(SvgObjectHandler::Type, m_svgHandler);
    m_codeHighlighter = new CodeHighlighter(m_preview->document(), m_theme.dark, this);
    m_codeHighlighter->setSyntaxPalette(m_theme.codeSyntax);
    m_linkHandler = new MarkdownLinkHandler(m_preview, this, this);
    connect(m_preview, &PreviewBrowser::imagePreviewRequested,
            this, &MainWindow::previewImageUrl);
    connect(m_preview, &PreviewBrowser::svgPreviewRequested,
            this, &MainWindow::previewSvg);
    CentralUiBuilder::addToSlot(m_previewPage, m_preview);

    m_sourcePage =
        CentralUiBuilder::createSlot(m_tabs, QString::fromLatin1(CentralUiSlotNames::Source));
    m_sourcePage->setProperty(
        ThemeSlotStyle::Property, QString::fromLatin1(ThemeSlotStyle::Source));
    m_source = new QPlainTextEdit(m_sourcePage);
    m_source->setObjectName(QStringLiteral("sourceView"));
    m_source->setReadOnly(true);
    m_source->setPlaceholderText(QStringLiteral("Markdown source"));
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    fixedFont.setPointSize(m_settingsData.sourceFontPointSize);
    m_source->setFont(fixedFont);
    m_sourceHighlighter = new MarkdownSourceHighlighter(m_source->document(), m_theme.dark, this);
    m_sourceHighlighter->setSyntaxPalette(m_theme.sourceSyntax);
    m_sourceHighlighter->setEnabled(m_settingsData.sourceHighlighting);
    CentralUiBuilder::addToSlot(m_sourcePage, m_source);

    m_tabs->addTab(m_previewPage, tr("Preview"));
    m_tabs->addTab(m_sourcePage, tr("Source"));
    m_tabs->setCurrentWidget(m_previewPage);
    m_tabs->setTabVisible(m_tabs->indexOf(m_sourcePage), m_settingsData.sourceVisible);

    m_outlineToggleButton = new QToolButton(m_tabs);
    m_outlineToggleButton->setObjectName(QStringLiteral("outlineToggleButton"));
    m_outlineToggleButton->setText(tr("Outline"));
    m_outlineToggleButton->setToolTip(tr("Show or hide the document outline"));
    m_outlineToggleButton->setCheckable(true);
    m_outlineToggleButton->setAutoRaise(true);
    m_outlineToggleButton->setChecked(m_settingsData.outlineVisible);
    m_tabs->setCornerWidget(m_outlineToggleButton, Qt::TopLeftCorner);

    updateToolbarHeights();
}

void MainWindow::populateCentralShell()
{
    if (!m_centralSlots.isValid() || !m_tabs) {
        return;
    }

    auto *leading = qobject_cast<QHBoxLayout *>(m_centralSlots.toolbarLeading->layout());
    auto *trailing = qobject_cast<QHBoxLayout *>(m_centralSlots.toolbarTrailing->layout());
    if (!leading || !trailing) {
        qWarning("Central layout toolbar slots are not horizontal layouts");
        return;
    }

    leading->addWidget(m_loadButton);
    leading->addWidget(m_renderButton);
    leading->addWidget(m_themeButton);
    leading->addWidget(m_settingsButton);
    leading->addSpacing(12);

    trailing->addSpacing(6);
    trailing->addWidget(m_status, 1);
    trailing->addWidget(m_backButton);
    trailing->addWidget(m_forwardButton);
    trailing->addWidget(m_historyCombo);

    CentralUiBuilder::setSlotEmpty(m_centralSlots.toolbarLeading, false);
    CentralUiBuilder::setSlotEmpty(m_centralSlots.toolbarTrailing, false);
    m_centralSlots.toolbarLeading->setSizePolicy(QSizePolicy::Maximum,
                                                 QSizePolicy::Fixed);
    m_centralSlots.toolbarTrailing->setSizePolicy(QSizePolicy::Expanding,
                                                  QSizePolicy::Fixed);
    CentralUiBuilder::addToSlot(m_centralSlots.content, m_tabs);
    m_centralSlots.content->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Expanding);
}

void MainWindow::rebuildCentralLayout()
{
    buildCentralShell();
    populateCentralShell();
    setCentralWidget(m_centralSlots.root);
    updateToolbarHeights();
}

MainWindow::~MainWindow()
{
    if (m_renderer) {
        m_renderer->shutdown();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_settings) {
        syncViewSettingsFromUi();
        m_settingsData.windowGeometry = saveGeometry();
        m_settings->save(m_settingsData);
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::configureRenderer()
{
    const QString root = runtimeRoot();
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    bool idleOk = false;
    const int idleEnv = qEnvironmentVariableIntValue("RICH_TEXT_RENDER_IDLE_MS", &idleOk);
    const int idleMs = idleOk && idleEnv > 0 ? idleEnv : m_settingsData.rendererIdleTimeoutMs;
    m_renderer->configure(qjsPath(),
                          root + QStringLiteral("/renderer/dist/mathjax-worker.mjs"),
                          root + QStringLiteral("/renderer/dist/mermaid-full-worker.mjs"),
                          cacheDir,
                          QStringLiteral("mathjax-3.2.2;mermaid-13.0.0-alpha.3;qjs-0.18.0;service-v3-full"),
                          idleMs,
                          m_settingsData.mathRequestTimeoutMs,
                          m_settingsData.mermaidRequestTimeoutMs);
}

void MainWindow::loadFile(const QString &path, bool reportErrors)
{
    const LoadErrorMode errorMode = reportErrors ? LoadErrorMode::Dialog
                                                 : LoadErrorMode::StatusOnly;
    if (!openLocalFile(path, QString(), HistoryMode::Record, errorMode)) {
        return;
    }
    m_settingsData.lastDirectory = QFileInfo(path).absolutePath();
}

void MainWindow::chooseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open file"),
        m_settingsData.lastDirectory,
        tr("Markdown files (*.md *.markdown);;Text files (*.txt);;All files (*)"),
        nullptr,
        QFileDialog::DontUseNativeDialog);
    if (path.isEmpty()) {
        return;
    }

    loadFile(path);
}

void MainWindow::showSettings()
{
    syncViewSettingsFromUi();

    AppSettingsData dialogData = m_settingsData;
    dialogData.themeId = m_theme.id;

    SettingsDialog dialog(dialogData, m_settings->defaults(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const AppSettingsData previous = m_settingsData;
    m_settingsData = dialog.values();
    applySettings(previous);
    m_settings->save(m_settingsData);
}

void MainWindow::applySettings(const AppSettingsData &previous)
{
    const bool themeChanged = previous.themeId != m_settingsData.themeId;
    const bool uiFontChanged =
        previous.uiFontPointSize != m_settingsData.uiFontPointSize;
    const bool sourceFontChanged =
        previous.sourceFontPointSize != m_settingsData.sourceFontPointSize;
    const bool sourceHighlightingChanged =
        previous.sourceHighlighting != m_settingsData.sourceHighlighting;
    const bool widthChanged = previous.maxSvgWidth != m_settingsData.maxSvgWidth;
    const bool rendererChanged =
        previous.rendererIdleTimeoutMs != m_settingsData.rendererIdleTimeoutMs
        || previous.mathRequestTimeoutMs != m_settingsData.mathRequestTimeoutMs
        || previous.mermaidRequestTimeoutMs != m_settingsData.mermaidRequestTimeoutMs;

    if (uiFontChanged) {
        applyApplicationFont(m_settingsData.uiFontPointSize);
    }
    if (sourceFontChanged) {
        applySourceFont(m_settingsData.sourceFontPointSize);
    }
    if (sourceHighlightingChanged) {
        m_sourceHighlighter->setEnabled(m_settingsData.sourceHighlighting);
    }

    {
        const QSignalBlocker blocker(m_outlineToggleButton);
        m_outlineToggleButton->setChecked(m_settingsData.outlineVisible);
    }
    m_outlineDock->setVisible(m_settingsData.outlineVisible);
    m_tabs->setTabVisible(m_tabs->indexOf(m_sourcePage), m_settingsData.sourceVisible);
    if (!m_settingsData.sourceVisible && m_tabs->currentWidget() == m_sourcePage) {
        m_tabs->setCurrentWidget(m_previewPage);
    }

    if (rendererChanged) {
        configureRenderer();
    }

    if (themeChanged) {
        setThemeId(m_settingsData.themeId);
    } else if ((uiFontChanged || widthChanged) && !m_source->toPlainText().isEmpty()) {
        renderNow();
    }
}

void MainWindow::applyApplicationFont(int pointSize)
{
    QFont font = m_baseApplicationFont;
    if (!m_theme.uiFontFamily.isEmpty()) {
        font.setFamily(m_theme.uiFontFamily);
    }
    font.setPointSize(pointSize);
    QApplication::setFont(font);
    applyPreviewFont(pointSize);
    updateToolbarHeights();
}

void MainWindow::applyPreviewFont(int pointSize)
{
    if (!m_preview) {
        return;
    }
    QFont font = m_baseApplicationFont;
    if (!m_theme.uiFontFamily.isEmpty()) {
        font.setFamily(m_theme.uiFontFamily);
    }
    if (!m_theme.previewFontFamily.isEmpty()) {
        font.setFamily(m_theme.previewFontFamily);
    }
    font.setPointSize(pointSize);
    m_preview->setFont(font);
    m_preview->document()->setDefaultFont(font);
}

void MainWindow::applySourceFont(int pointSize)
{
    if (!m_source) {
        return;
    }
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (!m_theme.codeFontFamily.isEmpty()) {
        font.setFamily(m_theme.codeFontFamily);
    }
    font.setPointSize(pointSize);
    m_source->setFont(font);
}

void MainWindow::updateToolbarHeights()
{
    if (!m_renderButton) {
        return;
    }
    const int height = m_renderButton->sizeHint().height();
    if (m_backButton) {
        m_backButton->setFixedHeight(height);
    }
    if (m_forwardButton) {
        m_forwardButton->setFixedHeight(height);
    }
    if (m_historyCombo) {
        m_historyCombo->setFixedHeight(height);
    }
}

void MainWindow::syncViewSettingsFromUi()
{
    if (m_outlineDock) {
        m_settingsData.outlineVisible = m_outlineDock->isVisible();
    }
    if (m_tabs && m_source) {
        m_settingsData.sourceVisible =
            m_tabs->isTabVisible(m_tabs->indexOf(m_sourcePage));
    }
}

bool MainWindow::openLocalFile(const QString &path,
                               const QString &fragment,
                               HistoryMode mode,
                               LoadErrorMode errorMode)
{
    if (MediaPreviewWindow::isPreviewableImageFile(path)) {
        m_status->setText(tr("Previewing image: %1").arg(QFileInfo(path).fileName()));
        MediaPreviewWindow::showImageFile(path, this);
        return true;
    }

    const LocalFileContent loaded = LocalFileLoader::load(path);
    if (!loaded.ok) {
        showLoadError(path, loaded.error, errorMode);
        return false;
    }

    logLoadedFile(loaded);
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
    m_loadAnchorRequested = !fragment.isEmpty();
    ++m_loadGeneration;

    if (mode == HistoryMode::Record) {
        m_history.visit(loaded.path, displayName);
    }

    renderNow();
    m_tabs->setCurrentWidget(m_previewPage);
    schedulePreviewRefresh();
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

void MainWindow::schedulePreviewRefresh()
{
    const int loadGeneration = m_loadGeneration;
    QTimer::singleShot(0, this, [this, loadGeneration] {
        if (loadGeneration != m_loadGeneration) {
            return;
        }
        finishPreviewLoad();
    });
}

void MainWindow::finishPreviewLoad()
{
    forcePreviewLayout();
    // A cached block callback may already have positioned a pending anchor.
    if (!m_loadAnchorRequested || !m_pendingAnchor.isEmpty()) {
        resetPreviewToStart();
        if (!m_pendingAnchor.isEmpty()) {
            if (m_blockCount == 0) {
                applyPendingAnchor();
            } else {
                // Keep the anchor pending so async embedding can reapply it.
                MarkdownLinkHandler::scrollToAnchor(m_preview, m_pendingAnchor);
            }
        }
    }
    m_loadAnchorRequested = false;
    m_preview->viewport()->update();
}

void MainWindow::forcePreviewLayout()
{
    // Reassigning the text width invalidates any partial lazy layout and
    // forces the complete document to be laid out before sizes are queried.
    const int width = m_preview->viewport()->width();
    if (width > 0) {
        m_preview->document()->setTextWidth(width);
    }
    m_preview->document()->documentLayout()->documentSize();
}

void MainWindow::resetPreviewToStart()
{
    m_preview->setTextCursor(QTextCursor(m_preview->document()));
    m_preview->verticalScrollBar()->setValue(0);
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

void MainWindow::scrollPreviewToBlock(int blockNumber)
{
    const QTextBlock block = m_preview->document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }

    m_tabs->setCurrentWidget(m_previewPage);
    QTextCursor cursor(block);
    m_preview->setTextCursor(cursor);

    const QRectF blockRect =
        m_preview->document()->documentLayout()->blockBoundingRect(block);
    const int target = qRound(blockRect.top()) - 8;
    m_preview->verticalScrollBar()->setValue(qMax(0, target));
    m_preview->viewport()->update();
}

void MainWindow::previewImageUrl(const QUrl &url)
{
    if (!url.isLocalFile()) {
        QMessageBox::information(this,
                                 tr("Image preview"),
                                 tr("Only local images can be previewed:\n\n%1")
                                     .arg(url.toDisplayString()));
        return;
    }
    MediaPreviewWindow::showImageFile(url.toLocalFile(), this);
}

void MainWindow::previewSvg(const QString &svg)
{
    MediaPreviewWindow::showSvg(svg, tr("SVG preview"), this);
}

void MainWindow::setThemeId(const QString &id)
{
    QString error;
    const ThemeSpec theme = ThemeManager::loadTheme(id, &error);
    if (!error.isEmpty()) {
        qWarning("Theme load failed: %s", qPrintable(error));
    }

    const QString previousLayout = m_activeLayoutFile;
    const int previousSpacing = m_theme.spacing;
    const QString previousUiFamily = m_theme.uiFontFamily;
    const QString previousPreviewFamily = m_theme.previewFontFamily;
    const QString previousCodeFamily = m_theme.codeFontFamily;
    m_theme = theme;
    ThemeManager::apply(m_theme);
    if (previousLayout != m_theme.layoutFile
        || (m_theme.layoutFile.isEmpty() && previousSpacing != m_theme.spacing)) {
        rebuildCentralLayout();
    }
    if (previousUiFamily != m_theme.uiFontFamily
        || previousPreviewFamily != m_theme.previewFontFamily) {
        applyApplicationFont(m_settingsData.uiFontPointSize);
    }
    if (previousCodeFamily != m_theme.codeFontFamily) {
        applySourceFont(m_settingsData.sourceFontPointSize);
    }
    if (m_preview) {
        m_preview->document()->setDocumentMargin(m_theme.documentMargin);
    }
    updateThemeButton();

    if (m_codeHighlighter) {
        m_codeHighlighter->setSyntaxPalette(m_theme.codeSyntax);
    }
    if (m_sourceHighlighter) {
        m_sourceHighlighter->setSyntaxPalette(m_theme.sourceSyntax);
    }
    if (m_source && !m_source->toPlainText().isEmpty()) {
        renderNow();
    }
}

void MainWindow::updateThemeButton()
{
    if (m_themeButton) {
        m_themeButton->setText(m_theme.dark ? QStringLiteral("Light mode")
                                           : QStringLiteral("Dark mode"));
    }
}

void MainWindow::toggleTheme()
{
    setDarkMode(!m_theme.dark);
    m_settingsData.themeId = m_theme.id;
    syncViewSettingsFromUi();
    m_settings->save(m_settingsData);
}

bool MainWindow::isDarkMode() const
{
    return m_theme.dark;
}

void MainWindow::setDarkMode(bool dark)
{
    const QString id = dark ? QStringLiteral("dark") : QStringLiteral("light");
    if (m_theme.id == id) {
        updateThemeButton();
        return;
    }
    setThemeId(id);
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

    QHash<QString, QString> tokenLabels;
    for (const RenderBlock &block : prepared.blocks) {
        tokenLabels.insert(block.token,
                           block.kind == RenderBlock::Kind::Math
                               ? block.source
                               : QStringLiteral("Mermaid"));
    }
    m_outlinePanel->setEntries(
        MarkdownOutline::fromDocument(*m_preview->document(), tokenLabels));

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
            const bool ok = MarkdownDocumentBuilder::embedSvg(
                m_preview->document(),
                block.token,
                result.svg,
                result.error,
                block.kind == RenderBlock::Kind::Mermaid,
                m_theme.dark,
                m_preview->palette().color(QPalette::Text),
                m_settingsData.maxSvgWidth,
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
                forcePreviewLayout();
                if (!m_pendingAnchor.isEmpty()) {
                    resetPreviewToStart();
                    applyPendingAnchor();
                }
                m_preview->viewport()->update();
            }
        };

        if (block.kind == RenderBlock::Kind::Math) {
            m_renderer->renderMath(block.source, block.display, callback);
        } else {
            m_renderer->renderMermaid(block.source, m_theme.dark, callback);
        }
    }
}
