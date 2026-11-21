#include "MarkdownLinkHandler.h"

#include "LinkResolver.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QHash>
#include <QMessageBox>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

namespace {

QString decodedFragment(const QString &fragment)
{
    QString text = QUrl::fromPercentEncoding(fragment.toUtf8()).trimmed();
    if (text.startsWith(QLatin1Char('#'))) {
        text.remove(0, 1);
    }
    return text;
}

QString githubHeadingSlug(const QString &heading)
{
    QString slug;
    slug.reserve(heading.size());
    for (const QChar character : heading.toLower()) {
        if (character == QLatin1Char(' ')) {
            slug.append(QLatin1Char('-'));
            continue;
        }
        if (character == QLatin1Char('-') || character == QLatin1Char('_')
            || character.isLetterOrNumber()
            || character.category() == QChar::Mark_NonSpacing
            || character.category() == QChar::Mark_SpacingCombining
            || character.category() == QChar::Mark_Enclosing) {
            slug.append(character);
        }
    }
    return slug;
}

class HeadingSlugger {
public:
    QString next(const QString &heading)
    {
        const QString base = githubHeadingSlug(heading);
        QString slug = base;
        while (m_occurrences.contains(slug)) {
            const int count = m_occurrences.value(base) + 1;
            m_occurrences.insert(base, count);
            slug = base + QLatin1Char('-') + QString::number(count);
        }
        m_occurrences.insert(slug, 0);
        return slug;
    }

private:
    QHash<QString, int> m_occurrences;
};

} // namespace

MarkdownLinkHandler::MarkdownLinkHandler(QTextBrowser *view, QWidget *dialogParent, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_dialogParent(dialogParent)
{
    if (m_view) {
        m_view->setOpenLinks(false);
        m_view->setOpenExternalLinks(false);
        connect(m_view, &QTextBrowser::anchorClicked, this, &MarkdownLinkHandler::handleUrl);
    }
}

void MarkdownLinkHandler::setBaseUrl(const QUrl &baseUrl)
{
    m_baseUrl = baseUrl;
    if (m_view) {
        m_view->document()->setBaseUrl(baseUrl);
    }
}

void MarkdownLinkHandler::scrollToAnchor(QTextBrowser *view, const QString &fragment)
{
    if (!view || fragment.isEmpty()) {
        return;
    }

    const QString wanted = githubHeadingSlug(decodedFragment(fragment));
    if (wanted.isEmpty()) {
        return;
    }

    // QTextDocument's Markdown importer does not create anchor names for
    // headings, so resolve GitHub-style heading slugs ourselves.
    HeadingSlugger slugger;
    for (QTextBlock block = view->document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() <= 0) {
            continue;
        }

        if (slugger.next(block.text()) == wanted) {
            QTextCursor cursor(block);
            view->setTextCursor(cursor);
            view->ensureCursorVisible();
            return;
        }
    }

    // Keep support for documents that contain explicit Qt anchors.
    view->scrollToAnchor(fragment);
}

void MarkdownLinkHandler::handleUrl(const QUrl &url)
{
    const LinkResolver::Resolved resolved = LinkResolver::resolve(url, m_baseUrl);

    switch (resolved.kind) {
    case LinkResolver::Kind::Anchor:
        scrollToAnchor(m_view, resolved.url.fragment());
        return;

    case LinkResolver::Kind::Remote: {
        const auto answer = QMessageBox::question(
            m_dialogParent,
            tr("Open external link?"),
            tr("This link points outside locvas:\n\n%1")
                .arg(resolved.url.toDisplayString()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QDesktopServices::openUrl(resolved.url);
        }
        return;
    }

    case LinkResolver::Kind::LocalFile: {
        if (!resolved.error.isEmpty()) {
            QMessageBox::warning(m_dialogParent,
                                 tr("Local link not found"),
                                 resolved.error);
            return;
        }

        const QFileInfo info(resolved.path);
        if (info.isDir()) {
            QMessageBox::warning(m_dialogParent,
                                 tr("Directory link"),
                                 tr("Directories cannot be displayed in the viewer:\n\n%1")
                                     .arg(resolved.path));
            return;
        }

        // Local text/Markdown navigation stays inside the application.
        // The window validates text vs binary and updates its history.
        emit localFileRequested(resolved.path, resolved.url.fragment());
        return;
    }

    case LinkResolver::Kind::Mail: {
        const auto answer = QMessageBox::question(
            m_dialogParent,
            tr("Compose mail?"),
            tr("This link opens an email client:\n\n%1")
                .arg(resolved.url.toDisplayString()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            QDesktopServices::openUrl(resolved.url);
        }
        return;
    }

    case LinkResolver::Kind::Empty:
        return;

    case LinkResolver::Kind::Rejected:
        QMessageBox::warning(m_dialogParent,
                             tr("Unsupported link"),
                             resolved.error.isEmpty()
                                 ? tr("This link is not supported.")
                                 : resolved.error);
        return;
    }
}
