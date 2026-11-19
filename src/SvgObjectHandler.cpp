#include "SvgObjectHandler.h"

#include <QHash>
#include <QPainter>
#include <QtAlgorithms>
#include <QRectF>
#include <QSvgRenderer>
#include <QTextDocument>

SvgObjectHandler::SvgObjectHandler(QObject *parent)
    : QObject(parent)
{
}

SvgObjectHandler::~SvgObjectHandler()
{
    qDeleteAll(m_renderers);
}

QSizeF SvgObjectHandler::intrinsicSize(QTextDocument *document,
                                       int positionInDocument,
                                       const QTextFormat &format)
{
    Q_UNUSED(document);
    Q_UNUSED(positionInDocument);
    const double width = format.doubleProperty(WidthProperty);
    const double height = format.doubleProperty(HeightProperty);
    if (width <= 0 || height <= 0) {
        return QSizeF(320, 80);
    }
    return QSizeF(width, height);
}

void SvgObjectHandler::drawObject(QPainter *painter,
                                  const QRectF &rect,
                                  QTextDocument *document,
                                  int positionInDocument,
                                  const QTextFormat &format)
{
    Q_UNUSED(document);
    Q_UNUSED(positionInDocument);
    const QString svg = format.property(SvgProperty).toString();
    if (svg.isEmpty()) {
        return;
    }

    auto it = m_renderers.find(svg);
    QSvgRenderer *renderer = nullptr;
    if (it != m_renderers.end()) {
        renderer = it.value();
    } else {
        if (m_renderers.size() >= 64) {
            qDeleteAll(m_renderers);
            m_renderers.clear();
        }
        renderer = new QSvgRenderer(svg.toUtf8(), this);
        m_renderers.insert(svg, renderer);
    }

    if (renderer->isValid()) {
        renderer->render(painter, rect);
    } else {
        painter->save();
        painter->setPen(Qt::red);
        painter->drawRect(rect.adjusted(0, 0, -1, -1));
        painter->drawText(rect, Qt::AlignCenter, QStringLiteral("invalid SVG"));
        painter->restore();
    }
}
