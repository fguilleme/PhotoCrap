#include "Document.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>

void Document::setArtHistoryStyle(int _value)
{
    endStroke();
    m_artHistoryStyle = qBound(0, _value, 9);
    emit settingsChanged();
}
void Document::setArtHistoryArea(int _value)
{
    endStroke();
    m_artHistoryArea = qBound(1, _value, 500);
    emit settingsChanged();
}
void Document::setArtHistoryTolerance(int _value)
{
    endStroke();
    m_artHistoryTolerance = qBound(0, _value, 100);
    emit settingsChanged();
}

void Document::artHistoryDab(const QPointF &_point)
{
    if (m_historyStrokeSource.isNull() || m_tiledStrokeCoverage.isNull())
        return;
    // Each dab samples only its local cluster, never a filtered full-size source.
    // Repeatable scattering keeps undo/replay and style comparisons predictable.
    const quint32 seed   = ++m_artHistoryDabIndex;
    const int     count  = qBound(3, 3 + m_artHistoryArea / 8, 64);
    const qreal   radius = m_artHistoryArea / 2.;
    const bool  loose = m_artHistoryStyle == 3 || m_artHistoryStyle == 4 || m_artHistoryStyle >= 8;
    const bool  curl  = m_artHistoryStyle >= 6;
    const qreal width = qMax(1., m_brushSize * .3);
    const qreal lengthFactor = m_artHistoryStyle == 0 ? 1.2
                               : (m_artHistoryStyle == 2 || m_artHistoryStyle == 4 ||
                                  m_artHistoryStyle == 7 || m_artHistoryStyle == 9)
                                   ? 4.
                                   : 2.;
    const QRect sampleBounds =
        QRectF(_point.x() - radius - 3, _point.y() - radius - 3, 2 * radius + 6, 2 * radius + 6)
            .toAlignedRect()
            .intersected(QRect(QPoint{}, m_size));
    if (sampleBounds.isEmpty())
        return;
    const QImage sourcePatch = m_historyStrokeSource.region(sampleBounds);
    if (sourcePatch.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    const auto sample = [&](int _x, int _y) {
        return sourcePatch.pixelColor(qBound(0, _x, m_size.width() - 1) - sampleBounds.x(),
                                      qBound(0, _y, m_size.height() - 1) - sampleBounds.y());
    };
    const qreal reach =
        m_artHistoryStyle == 5 ? width / 2 + 2 : width * lengthFactor / 2 + width + 2;
    const QRect dabBounds = QRectF(_point.x() - radius - reach, _point.y() - radius - reach,
                                   2 * (radius + reach), 2 * (radius + reach))
                                .toAlignedRect();
    if (!m_tiledStrokeCoverage.paintTransient(dabBounds, [&](QPainter &painter) {
            painter.setRenderHint(QPainter::Antialiasing);
            for (int i = 0; i < count; ++i) {
                const qreal   phase  = (i * 2.39996323) + seed * .731;
                const qreal   offset = i == 0 ? 0. : radius * std::sqrt(qreal(i) / count);
                const QPointF centre = _point + QPointF(std::cos(phase), std::sin(phase)) * offset;
                if (!QRect(QPoint{}, m_size).contains(centre.toPoint()))
                    continue;
                const int x = qFloor(centre.x()), y = qFloor(centre.y());
                QColor    color = sample(x, y);
                if (color.alpha() == 0)
                    continue;
                const qreal dx = qGray(sample(x + 2, y).rgb()) - qGray(sample(x - 2, y).rgb());
                const qreal dy = qGray(sample(x, y + 2).rgb()) - qGray(sample(x, y - 2).rgb());
                const qreal angle =
                    (qAbs(dx) + qAbs(dy) > 2 ? qRadiansToDegrees(std::atan2(dy, dx)) + 90
                                             : m_brushAngle) +
                    (loose ? std::sin(phase) * 65 : std::sin(phase) * 12);
                QPainterPath path;
                const qreal  length = width * lengthFactor;
                if (curl) {
                    path.moveTo(-length / 2, 0);
                    path.cubicTo(-length / 3, -length / 2, length / 3, length / 2, length / 2, 0);
                } else {
                    path.moveTo(-length / 2, 0);
                    path.lineTo(length / 2, 0);
                }
                painter.save();
                painter.translate(centre);
                painter.rotate(angle);
                painter.scale(1., m_brushRoundness);
                if (m_artHistoryStyle == 5) {
                    QRadialGradient gradient(QPointF{}, width / 2);
                    color.setAlpha(qRound(color.alpha() * m_brushFlow));
                    gradient.setColorAt(qBound(0., m_brushHardness, .999), color);
                    QColor transparent = color;
                    transparent.setAlpha(0);
                    gradient.setColorAt(1, transparent);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(gradient);
                    painter.drawEllipse(QPointF{}, width / 2, width / 2);
                } else {
                    // Soft tips feather the mark, independently of stroke opacity.
                    for (int pass = 2; pass >= 0; --pass) {
                        if (pass && m_brushHardness >= .999)
                            continue;
                        QColor ink = color;
                        ink.setAlpha(qRound(color.alpha() * m_brushFlow *
                                            (pass ? .15 : .7 + .3 * m_brushHardness)));
                        painter.setPen(QPen(ink, width * (1 + pass * (1 - m_brushHardness) * .35),
                                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                        painter.drawPath(path);
                    }
                }
                painter.restore();
                // Include rotated strokes and soft tips in the dirty rectangle.
                const qreal reach = m_artHistoryStyle == 5 ? width / 2 + 2 : length / 2 + width + 2;
                const QRectF bounds(centre.x() - reach, centre.y() - reach, reach * 2, reach * 2);
                m_strokeBounds = m_strokeBounds.isEmpty() ? bounds : m_strokeBounds.united(bounds);
            }
        }))
        m_strokeCoverageFailed = true;
}

void Document::renderArtHistoryStroke()
{
    if (m_strokePlaneBase.isNull() || m_strokeBounds.isEmpty())
        return;
    const QRect   bounds = m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{}, m_size));
    const QRegion region = hasSelection() ? QRegion(bounds) & m_selectionRegion : QRegion(bounds);
    if (region.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    const QRect work = region.boundingRect();
    QImage      base =
        m_strokePlaneBase.region(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage paint   = m_tiledStrokeCoverage.region(work);
    QImage current = toolPlane().region(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (base.isNull() || paint.isNull() || current.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    if (m_artHistoryTolerance > 0) {
        const QImage source =
            m_historyStrokeSource.region(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if (source.isNull()) {
            m_strokeCoverageFailed = true;
            return;
        }
        const int threshold = qRound(m_artHistoryTolerance * 255. / 100);
        for (int y = 0; y < work.height(); ++y) {
            auto       *ink = reinterpret_cast<QRgb *>(paint.scanLine(y));
            const auto *old = reinterpret_cast<const QRgb *>(base.constScanLine(y));
            const auto *src = reinterpret_cast<const QRgb *>(source.constScanLine(y));
            for (int x = 0; x < work.width(); ++x) {
                const QRgb a = qUnpremultiply(old[x]), b = qUnpremultiply(src[x]);
                const int  delta =
                    std::max({qAbs(qRed(a) - qRed(b)), qAbs(qGreen(a) - qGreen(b)),
                              qAbs(qBlue(a) - qBlue(b)), qAbs(qAlpha(a) - qAlpha(b))});
                if (delta <= threshold)
                    ink[x] = 0;
            }
        }
    }
    SelectionCoverage::attenuate(paint, m_selectionMask, work.topLeft());
    const QImage output = blendImages(base, paint, m_brushBlend, m_brushOpacity);
    if (output.isNull()) {
        m_strokeCoverageFailed = true;
        return;
    }
    QRect dirty;
    for (const QRect &span : region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(output.constScanLine(y - work.y()));
            const auto *old = reinterpret_cast<const QRgb *>(current.constScanLine(y - work.y()));
            for (int x = span.left(); x <= span.right(); ++x)
                if (row[x - work.x()] != old[x - work.x()])
                    dirty |= QRect(x, y, 1, 1);
        }
    if (dirty.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    QPainter painter(&current);
    painter.setClipRegion(region.translated(-work.topLeft()));
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(0, 0, output);
    painter.end();
    if (!writeToolRegion(work.topLeft(), current)) {
        m_strokeCoverageFailed = true;
        return;
    }
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.united(dirty);
}
