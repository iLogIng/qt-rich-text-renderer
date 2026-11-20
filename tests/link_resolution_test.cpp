#include "MarkdownLinkHandler.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTextStream>
#include <QUrl>

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
    const QString sourceDir = QDir::currentPath() + QStringLiteral("/tests/sources/");
    const QUrl base = QUrl::fromLocalFile(sourceDir);

    const auto relative = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("bench.md")), base);
    expect(relative.kind == MarkdownLinkHandler::Kind::LocalFile, QStringLiteral("relative file kind"));
    expect(relative.error.isEmpty(), QStringLiteral("relative file resolves"));

    const auto anchor = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("#section")), base);
    expect(anchor.kind == MarkdownLinkHandler::Kind::Anchor, QStringLiteral("anchor kind"));

    const auto remote = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("https://example.com")), base);
    expect(remote.kind == MarkdownLinkHandler::Kind::Remote, QStringLiteral("remote kind"));

    const auto rejected = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("javascript:alert(1)")), base);
    expect(rejected.kind == MarkdownLinkHandler::Kind::Rejected, QStringLiteral("javascript rejected"));

    const auto missing = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("missing.md")), base);
    expect(missing.kind == MarkdownLinkHandler::Kind::LocalFile, QStringLiteral("missing file kind"));
    expect(!missing.error.isEmpty(), QStringLiteral("missing file reports error"));

    const auto noBase = MarkdownLinkHandler::resolve(QUrl(QStringLiteral("sample.md")), QUrl());
    expect(noBase.kind == MarkdownLinkHandler::Kind::Rejected, QStringLiteral("relative link without base rejected"));

    QTextBrowser view;
    MarkdownLinkHandler handler(&view, nullptr);
    handler.setBaseUrl(base);
    QString requestedPath;
    QString requestedFragment;
    QObject::connect(&handler,
                     &MarkdownLinkHandler::localFileRequested,
                     &view,
                     [&](const QString &path, const QString &fragment) {
                         requestedPath = path;
                         requestedFragment = fragment;
                     });
    handler.handleUrl(QUrl(QStringLiteral("bench.md#details")));
    expect(!requestedPath.isEmpty(), QStringLiteral("local link emits in-window request"));
    expect(requestedPath == QFileInfo(sourceDir + QStringLiteral("bench.md")).canonicalFilePath(),
           QStringLiteral("local link request uses canonical path"));
    expect(requestedFragment == QStringLiteral("details"),
           QStringLiteral("local link request keeps fragment"));

    QString markdown = QStringLiteral("# First section\n\n");
    for (int i = 0; i < 80; ++i) {
        markdown += QStringLiteral("line %1\n\n").arg(i);
    }
    markdown += QStringLiteral("## Target Section\n\nbody\n");
    view.resize(400, 220);
    view.setMarkdown(markdown);
    view.show();
    QCoreApplication::processEvents();
    view.verticalScrollBar()->setValue(0);
    MarkdownLinkHandler::scrollToAnchor(&view, QStringLiteral("target-section"));
    QCoreApplication::processEvents();
    expect(view.verticalScrollBar()->value() > 0,
           QStringLiteral("heading anchor scrolls the document"));

    if (failures == 0) {
        QTextStream(stdout) << "link resolution tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
