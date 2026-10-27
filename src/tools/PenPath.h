#pragma once
#include <QJsonArray>
#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QVector>

struct PenPoint {
    QPointF position, incoming, outgoing;
    bool    smooth                             = false;
    bool    operator==(const PenPoint &) const = default;
};

struct PenPath {
    QString           name;
    QVector<PenPoint> points;
    bool              closed = false;
    QPainterPath      painterPath() const;
    bool              operator==(const PenPath &) const = default;
};

namespace PenGeometry
{
constexpr int MaxPaths = 128, MaxPoints = 4096, MaxTotalPoints = 32768;
struct SegmentHit {
    int   index = -1;
    qreal t = 0, distance = 0;
};
SegmentHit nearestSegment(const PenPath &_path, const QPointF &_point, qreal _tolerance);
int        splitSegment(PenPath &_path, int _index, qreal _t);
QJsonArray encode(const QVector<PenPath> &_paths);
bool       decode(const QJsonArray &_array, QVector<PenPath> &_paths);
bool       transform(QVector<PenPath> &_paths, const QTransform &_transform);
} // namespace PenGeometry
