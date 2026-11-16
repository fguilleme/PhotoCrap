#pragma once

#include <QByteArray>
#include <QList>
#include <QVector4D>

namespace PhotoCrap::Model3D
{
struct PaintPicking;
struct PaintDab {
    QVector4D centerRadius;
    QVector4D normalHardness;
    QVector4D colorOpacity;
};

using Paint                       = QList<PaintDab>;
constexpr int       MaxPaintDabs  = 8192;
constexpr qsizetype MaxPaintBytes = MaxPaintDabs * 12 * 4;

QByteArray encodePaint(const Paint &paint);
bool       decodePaint(const QByteArray &bytes, Paint &paint);
} // namespace PhotoCrap::Model3D
