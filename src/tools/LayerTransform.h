#pragma once

#include "Layer.h"
#include <QHash>
#include <QPolygonF>
#include <QSet>
#include <QTransform>
#include <functional>

namespace PhotoCrap::Transform
{
struct Mesh {
    QVector<qreal>      columns, rows;
    QPolygonF           points;
    QSet<int>           corners;
    QHash<int, QPointF> tangents;
    // Direction indices: left, right, up, down.
    bool    hasHandle(int _point, int _direction) const;
    QPointF handlePoint(int _point, int _direction) const;
    void    moveHandle(int _point, int _direction, const QPointF &_position);
    QPointF derivative(int _point, int _direction) const;

    bool empty() const
    {
        return points.isEmpty();
    }
    void    reset(int _columns, int _rows);
    QPointF sample(qreal _u, qreal _v) const;
    void    split(const QPointF &_at, bool _horizontal, bool _vertical);
    void    remove(int _point);
    void    preset(const QString &_style, qreal _bend, bool _vertical);
};

struct Mapping {
    QRectF    source;
    QPolygonF quad;
    Mesh      mesh;
    bool      matrix(QTransform &_matrix) const;
    Mapping   scaled(const QSizeF &_scale) const;
    QPointF   warpPoint(const QPointF &_normalized) const;
    QPointF   sourcePoint(const QPointF &_normalized) const;
    QRect     destination(const QRect &_canvas) const;
    bool      unchanged() const;
};

struct Job {
    Mapping      mapping;
    QVector<int> layers;
    QImage       selection;
    int          path = -1;
    bool         mask = false, vectorMask = false, selectionOnly = false, copy = false;
    bool         maskView = false, allowWarp = true, allowProjective = true;
};

struct Session {
    Job              job;
    QString          mode;
    QPointF          reference{.5, .5};
    bool             linked = true, showReference = false, showGuides = true, vertical = false;
    QString          preset       = QStringLiteral("custom"), split;
    qreal            bend         = 50.;
    int              selectedNode = -1;
    int              handle       = -1;
    QPointF          start;
    Mapping          initial;
    QList<QPolygonF> selectionOutlines;
    QPointF          initialReference;
    QString          gesture;
};

QPolygonF corners(const QRectF &_rect);
QPointF   handle(const QPolygonF &_quad, int _index);
QPolygonF dragQuad(const QPolygonF &_quad, const QPointF &_start, const QPointF &_point,
                   const QPointF &_pivot, int _handle, const QString &_mode, bool _linked,
                   bool _shift, bool _alternate);
QRect     contentBounds(const PixelPlane &_plane, const QSize &_size);
bool      applyLayers(QVector<Layer> &_layers, const QSize &_size, const QSize &_nativeSize,
                      const Job &_job, bool _preview, const std::function<bool()> &_continue = {});
QImage    transformSelection(const QImage &_selection, const Mapping &_mapping);
} // namespace PhotoCrap::Transform
