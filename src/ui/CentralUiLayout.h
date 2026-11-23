#pragma once

#include <QString>

class QWidget;
struct ThemeSpec;

// Stable object names shared by the C++ default skeleton and theme layouts.
// The central.preview / central.source pages remain inside central.content.
namespace CentralUiSlotNames {

inline constexpr char Root[] = "central.root";
inline constexpr char Top[] = "central.top";
inline constexpr char ToolbarLeading[] = "central.toolbar.leading";
inline constexpr char ToolbarTrailing[] = "central.toolbar.trailing";
inline constexpr char Content[] = "central.content";
inline constexpr char Preview[] = "central.preview";
inline constexpr char Source[] = "central.source";
inline constexpr char Bottom[] = "central.bottom";
inline constexpr char Empty[] = "central.empty";

} // namespace CentralUiSlotNames

// The root widget becomes QMainWindow::centralWidget(). Only the slots listed
// here are safe injection points; optional slots are hidden when unused.
struct CentralUiLayout {
    QWidget *root = nullptr;
    QWidget *top = nullptr;
    QWidget *toolbarLeading = nullptr;
    QWidget *toolbarTrailing = nullptr;
    QWidget *content = nullptr;
    QWidget *bottom = nullptr;
    QWidget *empty = nullptr;
    bool fromTheme = false;

    bool isValid() const { return root != nullptr; }
};

class CentralUiBuilder {
public:
    // The default builder preserves the existing C++ central layout.
    static CentralUiLayout createDefault(QWidget *parent);

    // Loads a validated QUiLoader tree from a theme package. On failure the
    // returned layout is invalid and the caller must fall back to createDefault().
    static CentralUiLayout loadTheme(const ThemeSpec &theme,
                                     QWidget *parent,
                                     QString *error = nullptr);

    static QWidget *createSlot(QWidget *parent, const QString &name);
    static QWidget *createToolbarSlot(QWidget *parent, const QString &name);
    static void addToSlot(QWidget *slot, QWidget *content);
    static void setSlotEmpty(QWidget *slot, bool empty);
    static bool slotHasContent(QWidget *slot);

    static bool isAllowedWidgetClass(const QString &className);

private:
    static CentralUiLayout resolveSlots(QWidget *root, QString *error);
};
