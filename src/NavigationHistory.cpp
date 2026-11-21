#include "NavigationHistory.h"

#include <iterator>

void NavigationHistory::clear()
{
    m_entries.clear();
    m_current = m_entries.end();
    m_hasCurrent = false;
}

void NavigationHistory::visit(const QString &path, const QString &displayName)
{
    if (path.isEmpty()) {
        return;
    }
    if (m_hasCurrent && m_current != m_entries.end() && m_current->path == path) {
        const auto next = std::next(m_current);
        if (next != m_entries.end()) {
            m_entries.erase(next, m_entries.end());
        }
        return;
    }

    if (m_hasCurrent && m_current != m_entries.end()) {
        const auto next = std::next(m_current);
        if (next != m_entries.end()) {
            m_entries.erase(next, m_entries.end());
        }
    }

    m_entries.push_back({path, displayName});
    m_current = std::prev(m_entries.end());
    m_hasCurrent = true;

    while (m_entries.size() > kMaxEntries) {
        if (m_current == m_entries.begin()) {
            ++m_current;
        }
        m_entries.erase(m_entries.begin());
    }
}

bool NavigationHistory::canBack() const
{
    return m_hasCurrent && m_current != m_entries.begin();
}

bool NavigationHistory::canForward() const
{
    if (!m_hasCurrent || m_current == m_entries.end()) {
        return false;
    }
    return std::next(m_current) != m_entries.end();
}

QString NavigationHistory::back()
{
    if (!canBack()) {
        return {};
    }
    --m_current;
    return m_current->path;
}

QString NavigationHistory::forward()
{
    if (!canForward()) {
        return {};
    }
    ++m_current;
    return m_current->path;
}

QString NavigationHistory::currentPath() const
{
    if (!m_hasCurrent || m_current == m_entries.end()) {
        return {};
    }
    return m_current->path;
}

QVector<NavigationEntry> NavigationHistory::entriesNewestFirst() const
{
    QVector<NavigationEntry> entries;
    entries.reserve(static_cast<int>(m_entries.size()));
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        entries.push_back(*it);
    }
    return entries;
}

int NavigationHistory::currentIndexNewestFirst() const
{
    if (!m_hasCurrent || m_current == m_entries.end()) {
        return -1;
    }
    const std::list<NavigationEntry>::const_iterator current = m_current;
    return static_cast<int>(std::distance(std::next(current), m_entries.cend()));
}

bool NavigationHistory::setCurrentIndexNewestFirst(int index)
{
    if (index < 0 || index >= static_cast<int>(m_entries.size())) {
        return false;
    }
    auto it = m_entries.begin();
    std::advance(it, static_cast<int>(m_entries.size()) - 1 - index);
    m_current = it;
    m_hasCurrent = true;
    return true;
}

std::size_t NavigationHistory::size() const
{
    return m_entries.size();
}
