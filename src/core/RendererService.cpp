#include "RendererService.h"

#include "RendererWorker.h"

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
                                int idleMs,
                                int mathRequestTimeoutMs,
                                int mermaidRequestTimeoutMs)
{
    m_cache.configure(cacheDir, version);

    m_mathWorker->configure(qjsPath,
                            mathWorkerScript,
                            mathRequestTimeoutMs,
                            idleMs);
    m_mermaidWorker->configure(qjsPath,
                               mermaidWorkerScript,
                               mermaidRequestTimeoutMs,
                               idleMs);
}

void RendererService::renderMath(const QString &tex, bool display, Callback callback)
{
    render(QStringLiteral("math"),
           tex,
           display,
           QString(),
           QString(),
           {},
           std::move(callback));
}

void RendererService::renderMermaid(const QString &source,
                                    const MermaidTheme &theme,
                                    Callback callback)
{
    render(QStringLiteral("mermaid"),
           source,
           true,
           theme.cacheKey(),
           theme.name,
           theme.variables,
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

void RendererService::render(const QString &kind,
                             const QString &source,
                             bool display,
                             const QString &cacheTheme,
                             const QString &workerTheme,
                             const QHash<QString, QString> &themeVariables,
                             Callback callback)
{
    const quint64 generation = m_generation;
    const QString key = m_cache.key(kind, source, display, cacheTheme);

    QString cachedSvg;
    if (m_cache.read(key, &cachedSvg)) {
        RenderResult result;
        result.svg = cachedSvg;
        result.fromCache = true;
        QTimer::singleShot(0, this, [this, generation, callback = std::move(callback), result]() {
            if (generation == m_generation) {
                callback(result);
            }
        });
        return;
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
    job.theme = workerTheme;
    job.themeVariables = themeVariables;
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
        m_cache.write(key, result.svg);
    }

    for (const Callback &callback : callbacks) {
        callback(result);
    }
}
