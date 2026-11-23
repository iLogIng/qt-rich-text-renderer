#include "CentralUiLayout.h"

#include "Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLayout>
#include <QMetaProperty>
#include <QPixmap>
#include <QPushButton>
#include <QMetaType>
#include <QSet>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QTextDocument>
#include <QToolButton>
#include <QUrl>
#include <QVariant>
#include <QXmlStreamReader>
#include <QUiLoader>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <utility>

namespace {

constexpr qint64 kMaxLayoutBytes = 512 * 1024;
constexpr int kMaxXmlElements = 4000;
constexpr int kMaxXmlDepth = 64;
constexpr int kMaxWidgetCount = 300;

bool fail(QString *error, const QString &message);

class ThemeAssetLabelController : public QObject {
public:
    ThemeAssetLabelController(QLabel *label, QImage image)
        : QObject(label)
        , m_label(label)
        , m_image(std::move(image))
    {
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setScaledContents(false);
        updatePixmap();
        m_label->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_label && event->type() == QEvent::Resize) {
            updatePixmap();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void updatePixmap()
    {
        QPixmap pixmap = QPixmap::fromImage(m_image);
        const QSize available = m_label->contentsRect().size();
        if (!available.isEmpty()
            && (pixmap.width() > available.width()
                || pixmap.height() > available.height())) {
            pixmap = pixmap.scaled(available,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        }
        m_label->setPixmap(pixmap);
    }

    QLabel *m_label = nullptr;
    QImage m_image;
};

bool applyThemeAssets(QWidget *root, const ThemeSpec &theme, QString *error)
{
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);
    for (QWidget *widget : widgets) {
        const QVariant value = widget->property(ThemeAssetProperty::Key);
        if (!value.isValid()) {
            continue;
        }
        const QString id = value.toString().trimmed();
        if (id.isEmpty() || !theme.assets.contains(id)) {
            return fail(error, QStringLiteral("unknown theme asset: %1").arg(id));
        }

        const ThemeAsset asset = theme.assets.value(id);
        QImageReader reader(asset.filePath);
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(true);
        const QImage image = reader.read();
        if (image.isNull()) {
            return fail(error,
                        QStringLiteral("cannot decode theme asset: %1").arg(id));
        }

        if (auto *label = qobject_cast<QLabel *>(widget)) {
            new ThemeAssetLabelController(label, image);
        } else if (auto *toolButton = qobject_cast<QToolButton *>(widget)) {
            toolButton->setIcon(QIcon(QPixmap::fromImage(image)));
        } else if (auto *pushButton = qobject_cast<QPushButton *>(widget)) {
            pushButton->setIcon(QIcon(QPixmap::fromImage(image)));
        } else {
            return fail(error,
                        QStringLiteral("theme asset host is not supported: %1").arg(id));
        }
    }
    return true;
}

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

bool isAllowedLayoutClass(const QString &className)
{
    static const QSet<QString> classes = {
        QStringLiteral("QHBoxLayout"),
        QStringLiteral("QVBoxLayout"),
        QStringLiteral("QGridLayout"),
        QStringLiteral("QFormLayout"),
        QStringLiteral("QStackedLayout"),
    };
    return classes.contains(className);
}

bool isAllowedXmlElement(const QString &name)
{
    static const QSet<QString> elements = {
        QStringLiteral("ui"),
        QStringLiteral("class"),
        QStringLiteral("widget"),
        QStringLiteral("property"),
        QStringLiteral("layout"),
        QStringLiteral("item"),
        QStringLiteral("spacer"),
        QStringLiteral("enum"),
        QStringLiteral("set"),
        QStringLiteral("string"),
        QStringLiteral("bool"),
        QStringLiteral("number"),
        QStringLiteral("double"),
        QStringLiteral("char"),
        QStringLiteral("cstring"),
        QStringLiteral("rect"),
        QStringLiteral("point"),
        QStringLiteral("size"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("x"),
        QStringLiteral("y"),
        QStringLiteral("sizepolicy"),
        QStringLiteral("horstretch"),
        QStringLiteral("verstretch"),
        QStringLiteral("sizehint"),
        QStringLiteral("font"),
        QStringLiteral("family"),
        QStringLiteral("weight"),
        QStringLiteral("italic"),
        QStringLiteral("bold"),
        QStringLiteral("underline"),
        QStringLiteral("strikeout"),
        QStringLiteral("kerning"),
        QStringLiteral("stylestrategy"),
        QStringLiteral("color"),
        QStringLiteral("red"),
        QStringLiteral("green"),
        QStringLiteral("blue"),
        QStringLiteral("alpha"),
        QStringLiteral("attribute"),
        QStringLiteral("stringlist"),
        QStringLiteral("cursorshape"),
        QStringLiteral("tabstops"),
        QStringLiteral("zorder"),
    };
    return elements.contains(name);
}

bool isForbiddenProperty(const QString &name)
{
    static const QSet<QString> properties = {
        QStringLiteral("stylesheet"),
        QStringLiteral("source"),
        QStringLiteral("url"),
        QStringLiteral("pixmap"),
        QStringLiteral("icon"),
        QStringLiteral("windowicon"),
        QStringLiteral("openexternallinks"),
        QStringLiteral("html"),
        QStringLiteral("markdown"),
        QStringLiteral("windowflags"),
        QStringLiteral("windowmodality"),
        QStringLiteral("windowstate"),
        QStringLiteral("openlinks"),
        QStringLiteral("searchpaths"),
    };
    return properties.contains(name.toLower());
}

bool containsForbiddenValue(const QString &value)
{
    const QString lower = value.toLower();
    return lower.contains(QStringLiteral("http://"))
        || lower.contains(QStringLiteral("https://"))
        || lower.contains(QStringLiteral("ftp://"))
        || lower.contains(QStringLiteral("file://"))
        || lower.contains(QStringLiteral("qrc://"))
        || lower.contains(QStringLiteral("src="))
        || lower.contains(QStringLiteral("href="))
        || lower.contains(QStringLiteral("style="))
        || lower.contains(QStringLiteral("background="))
        || lower.contains(QStringLiteral("<img"))
        || lower.contains(QStringLiteral("<style"))
        || lower.contains(QStringLiteral("<script"))
        || lower.contains(QStringLiteral("<iframe"))
        || lower.contains(QStringLiteral("<object"))
        || lower.contains(QStringLiteral("<embed"))
        || lower.contains(QStringLiteral("<link"));
}

bool validateXml(const QByteArray &data, QString *error)
{
    if (data.contains("<![CDATA[")) {
        return fail(error, QStringLiteral("layout XML must not contain CDATA sections"));
    }

    QXmlStreamReader reader(data);
    int depth = 0;
    int elements = 0;

    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType token = reader.readNext();
        if (token == QXmlStreamReader::Invalid) {
            return fail(error,
                        QStringLiteral("invalid layout XML: %1").arg(reader.errorString()));
        }
        if (token == QXmlStreamReader::DTD) {
            return fail(error, QStringLiteral("layout XML must not contain a DTD"));
        }
        if (token == QXmlStreamReader::EntityReference) {
            return fail(error, QStringLiteral("layout XML must not contain entities"));
        }
        if (token == QXmlStreamReader::Comment) {
            return fail(error, QStringLiteral("layout XML comments are not allowed"));
        }
        if (token == QXmlStreamReader::ProcessingInstruction) {
            return fail(error,
                        QStringLiteral("layout XML processing instructions are not allowed"));
        }
        if (token == QXmlStreamReader::StartElement) {
            ++depth;
            ++elements;
            if (depth > kMaxXmlDepth) {
                return fail(error, QStringLiteral("layout XML nesting is too deep"));
            }
            if (elements > kMaxXmlElements) {
                return fail(error, QStringLiteral("layout XML has too many elements"));
            }

            const QString element = reader.name().toString();
            if (!isAllowedXmlElement(element)) {
                return fail(error,
                            QStringLiteral("layout element is not allowed: %1").arg(element));
            }
            if (element == QStringLiteral("widget")) {
                const QString className =
                    reader.attributes().value(QStringLiteral("class")).toString();
                if (!CentralUiBuilder::isAllowedWidgetClass(className)) {
                    return fail(error,
                                QStringLiteral("layout widget class is not allowed: %1")
                                    .arg(className));
                }
                if (reader.attributes().hasAttribute(QStringLiteral("native"))
                    && reader.attributes().value(QStringLiteral("native")) == QStringLiteral("true")) {
                    return fail(error, QStringLiteral("native layout widgets are not allowed"));
                }
            } else if (element == QStringLiteral("layout")) {
                const QString className =
                    reader.attributes().value(QStringLiteral("class")).toString();
                if (!isAllowedLayoutClass(className)) {
                    return fail(error,
                                QStringLiteral("layout class is not allowed: %1").arg(className));
                }
            } else if (element == QStringLiteral("property")) {
                const QString property =
                    reader.attributes().value(QStringLiteral("name")).toString();
                if (isForbiddenProperty(property)) {
                    return fail(error,
                                QStringLiteral("layout property is not allowed: %1")
                                    .arg(property));
                }
            }

            for (const QXmlStreamAttribute &attribute : reader.attributes()) {
                const QString attributeName = attribute.name().toString();
                if (attributeName.compare(QStringLiteral("styleSheet"), Qt::CaseInsensitive) == 0) {
                    return fail(error, QStringLiteral("inline style sheets are not allowed"));
                }
                if (containsForbiddenValue(attribute.value().toString())) {
                    return fail(error,
                                QStringLiteral("layout contains a forbidden URL or resource reference"));
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            --depth;
        } else if (token == QXmlStreamReader::Characters
                   && !reader.isWhitespace()
                   && containsForbiddenValue(reader.text().toString())) {
            return fail(error,
                        QStringLiteral("layout text contains a forbidden URL or resource reference"));
        }
    }

    if (reader.hasError()) {
        return fail(error,
                    QStringLiteral("invalid layout XML: %1").arg(reader.errorString()));
    }
    return true;
}

class SafeUiLoader : public QUiLoader {
public:
    QString validationError() const { return m_validationError; }

    QWidget *createWidget(const QString &className,
                          QWidget *parent = nullptr,
                          const QString &name = QString()) override
    {
        if (!CentralUiBuilder::isAllowedWidgetClass(className)) {
            m_validationError =
                QStringLiteral("layout widget class is not allowed: %1").arg(className);
            return nullptr;
        }
        return QUiLoader::createWidget(className, parent, name);
    }

    QLayout *createLayout(const QString &className,
                          QObject *parent = nullptr,
                          const QString &name = QString()) override
    {
        if (!isAllowedLayoutClass(className)) {
            m_validationError =
                QStringLiteral("layout class is not allowed: %1").arg(className);
            return nullptr;
        }
        return QUiLoader::createLayout(className, parent, name);
    }

    QAction *createAction(QObject *parent = nullptr,
                          const QString &name = QString()) override
    {
        Q_UNUSED(parent);
        Q_UNUSED(name);
        m_validationError = QStringLiteral("layout actions are not allowed");
        return nullptr;
    }

    QActionGroup *createActionGroup(QObject *parent = nullptr,
                                    const QString &name = QString()) override
    {
        Q_UNUSED(parent);
        Q_UNUSED(name);
        m_validationError = QStringLiteral("layout action groups are not allowed");
        return nullptr;
    }

private:
    QString m_validationError;
};

bool readLayoutFile(const QString &path, QByteArray *data, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error,
                    QStringLiteral("cannot read layout: %1").arg(file.errorString()));
    }
    if (file.size() > kMaxLayoutBytes) {
        return fail(error, QStringLiteral("layout file is too large"));
    }
    *data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return fail(error,
                    QStringLiteral("cannot read layout: %1").arg(file.errorString()));
    }
    return true;
}

bool validateLayoutPath(const ThemeSpec &theme, QString *error)
{
    if (theme.layoutApiVersion != 1) {
        return fail(error,
                    QStringLiteral("unsupported layout API version: %1")
                        .arg(theme.layoutApiVersion));
    }
    if (theme.layoutFile.isEmpty()) {
        return fail(error, QStringLiteral("theme does not provide a layout file"));
    }
    if (theme.packageDirectory.isEmpty()) {
        return fail(error, QStringLiteral("theme package directory is missing"));
    }

    const QFileInfo packageInfo(theme.packageDirectory);
    const QString packagePath = packageInfo.canonicalFilePath();
    if (packagePath.isEmpty() || !packageInfo.isDir()) {
        return fail(error, QStringLiteral("theme package directory is not accessible"));
    }

    const QFileInfo layoutInfo(theme.layoutFile);
    if (!layoutInfo.isAbsolute() || !layoutInfo.isFile()) {
        return fail(error, QStringLiteral("layout file is missing"));
    }
    if (layoutInfo.suffix().compare(QStringLiteral("ui"), Qt::CaseInsensitive) != 0) {
        return fail(error, QStringLiteral("layout file must use the .ui suffix"));
    }

    const QString layoutPath = layoutInfo.canonicalFilePath();
    if (layoutPath.isEmpty()) {
        return fail(error, QStringLiteral("layout file is not accessible"));
    }
    if (!layoutPath.startsWith(packagePath + QDir::separator())) {
        return fail(error, QStringLiteral("layout file is outside the theme package"));
    }
    return true;
}

QWidget *findUniqueSlot(QWidget *root,
                        const QString &name,
                        bool required,
                        QString *error)
{
    const QList<QWidget *> matches = root->findChildren<QWidget *>(name);
    if (matches.size() > 1) {
        fail(error, QStringLiteral("layout slot is defined more than once: %1").arg(name));
        return nullptr;
    }
    if (matches.isEmpty()) {
        if (required) {
            fail(error, QStringLiteral("required layout slot is missing: %1").arg(name));
        }
        return nullptr;
    }

    QWidget *slot = matches.first();
    const QString className = slot->metaObject()->className();
    if (className != QStringLiteral("QWidget") && className != QStringLiteral("QFrame")) {
        fail(error,
             QStringLiteral("layout slot must be a plain QWidget or QFrame: %1").arg(name));
        return nullptr;
    }
    return slot;
}

bool ensureToolbarSlotLayout(QWidget *slot, const QString &name, QString *error)
{
    if (!slot->layout()) {
        auto *layout = new QHBoxLayout(slot);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        return true;
    }
    if (!qobject_cast<QHBoxLayout *>(slot->layout())) {
        return fail(error,
                    QStringLiteral("toolbar layout slot must use QHBoxLayout: %1").arg(name));
    }
    return true;
}

bool validateStringValue(const QString &value,
                         const QString &source,
                         QString *error)
{
    if (containsForbiddenValue(value)) {
        return fail(error,
                    QStringLiteral("layout value is not allowed: %1").arg(source));
    }
    return true;
}

bool validateLoadedValues(QWidget *root, QString *error)
{
    QList<QObject *> objects = root->findChildren<QObject *>();
    objects.prepend(root);

    for (QObject *object : objects) {
        if (qobject_cast<QTextDocument *>(object)) {
            // The generated HTML/markdown representation contains Qt's own
            // style elements; the editable properties are checked on the
            // owning QTextEdit/QTextBrowser instead.
            continue;
        }
        const QMetaObject *meta = object->metaObject();
        for (int i = 0; i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (!property.isReadable()) {
                continue;
            }
            const QByteArray propertyName(property.name());
            if (propertyName == "html" || propertyName == "markdown") {
                continue;
            }
            const QVariant value = property.read(object);
            const int type = value.metaType().id();
            const QString source =
                QStringLiteral("%1.%2").arg(QString::fromLatin1(object->metaObject()->className()),
                                            QString::fromLatin1(property.name()));
            if (propertyName == "styleSheet" && !value.toString().isEmpty()) {
                return fail(error, QStringLiteral("inline style sheets are not allowed"));
            }
            if (type == QMetaType::QString
                && !validateStringValue(value.toString(), source, error)) {
                return false;
            }
            if (type == QMetaType::QStringList) {
                for (const QString &item : value.toStringList()) {
                    if (!validateStringValue(item, source, error)) {
                        return false;
                    }
                }
            }
            if (type == QMetaType::QUrl
                && !validateStringValue(value.toUrl().toString(), source, error)) {
                return false;
            }
        }

        for (const QByteArray &name : object->dynamicPropertyNames()) {
            const QVariant value = object->property(name.constData());
            const QString source =
                QStringLiteral("%1.%2")
                    .arg(QString::fromLatin1(object->metaObject()->className()),
                         QString::fromLatin1(name.constData(), name.size()));
            if (value.metaType().id() == QMetaType::QString
                && !validateStringValue(value.toString(), source, error)) {
                return false;
            }
        }
    }
    return true;
}

void setSlotStyleProperty(QWidget *slot, const QString &value)
{
    if (!slot) {
        return;
    }
    slot->setProperty(ThemeSlotStyle::Property, value);
    if (QStyle *style = slot->style()) {
        style->unpolish(slot);
        style->polish(slot);
    }
}

bool validateWidgetCount(QWidget *root, QString *error)
{
    int count = 0;
    const QList<QWidget *> widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (widget->parentWidget() == nullptr) {
            continue;
        }
        ++count;
        if (count > kMaxWidgetCount) {
            return fail(error, QStringLiteral("layout has too many widgets"));
        }
    }
    return true;
}

} // namespace

bool CentralUiBuilder::isAllowedWidgetClass(const QString &className)
{
    static const QSet<QString> classes = {
        QStringLiteral("QWidget"),
        QStringLiteral("QFrame"),
        QStringLiteral("QLabel"),
        QStringLiteral("QPushButton"),
        QStringLiteral("QToolButton"),
        QStringLiteral("QCheckBox"),
        QStringLiteral("QRadioButton"),
        QStringLiteral("QLineEdit"),
        QStringLiteral("QComboBox"),
        QStringLiteral("QSpinBox"),
        QStringLiteral("QDoubleSpinBox"),
        QStringLiteral("QGroupBox"),
        QStringLiteral("QTabWidget"),
        QStringLiteral("QStackedWidget"),
        QStringLiteral("QSplitter"),
        QStringLiteral("QScrollArea"),
        QStringLiteral("QListWidget"),
        QStringLiteral("QTreeWidget"),
        QStringLiteral("QTableWidget"),
        QStringLiteral("QSlider"),
        QStringLiteral("QProgressBar"),
        QStringLiteral("QTextEdit"),
        QStringLiteral("QPlainTextEdit"),
        QStringLiteral("QTextBrowser"),
        QStringLiteral("QDialogButtonBox"),
    };
    return classes.contains(className);
}

QWidget *CentralUiBuilder::createSlot(QWidget *parent, const QString &name)
{
    auto *slot = new QWidget(parent);
    slot->setObjectName(name);
    auto *layout = new QVBoxLayout(slot);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return slot;
}

QWidget *CentralUiBuilder::createToolbarSlot(QWidget *parent,
                                             const QString &name,
                                             int spacing)
{
    auto *slot = new QWidget(parent);
    slot->setObjectName(name);
    auto *layout = new QHBoxLayout(slot);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(spacing);
    return slot;
}

void CentralUiBuilder::setSlotEmpty(QWidget *slot, bool empty)
{
    if (!slot) {
        return;
    }

    slot->setMinimumSize(0, 0);
    if (empty) {
        slot->setMaximumSize(0, 0);
        slot->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        slot->hide();
        return;
    }

    slot->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    const QSizePolicy policy = slot->sizePolicy();
    if (policy.horizontalPolicy() == QSizePolicy::Ignored
        && policy.verticalPolicy() == QSizePolicy::Ignored) {
        slot->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }
    slot->show();
}

void CentralUiBuilder::addToSlot(QWidget *slot, QWidget *content)
{
    if (!slot || !content) {
        return;
    }
    if (!slot->layout()) {
        auto *layout = new QVBoxLayout(slot);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }
    setSlotEmpty(slot, false);
    slot->layout()->addWidget(content);
}

bool CentralUiBuilder::slotHasContent(QWidget *slot)
{
    if (!slot) {
        return false;
    }
    if (slot->layout() && slot->layout()->count() > 0) {
        return true;
    }
    return !slot->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly).isEmpty();
}

CentralUiLayout CentralUiBuilder::createDefault(QWidget *parent, int spacing)
{
    CentralUiLayout layout;
    layout.root = new QWidget(parent);
    layout.root->setObjectName(QString::fromLatin1(CentralUiSlotNames::Root));

    auto *outer = new QVBoxLayout(layout.root);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(spacing);

    layout.top = createSlot(layout.root, QString::fromLatin1(CentralUiSlotNames::Top));
    setSlotStyleProperty(layout.top, QString::fromLatin1(ThemeSlotStyle::Top));
    setSlotEmpty(layout.top, true);
    outer->addWidget(layout.top);

    auto *toolbarRow = new QHBoxLayout;
    toolbarRow->setContentsMargins(0, 0, 0, 0);
    toolbarRow->setSpacing(0);

    layout.toolbarLeading = createToolbarSlot(
        layout.root, QString::fromLatin1(CentralUiSlotNames::ToolbarLeading), spacing);
    layout.toolbarTrailing = createToolbarSlot(
        layout.root, QString::fromLatin1(CentralUiSlotNames::ToolbarTrailing), spacing);
    setSlotStyleProperty(layout.toolbarLeading,
                         QString::fromLatin1(ThemeSlotStyle::ToolbarLeading));
    setSlotStyleProperty(layout.toolbarTrailing,
                         QString::fromLatin1(ThemeSlotStyle::ToolbarTrailing));
    setSlotEmpty(layout.toolbarLeading, true);
    setSlotEmpty(layout.toolbarTrailing, true);
    toolbarRow->addWidget(layout.toolbarLeading, 0);
    toolbarRow->addWidget(layout.toolbarTrailing, 1);
    outer->addLayout(toolbarRow);

    layout.content = createSlot(layout.root, QString::fromLatin1(CentralUiSlotNames::Content));
    setSlotStyleProperty(layout.content, QString::fromLatin1(ThemeSlotStyle::Content));
    setSlotEmpty(layout.content, true);
    layout.content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    outer->addWidget(layout.content, 1);

    layout.bottom = createSlot(layout.root, QString::fromLatin1(CentralUiSlotNames::Bottom));
    setSlotStyleProperty(layout.bottom, QString::fromLatin1(ThemeSlotStyle::Bottom));
    setSlotEmpty(layout.bottom, true);
    outer->addWidget(layout.bottom);

    layout.empty = createSlot(layout.root, QString::fromLatin1(CentralUiSlotNames::Empty));
    setSlotStyleProperty(layout.empty, QString::fromLatin1(ThemeSlotStyle::Empty));
    setSlotEmpty(layout.empty, true);
    outer->addWidget(layout.empty);

    return layout;
}

CentralUiLayout CentralUiBuilder::resolveSlots(QWidget *root, bool slotsMode, QString *error)
{
    static const QSet<QString> allowedNames = {
        QString::fromLatin1(CentralUiSlotNames::Root),
        QString::fromLatin1(CentralUiSlotNames::Top),
        QString::fromLatin1(CentralUiSlotNames::ToolbarLeading),
        QString::fromLatin1(CentralUiSlotNames::ToolbarTrailing),
        QString::fromLatin1(CentralUiSlotNames::Content),
        QString::fromLatin1(CentralUiSlotNames::Preview),
        QString::fromLatin1(CentralUiSlotNames::Source),
        QString::fromLatin1(CentralUiSlotNames::Outline),
        QString::fromLatin1(CentralUiSlotNames::Bottom),
        QString::fromLatin1(CentralUiSlotNames::Empty),
    };

    if (!root->findChildren<QWidget *>(QString::fromLatin1(CentralUiSlotNames::Root)).isEmpty()) {
        fail(error, QStringLiteral("layout slot is defined more than once: central.root"));
        return {};
    }
    for (QWidget *widget : root->findChildren<QWidget *>()) {
        const QString name = widget->objectName();
        if (name.startsWith(QStringLiteral("central.")) && !allowedNames.contains(name)) {
            fail(error, QStringLiteral("reserved layout slot is not allowed here: %1").arg(name));
            return {};
        }
    }

    CentralUiLayout layout;
    layout.root = root;
    layout.slotsMode = slotsMode;

    layout.toolbarLeading =
        findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::ToolbarLeading), true, error);
    if (!layout.toolbarLeading) {
        return {};
    }
    layout.toolbarTrailing =
        findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::ToolbarTrailing), true, error);
    if (!layout.toolbarTrailing) {
        return {};
    }
    if (!ensureToolbarSlotLayout(layout.toolbarLeading,
                                 QString::fromLatin1(CentralUiSlotNames::ToolbarLeading),
                                 error)
        || !ensureToolbarSlotLayout(layout.toolbarTrailing,
                                    QString::fromLatin1(CentralUiSlotNames::ToolbarTrailing),
                                    error)) {
        return {};
    }

    if (slotsMode) {
        layout.preview =
            findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Preview), true, error);
        if (!layout.preview) {
            return {};
        }
        layout.source =
            findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Source), true, error);
        if (!layout.source) {
            return {};
        }
        if (findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Content), false, error)) {
            fail(error, QStringLiteral("central.content is not allowed in slots mode"));
            return {};
        }
        layout.outline =
            findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Outline), false, error);
        if (error && !error->isEmpty()) {
            return {};
        }
    } else {
        layout.content =
            findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Content), true, error);
        if (!layout.content) {
            return {};
        }
        if (findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Preview), false, error)
            || findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Source), false, error)
            || findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Outline), false, error)) {
            fail(error, QStringLiteral("preview/source/outline slots require slots mode"));
            return {};
        }
        if (error && !error->isEmpty()) {
            return {};
        }
    }

    layout.top = findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Top), false, error);
    if (error && !error->isEmpty()) {
        return {};
    }
    layout.bottom = findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Bottom), false, error);
    if (error && !error->isEmpty()) {
        return {};
    }
    layout.empty = findUniqueSlot(root, QString::fromLatin1(CentralUiSlotNames::Empty), false, error);
    if (error && !error->isEmpty()) {
        return {};
    }

    if (layout.top) {
        setSlotStyleProperty(layout.top, QString::fromLatin1(ThemeSlotStyle::Top));
    }
    if (layout.bottom) {
        setSlotStyleProperty(layout.bottom, QString::fromLatin1(ThemeSlotStyle::Bottom));
    }
    if (layout.empty) {
        setSlotStyleProperty(layout.empty, QString::fromLatin1(ThemeSlotStyle::Empty));
    }
    setSlotStyleProperty(layout.toolbarLeading,
                         QString::fromLatin1(ThemeSlotStyle::ToolbarLeading));
    setSlotStyleProperty(layout.toolbarTrailing,
                         QString::fromLatin1(ThemeSlotStyle::ToolbarTrailing));
    if (layout.content) {
        setSlotStyleProperty(layout.content, QString::fromLatin1(ThemeSlotStyle::Content));
    }
    if (layout.preview) {
        setSlotStyleProperty(layout.preview, QString::fromLatin1(ThemeSlotStyle::Preview));
    }
    if (layout.source) {
        setSlotStyleProperty(layout.source, QString::fromLatin1(ThemeSlotStyle::Source));
    }
    if (layout.outline) {
        setSlotStyleProperty(layout.outline, QString::fromLatin1(ThemeSlotStyle::Outline));
    }

    for (QWidget *slot : {layout.top, layout.bottom, layout.empty}) {
        if (slot) {
            setSlotEmpty(slot, !slotHasContent(slot));
        }
    }
    for (QWidget *slot : {layout.toolbarLeading,
                           layout.toolbarTrailing,
                           layout.content,
                           layout.preview,
                           layout.source}) {
        if (slot) {
            setSlotEmpty(slot, !slotHasContent(slot));
        }
    }

    return layout;
}

CentralUiLayout CentralUiBuilder::loadTheme(const ThemeSpec &theme,
                                           QWidget *parent,
                                           QString *error)
{
    if (error) {
        error->clear();
    }
    if (!theme.layoutError.isEmpty()) {
        fail(error, theme.layoutError);
        return {};
    }
    if (!theme.assetError.isEmpty()) {
        fail(error, theme.assetError);
        return {};
    }
    if (!validateLayoutPath(theme, error)) {
        return {};
    }

    QByteArray data;
    if (!readLayoutFile(theme.layoutFile, &data, error)) {
        return {};
    }
    if (!validateXml(data, error)) {
        return {};
    }

    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("cannot open the layout buffer"));
        return {};
    }

    SafeUiLoader loader;
    loader.clearPluginPaths();
    loader.setWorkingDirectory(QDir(theme.packageDirectory));
    QWidget *root = loader.load(&buffer, parent);
    if (!root) {
        fail(error,
             loader.validationError().isEmpty()
                 ? QStringLiteral("cannot load layout: %1").arg(loader.errorString())
                 : loader.validationError());
        return {};
    }

    std::unique_ptr<QWidget> guard(root);
    if (!loader.validationError().isEmpty()) {
        fail(error, loader.validationError());
        return {};
    }
    if (!validateWidgetCount(root, error) || !validateLoadedValues(root, error)) {
        return {};
    }

    const QString rootClass = root->metaObject()->className();
    if (rootClass != QStringLiteral("QWidget") && rootClass != QStringLiteral("QFrame")) {
        fail(error, QStringLiteral("layout root must be a QWidget or QFrame"));
        return {};
    }
    if (root->objectName() != QString::fromLatin1(CentralUiSlotNames::Root)) {
        fail(error,
             QStringLiteral("layout root must be named %1")
                 .arg(QString::fromLatin1(CentralUiSlotNames::Root)));
        return {};
    }
    if (!applyThemeAssets(root, theme, error)) {
        return {};
    }

    CentralUiLayout layout = resolveSlots(root, theme.contentMode == QStringLiteral("slots"), error);
    if (!layout.isValid()) {
        return {};
    }
    layout.root = guard.release();
    layout.fromTheme = true;
    return layout;
}
