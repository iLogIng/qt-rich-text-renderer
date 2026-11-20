#pragma once

#include <QString>
#include <QVector>

#include <cstddef>
#include <list>

struct NavigationEntry {
    QString path;
    QString displayName;
};

class NavigationHistory {
public:
    NavigationHistory() = default;
    NavigationHistory(const NavigationHistory &) = delete;
    NavigationHistory &operator=(const NavigationHistory &) = delete;
    NavigationHistory(NavigationHistory &&) = delete;
    NavigationHistory &operator=(NavigationHistory &&) = delete;

    static constexpr std::size_t kMaxEntries = 64;

    void clear();
    void visit(const QString &path, const QString &displayName);

    bool canBack() const;
    bool canForward() const;

    QString back();
    QString forward();

    QString currentPath() const;
    QString currentDisplayName() const;

    QVector<NavigationEntry> entriesNewestFirst() const;
    int currentIndexNewestFirst() const;
    bool setCurrentIndexNewestFirst(int index);

    std::size_t size() const;

private:
    std::list<NavigationEntry> m_entries;
    std::list<NavigationEntry>::iterator m_current = m_entries.end();
    bool m_hasCurrent = false;
};
