#include "RendererService.h"

#include "RendererWorker.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>

#include <utility>

RendererService::RendererService(QObject *parent)
    : QObject(parent)
    , m_mathWorker(new RendererWorker(QStringLiteral("mathjax"), this))
    , m_mermaidWorker(new RendererWorker(QStringLiteral("mermaid"), this))
{
    connect(m_mathWorker, &RendererWorker::logLine, this, [](const QString &line) {
        qWarning().noquote() << "[mathjax worker]" << line;
    });
    connect(m_mermaidWorker, &RendererWorker::logLine, this, [](const QString &line) {
        qWarning().noquote() << "[mermaid worker]" << line;
    });
}

void RendererService::configure(const QString &qjsPath,
                                const QString &mathWorkerScript,
                                const QString &mermaidWorkerScript,
                                const QString &cacheDir,
                                const QString &version,
                                int idleMs)
{
    m_cacheDir = cacheDir;
    m_version = version;
    QDir().mkpath(m_cacheDir);

    m_mathWorker->configure(qjsPath, mathWorkerScript, 10000, idleMs);
    m_mermaidWorker->configure(qjsPath, mermaidWorkerScript, 15000, idleMs);
}

void RendererService::renderMath(const QString &tex, bool display, Callback callback)
{
    render(QStringLiteral("math"), tex, display, QString(), std::move(callback));
}

void RendererService::renderMermaid(const QString &source, bool dark, Callback callback)
{
    render(QStringLiteral("mermaid"), source, true,
           dark ? QStringLiteral("dark") : QStringLiteral("default"),
           std::move(callback));
}

void RendererService::cancelAll()
{
    ++m_generation;
    m_inflight.clear();
    m_mathWorker->cancelGeneration(m_generation);
    m_mermaidWorker->cancelGeneration(m_generation);
}

void RendererService::shutdown()
{
    m_mathWorker->shutdown();
    m_mermaidWorker->shutdown();
}

int RendererService::pendingCount() const
{
    return m_mathWorker->pendingCount() + m_mermaidWorker->pendingCount();
}

QString RendererService::cacheKey(const QString &kind,
                                  const QString &source,
                                  bool display,
                                  const QString &theme) const
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("rich-text-render-service-v2");
    hash.addData(kind.toUtf8());
    hash.addData(m_version.toUtf8());
    hash.addData(source.toUtf8());
    if (kind == QStringLiteral("math")) {
        hash.addData(display ? "display" : "inline");
    } else {
        hash.addData(theme.toUtf8());
    }
    return QString::fromLatin1(hash.result().toHex());
}

void RendererService::render(const QString &kind,
                             const QString &source,
                             bool display,
                             const QString &theme,
                             Callback callback)
{
    const quint64 generation = m_generation;
    const QString key = cacheKey(kind, source, display, theme);
    const QString cachePath = QDir(m_cacheDir).filePath(key + QStringLiteral(".svg"));

    if (QFileInfo::exists(cachePath)) {
        QFile cache(cachePath);
        if (cache.open(QIODevice::ReadOnly)) {
            RenderResult result;
            result.svg = QString::fromUtf8(cache.readAll());
            result.fromCache = true;
            if (!result.svg.isEmpty()) {
                QTimer::singleShot(0, this, [this, generation, callback = std::move(callback), result]() {
                    if (generation == m_generation) {
                        callback(result);
                    }
                });
                return;
            }
        }
    }

    const auto existing = m_inflight.constFind(key);
    if (existing != m_inflight.constEnd() && existing->generation == generation) {
        m_inflight[key].callbacks.append(std::move(callback));
        return;
    }

    Inflight inflight;
    inflight.generation = generation;
    inflight.callbacks.append(std::move(callback));
    m_inflight.insert(key, inflight);

    RendererJob job;
    job.id = key;
    job.source = source;
    job.display = display;
    job.theme = theme;
    job.generation = generation;
    job.callback = [this, key, generation](const RenderResult &result) {
        handleWorkerResult(key, generation, result);
    };

    RendererWorker *worker = kind == QStringLiteral("math") ? m_mathWorker : m_mermaidWorker;
    worker->submit(std::move(job));
}

void RendererService::handleWorkerResult(const QString &key,
                                         quint64 generation,
                                         const RenderResult &result)
{
    const auto it = m_inflight.find(key);
    if (it == m_inflight.end() || it->generation != generation) {
        return;
    }

    const QVector<Callback> callbacks = it->callbacks;
    m_inflight.erase(it);

    if (generation != m_generation) {
        return;
    }

    if (result.error.isEmpty() && !result.svg.isEmpty()) {
        QFile cache(QDir(m_cacheDir).filePath(key + QStringLiteral(".svg")));
        if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            cache.write(result.svg.toUtf8());
        }
    }

    for (const Callback &callback : callbacks) {
        callback(result);
    }
}
