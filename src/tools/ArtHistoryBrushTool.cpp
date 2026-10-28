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
    if (m_historyStrokeSource.isNull() || m_strokeCoverage.isNull())
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
    QPainter    painter(&m_strokeCoverage);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto sample = [&](int _x, int _y) {
        return m_historyStrokeSource.pixelColor(qBound(0, _x, m_size.width() - 1),
                                                qBound(0, _y, m_size.height() - 1));
    };
    for (int i = 0; i < count; ++i) {
        const qreal   phase  = (i * 2.39996323) + seed * .731;
        const qreal   offset = i == 0 ? 0. : radius * std::sqrt(qreal(i) / count);
        const QPointF centre = _point + QPointF(std::cos(phase), std::sin(phase)) * offset;
        if (!m_strokeCoverage.rect().contains(centre.toPoint()))
            continue;
        const int x = qFloor(centre.x()), y = qFloor(centre.y());
        QColor    color = sample(x, y);
        if (color.alpha() == 0)
            continue;
        const qreal dx = qGray(sample(x + 2, y).rgb()) - qGray(sample(x - 2, y).rgb());
        const qreal dy = qGray(sample(x, y + 2).rgb()) - qGray(sample(x, y - 2).rgb());
        const qreal angle =
            (qAbs(dx) + qAbs(dy) > 2 ? qRadiansToDegrees(std::atan2(dy, dx)) + 90 : m_brushAngle) +
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
                ink.setAlpha(
                    qRound(color.alpha() * m_brushFlow * (pass ? .15 : .7 + .3 * m_brushHardness)));
                painter.setPen(QPen(ink, width * (1 + pass * (1 - m_brushHardness) * .35),
                                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPath(path);
            }
        }
        painter.restore();
        // Include rotated strokes and soft tips in the dirty rectangle.
        const qreal  reach = m_artHistoryStyle == 5 ? width / 2 + 2 : length / 2 + width + 2;
        const QRectF bounds(centre.x() - reach, centre.y() - reach, reach * 2, reach * 2);
        m_strokeBounds = m_strokeBounds.isEmpty() ? bounds : m_strokeBounds.united(bounds);
    }
}

void Document::renderArtHistoryStroke()
{
    QImage *target = paintTarget();
    if (!target || m_strokeBase.isNull() || m_strokeBounds.isEmpty())
        return;
    const QRect   bounds = m_strokeBounds.toAlignedRect().intersected(target->rect());
    const QRegion region = hasSelection() ? QRegion(bounds) & m_selectionRegion : QRegion(bounds);
    if (region.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    const QRect work = region.boundingRect();
    QImage      base = m_strokeBase.copy(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage      paint = m_strokeCoverage.copy(work);
    if (m_artHistoryTolerance > 0) {
        const QImage source =
            m_historyStrokeSource.copy(work).convertToFormat(QImage::Format_ARGB32_Premultiplied);
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
    QRect        dirty;
    for (const QRect &span : region)
        for (int y = span.top(); y <= span.bottom(); ++y) {
            const auto *row = reinterpret_cast<const QRgb *>(output.constScanLine(y - work.y()));
            const auto *old = reinterpret_cast<const QRgb *>(target->constScanLine(y));
            for (int x = span.left(); x <= span.right(); ++x)
                if (row[x - work.x()] != old[x])
                    dirty |= QRect(x, y, 1, 1);
        }
    if (dirty.isEmpty()) {
        m_strokeBounds = {};
        return;
    }
    QPainter painter(target);
    painter.setClipRegion(region);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(work.topLeft(), output);
    painter.end();
    if (!m_fullCompositeDirty)
        m_dirtyRegion = m_dirtyRegion.united(dirty);
}
