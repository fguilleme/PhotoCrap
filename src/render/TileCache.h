#pragma once
#include <QList>
#include <QSet>

namespace PhotoCrap::Render
{
struct TileCacheEntry {
    quint64 key;
    qint64  bytes;
    quint64 lastUse;
    bool    visible;
};

// Visible tiles are pinned. Remaining space retains the most recently used
// offscreen tiles; zero disables offscreen retention without hiding the image.
QSet<quint64> retainedTiles(const QList<TileCacheEntry> &_entries, qint64 _budget);
} // namespace PhotoCrap::Render
