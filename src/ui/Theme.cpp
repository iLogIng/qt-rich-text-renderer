#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

void applyTheme(bool dark)
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    if (!dark) {
        // Explicit light palette: do not derive it from the system style.
        // A system dark theme can otherwise make "light mode" gray/dark.
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#111111")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f5f5f5")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffdc")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#111111")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#111111")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#f0f0f0")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#111111")));
        palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff0000")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#308cc6")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::Link, QColor(QStringLiteral("#0b57d0")));
        palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#6b3fa0")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#888888")));
    } else {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#1e1e1e")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e6e6e6")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#1e1e1e")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#252526")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#252526")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e6e6e6")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#e6e6e6")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#2d2d30")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e6e6e6")));
        palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#264f78")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
        palette.setColor(QPalette::Link, QColor(QStringLiteral("#8ab4f8")));
        palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#c58af9")));
        palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#8c8c8c")));
    }
    QApplication::setPalette(palette);
}
