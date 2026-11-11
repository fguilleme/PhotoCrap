#include "Document.h"

#include "DocumentStorage.h"
#include "LayerTransform.h"
#include "masks/VectorMask.h"
#include "shapes/ShapeLayer.h"
#include "text/TextLayer.h"

#include <QLineF>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

using namespace PhotoCrap;

namespace
{
QPointF pivot(const Transform::Session &_session)
{
    return _session.job.mapping.warpPoint(_session.reference);
}

bool pathTool(const QString &_tool)
{
    return _tool == "pen" || _tool == "freeformPen" || _tool == "curvaturePen" ||
           _tool == "pathSelection" || _tool == "directSelection";
}

QVariantList points(const QPolygonF &_polygon)
{
    QVariantList list;
    for (const auto &point : _polygon)
        list.append(point);
    return list;
}

qreal length(const QPointF &_point)
{
    return std::hypot(_point.x(), _point.y());
}

QPointF normalized(const QPointF &_point, const QRectF &_source)
{
    return {(_point.x() - _source.x()) / _source.width(),
            (_point.y() - _source.y()) / _source.height()};
}

bool meaningful(const QRectF &_bounds)
{
    return _bounds.width() >= 1. && _bounds.height() >= 1.;
}
} // namespace

bool Document::beginTransform(const QString &_mode, bool _copy, bool _selectionOnly)
{
    if (m_transform)
        return setTransformMode(_mode);
    if (m_size.isEmpty() || operationBusy() || pixelEditBusy() || selectionBusy() || rawLoading())
        return false;

    finishTextEdit();
    endPenGesture();
    endStroke();
    finishLayerPreview();
    cancelCrop();
    auto  session     = std::make_shared<Transform::Session>();
    auto &job         = session->job;
    job.selectionOnly = _selectionOnly;
    job.copy          = _copy;
    QRectF bounds;
    if (_selectionOnly) {
        if (!hasSelection())
            return false;
        job.selection = selectionMask();
        bounds        = m_selection;
    } else if (pathTool(m_tool) && !m_editingMask && m_activePenPath >= 0 &&
               m_activePenPath < m_penPaths.size() &&
               !m_penPaths[m_activePenPath].points.isEmpty() &&
               m_penPaths[m_activePenPath].shapeLayerId.isEmpty() &&
               m_penPaths[m_activePenPath].maskLayerId.isEmpty()) {
        job.path            = m_activePenPath;
        job.allowWarp       = false;
        job.allowProjective = false;
        bounds              = m_penPaths[job.path].painterPath().boundingRect();
        if (job.copy &&
            (m_penPaths.size() >= PenGeometry::MaxPaths ||
             m_penPaths[job.path].points.size() +
                     std::accumulate(m_penPaths.cbegin(), m_penPaths.cend(), qsizetype{},
                                     [](qsizetype count, const PenPath &path) {
                                         return count + path.points.size();
                                     }) >
                 PenGeometry::MaxTotalPoints))
            return fail(tr("La limite de tracés est atteinte."));
        // A straight path can still be moved, rotated and scaled.
        if (bounds.width() < 1.)
            bounds.adjust(-.5, 0., .5, 0.);
        if (bounds.height() < 1.)
            bounds.adjust(0., -.5, 0., .5);
    } else {
        if (m_active < 0 || m_active >= m_layers.size())
            return false;
        job.mask       = m_editingMask && !editingVectorMask();
        job.vectorMask = editingVectorMask();
        job.maskView   = m_maskOnlyView && (job.mask || job.vectorMask);
        if (job.copy && (job.mask || job.vectorMask))
            return fail(tr("Le masque ne peut pas être transformé en copie."));
        if (hasSelection() && !job.vectorMask)
            job.selection = selectionMask();
        QSet<int> selected = job.mask || job.vectorMask ? QSet<int>{m_active} : m_selectedLayers;
        if (selected.isEmpty())
            selected.insert(m_active);
        QSet<QString> groups;
        for (const int i : selected)
            if (i >= 0 && i < m_layers.size() && m_layers[i].kind == "group") {
                if (!penShapeEditable(i))
                    return false;
                groups.insert(m_layers[i].groupId);
            }
        for (int i = 0; i < m_layers.size(); ++i) {
            const auto &layer = m_layers[i];
            if ((!selected.contains(i) && !groups.contains(layer.groupId)) || layer.kind == "group")
                continue;
            if (!penShapeEditable(i))
                return false;
            if (!layer.source.isNull())
                return fail(tr(
                    "Convertissez la séparation de fréquences en pixels avant de la transformer."));
            if (!job.selection.isNull() && !job.mask && layer.kind != "pixel")
                return fail(tr("Désélectionnez les pixels pour transformer un calque éditable."));
            QRectF layerBounds;
            if (job.vectorMask) {
                layerBounds         = Masks::vectorPath(layer).painterPath().boundingRect();
                job.allowProjective = job.allowWarp = false;
            } else if (job.mask) {
                layerBounds = Transform::contentBounds(layer.mask, m_size);
            } else if (layer.kind == "shape") {
                layerBounds         = Shape::bounds(layer.parameters, m_size);
                job.allowProjective = job.allowWarp = false;
            } else if (layer.kind == "text") {
                layerBounds         = Text::bounds(layer.parameters, m_size);
                job.allowProjective = job.allowWarp = false;
            } else if (layer.model3D) {
                layerBounds         = Transform::contentBounds(layer.image, m_size);
                job.allowProjective = job.allowWarp = false;
            } else if (layer.kind == "pixel") {
                layerBounds = job.selection.isNull() ? Transform::contentBounds(layer.image, m_size)
                                                     : QRectF(m_selection);
            } else if (!layer.mask.isNull() && !layer.mask.isUniform()) {
                layerBounds = Transform::contentBounds(layer.mask, m_size);
            }
            if (!job.mask && Masks::hasVector(layer))
                job.allowProjective = job.allowWarp = false;
            if (layerBounds.isEmpty())
                continue;
            job.layers.append(i);
            bounds = bounds.united(layerBounds);
        }
        if (job.copy && !job.layers.isEmpty() &&
            !validSize(width(), height(), m_layers.size() + job.layers.size()))
            return false;
    }
    if ((!job.selectionOnly && job.path < 0 && job.layers.isEmpty()) || !meaningful(bounds))
        return fail(tr("Aucun contenu à transformer."));
    if ((_selectionOnly || (hasSelection() && job.path < 0 && !job.vectorMask)) &&
        job.selection.isNull())
        return fail(tr("Impossible de préparer la sélection."));
    if (!job.selection.isNull() && selectionOutline().rectCount() < 3000 &&
        m_selectionRegion.rectCount() < 3000) {
        QPainterPath selectionPath;
        selectionPath.addRegion(m_selectionRegion);
        session->selectionOutlines = selectionPath.simplified().toSubpathPolygons();
    }
    job.mapping.source = bounds;
    job.mapping.quad   = Transform::corners(bounds);
    session->mode      = "free";
    {
        QMutexLocker locker(&m_previewImageMutex);
        m_beforeTransformPreview = m_previewComposite;
    }
    m_transform = std::move(session);
    if (!setTransformMode(_mode)) {
        cancelTransform();
        return false;
    }
    return true;
}

bool Document::setTransformMode(const QString &_mode)
{
    if (!m_transform)
        return false;
    auto &s = *m_transform;
    if (_mode != "free" && _mode != "scale" && _mode != "rotate" && _mode != "skew" &&
        _mode != "distort" && _mode != "perspective" && _mode != "warp")
        return false;
    if ((_mode == "warp" && !s.job.allowWarp) ||
        ((_mode == "distort" || _mode == "perspective") && !s.job.allowProjective))
        return fail(tr("Ce mode de transformation est réservé aux pixels et aux masques raster."));
    if (_mode == "warp") {
        QTransform matrix;
        if (!s.job.mapping.matrix(matrix) || !matrix.isAffine())
            return fail(tr("Validez la perspective avant de passer à la déformation."));
        if (s.job.mapping.mesh.empty())
            s.job.mapping.mesh.reset(1, 1);
    }
    s.mode = _mode;
    s.gesture.clear();
    s.split.clear();
    emit transformChanged();
    return true;
}

QVariantMap Document::transformOptions() const
{
    if (!m_transform)
        return {};
    const auto &s    = *m_transform;
    const auto &m    = s.job.mapping;
    const auto &mesh = m.mesh;
    const auto  x = m.quad[1] - m.quad[0], y = m.quad[3] - m.quad[0];
    const auto  reference = pivot(s);
    QVariantMap result{{"mode", s.mode},
                       {"quad", points(m.quad)},
                       {"pivot", reference},
                       {"x", reference.x()},
                       {"y", reference.y()},
                       {"width", 100. * length(x) / m.source.width()},
                       {"height", 100. * length(y) / m.source.height()},
                       {"angle", qRadiansToDegrees(std::atan2(x.y(), x.x()))},
                       {"linked", s.linked},
                       {"showReference", s.showReference},
                       {"reference", s.reference},
                       {"showGuides", s.showGuides},
                       {"allowWarp", s.job.allowWarp},
                       {"allowProjective", s.job.allowProjective},
                       {"preset", s.preset},
                       {"bend", s.bend},
                       {"vertical", s.vertical},
                       {"split", s.split},
                       {"node", s.selectedNode},
                       {"columns", mesh.columns.size() - 1},
                       {"rows", mesh.rows.size() - 1}};
    if (s.mode == "warp") {
        QPolygonF    nodes;
        QVariantList guides;
        for (const auto &point : mesh.points)
            nodes.append(m.warpPoint(point));
        result.insert("nodes", points(nodes));
        QVariantList handles;
        for (int direction = 0; direction < 4; ++direction)
            if (mesh.hasHandle(s.selectedNode, direction))
                handles.append(QVariantMap{
                    {"direction", direction},
                    {"point", m.warpPoint(mesh.handlePoint(s.selectedNode, direction))}});
        result.insert("handles", handles);
        for (const bool horizontal : {true, false})
            for (const qreal v : horizontal ? mesh.rows : mesh.columns) {
                QPolygonF guide;
                for (int i = 0; i <= 64; ++i) {
                    const qreal u = i / 64.;
                    guide.append(m.warpPoint(horizontal ? mesh.sample(u, v) : mesh.sample(v, u)));
                }
                guides.append(QVariant(points(guide)));
            }
        result.insert("guides", guides);
    }
    QTransform transform;
    if (s.job.path >= 0 && m.matrix(transform)) {
        QVariantList outlines;
        for (const auto &polygon :
             transform.map(m_penPaths[s.job.path].painterPath()).toSubpathPolygons())
            outlines.append(QVariant(points(polygon)));
        result.insert("outlines", outlines);
    } else if (!s.job.selection.isNull() && m.matrix(transform)) {
        // A bounded contour for large/fragmented selections. The applied selection
        // uses the original coverage, including antialiasing and feathering.
        QVariantList outlines;
        for (const auto &polygon : s.selectionOutlines) {
            QPolygonF mapped;
            for (const auto &point : polygon) {
                const auto n = normalized(point, m.source);
                mapped.append(m.warpPoint(mesh.sample(n.x(), n.y())));
            }
            outlines.append(QVariant(points(mapped)));
        }
        result.insert("outlines", outlines);
    }
    return result;
}

QString Document::transformHit(const QPointF &_point, qreal _tolerance) const
{
    if (!m_transform)
        return {};
    const auto &s = *m_transform;
    const auto &m = s.job.mapping;
    if (s.mode == "warp") {
        if (!s.split.isEmpty())
            return "split";
        for (int direction = 0; direction < 4; ++direction)
            if (m.mesh.hasHandle(s.selectedNode, direction) &&
                length(_point - m.warpPoint(m.mesh.handlePoint(s.selectedNode, direction))) <
                    _tolerance)
                return QStringLiteral("tangent:%1").arg(direction);
        int   hit  = -1;
        qreal best = _tolerance;
        for (int i = 0; i < m.mesh.points.size(); ++i) {
            const auto distance = length(_point - m.warpPoint(m.mesh.points[i]));
            if (distance <= best) {
                best = distance;
                hit  = i;
            }
        }
        return hit >= 0 ? QStringLiteral("node:%1").arg(hit) : QStringLiteral("mesh");
    }
    if (s.showReference && length(_point - pivot(s)) < _tolerance)
        return "pivot";
    if (s.mode != "rotate")
        for (int i = 0; i < 8; ++i)
            if ((s.mode != "skew" || i >= 4) && (s.mode != "perspective" || i < 4) &&
                length(_point - Transform::handle(m.quad, i)) < _tolerance)
                return QStringLiteral("handle:%1").arg(i);
    if (m.quad.containsPoint(_point, Qt::OddEvenFill))
        return s.mode == "rotate" ? QStringLiteral("rotate") : QStringLiteral("move");
    return "rotate";
}

void Document::beginTransformGesture(const QPointF &_point, qreal _tolerance, int _modifiers)
{
    if (!m_transform)
        return;
    auto &        s   = *m_transform;
    auto &        m   = s.job.mapping;
    const QString hit = transformHit(_point, _tolerance);
    if (hit == "split") {
        QTransform transform;
        if (!m.matrix(transform))
            return;
        const auto at = normalized(transform.inverted().map(_point), m.source);
        if (at.x() < 0. || at.x() > 1. || at.y() < 0. || at.y() > 1.)
            return;
        m.mesh.split(at, s.split != "vertical", s.split != "horizontal");
        s.selectedNode = -1;
        s.split.clear();
        emit transformChanged();
        return;
    }
    s.start            = _point;
    s.initial          = m;
    s.initialReference = s.reference;
    s.handle           = -1;
    s.gesture          = hit;
    if (hit.startsWith("node:")) {
        s.selectedNode = hit.section(':', 1).toInt();
        s.gesture      = "node";
        emit transformChanged();
    } else if (hit.startsWith("tangent:")) {
        s.handle  = hit.section(':', 1).toInt();
        s.gesture = "tangent";
    } else if (hit == "mesh") {
        // Dragging within a cell moves its four surrounding grid anchors.
        if (!m.quad.containsPoint(_point, Qt::OddEvenFill))
            s.gesture.clear();
    } else if (hit.startsWith("handle:")) {
        s.handle  = hit.section(':', 1).toInt();
        s.gesture = s.mode == "free" ? QStringLiteral("scale") : s.mode;
    }
    updateTransformGesture(_point, _modifiers);
}

void Document::updateTransformGesture(const QPointF &_point, int _modifiers)
{
    if (!m_transform || m_transform->gesture.isEmpty())
        return;
    auto &     s         = *m_transform;
    auto &     m         = s.job.mapping;
    auto       candidate = s.initial;
    const bool shift     = _modifiers & Qt::ShiftModifier;
    const bool control   = _modifiers & Qt::ControlModifier;
    const bool alternate = _modifiers & Qt::AltModifier;
    QTransform matrix;
    if (!candidate.matrix(matrix))
        return;
    const auto inverse = matrix.inverted();
    const auto at      = normalized(inverse.map(_point), candidate.source);
    if (s.gesture == "pivot") {
        s.reference = at;
        emit transformChanged();
        return;
    }
    if (s.gesture == "node" || s.gesture == "mesh" || s.gesture == "tangent") {
        auto &     mesh   = candidate.mesh;
        const auto origin = normalized(inverse.map(s.start), candidate.source);
        const auto delta  = at - origin;
        if (s.gesture == "tangent" && s.selectedNode >= 0) {
            if (!qFuzzyIsNull(length(delta)))
                mesh.moveHandle(s.selectedNode, s.handle,
                                mesh.handlePoint(s.selectedNode, s.handle) + delta);
        } else if (s.gesture == "node" && s.selectedNode >= 0)
            mesh.points[s.selectedNode] += delta;
        else {
            const auto segment = [](const auto &axis, qreal v) {
                return qBound(0,
                              int(std::upper_bound(axis.begin(), axis.end(), v) - axis.begin()) - 1,
                              int(axis.size()) - 2);
            };
            const int x = segment(mesh.columns, origin.x()), y = segment(mesh.rows, origin.y());
            for (int row = y; row <= y + 1; ++row)
                for (int col = x; col <= x + 1; ++col)
                    mesh.points[row * mesh.columns.size() + col] += delta;
        }
        s.preset = "custom";
    } else {
        QString mode = s.gesture;
        if (s.mode == "free" && s.handle >= 0 && control) {
            if (s.handle >= 4 && shift)
                mode = "skew";
            else if (s.job.allowProjective)
                mode = shift && alternate && s.handle < 4 ? "perspective" : "distort";
        }
        if (mode == "rotate" && length(s.start - s.initial.warpPoint(s.reference)) < .01)
            return;
        candidate.quad =
            Transform::dragQuad(candidate.quad, s.start, _point, candidate.warpPoint(s.reference),
                                s.handle, mode, s.linked, shift, alternate);
    }
    if (candidate.quad == m.quad && candidate.mesh.points == m.mesh.points &&
        candidate.mesh.tangents == m.mesh.tangents)
        return;
    if (candidate.matrix(matrix) && (candidate.mesh.empty() || matrix.isAffine())) {
        m = std::move(candidate);
        updateTransformPreview();
    }
}

void Document::endTransformGesture()
{
    if (m_transform)
        m_transform->gesture.clear();
}

void Document::setTransformParameter(const QString &_key, const QVariant &_value)
{
    if (!m_transform)
        return;
    auto &s = *m_transform;
    auto &m = s.job.mapping;
    if (_key == "linked")
        s.linked = _value.toBool();
    else if (_key == "showReference")
        s.showReference = _value.toBool();
    else if (_key == "reference") {
        const auto p = _value.toPointF();
        if (std::isfinite(p.x()) && std::isfinite(p.y()))
            s.reference = p;
    } else if (_key == "showGuides")
        s.showGuides = _value.toBool();
    else if (_key == "split" && s.mode == "warp")
        s.split = _value.toString();
    else if (_key == "grid" && s.mode == "warp") {
        const auto size = _value.toPoint();
        m.mesh.reset(size.x(), size.y());
        s.selectedNode = -1;
        s.preset       = "custom";
        updateTransformPreview();
        return;
    } else if (_key == "preset" || _key == "bend" || _key == "vertical") {
        if (s.mode != "warp")
            return;
        if (_key == "preset")
            s.preset = _value.toString();
        if (_key == "bend") {
            if (!std::isfinite(_value.toDouble()))
                return;
            s.bend = qBound(-100., _value.toDouble(), 100.);
        }
        if (_key == "vertical")
            s.vertical = _value.toBool();
        if (s.preset != "custom") {
            m.mesh.preset(s.preset, s.bend, s.vertical);
            s.selectedNode = -1;
            updateTransformPreview();
            return;
        }
    } else {
        bool        ok    = false;
        const qreal value = _value.toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return;
        const auto p         = pivot(s);
        auto       candidate = m;
        if (_key == "x" || _key == "y") {
            const auto delta =
                _key == "x" ? QPointF(value - p.x(), 0.) : QPointF(0., value - p.y());
            candidate.quad.translate(delta);
        } else if (_key == "angle") {
            const auto  x   = m.quad[1] - m.quad[0];
            const qreal old = qRadiansToDegrees(std::atan2(x.y(), x.x()));
            QTransform  rotation;
            rotation.translate(p.x(), p.y()).rotate(value - old).translate(-p.x(), -p.y());
            candidate.quad = rotation.map(m.quad);
        } else if (_key == "width" || _key == "height") {
            const auto       x = m.quad[1] - m.quad[0], y = m.quad[3] - m.quad[0];
            const qreal      old    = _key == "width" ? 100. * length(x) / m.source.width()
                                                      : 100. * length(y) / m.source.height();
            const qreal      factor = value / old;
            const QTransform axes(x.x(), x.y(), y.x(), y.y(), p.x(), p.y());
            const auto       inv = axes.inverted();
            const auto scale     = QTransform::fromScale(s.linked || _key == "width" ? factor : 1.,
                                                     s.linked || _key == "height" ? factor : 1.);
            candidate.quad = (inv * scale * axes).map(m.quad);
        } else {
            return;
        }
        QTransform matrix;
        if (candidate.matrix(matrix)) {
            m = std::move(candidate);
            updateTransformPreview();
        }
        return;
    }
    emit transformChanged();
}

void Document::transformCommand(const QString &_command)
{
    if (_command == "corner" || _command == "removeSplit") {
        if (!m_transform || m_transform->mode != "warp" || m_transform->selectedNode < 0)
            return;
        auto &s    = *m_transform;
        auto &mesh = s.job.mapping.mesh;
        if (_command == "removeSplit") {
            mesh.remove(s.selectedNode);
            s.selectedNode = -1;
        } else {
            for (int direction = 0; direction < 4; ++direction)
                mesh.tangents.remove(s.selectedNode * 4 + direction);
            if (!mesh.corners.remove(s.selectedNode))
                mesh.corners.insert(s.selectedNode);
        }
        s.preset = "custom";
        updateTransformPreview();
        return;
    }
    const bool ownSession = !m_transform;
    if (ownSession && !beginTransform())
        return;
    auto &     s = *m_transform;
    auto &     m = s.job.mapping;
    QTransform transform;
    const auto p = pivot(s);
    transform.translate(p.x(), p.y());
    if (_command == "flipX")
        transform.scale(-1, 1);
    else if (_command == "flipY")
        transform.scale(1, -1);
    else if (_command == "rotate180")
        transform.rotate(180);
    else if (_command == "rotateCW")
        transform.rotate(90);
    else if (_command == "rotateCCW")
        transform.rotate(-90);
    else {
        if (ownSession)
            cancelTransform();
        return;
    }
    transform.translate(-p.x(), -p.y());
    m.quad = transform.map(m.quad);
    if (ownSession)
        applyTransform();
    else
        updateTransformPreview();
}

void Document::updateTransformPreview()
{
    if (!m_transform)
        return;
    emit        transformChanged();
    const auto &job = m_transform->job;
    if (job.path < 0 && !job.selectionOnly)
        requestPreviewRender(true, false, false);
}

void Document::clearTransformPreview()
{
    ++m_previewRequestGeneration;
    m_pendingTransform.reset();
    m_previewRenderPending = false;
    m_previewRenderActive.store(false);
    QMutexLocker locker(&m_previewImageMutex);
    m_previewComposite = {};
}

void Document::cancelTransform()
{
    if (!m_transform)
        return;
    m_transform.reset();
    clearTransformPreview();
    {
        QMutexLocker locker(&m_previewImageMutex);
        m_previewComposite = std::exchange(m_beforeTransformPreview, {});
    }
    emit transformChanged();
    emit imageChanged();
    emit gpuStackChanged();
}

bool Document::applyTransformJob(const Transform::Job &_job, const QString &_label)
{
    const auto before = snapshot(_label);
    if (_job.path >= 0) {
        if (_job.path >= m_penPaths.size())
            return false;
        QTransform       matrix;
        QVector<PenPath> path{m_penPaths[_job.path]};
        if (!_job.mapping.matrix(matrix) || !PenGeometry::transform(path, matrix))
            return fail(tr("Impossible de transformer le tracé."));
        if (_job.copy) {
            path[0].name    = tr("%1 — copie").arg(path[0].name);
            m_activePenPath = m_penPaths.size();
            m_penPaths.append(path[0]);
        } else {
            m_penPaths[_job.path] = path[0];
        }
        commitPenEdit(before, _label);
        return true;
    }
    auto layers = m_layers;
    if (!_job.selectionOnly && !Transform::applyLayers(layers, m_size, m_size, _job, false, [this] {
            return !m_backgroundCancel || !m_backgroundCancel->load();
        }))
        return fail(tr("Impossible de transformer les calques."));
    if (!_job.selectionOnly) {
        int colorPlanes = 0, maskPlanes = 0;
        for (const auto &layer : std::as_const(layers)) {
            colorPlanes += (!layer.image.isNull() && layer.image.size() != QSize(1, 1)) +
                           !layer.source.isNull();
            maskPlanes += !layer.mask.isNull() && layer.mask.size() != QSize(1, 1);
        }
        if (!validStorageBytes(DocumentStorage::estimateBytes(width(), height(), colorPlanes,
                                                              maskPlanes, channelBits())))
            return false;
    }
    SelectionCoverage::Result coverage;
    if (!_job.selection.isNull()) {
        auto mask = Transform::transformSelection(_job.selection, _job.mapping);
        if (mask.isNull())
            return fail(tr("Impossible de transformer la sélection."));
        const auto cancel =
            m_backgroundCancel ? m_backgroundCancel : std::make_shared<std::atomic_bool>(false);
        coverage = SelectionCoverage::describe(std::move(mask), cancel);
        if (!coverage.valid)
            return cancel->load() ? false : fail(tr("Impossible de transformer la sélection."));
    }
    if (m_backgroundCancel && m_backgroundCancel->load())
        return false;
    if (!_job.selection.isNull()) {
        applySelectionCoverage(coverage);
        emit selectionChanged();
    }
    if (_job.selectionOnly) {
        commitSelectionEdit(before, _label);
        return true;
    }
    if (_job.copy) {
        QSet<int> selected;
        int       shift  = 0;
        int       active = -1;
        for (const int i : _job.layers) {
            const int inserted    = i + (++shift);
            layers[inserted].name = tr("%1 — copie").arg(layers[inserted].name);
            selected.insert(inserted);
            if (i == m_active)
                active = inserted;
        }
        m_active = active >= 0 ? active : *std::min_element(selected.begin(), selected.end());
        m_selectedLayers       = selected;
        m_layerSelectionAnchor = m_active;
    }
    m_layers      = std::move(layers);
    m_maskPenBase = {};
    syncPenShapePath(true);
    if (!m_backgroundWorker)
        commit(before, _label);
    return true;
}

bool Document::applyTransform()
{
    if (!m_transform)
        return false;
    auto job = std::move(m_transform->job);
    if (job.mapping.unchanged()) {
        cancelTransform();
        return true;
    }
    m_transform.reset();
    // Keep the last displayed proxy while the native pixels are processed.
    ++m_previewRequestGeneration;
    m_pendingTransform.reset();
    m_previewRenderPending = false;
    m_previewRenderActive.store(false);
    emit        transformChanged();
    const auto  label = job.selectionOnly ? tr("Transformer la sélection") : tr("Transformation");
    const QRect canvas(QPoint{}, m_size);
    const auto  source      = job.mapping.source.toAlignedRect().intersected(canvas);
    const auto  destination = job.mapping.destination(canvas);
    const auto  area = [](const QRect &rect) { return qint64(rect.width()) * rect.height(); };
    const auto  cost = qMax(area(source), area(destination)) * qMax(1, int(job.layers.size())) *
                          (job.mapping.mesh.empty() ? 1 : 4) +
                      (job.selection.isNull() ? 0 : qint64(width()) * height());
    const auto remember = [this, mapping = job.mapping] {
        m_beforeTransformPreview = {};
        m_lastTransform          = std::make_shared<Transform::Mapping>(mapping);
        emit transformChanged();
    };
    if (job.selectionOnly && cost >= m_backgroundEditThreshold) {
        auto       coverage = std::make_shared<SelectionCoverage::Result>();
        const auto before   = snapshot(label);
        if (deferOperation(
                label, cost,
                [job, coverage](Document &worker) {
                    auto mask = Transform::transformSelection(job.selection, job.mapping);
                    if (mask.isNull()) {
                        worker.fail(tr("Impossible de transformer la sélection."));
                        return;
                    }
                    *coverage =
                        SelectionCoverage::describe(std::move(mask), worker.m_backgroundCancel);
                },
                false,
                [this, coverage, before, label] {
                    if (!coverage->valid)
                        return;
                    applySelectionCoverage(*coverage);
                    emit selectionChanged();
                    commitSelectionEdit(before, label);
                    m_beforeTransformPreview = {};
                },
                false))
            return true;
    }
    if (job.path < 0 && !job.selectionOnly && cost >= m_backgroundEditThreshold &&
        !m_backgroundWorker) {
        const QImage displayed = [this] {
            QMutexLocker locker(&m_previewImageMutex);
            return m_previewComposite;
        }();
        const auto oldState   = m_stateId;
        const auto oldSession = m_documentSession;
        auto       connection = std::make_shared<QMetaObject::Connection>();
        *connection           = connect(this, &Document::operationChanged, this,
                              [this, connection, oldState, oldSession, remember, displayed, label] {
                                  if (operationBusy())
                                      return;
                                  disconnect(*connection);
                                  if (m_documentSession != oldSession)
                                      return;
                                  if (!m_undo.isEmpty() && m_undo.constLast().id == oldState &&
                                      m_undo.constLast().label == label) {
                                      remember();
                                      if (!m_maskOnlyView) {
                                          if (!displayed.isNull()) {
                                              QMutexLocker locker(&m_previewImageMutex);
                                              m_previewComposite = displayed;
                                          }
                                          requestPreviewRender(false, false, false);
                                      }
                                  } else if (m_stateId == oldState) {
                                      clearTransformPreview();
                                      {
                                          QMutexLocker locker(&m_previewImageMutex);
                                          m_previewComposite =
                                              std::exchange(m_beforeTransformPreview, {});
                                      }
                                      emit imageChanged();
                                      emit gpuStackChanged();
                                  } else {
                                      m_beforeTransformPreview = {};
                                  }
                              });
        if (deferEdit(label, cost,
                      [job, label](Document &worker) { worker.applyTransformJob(job, label); }))
            return true;
        disconnect(*connection);
    }
    const bool applied = applyTransformJob(job, label);
    clearTransformPreview();
    emit imageChanged();
    emit gpuStackChanged();
    if (applied && !job.selectionOnly)
        remember();
    else
        m_beforeTransformPreview = {};
    return applied;
}

bool Document::repeatTransform(bool _copy)
{
    if (!m_lastTransform || m_transform || !beginTransform("free", _copy))
        return false;
    const auto &previous = *m_lastTransform;
    auto &      job      = m_transform->job;
    QTransform  matrix;
    if ((!job.allowWarp && !previous.mesh.empty()) || !previous.matrix(matrix) ||
        (!job.allowProjective && !matrix.isAffine())) {
        cancelTransform();
        return false;
    }
    const auto offset    = job.mapping.source.center() - previous.source.center();
    const auto translate = QTransform::fromTranslate(offset.x(), offset.y());
    job.mapping.quad     = (translate.inverted() * matrix * translate).map(job.mapping.quad);
    job.mapping.mesh     = previous.mesh;
    return applyTransform();
}
