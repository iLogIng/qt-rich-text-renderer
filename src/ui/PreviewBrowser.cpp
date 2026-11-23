#include "PreviewBrowser.h"

#include "SvgObjectHandler.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>

PreviewBrowser::PreviewBrowser(QWidget *parent)
    : QTextBrowser(parent)
{
}

QMargins PreviewBrowser::contentMargins() const
{
    return viewportMargins();
}

void PreviewBrowser::setContentMargins(const QMargins &margins)
{
    setViewportMargins(margins);
}

void PreviewBrowser::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && tryPreviewMedia(event->position().toPoint())) {
        event->accept();
        return;
    }
    QTextBrowser::mousePressEvent(event);
}

void PreviewBrowser::resizeEvent(QResizeEvent *event)
{
    QTextBrowser::resizeEvent(event);
    emit viewportResized();
}

bool PreviewBrowser::tryPreviewMedia(const QPoint &position)
{
    const QTextCursor cursor = cursorForPosition(position);
    if (cursor.isNull()) {
        return false;
    }

    const auto inspectFormat = [this](const QTextCharFormat &format) {
        if (format.isImageFormat()) {
            const QTextImageFormat image = format.toImageFormat();
            const QString name = image.name();
            if (name.isEmpty()) {
                return false;
            }

            QUrl url(name);
            if (url.scheme().isEmpty()) {
                const QFileInfo info(name);
                if (info.isAbsolute()) {
                    url = QUrl::fromLocalFile(name);
                } else if (!document()->baseUrl().isEmpty()) {
                    url = document()->baseUrl().resolved(QUrl(name));
                } else {
                    url = QUrl::fromLocalFile(info.absoluteFilePath());
                }
            }
            emit imagePreviewRequested(url);
            return true;
        }

        if (format.objectType() == SvgObjectHandler::Type) {
            const QString svg =
                format.property(SvgObjectHandler::SvgProperty).toString();
            if (!svg.isEmpty()) {
                emit svgPreviewRequested(svg);
                return true;
            }
        }
        return false;
    };

    if (inspectFormat(cursor.charFormat())) {
        return true;
    }

    if (cursor.position() > 0) {
        QTextCursor previous = cursor;
        previous.setPosition(cursor.position() - 1);
        if (inspectFormat(previous.charFormat())) {
            return true;
        }
    }
    return false;
}
