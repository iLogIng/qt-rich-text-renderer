#include "MarkdownPreprocessor.h"

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

int main()
{
    const PreparedMarkdown inlineMath =
        MarkdownPreprocessor::prepare(QStringLiteral("Inline $a+b$ and `code $x$` and $5.\n"));
    expect(inlineMath.blocks.size() == 1, QStringLiteral("one inline math block"));
    if (inlineMath.blocks.size() == 1) {
        expect(inlineMath.blocks.first().kind == RenderBlock::Kind::Math,
               QStringLiteral("inline math block kind"));
        expect(!inlineMath.blocks.first().display,
               QStringLiteral("inline math is not display math"));
        expect(inlineMath.blocks.first().source == QStringLiteral("a+b"),
               QStringLiteral("inline math source"));
        expect(inlineMath.markdown.contains(QStringLiteral("`code $x$`")),
               QStringLiteral("inline code remains literal"));
    }

    const PreparedMarkdown fenced =
        MarkdownPreprocessor::prepare(QStringLiteral("```python\nprint('$x$')\n```\n"));
    expect(fenced.blocks.isEmpty(), QStringLiteral("fenced code has no render blocks"));
    expect(fenced.markdown.contains(QStringLiteral("print('$x$')")),
           QStringLiteral("fenced code content is preserved"));

    const PreparedMarkdown mermaid = MarkdownPreprocessor::prepare(
        QStringLiteral("```mermaid\ngraph TD\nA-->B\n```\n"));
    expect(mermaid.blocks.size() == 1, QStringLiteral("one Mermaid block"));
    if (mermaid.blocks.size() == 1) {
        expect(mermaid.blocks.first().kind == RenderBlock::Kind::Mermaid,
               QStringLiteral("Mermaid block kind"));
        expect(mermaid.blocks.first().source == QStringLiteral("graph TD\nA-->B\n"),
               QStringLiteral("Mermaid source excludes fence markers"));
        expect(mermaid.markdown.contains(mermaid.blocks.first().token),
               QStringLiteral("Mermaid token is present in prepared Markdown"));
        expect(!mermaid.markdown.contains(QStringLiteral("graph TD")),
               QStringLiteral("Mermaid source is removed from prepared Markdown"));
    }

    const PreparedMarkdown displayMath =
        MarkdownPreprocessor::prepare(QStringLiteral("$$\na+b\n$$\n"));
    expect(displayMath.blocks.size() == 1, QStringLiteral("one display math block"));
    if (displayMath.blocks.size() == 1) {
        expect(displayMath.blocks.first().display,
               QStringLiteral("multi-line dollar math is display math"));
        expect(displayMath.blocks.first().source == QStringLiteral("a+b"),
               QStringLiteral("multi-line dollar math source"));
    }

    const PreparedMarkdown escapedMath =
        MarkdownPreprocessor::prepare(QStringLiteral("\\(c^2\\)\n"));
    expect(escapedMath.blocks.size() == 1, QStringLiteral("parenthesis math block"));
    if (escapedMath.blocks.size() == 1) {
        expect(escapedMath.blocks.first().source == QStringLiteral("c^2"),
               QStringLiteral("parenthesis math source"));
    }

    const PreparedMarkdown noMath =
        MarkdownPreprocessor::prepare(QStringLiteral("Price: $5 and $10, no formula.\n"));
    expect(noMath.blocks.isEmpty(), QStringLiteral("currency is not parsed as math"));

    if (failures == 0) {
        QTextStream(stdout) << "markdown preprocessor tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
