#include "LinkResolver.h"

#include <QFileInfo>
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

int main()
{
    const QString sourceDir = QStringLiteral(TEST_SOURCE_DIR) + QLatin1Char('/');
    const QString benchPath = sourceDir + QStringLiteral("bench.md");
    const QUrl base = QUrl::fromLocalFile(sourceDir);

    const auto relative = LinkResolver::resolve(QUrl(QStringLiteral("bench.md")), base);
    expect(relative.kind == LinkResolver::Kind::LocalFile, QStringLiteral("relative file kind"));
    expect(relative.error.isEmpty(), QStringLiteral("relative file resolves"));
    expect(relative.path == QFileInfo(benchPath).canonicalFilePath(),
           QStringLiteral("relative file resolves to canonical path"));

    const auto anchor = LinkResolver::resolve(QUrl(QStringLiteral("#section")), base);
    expect(anchor.kind == LinkResolver::Kind::Anchor, QStringLiteral("anchor kind"));

    const auto remote = LinkResolver::resolve(QUrl(QStringLiteral("https://example.com")), base);
    expect(remote.kind == LinkResolver::Kind::Remote, QStringLiteral("remote kind"));
    expect(remote.url == QUrl(QStringLiteral("https://example.com")),
           QStringLiteral("remote URL preserved"));

    const auto mail = LinkResolver::resolve(QUrl(QStringLiteral("mailto:dev@example.com")), base);
    expect(mail.kind == LinkResolver::Kind::Mail, QStringLiteral("mail kind"));

    const auto rejected = LinkResolver::resolve(QUrl(QStringLiteral("javascript:alert(1)")), base);
    expect(rejected.kind == LinkResolver::Kind::Rejected,
           QStringLiteral("javascript scheme rejected"));

    const auto missing = LinkResolver::resolve(QUrl(QStringLiteral("missing.md")), base);
    expect(missing.kind == LinkResolver::Kind::LocalFile, QStringLiteral("missing file kind"));
    expect(!missing.error.isEmpty(), QStringLiteral("missing file reports an error"));

    const auto noBase = LinkResolver::resolve(QUrl(QStringLiteral("sample.md")), QUrl());
    expect(noBase.kind == LinkResolver::Kind::Rejected,
           QStringLiteral("relative link without base rejected"));

    if (failures == 0) {
        QTextStream(stdout) << "link resolver tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
