#include "CodeHighlighter.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>

namespace {

QTextCharFormat makeFormat(const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    return format;
}

bool isCpp(const QString &language)
{
    return language == QStringLiteral("cpp") || language == QStringLiteral("c++")
           || language == QStringLiteral("c") || language == QStringLiteral("h")
           || language == QStringLiteral("hpp");
}

bool isPython(const QString &language)
{
    return language == QStringLiteral("python") || language == QStringLiteral("py");
}

bool isJavaScript(const QString &language)
{
    return language == QStringLiteral("javascript") || language == QStringLiteral("js")
           || language == QStringLiteral("typescript") || language == QStringLiteral("ts");
}

bool isBash(const QString &language)
{
    return language == QStringLiteral("bash") || language == QStringLiteral("sh")
           || language == QStringLiteral("shell") || language == QStringLiteral("zsh");
}

bool isLatex(const QString &language)
{
    return language == QStringLiteral("latex") || language == QStringLiteral("tex");
}

} // namespace

CodeHighlighter::CodeHighlighter(QTextDocument *document, bool dark, QObject *parent)
    : QSyntaxHighlighter(document)
    , m_palette(defaultCodeSyntaxPalette(dark))
{
    if (parent) {
        setParent(parent);
    }
    rebuildFormats();
}

void CodeHighlighter::setDark(bool dark)
{
    m_palette = defaultCodeSyntaxPalette(dark);
    rebuildFormats();
    rehighlight();
}

void CodeHighlighter::setSyntaxPalette(const CodeSyntaxPalette &palette)
{
    m_palette = palette;
    rebuildFormats();
    rehighlight();
}

void CodeHighlighter::rebuildFormats()
{
    m_keyword = makeFormat(m_palette.keyword);
    m_string = makeFormat(m_palette.string);
    m_number = makeFormat(m_palette.number);
    m_comment = makeFormat(m_palette.comment);
    m_preprocessor = makeFormat(m_palette.preprocessor);
    m_type = makeFormat(m_palette.type);
    m_function = makeFormat(m_palette.function);
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    const QString language = currentBlock().blockFormat()
                                 .stringProperty(QTextFormat::BlockCodeLanguage)
                                 .trimmed()
                                 .toLower();
    if (language.isEmpty() || language == QStringLiteral("mermaid")) {
        return;
    }

    if (isCpp(language)) {
        highlightCpp(text);
    } else if (isPython(language)) {
        highlightPython(text);
    } else if (language == QStringLiteral("json")) {
        highlightJson(text);
    } else if (isJavaScript(language)) {
        highlightJavaScript(text);
    } else if (isBash(language)) {
        highlightBash(text);
    } else if (isLatex(language)) {
        highlightLatex(text);
    }
}

void CodeHighlighter::applyRegex(const QString &text,
                                 const QRegularExpression &expression,
                                 const QTextCharFormat &format)
{
    auto it = expression.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), format);
    }
}

void CodeHighlighter::highlightCpp(const QString &text)
{
    const QRegularExpression keywords(
        QStringLiteral("\\b(alignas|alignof|auto|bool|break|case|catch|char|class|const|constexpr|"
                       "continue|default|delete|do|double|else|enum|explicit|export|extern|false|"
                       "float|for|friend|if|inline|int|long|namespace|new|nullptr|operator|private|"
                       "protected|public|return|short|signed|sizeof|static|struct|switch|template|"
                       "this|throw|true|try|typedef|typename|union|unsigned|using|virtual|void|"
                       "volatile|while)\\b"));
    const QRegularExpression types(
        QStringLiteral("\\b(std|size_t|uint\\d+_t|int\\d+_t|QString|QVector|QStringList|QTextDocument|"
                       "QWidget|QObject|QImage|QColor|QSizeF|QProcess|QTimer)\\b"));
    const QRegularExpression numbers(
        QStringLiteral("\\b(0[xX][0-9a-fA-F]+|\\d+(?:\\.\\d+)?)\\b"));
    const QRegularExpression preprocessor(
        QStringLiteral("^\\s*#\\s*[A-Za-z_]+"));
    const QRegularExpression functions(
        QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*(?=\\()"));
    const QRegularExpression strings(
        QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'"));
    const QRegularExpression lineComments(QStringLiteral("//.*"));

    applyRegex(text, keywords, m_keyword);
    applyRegex(text, types, m_type);
    applyRegex(text, numbers, m_number);
    applyRegex(text, preprocessor, m_preprocessor);
    applyRegex(text, functions, m_function);
    applyRegex(text, strings, m_string);
    applyRegex(text, lineComments, m_comment);

    int state = previousBlockState();
    int searchFrom = 0;
    if (state == 1) {
        const int end = text.indexOf(QStringLiteral("*/"));
        if (end < 0) {
            setFormat(0, text.size(), m_comment);
            setCurrentBlockState(1);
            return;
        }
        setFormat(0, end + 2, m_comment);
        searchFrom = end + 2;
        setCurrentBlockState(0);
    }
    while (searchFrom < text.size()) {
        const int start = text.indexOf(QStringLiteral("/*"), searchFrom);
        if (start < 0) {
            break;
        }
        const int end = text.indexOf(QStringLiteral("*/"), start + 2);
        if (end < 0) {
            setFormat(start, text.size() - start, m_comment);
            setCurrentBlockState(1);
            break;
        }
        setFormat(start, end - start + 2, m_comment);
        searchFrom = end + 2;
    }
}

void CodeHighlighter::highlightPython(const QString &text)
{
    const QRegularExpression keywords(
        QStringLiteral("\\b(and|as|assert|async|await|break|class|continue|def|del|elif|else|except|"
                       "False|finally|for|from|global|if|import|in|is|lambda|None|nonlocal|not|or|"
                       "pass|raise|return|True|try|while|with|yield)\\b"));
    const QRegularExpression numbers(QStringLiteral("\\b(\\d+(?:\\.\\d+)?)\\b"));
    const QRegularExpression functions(
        QStringLiteral("\\b([A-Za-z_][A-Za-z0-9_]*)\\s*(?=\\()"));
    const QRegularExpression strings(
        QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'"));
    const QRegularExpression comments(QStringLiteral("#.*"));

    applyRegex(text, keywords, m_keyword);
    applyRegex(text, numbers, m_number);
    applyRegex(text, functions, m_function);
    applyRegex(text, strings, m_string);
    applyRegex(text, comments, m_comment);
}

void CodeHighlighter::highlightJson(const QString &text)
{
    const QRegularExpression strings(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
    const QRegularExpression numbers(
        QStringLiteral("\\b-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"));
    const QRegularExpression literals(QStringLiteral("\\b(true|false|null)\\b"));

    applyRegex(text, strings, m_string);
    applyRegex(text, numbers, m_number);
    applyRegex(text, literals, m_keyword);
}

void CodeHighlighter::highlightJavaScript(const QString &text)
{
    const QRegularExpression keywords(
        QStringLiteral("\\b(async|await|break|case|catch|class|const|continue|debugger|default|"
                       "delete|do|else|export|extends|false|finally|for|from|function|if|import|in|"
                       "instanceof|let|new|null|of|return|static|super|switch|this|throw|true|try|"
                       "typeof|undefined|var|void|while|with|yield)\\b"));
    const QRegularExpression numbers(
        QStringLiteral("\\b(0[xX][0-9a-fA-F]+|\\d+(?:\\.\\d+)?)\\b"));
    const QRegularExpression functions(
        QStringLiteral("\\b([A-Za-z_$][A-Za-z0-9_$]*)\\s*(?=\\()"));
    const QRegularExpression strings(
        QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'|`(?:\\\\.|[^`\\\\])*`"));
    const QRegularExpression lineComments(QStringLiteral("//.*"));

    applyRegex(text, keywords, m_keyword);
    applyRegex(text, numbers, m_number);
    applyRegex(text, functions, m_function);
    applyRegex(text, strings, m_string);
    applyRegex(text, lineComments, m_comment);
}

void CodeHighlighter::highlightBash(const QString &text)
{
    const QRegularExpression keywords(
        QStringLiteral("\\b(if|then|else|elif|fi|for|while|do|done|case|esac|function|in|select|"
                       "until|time|export|local|readonly|unset)\\b"));
    const QRegularExpression variables(QStringLiteral("\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?"));
    const QRegularExpression strings(
        QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'[^']*'"));
    const QRegularExpression comments(QStringLiteral("(^|\\s)#.*"));

    applyRegex(text, keywords, m_keyword);
    applyRegex(text, variables, m_function);
    applyRegex(text, strings, m_string);
    applyRegex(text, comments, m_comment);
}

void CodeHighlighter::highlightLatex(const QString &text)
{
    const QRegularExpression commands(QStringLiteral("\\\\[A-Za-z@]+"));
    const QRegularExpression braces(QStringLiteral("[{}]"));
    const QRegularExpression comments(QStringLiteral("(^|[^\\\\])%.*"));

    applyRegex(text, commands, m_keyword);
    applyRegex(text, braces, m_function);
    applyRegex(text, comments, m_comment);
}
