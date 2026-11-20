#include "LocalFileLoader.h"

#include <QFile>
#include <QStringConverter>
#include <QStringEncoder>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int failures = 0;

void expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << "\n";
        ++failures;
    }
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

} // namespace

int main()
{
    QTemporaryDir dir;
    expect(dir.isValid(), QStringLiteral("temporary directory available"));
    if (!dir.isValid()) {
        return 1;
    }

    const QString textPath = dir.filePath(QStringLiteral("note.unknown"));
    expect(writeFile(textPath, QByteArray("hello\nworld\n")), QStringLiteral("write text fixture"));
    const LocalFileContent text = LocalFileLoader::load(textPath);
    expect(text.ok, QStringLiteral("text file accepted"));
    expect(text.text == QStringLiteral("hello\nworld\n"), QStringLiteral("text file decoded"));
    expect(!text.path.isEmpty(), QStringLiteral("text file path returned"));

    const QString emptyPath = dir.filePath(QStringLiteral("empty.txt"));
    expect(writeFile(emptyPath, QByteArray()), QStringLiteral("write empty fixture"));
    const LocalFileContent empty = LocalFileLoader::load(emptyPath);
    expect(empty.ok && empty.text.isEmpty(), QStringLiteral("empty text file accepted"));

    const QString binaryPath = dir.filePath(QStringLiteral("binary.dat"));
    expect(writeFile(binaryPath, QByteArray("abc\0def", 7)), QStringLiteral("write binary fixture"));
    const LocalFileContent binary = LocalFileLoader::load(binaryPath);
    expect(!binary.ok, QStringLiteral("binary file rejected"));
    expect(binary.error.contains(QStringLiteral("binary")), QStringLiteral("binary rejection explains reason"));

    const LocalFileContent directory = LocalFileLoader::load(dir.path());
    expect(!directory.ok, QStringLiteral("directory rejected"));
    expect(directory.error.contains(QStringLiteral("directory")), QStringLiteral("directory rejection explains reason"));

    const LocalFileContent missing = LocalFileLoader::load(dir.filePath(QStringLiteral("missing.md")));
    expect(!missing.ok, QStringLiteral("missing file rejected"));
    expect(missing.error.contains(QStringLiteral("does not exist")), QStringLiteral("missing file rejection explains reason"));

    QString utf16Text = QStringLiteral("hello 世界\n");
    QStringEncoder encoder(QStringEncoder::Utf16LE);
    QByteArray utf16Data = QByteArray::fromHex("fffe") + encoder.encode(utf16Text);
    const QString utf16Path = dir.filePath(QStringLiteral("utf16.txt"));
    expect(writeFile(utf16Path, utf16Data), QStringLiteral("write UTF-16 fixture"));
    const LocalFileContent utf16 = LocalFileLoader::load(utf16Path);
    expect(utf16.ok, QStringLiteral("UTF-16 file accepted"));
    expect(utf16.text == utf16Text, QStringLiteral("UTF-16 file decoded"));

    const QString utf8BomPath = dir.filePath(QStringLiteral("utf8-bom.txt"));
    expect(writeFile(utf8BomPath, QByteArray::fromHex("efbbbf") + QByteArray("hello\n")),
           QStringLiteral("write UTF-8 BOM fixture"));
    const LocalFileContent utf8Bom = LocalFileLoader::load(utf8BomPath);
    expect(utf8Bom.ok, QStringLiteral("UTF-8 BOM file accepted"));
    expect(utf8Bom.text == QStringLiteral("hello\n"), QStringLiteral("UTF-8 BOM stripped"));

    const QString latin1Path = dir.filePath(QStringLiteral("latin1.txt"));
    expect(writeFile(latin1Path, QByteArray("caf\xe9\n")), QStringLiteral("write Latin-1 fixture"));
    const LocalFileContent latin1 = LocalFileLoader::load(latin1Path);
    expect(latin1.ok, QStringLiteral("Latin-1 text accepted"));
    expect(latin1.text == QStringLiteral("café\n"), QStringLiteral("Latin-1 text decoded"));

    const QString largePath = dir.filePath(QStringLiteral("large.txt"));
    QFile largeFile(largePath);
    expect(largeFile.open(QIODevice::WriteOnly), QStringLiteral("create large fixture"));
    expect(largeFile.resize(LocalFileLoader::kMaxFileBytes + 1), QStringLiteral("size large fixture"));
    largeFile.close();
    const LocalFileContent large = LocalFileLoader::load(largePath);
    expect(!large.ok, QStringLiteral("oversized file rejected"));
    expect(large.error.contains(QStringLiteral("too large")), QStringLiteral("oversized rejection explains reason"));

    if (failures == 0) {
        QTextStream(stdout) << "local file loader tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
