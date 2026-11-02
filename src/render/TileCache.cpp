#include "TileCache.h"
#include <algorithm>

namespace PhotoCrap::Render
{
QSet<quint64> retainedTiles(const QList<TileCacheEntry> &_entries, qint64 _budget)
{
    QSet<quint64> retained;
    qint64        remaining = qMax(qint64(0), _budget);
    for (const auto &entry : _entries)
        if (entry.visible) {
            retained.insert(entry.key);
            remaining = qMax(qint64(0), remaining - qMax(qint64(0), entry.bytes));
        }
    if (remaining == 0)
        return retained;

    QList<const TileCacheEntry *> candidates;
    qint64                        unallocated = remaining;
    bool                          allFit      = true;
    for (const auto &entry : _entries) {
        if (entry.visible || entry.bytes <= 0 || entry.bytes > remaining)
            continue;
        candidates.append(&entry);
        if (entry.bytes <= unallocated)
            unallocated -= entry.bytes;
        else
            allFit = false;
    }
    if (allFit) {
        for (const auto *entry : candidates)
            retained.insert(entry->key);
        return retained;
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto *_a, const auto *_b) {
        return _a->lastUse == _b->lastUse ? _a->key < _b->key : _a->lastUse > _b->lastUse;
    });
    for (const auto *entry : candidates)
        if (entry->bytes <= remaining) {
            retained.insert(entry->key);
            remaining -= entry->bytes;
        }
    return retained;
}
} // namespace PhotoCrap::Render
