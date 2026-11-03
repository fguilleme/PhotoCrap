#include "GraphicsBudget.h"
#include <QMutexLocker>
namespace PhotoCrap::Render
{
std::shared_ptr<GraphicsBudget> GraphicsBudget::shared()
{
    static auto budget = std::make_shared<GraphicsBudget>();
    return budget;
}
std::shared_ptr<GraphicsBudget::Client>
GraphicsBudget::attach(const std::shared_ptr<GraphicsBudget> &budget, qint64 target)
{
    QMutexLocker lock(&budget->m_mutex);
    const auto   id              = ++budget->m_next;
    budget->m_entries[id].target = qMax(qint64(0), target);
    return std::shared_ptr<Client>(new Client(budget, id));
}
GraphicsBudget::Client::~Client()
{
    QMutexLocker lock(&m_budget->m_mutex);
    m_budget->m_entries.remove(m_id);
    lock.unlock();
    invalidateOthers();
}
void GraphicsBudget::Client::invalidateWith(std::function<void()> callback)
{
    QMutexLocker lock(&m_budget->m_mutex);
    m_budget->m_entries[m_id].invalidate = std::move(callback);
}
void GraphicsBudget::Client::invalidateOthers()
{
    QList<std::function<void()>> callbacks;
    {
        QMutexLocker lock(&m_budget->m_mutex);
        for (auto it = m_budget->m_entries.cbegin(); it != m_budget->m_entries.cend(); ++it)
            if (it.key() != m_id && it->invalidate)
                callbacks.append(it->invalidate);
    }
    for (const auto &callback : callbacks)
        callback();
}
void GraphicsBudget::Client::configure(qint64 target)
{
    QMutexLocker lock(&m_budget->m_mutex);
    m_budget->m_entries[m_id].target = qMax(qint64(0), target);
    lock.unlock();
    invalidateOthers();
}
void GraphicsBudget::Client::pin(qint64 bytes)
{
    QMutexLocker lock(&m_budget->m_mutex);
    bytes = qMax(qint64(0), bytes);
    if (m_budget->m_entries[m_id].pinned == bytes)
        return;
    m_budget->m_entries[m_id].pinned = bytes;
    lock.unlock();
    invalidateOthers();
}
void GraphicsBudget::Client::report(Category category, qint64 bytes)
{
    QMutexLocker lock(&m_budget->m_mutex);
    m_budget->m_entries[m_id].bytes[category] = qMax(qint64(0), bytes);
    m_budget->m_peak = qMax(m_budget->m_peak, m_budget->usageLocked().resident);
}
GraphicsBudget::Usage GraphicsBudget::usageLocked() const
{
    Usage result;
    result.peak = m_peak;
    for (const auto &entry : m_entries) {
        result.target = qMax(result.target, entry.target);
        result.pinned += entry.pinned;
        for (size_t i = 0; i < result.categories.size(); ++i) {
            result.categories[i] += entry.bytes[i];
            result.resident += entry.bytes[i];
        }
    }
    return result;
}
GraphicsBudget::Usage GraphicsBudget::usage() const
{
    QMutexLocker lock(&m_mutex);
    return usageLocked();
}
qint64 GraphicsBudget::Client::allowance() const
{
    QMutexLocker lock(&m_budget->m_mutex);
    const auto   usage  = m_budget->usageLocked();
    const auto   own    = m_budget->m_entries.value(m_id);
    int          active = 0;
    for (const auto &entry : m_budget->m_entries)
        active += entry.pinned > 0;
    if (own.pinned <= 0 || own.target <= 0)
        return own.pinned;
    // Share only spare space; visible working sets must never disappear mid-frame.
    return own.pinned + qMin(qMax(qint64(0), own.target - own.pinned),
                             qMax(qint64(0), usage.target - usage.pinned) / qMax(1, active));
}
qint64 GraphicsBudget::Client::available() const
{
    QMutexLocker lock(&m_budget->m_mutex);
    const auto   usage = m_budget->usageLocked();
    const auto   own   = m_budget->m_entries.value(m_id);
    return qMin(own.target, qMax(qint64(0), usage.target - usage.pinned + own.pinned));
}
} // namespace PhotoCrap::Render
