#include "LocalFileLoader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QStringDecoder>

namespace {

QString decodeUtf8(const QByteArray &data, bool *ok)
{
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder.decode(data);
    *ok = !decoder.hasError();
    return text;
}

QString decodeUtf16(const QByteArray &data, QStringDecoder::Encoding encoding, bool *ok)
{
    QStringDecoder decoder(encoding);
    QString text = decoder.decode(data);
    *ok = !decoder.hasError();
    return text;
}

bool isTextMimeType(const QMimeType &mime)
{
    const QString name = mime.name();
    return mime.inherits(QStringLiteral("text/plain"))
           || name.startsWith(QStringLiteral("text/"))
           || name == QStringLiteral("application/json")
           || name == QStringLiteral("application/xml")
           || name == QStringLiteral("application/javascript")
           || name == QStringLiteral("application/x-javascript")
           || name == QStringLiteral("application/x-shellscript")
           || name == QStringLiteral("application/x-yaml")
           || name == QStringLiteral("application/yaml")
           || name == QStringLiteral("application/toml")
           || name.endsWith(QStringLiteral("+xml"))
           || name.endsWith(QStringLiteral("+json"));
}

bool isKnownBinaryMimeType(const QMimeType &mime)
{
    const QString name = mime.name();
    if (name == QStringLiteral("image/svg+xml") || name.endsWith(QStringLiteral("+xml"))
        || name.endsWith(QStringLiteral("+json"))) {
        return false;
    }

    return name.startsWith(QStringLiteral("image/"))
           || name.startsWith(QStringLiteral("audio/"))
           || name.startsWith(QStringLiteral("video/"))
           || name.startsWith(QStringLiteral("font/"))
           || name.startsWith(QStringLiteral("model/"))
           || name == QStringLiteral("application/pdf")
           || name == QStringLiteral("application/zip")
           || name == QStringLiteral("application/gzip")
           || name == QStringLiteral("application/x-tar")
           || name == QStringLiteral("application/x-bzip2")
           || name == QStringLiteral("application/x-xz")
           || name == QStringLiteral("application/x-7z-compressed")
           || name == QStringLiteral("application/vnd.rar")
           || name == QStringLiteral("application/x-executable")
           || name == QStringLiteral("application/x-sharedlib")
           || name == QStringLiteral("application/x-object")
           || name == QStringLiteral("application/x-elf");
}

bool containsNul(const QByteArray &data)
{
    return data.contains('\0');
}

bool containsDecodedNul(const QString &text)
{
    return text.contains(QChar(0));
}

} // namespace

LocalFileContent LocalFileLoader::load(const QString &path)
{
    LocalFileContent result;
    const QFileInfo info(path);

    if (!info.exists()) {
        result.error = QStringLiteral("file does not exist");
        return result;
    }
    if (info.isDir()) {
        result.error = QStringLiteral("path is a directory");
        return result;
    }
    if (!info.isFile()) {
        result.error = QStringLiteral("path is not a regular file");
        return result;
    }
    if (info.size() > kMaxFileBytes) {
        result.error = QStringLiteral("file is too large to display (limit: %1 MiB)")
                           .arg(kMaxFileBytes / (1024 * 1024));
        return result;
    }

    const QString canonical = info.canonicalFilePath();
    result.path = canonical.isEmpty() ? info.absoluteFilePath() : canonical;

    QFile file(result.path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("cannot read file: %1").arg(file.errorString());
        return result;
    }

    const QByteArray data = file.read(kMaxFileBytes + 1);
    if (data.size() > kMaxFileBytes) {
        result.error = QStringLiteral("file is too large to display (limit: %1 MiB)")
                           .arg(kMaxFileBytes / (1024 * 1024));
        return result;
    }
    if (file.error() != QFileDevice::NoError) {
        result.error = QStringLiteral("cannot read file: %1").arg(file.errorString());
        return result;
    }

    if (data.isEmpty()) {
        result.ok = true;
        return result;
    }

    if (data.startsWith("\xEF\xBB\xBF")) {
        bool ok = false;
        result.text = decodeUtf8(data.mid(3), &ok);
        if (!ok) {
            result.error = QStringLiteral("file contains invalid UTF-8");
        } else if (containsDecodedNul(result.text)) {
            result.error = QStringLiteral("file appears to be binary");
        } else {
            result.ok = true;
        }
        return result;
    }

    if (data.startsWith("\xFF\xFE")) {
        bool ok = false;
        result.text = decodeUtf16(data.mid(2), QStringDecoder::Utf16LE, &ok);
        if (!ok) {
            result.error = QStringLiteral("file contains invalid UTF-16");
        } else if (containsDecodedNul(result.text)) {
            result.error = QStringLiteral("file appears to be binary");
        } else {
            result.ok = true;
        }
        return result;
    }

    if (data.startsWith("\xFE\xFF")) {
        bool ok = false;
        result.text = decodeUtf16(data.mid(2), QStringDecoder::Utf16BE, &ok);
        if (!ok) {
            result.error = QStringLiteral("file contains invalid UTF-16");
        } else if (containsDecodedNul(result.text)) {
            result.error = QStringLiteral("file appears to be binary");
        } else {
            result.ok = true;
        }
        return result;
    }

    if (containsNul(data)) {
        result.error = QStringLiteral("file appears to be binary");
        return result;
    }

    const QMimeDatabase mimeDatabase;
    const QMimeType mime = mimeDatabase.mimeTypeForData(data.left(8192));
    if (isKnownBinaryMimeType(mime)) {
        result.error = QStringLiteral("file appears to be binary (%1)").arg(mime.name());
        return result;
    }

    bool utf8Ok = false;
    result.text = decodeUtf8(data, &utf8Ok);
    if (utf8Ok) {
        result.ok = true;
        return result;
    }

    // Legacy single-byte encodings are still text. Only fall back when the
    // content is not identifiable as a known binary format.
    if (isTextMimeType(mime)) {
        result.text = QString::fromLatin1(data);
        result.ok = true;
        return result;
    }

    result.error = QStringLiteral("file appears to be binary");
    return result;
}
