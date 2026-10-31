#include "TileCache.h"
#include <algorithm>

namespace PhotoCrap::Render
{
QSet<quint64> retainedTiles(QList<TileCacheEntry> _entries, qint64 _budget)
{
    QSet<quint64> retained;
    qint64        remaining = qMax(qint64(0), _budget);
    for (const auto &entry : _entries)
        if (entry.visible) {
            retained.insert(entry.key);
            remaining = qMax(qint64(0), remaining - qMax(qint64(0), entry.bytes));
        }
    std::sort(_entries.begin(), _entries.end(), [](const auto &_a, const auto &_b) {
        return _a.lastUse == _b.lastUse ? _a.key < _b.key : _a.lastUse > _b.lastUse;
    });
    for (const auto &entry : _entries)
        if (!entry.visible && entry.bytes > 0 && entry.bytes <= remaining) {
            retained.insert(entry.key);
            remaining -= entry.bytes;
        }
    return retained;
}
} // namespace PhotoCrap::Render
