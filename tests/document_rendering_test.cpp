#include "CodeHighlighter.h"
#include "MarkdownDocumentBuilder.h"
#include "MarkdownSourceHighlighter.h"
#include "SyntaxPalette.h"

#include <QAbstractTextDocumentLayout>
#include <QFont>
#include <QGuiApplication>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
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

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QTextDocument highlightedDocument;
    QTextCursor codeCursor(&highlightedDocument);
    QTextBlockFormat codeFormat;
    codeFormat.setProperty(QTextFormat::BlockCodeLanguage, QStringLiteral("cpp"));
    codeCursor.setBlockFormat(codeFormat);
    codeCursor.insertText(QStringLiteral("int main() { return 0; }"));

    CodeHighlighter highlighter(&highlightedDocument, false);
    highlighter.rehighlight();
    highlightedDocument.documentLayout()->documentSize();
    const QTextBlock codeBlock = highlightedDocument.firstBlock();
    expect(codeBlock.layout() != nullptr, QStringLiteral("code block layout exists"));
    if (codeBlock.layout()) {
        expect(!codeBlock.layout()->formats().isEmpty(),
               QStringLiteral("C++ code receives text formats"));
    }

    highlighter.setDark(true);
    highlightedDocument.documentLayout()->documentSize();
    expect(codeBlock.layout() != nullptr, QStringLiteral("code block layout survives theme switch"));

    QTextDocument paletteDocument;
    QTextCursor paletteCursor(&paletteDocument);
    QTextBlockFormat paletteFormat;
    paletteFormat.setProperty(QTextFormat::BlockCodeLanguage, QStringLiteral("cpp"));
    paletteCursor.setBlockFormat(paletteFormat);
    paletteCursor.insertText(QStringLiteral("return"));
    CodeSyntaxPalette codePalette = defaultCodeSyntaxPalette(false);
    codePalette.keyword = QColor(QStringLiteral("#123456"));
    CodeHighlighter paletteHighlighter(&paletteDocument, false);
    paletteHighlighter.setSyntaxPalette(codePalette);
    paletteHighlighter.rehighlight();
    paletteDocument.documentLayout()->documentSize();
    const QTextBlock paletteBlock = paletteDocument.firstBlock();
    expect(paletteBlock.layout() != nullptr, QStringLiteral("palette code block layout exists"));
    if (paletteBlock.layout() && !paletteBlock.layout()->formats().isEmpty()) {
        expect(paletteBlock.layout()->formats().first().format.foreground().color()
                   == codePalette.keyword,
               QStringLiteral("custom code syntax palette is applied"));
    }

    QTextDocument sourceDocument;
    sourceDocument.setPlainText(QStringLiteral("# Heading\n\n**bold** text\n"));
    MarkdownSourceHighlighter sourceHighlighter(&sourceDocument, false);
    MarkdownSourcePalette sourcePalette = defaultMarkdownSourcePalette(false);
    sourcePalette.heading = QColor(QStringLiteral("#654321"));
    sourceHighlighter.setSyntaxPalette(sourcePalette);
    sourceHighlighter.rehighlight();
    sourceDocument.documentLayout()->documentSize();
    const QTextBlock headingBlock = sourceDocument.firstBlock();
    expect(headingBlock.layout() != nullptr, QStringLiteral("source heading layout exists"));
    if (headingBlock.layout()) {
        expect(!headingBlock.layout()->formats().isEmpty(),
               QStringLiteral("Markdown source heading receives formats"));
        if (!headingBlock.layout()->formats().isEmpty()) {
            expect(headingBlock.layout()->formats().first().format.foreground().color()
                       == sourcePalette.heading,
                   QStringLiteral("custom source syntax palette is applied"));
        }
    }
    sourceHighlighter.setEnabled(false);
    sourceDocument.documentLayout()->documentSize();
    if (headingBlock.layout()) {
        expect(headingBlock.layout()->formats().isEmpty(),
               QStringLiteral("disabling source highlighting clears formats"));
    }

    QTextDocument fencedDocument;
    fencedDocument.setPlainText(
        QStringLiteral("# Heading\n```\n# not a heading\n```\n"));
    MarkdownSourceHighlighter fencedHighlighter(&fencedDocument, false);
    fencedHighlighter.rehighlight();
    fencedDocument.documentLayout()->documentSize();
    const QTextBlock codeLine = fencedDocument.findBlockByNumber(2);
    if (codeLine.layout() && !codeLine.layout()->formats().isEmpty()) {
        expect(codeLine.layout()->formats().first().format.fontWeight() != QFont::Bold,
               QStringLiteral("hash inside fenced code is not styled as a heading"));
    } else {
        expect(false, QStringLiteral("fenced code line receives a source format"));
    }

    QTextDocument document;
    document.setPlainText(QStringLiteral("before TOKEN after"));
    QString error;
    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\">"
        "<rect width=\"40\" height=\"20\" fill=\"#336699\"/>"
        "</svg>");
    const bool inserted = MarkdownDocumentBuilder::embedSvg(&document,
                                                            QStringLiteral("TOKEN"),
                                                            svg,
                                                            QString(),
                                                            true,
                                                            false,
                                                            QColor(Qt::black),
                                                            120,
                                                            &error);
    expect(inserted, QStringLiteral("valid SVG is inserted"));
    expect(error.isEmpty(), QStringLiteral("valid SVG has no error"));
    expect(!document.toPlainText().contains(QStringLiteral("TOKEN")),
           QStringLiteral("token is replaced"));
    expect(document.toPlainText().contains(QChar::ObjectReplacementCharacter),
           QStringLiteral("SVG object replacement character is present"));

    QTextDocument failedDocument;
    failedDocument.setPlainText(QStringLiteral("TOKEN"));
    QString failure;
    const bool failed = MarkdownDocumentBuilder::embedSvg(&failedDocument,
                                                          QStringLiteral("TOKEN"),
                                                          QStringLiteral("<svg"),
                                                          QString(),
                                                          true,
                                                          false,
                                                          QColor(Qt::black),
                                                          120,
                                                          &failure);
    expect(!failed, QStringLiteral("invalid SVG is rejected"));
    expect(!failure.isEmpty(), QStringLiteral("invalid SVG reports an error"));
    expect(failedDocument.toPlainText().contains(QStringLiteral("[render error:")),
           QStringLiteral("invalid SVG becomes an inline render error"));

    if (failures == 0) {
        QTextStream(stdout) << "document rendering tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
