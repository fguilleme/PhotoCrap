#include "Document.h"

#include <QByteArray>
#include <QPainter>
#include <QtMath>
#include <cmath>
#include <limits>

namespace
{
bool redPupilPixel(QRgb _pixel)
{
    const QRgb colour = qUnpremultiply(_pixel);
    const int  red = qRed(colour), other = qMax(qGreen(colour), qBlue(colour));
    return qAlpha(colour) > 0 && red > 60 && red > other * 1.35 && red - other > 25;
}

bool insideEllipse(const QPointF &_point, const QRectF &_ellipse)
{
    if (_ellipse.isEmpty())
        return false;
    const qreal x = (_point.x() - _ellipse.center().x()) / (_ellipse.width() / 2.);
    const qreal y = (_point.y() - _ellipse.center().y()) / (_ellipse.height() / 2.);
    return x * x + y * y <= 1.;
}
} // namespace

void Document::setRedEyePupilSize(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    endStroke();
    _value = qBound(.01, _value, 1.);
    if (qFuzzyCompare(m_redEyePupilSize, _value))
        return;
    m_redEyePupilSize = _value;
    emit settingsChanged();
}

void Document::setRedEyeDarken(qreal _value)
{
    if (!std::isfinite(_value))
        return;
    endStroke();
    _value = qBound(0., _value, 1.);
    if (qFuzzyCompare(m_redEyeDarken + 1, _value + 1))
        return;
    m_redEyeDarken = _value;
    emit settingsChanged();
}

void Document::correctRedEye()
{
    QImage *target = paintTarget();
    if (!target || m_editingMask)
        return;
    const bool dragged = m_redEyeRegion.width() >= 3 && m_redEyeRegion.height() >= 3;
    if (!dragged && !QRectF(target->rect()).contains(m_anchor))
        return;
    QRectF pupilArea;
    QRect  region;
    if (dragged) {
        // A box around one eye defines the search area. Pupil Size restricts
        // correction to a centred ellipse; it never recolours the entire box.
        const QSizeF size = m_redEyeRegion.size() * m_redEyePupilSize;
        pupilArea =
            QRectF(m_redEyeRegion.center() - QPointF(size.width(), size.height()) / 2., size);
        region = pupilArea.toAlignedRect().intersected(target->rect());
    } else {
        // A click searches locally, including when it falls on a white catchlight.
        // Never scan the full document to find a distant red object.
        const int radius = qBound(16, qRound(qMin(width(), height()) * .05), 128);
        region = QRect(qFloor(m_anchor.x()) - radius, qFloor(m_anchor.y()) - radius, 2 * radius + 1,
                       2 * radius + 1)
                     .intersected(target->rect());
    }
    if (hasSelection())
        region = region.intersected(m_selectionRegion.boundingRect());
    if (region.isEmpty())
        return;

    QImage        patch = target->copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QPointF centre    = dragged ? m_redEyeRegion.center() : m_anchor;
    const auto    candidate = [&](int _x, int _y) {
        const QPoint imagePoint = region.topLeft() + QPoint(_x, _y);
        return (!hasSelection() || m_selectionRegion.contains(imagePoint)) &&
               (!dragged || insideEllipse(QPointF(imagePoint) + QPointF(.5, .5), pupilArea)) &&
               redPupilPixel(reinterpret_cast<const QRgb *>(patch.constScanLine(_y))[_x]);
    };
    QPoint seed(-1, -1);
    qreal  closest = std::numeric_limits<qreal>::max();
    for (int y = 0; y < patch.height(); ++y)
        for (int x = 0; x < patch.width(); ++x) {
            if (!candidate(x, y))
                continue;
            const QPointF delta =
                QPointF(region.topLeft() + QPoint(x, y)) + QPointF(.5, .5) - centre;
            const qreal distance = delta.x() * delta.x() + delta.y() * delta.y();
            if (distance < closest) {
                closest = distance;
                seed    = QPoint(x, y);
            }
        }
    if (seed.x() < 0) {
        emit statusMessage(tr("Aucune pupille rouge trouvée dans cette zone."));
        return;
    }

    // Only the connected red component nearest the pointer is corrected, leaving
    // separate red objects alone even when they are in the local search window.
    QByteArray      visited(patch.width() * patch.height(), '\0');
    QVector<QPoint> pixels{seed};
    visited[seed.y() * patch.width() + seed.x()] = 1;
    QRect bounds(seed, QSize(1, 1));
    for (qsizetype i = 0; i < pixels.size(); ++i) {
        const QPoint point = pixels[i];
        bounds |= QRect(point, QSize(1, 1));
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const QPoint next = point + QPoint(dx, dy);
                if (!patch.rect().contains(next))
                    continue;
                const int index = next.y() * patch.width() + next.x();
                if (visited[index])
                    continue;
                visited[index] = 1;
                if (candidate(next.x(), next.y()))
                    pixels.append(next);
            }
    }
    if (!dragged) {
        const QSizeF size = QSizeF(bounds.size()) * (.5 + m_redEyePupilSize);
        pupilArea         = QRectF(QRectF(bounds).center() + region.topLeft() -
                                       QPointF(size.width(), size.height()) / 2.,
                                   size);
    }
    QRect modified;
    for (const QPoint point : std::as_const(pixels)) {
        if (!insideEllipse(QPointF(point + region.topLeft()) + QPointF(.5, .5), pupilArea))
            continue;
        auto       *line     = reinterpret_cast<QRgb *>(patch.scanLine(point.y()));
        const QRgb  original = line[point.x()], colour = qUnpremultiply(original);
        const int   r = qRed(colour), g = qGreen(colour), b = qBlue(colour);
        const qreal strength  = qBound(0., (r - qMax(g, b) - 15.) / 45., 1.);
        const int   neutral   = qRound((g + b) / 2. * (1. - .9 * m_redEyeDarken));
        const QRgb  corrected = qPremultiply(
            qRgba(qRound(r + (neutral - r) * strength), qRound(g + (neutral - g) * strength),
                  qRound(b + (neutral - b) * strength), qAlpha(colour)));
        if (corrected != original) {
            line[point.x()] = corrected;
            modified |= QRect(point, QSize(1, 1));
        }
    }
    if (modified.isEmpty())
        return;
    const QRect dirty = modified.translated(region.topLeft());
    QPainter    painter(target);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(dirty.topLeft(), patch, modified);
    painter.end();
    m_dirtyRegion   = m_dirtyRegion.isEmpty() ? dirty : m_dirtyRegion.united(dirty);
    m_strokeChanged = true;
}
