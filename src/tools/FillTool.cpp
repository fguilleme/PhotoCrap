#include "Document.h"
#include "ImageUtils.h"

#include <QPainter>
#include <QtMath>

namespace
{
QColor maskColor(const QColor &_color)
{
    const int value = qGray(_color.rgb());
    return QColor(value, value, value);
}
} // namespace

void Document::floodFill(QPoint _point)
{
    QImage *paintImage = paintTarget();
    if (!paintImage)
        return;
    auto       &image  = *paintImage;
    const QRect bounds = hasSelection() ? m_selection : image.rect();
    if (!bounds.contains(_point))
        return;
    const QColor targetColor = image.pixelColor(_point);
    QImage       solid(1, 1, ImageUtils::workingFormat(image));
    solid.fill(m_editingMask ? maskColor(m_color) : m_color);
    const QColor replacement = solid.pixelColor(0, 0);
    if (targetColor == replacement && qFuzzyCompare(m_brushOpacity, 1.))
        return;
    // Scanline flood fill: visited pixels are kept separate so semi-transparent
    // replacement colors cannot cause pixels to be queued repeatedly.
    QImage mask(image.size(), QImage::Format_Grayscale8);
    mask.fill(0);
    QVector<QPoint> stack{_point};
    auto            matches = [&](int _x, int _y) {
        return (!hasSelection() || m_selectionRegion.contains(QPoint(_x, _y))) &&
               !mask.constScanLine(_y)[_x] && image.pixelColor(_x, _y) == targetColor;
    };
    while (!stack.isEmpty()) {
        auto seed = stack.takeLast();
        if (!matches(seed.x(), seed.y()))
            continue;
        int left = seed.x(), right = seed.x();
        while (left > bounds.left() && matches(left - 1, seed.y()))
            --left;
        while (right < bounds.right() && matches(right + 1, seed.y()))
            ++right;
        auto row = mask.scanLine(seed.y());
        for (int x = left; x <= right; ++x)
            row[x] = 255;
        for (int y : {seed.y() - 1, seed.y() + 1})
            if (y >= bounds.top() && y <= bounds.bottom()) {
                bool run = false;
                for (int x = left; x <= right; ++x) {
                    bool match = matches(x, y);
                    if (match && !run)
                        stack.append({x, y});
                    run = match;
                }
            }
    }
    QImage overlay = ImageUtils::blank(m_size, workingFormat());
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        auto selected = mask.constScanLine(y);
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            if (selected[x])
                overlay.setPixelColor(x, y, replacement);
    }
    SelectionCoverage::attenuate(overlay, m_selectionMask);
    QPainter painter(&image);
    painter.setOpacity(m_brushOpacity);
    painter.drawImage(0, 0, overlay);
    painter.end();
    m_strokeChanged = true;
}
