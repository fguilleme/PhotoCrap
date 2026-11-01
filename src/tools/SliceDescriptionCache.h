#pragma once
#include <QRect>
#include <QSize>
#include <QVariantList>
#include <QVector>

namespace PhotoCrap::Tools
{
// GUI-owned memoization. Selection and draft gestures do not change this key.
class SliceDescriptionCache
{
public:
    const QVariantList &records(const QSize &_size, const QVector<QRect> &_manuals,
                                bool _showAutomatic);
    quint64             revision() const
    {
        return m_revision;
    }

private:
    bool           m_valid = false, m_showAutomatic = false;
    QSize          m_size;
    QVector<QRect> m_manuals;
    QVariantList   m_records;
    quint64        m_revision = 0;
};
} // namespace PhotoCrap::Tools
