#include "Document.h"
#include "ImageUtils.h"

#include <QFutureWatcher>
#include <QLineF>
#include <QTransform>
#include <QtConcurrent>
#include <cmath>

namespace
{
bool validQuad(const QPolygonF &_points)
{
    if (_points.size() != 4)
        return false;
    // Consistent clockwise winding rejects crossed and degenerate quadrilaterals.
    for (int i = 0; i < 4; ++i) {
        const QPointF a = _points[(i + 1) % 4] - _points[i];
        const QPointF b = _points[(i + 2) % 4] - _points[(i + 1) % 4];
        if (!std::isfinite(a.x()) || !std::isfinite(a.y()) || a.x() * b.y() - a.y() * b.x() < 1)
            return false;
    }
    return true;
}
struct CropResult {
    QVector<Layer> layers;
    QImage         composite;
    bool           valid = true;
};
} // namespace

QVariantList Document::cropPoints() const
{
    QVariantList result;
    for (const auto &point : m_cropPoints)
        result.append(QVariantMap{{"x", point.x()}, {"y", point.y()}});
    return result;
}
bool Document::hasCrop() const
{
    return validQuad(m_cropPoints);
}

void Document::setCropRectangle(const QRectF &_rect)
{
    const QRectF rect = _rect.normalized().intersected(QRectF(QPointF{}, m_size));
    m_cropPoints      = {rect.topLeft(), rect.topRight(), rect.bottomRight(), rect.bottomLeft()};
    emit cropChanged();
}
void Document::setCropRatio(qreal _ratio)
{
    if (!std::isfinite(_ratio) || _ratio < 0 || _ratio > 1000 || m_cropBusy)
        return;
    m_cropRatio = _ratio;
    if (m_tool == "crop" && hasCrop() && _ratio > 0) {
        QRectF rect = m_cropPoints.boundingRect();
        QSizeF size = rect.size();
        if (size.width() / size.height() > _ratio)
            size.setWidth(size.height() * _ratio);
        else
            size.setHeight(size.width() / _ratio);
        setCropRectangle(QRectF(rect.center() - QPointF(size.width(), size.height()) / 2, size));
    }
    emit cropChanged();
}
void Document::setCropOutputSize(const QSize &_size)
{
    if (m_cropBusy || _size.width() < 0 || _size.height() < 0 || _size.width() > 32768 ||
        _size.height() > 32768)
        return;
    m_cropOutputSize = _size;
    emit cropChanged();
}
void Document::cancelCrop()
{
    m_cropDragging = false;
    m_cropPoints.clear();
    m_cropGesturePoints.clear();
    emit cropChanged();
}
void Document::resetCrop()
{
    if (m_cropBusy)
        return;
    cancelCrop();
    m_cropRatio      = 0;
    m_cropOutputSize = {0, 0};
    setCropRectangle(QRectF(QPointF{}, m_size));
}
void Document::beginCropGesture(qreal _x, qreal _y, qreal _tolerance)
{
    if (m_cropBusy || !std::isfinite(_x) || !std::isfinite(_y) ||
        (m_tool != "crop" && m_tool != "perspectiveCrop"))
        return;
    m_cropAnchor        = QPointF(qBound(0., _x, qreal(width())), qBound(0., _y, qreal(height())));
    m_geometryPointer   = m_cropAnchor;
    m_cropGesturePoints = m_cropPoints;
    m_cropHandle        = -2; // Draw a new frame.
    const qreal tolerance = std::isfinite(_tolerance) ? qMax(0., _tolerance) : 8;
    if (hasCrop()) {
        for (int i = 0; i < 4; ++i)
            if (QLineF(m_cropAnchor, m_cropPoints[i]).length() <= tolerance) {
                m_cropHandle = i;
                break;
            }
        if (m_cropHandle == -2 && m_tool == "crop")
            for (int i = 0; i < 4; ++i)
                if (QLineF(m_cropAnchor, (m_cropPoints[i] + m_cropPoints[(i + 1) % 4]) / 2)
                        .length() <= tolerance) {
                    m_cropHandle = 4 + i;
                    break;
                }
        if (m_cropHandle == -2 && m_cropPoints.containsPoint(m_cropAnchor, Qt::OddEvenFill))
            m_cropHandle = -1; // Move the whole frame.
    }
    m_cropDragging = true;
    if (m_cropHandle == -2)
        setCropRectangle(QRectF(m_cropAnchor, QSizeF(0, 0)));
}
void Document::setCropCorner(int _index, qreal _x, qreal _y)
{
    if (m_cropBusy || m_tool != "perspectiveCrop" || _index < 0 || _index >= 4 ||
        m_cropPoints.size() != 4 || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    QPolygonF next = m_cropPoints;
    next[_index]   = QPointF(qBound(0., _x, qreal(width())), qBound(0., _y, qreal(height())));
    if (validQuad(next)) {
        m_cropPoints = next;
        emit cropChanged();
    }
}
void Document::updateCropGesture(qreal _x, qreal _y)
{
    if (!m_cropDragging || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    QPointF point(qBound(0., _x, qreal(width())), qBound(0., _y, qreal(height())));
    if (m_toolSpace && m_cropHandle == -2)
        m_cropAnchor += point - m_geometryPointer;
    m_geometryPointer    = point;
    const QRectF initial = m_cropGesturePoints.boundingRect();
    const qreal  ratio   = m_cropRatio > 0 ? m_cropRatio
                           : m_toolShift && m_tool == "crop"
                               ? (initial.height() > 0 ? initial.width() / initial.height() : 1.)
                               : 0;
    if (m_cropHandle == -1) {
        const QRectF bounds = m_cropGesturePoints.boundingRect();
        QPointF      delta  = point - m_cropAnchor;
        delta.setX(qBound(-bounds.left(), delta.x(), width() - bounds.right()));
        delta.setY(qBound(-bounds.top(), delta.y(), height() - bounds.bottom()));
        m_cropPoints = m_cropGesturePoints.translated(delta);
        emit cropChanged();
        return;
    }
    if (m_tool == "perspectiveCrop" && m_cropHandle >= 0) {
        setCropCorner(m_cropHandle, point.x(), point.y());
        return;
    }
    QPointF fixed = m_cropAnchor;
    if (m_cropHandle >= 0 && m_cropHandle < 4)
        fixed = m_cropGesturePoints[(m_cropHandle + 2) % 4];
    const bool centered = m_toolAlternate && m_tool == "crop";
    if (centered && m_cropHandle >= 0)
        fixed = initial.center();
    QRectF rect(centered ? fixed - (point - fixed) : fixed, point);
    if (m_cropHandle >= 4) {
        rect = m_cropGesturePoints.boundingRect();
        if (m_cropHandle == 4)
            rect.setTop(qMin(point.y(), rect.bottom() - 1));
        if (m_cropHandle == 5)
            rect.setRight(qMax(point.x(), rect.left() + 1));
        if (m_cropHandle == 6)
            rect.setBottom(qMax(point.y(), rect.top() + 1));
        if (m_cropHandle == 7)
            rect.setLeft(qMin(point.x(), rect.right() - 1));
        if (centered) {
            if (m_cropHandle == 4)
                rect.setBottom(initial.center().y() + (initial.center().y() - rect.top()));
            if (m_cropHandle == 5)
                rect.setLeft(initial.center().x() - (rect.right() - initial.center().x()));
            if (m_cropHandle == 6)
                rect.setTop(initial.center().y() - (rect.bottom() - initial.center().y()));
            if (m_cropHandle == 7)
                rect.setRight(initial.center().x() + (initial.center().x() - rect.left()));
        }
        if (ratio > 0) {
            const bool vertical = m_cropHandle == 4 || m_cropHandle == 6;
            QSizeF     size     = rect.size();
            if (vertical)
                size.setWidth(size.height() * ratio);
            else
                size.setHeight(size.width() / ratio);
            const qreal fit = qMin(1., qMin(width() / size.width(), height() / size.height()));
            size *= fit;
            QPointF origin = rect.center() - QPointF(size.width(), size.height()) / 2;
            origin.setX(qBound(0., origin.x(), width() - size.width()));
            origin.setY(qBound(0., origin.y(), height() - size.height()));
            rect = QRectF(origin, size);
        }
    } else if (ratio > 0 && m_tool == "crop") {
        qreal w = qAbs(point.x() - fixed.x()), h = qAbs(point.y() - fixed.y());
        if (w / qMax(.001, h) > ratio)
            h = w / ratio;
        else
            w = h * ratio;
        const qreal availableW = centered                ? qMin(fixed.x(), width() - fixed.x())
                                 : point.x() < fixed.x() ? fixed.x()
                                                         : width() - fixed.x();
        const qreal availableH = centered                ? qMin(fixed.y(), height() - fixed.y())
                                 : point.y() < fixed.y() ? fixed.y()
                                                         : height() - fixed.y();
        const qreal fit = qMin(1., qMin(availableW / qMax(.001, w), availableH / qMax(.001, h)));
        point           = fixed + QPointF((point.x() < fixed.x() ? -w : w) * fit,
                                          (point.y() < fixed.y() ? -h : h) * fit);
        rect            = QRectF(centered ? fixed - (point - fixed) : fixed, point);
    }
    setCropRectangle(rect);
}
void Document::endCropGesture()
{
    m_cropDragging = false;
    m_cropGesturePoints.clear();
    emit cropChanged();
}
void Document::applyCrop()
{
    if (m_cropBusy || !hasCrop() || (m_tool != "crop" && m_tool != "perspectiveCrop"))
        return;
    endStroke();
    const QPolygonF points = m_cropPoints;
    int             w      = qMax(
        1, qRound((QLineF(points[0], points[1]).length() + QLineF(points[3], points[2]).length()) /
                  2));
    int h = qMax(
        1, qRound((QLineF(points[0], points[3]).length() + QLineF(points[1], points[2]).length()) /
                  2));
    if (m_cropOutputSize.width() > 0)
        w = m_cropOutputSize.width();
    if (m_cropOutputSize.height() > 0)
        h = m_cropOutputSize.height();
    if (!validSize(w, h, int(m_layers.size())))
        return;
    QTransform      transform;
    const QPolygonF target{QPointF(0, 0), QPointF(w, 0), QPointF(w, h), QPointF(0, h)};
    if (!QTransform::quadToQuad(points, target, transform))
        return;
    const QSize   outputSize(w, h);
    const QString label =
        m_tool == "perspectiveCrop" ? tr("Recadrer en perspective") : tr("Recadrer");
    const Snapshot before            = snapshot(label);
    const quint64  session           = m_documentSession;
    const quint64  contentGeneration = m_previewRequestGeneration;
    m_cropBusy                       = true;
    emit  cropChanged();
    emit  statusMessage(tr("Recadrage en cours…"));
    auto *watcher = new QFutureWatcher<CropResult>(this);
    connect(watcher, &QFutureWatcher<CropResult>::finished, this,
            [this, watcher, before, outputSize, session, label, contentGeneration] {
                CropResult result = watcher->result();
                watcher->deleteLater();
                // Discard work if an external caller edited or replaced the document meanwhile.
                if (result.valid && session == m_documentSession && before.id == m_stateId &&
                    contentGeneration == m_previewRequestGeneration && !m_drawing) {
                    m_layers = std::move(result.layers);
                    m_size   = outputSize;
                    m_slices.clear();
                    m_activeSlice = -1;
                    emit slicesChanged();
                    m_selection       = {};
                    m_selectionRegion = {};
                    m_selectionPath.clear();
                    m_selectionShape     = 0;
                    m_polygonLassoActive = false;
                    commit(before, label, true, false);
                    {
                        QMutexLocker lock(&m_previewImageMutex);
                        m_composite = result.composite;
                        m_previewComposite =
                            result.composite.scaled(m_previewTargetSize.boundedTo(outputSize),
                                                    Qt::KeepAspectRatio, Qt::FastTransformation);
                    }
                    m_cacheDirty = m_fullCompositeDirty = false;
                    m_dirtyRegion                       = {};
                    cancelCrop();
                    emit canvasSizeChanged();
                    emit selectionChanged();
                    emit documentChanged();
                    emit imageChanged();
                    emit statusMessage(tr("Recadrage appliqué"));
                } else if (!result.valid)
                    fail(tr("Impossible de calculer le recadrage."));
                m_cropBusy = false;
                emit cropChanged();
            });
    watcher->setFuture(QtConcurrent::run([layers = m_layers, transform, outputSize]() mutable {
        CropResult result;
        const auto warp = [&transform, outputSize, &result](QImage &_image) {
            if (_image.isNull())
                return;
            QImage output = ImageUtils::blank(outputSize, ImageUtils::workingFormat(_image));
            if (output.isNull()) {
                result.valid = false;
                return;
            }
            QPainter painter(&output);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.setWorldTransform(transform);
            painter.drawImage(QPointF{}, _image);
            painter.end();
            _image = std::move(output);
        };
        for (auto &layer : layers) {
            warp(layer.image);
            warp(layer.mask);
            warp(layer.source);
            if (!result.valid)
                return result;
        }
        result.composite = renderPreviewSnapshot(layers, outputSize, outputSize, false);
        result.valid     = !result.composite.isNull();
        result.layers    = std::move(layers);
        return result;
    }));
}
