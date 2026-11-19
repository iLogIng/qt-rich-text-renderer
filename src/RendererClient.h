#pragma once

#include "RenderTypes.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

class RendererClient : public QObject {
    Q_OBJECT

public:
    using Callback = std::function<void(const RenderResult &)>;

    explicit RendererClient(QObject *parent = nullptr);

    void configure(const QString &qjsPath,
                   const QString &mathScript,
                   const QString &mermaidScript,
                   const QString &cacheDir,
                   const QString &version);

    void renderMath(const QString &tex, bool display, Callback callback);
    void renderMermaid(const QString &source, bool dark, Callback callback);
    void cancelAll();

private:
    void render(const QString &kind,
                const QString &source,
                const QString &script,
                const QStringList &extraArgs,
                Callback callback);
    QString cacheKey(const QString &kind, const QString &source, const QStringList &extraArgs) const;

    QString m_qjsPath;
    QString m_mathScript;
    QString m_mermaidScript;
    QString m_cacheDir;
    QString m_version;
    QSet<QProcess *> m_activeProcesses;
    quint64 m_generation = 0;
};
