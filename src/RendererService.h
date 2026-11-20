#pragma once

#include "RenderTypes.h"
#include "SvgCache.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class RendererWorker;

class RendererService : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const RenderResult &)>;

    explicit RendererService(QObject *parent = nullptr);

    void configure(const QString &qjsPath,
                   const QString &mathWorkerScript,
                   const QString &mermaidWorkerScript,
                   const QString &cacheDir,
                   const QString &version,
                   int idleMs = 60000,
                   int mathRequestTimeoutMs = 10000,
                   int mermaidRequestTimeoutMs = 15000);

    void renderMath(const QString &tex, bool display, Callback callback);
    void renderMermaid(const QString &source, bool dark, Callback callback);
    void cancelAll();
    void shutdown();

    int pendingCount() const;

private:
    struct Inflight {
        quint64 generation = 0;
        QVector<Callback> callbacks;
    };

    void render(const QString &kind,
                const QString &source,
                bool display,
                const QString &theme,
                Callback callback);
    void handleWorkerResult(const QString &key, quint64 generation, const RenderResult &result);

    RendererWorker *m_mathWorker = nullptr;
    RendererWorker *m_mermaidWorker = nullptr;
    SvgCache m_cache;
    QHash<QString, Inflight> m_inflight;
    quint64 m_generation = 0;
};
