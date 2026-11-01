#pragma once
#include <QDebug>
#include <QImage>
#include <QRect>
#include <QString>

// Compare the independent raster reference with scene-graph output. GPU paths
// are antialiased once; rotated Canvas textures are antialiased then filtered.
inline bool compareOverlayReference(const QImage &_actual, const QImage &_reference, QRect _bounds,
                                    bool _analytic, const QString &_prefix)
{
    if (_actual.isNull() || _actual.size() != _reference.size())
        return false;
    _bounds = _bounds.intersected(_actual.rect());
    if (_bounds.isEmpty())
        return false;
    const auto filtered = [&](const QImage &_image, int _x, int _y) {
        int rgb[3] = {0, 0, 0}, count = 0;
        for (int y = qMax(_bounds.top(), _y - 2); y <= qMin(_bounds.bottom(), _y + 2); ++y)
            for (int x = qMax(_bounds.left(), _x - 2); x <= qMin(_bounds.right(), _x + 2); ++x) {
                const auto c = _image.pixelColor(x, y);
                rgb[0] += c.red();
                rgb[1] += c.green();
                rgb[2] += c.blue();
                ++count;
            }
        return QColor(rgb[0] / count, rgb[1] / count, rgb[2] / count);
    };
    const auto near = [](QColor _a, QColor _b, int _tolerance) {
        return qAbs(_a.red() - _b.red()) <= _tolerance &&
               qAbs(_a.green() - _b.green()) <= _tolerance &&
               qAbs(_a.blue() - _b.blue()) <= _tolerance;
    };
    for (int y = _bounds.top(); y <= _bounds.bottom(); ++y)
        for (int x = _bounds.left(); x <= _bounds.right(); ++x) {
            const auto a = _actual.pixelColor(x, y), b = _reference.pixelColor(x, y);
            if (near(a, b, 4) ||
                (_analytic && near(filtered(_actual, x, y), filtered(_reference, x, y), 12)))
                continue;
            qWarning() << "Overlay reference difference" << _prefix << QPoint(x, y) << a << b;
            _actual.save(_prefix + "-actual.png");
            _reference.save(_prefix + "-reference.png");
            return false;
        }
    return true;
}
