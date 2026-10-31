#include "SliceDescriptionCache.h"
#include <QRegion>
#include <QVariantMap>
#include <utility>

namespace PhotoCrap::Tools
{
const QVariantList &SliceDescriptionCache::records(const QSize          &_size,
                                                   const QVector<QRect> &_manuals,
                                                   bool                  _showAutomatic)
{
    if (m_valid && m_size == _size && m_manuals == _manuals && m_showAutomatic == _showAutomatic)
        return m_records;
    QVariantList records;
    QRegion      uncovered(QRect(QPoint{}, _size));
    const auto   append = [&](const QRect &_rect, int _index, bool _automatic) {
        records.append(QVariantMap{{"x", _rect.x()},
                                   {"y", _rect.y()},
                                   {"width", _rect.width()},
                                   {"height", _rect.height()},
                                   {"index", _index},
                                   {"automatic", _automatic}});
    };
    for (int i = 0; i < _manuals.size(); ++i) {
        append(_manuals[i], i, false);
        if (_showAutomatic)
            uncovered -= _manuals[i];
    }
    if (_showAutomatic) {
        int index = int(_manuals.size());
        for (const QRect &rect : uncovered) {
            if (index >= 1024)
                break;
            append(rect, index++, true);
        }
    }
    m_records       = std::move(records);
    m_size          = _size;
    m_manuals       = _manuals;
    m_showAutomatic = _showAutomatic;
    m_valid         = true;
    return m_records;
}
} // namespace PhotoCrap::Tools
