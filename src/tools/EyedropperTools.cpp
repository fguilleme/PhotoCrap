#include "Document.h"

#include <QMutexLocker>
#include <QtMath>
#include <algorithm>

void Document::setEyedropperSize(int _value)
{
    static constexpr int sizes[]{1, 3, 5, 11, 31, 51, 101};
    m_eyedropperSize =
        *std::min_element(std::begin(sizes), std::end(sizes), [_value](int _a, int _b) {
            return qAbs(_a - qBound(1, _value, 101)) < qAbs(_b - qBound(1, _value, 101));
        });
    emit settingsChanged();
}
void Document::setEyedropperSource(int _value)
{
    m_eyedropperSource = qBound(0, _value, 1);
    emit settingsChanged();
}
void Document::setEyedropperRing(bool _value)
{
    m_eyedropperRing = _value;
    emit settingsChanged();
}
QColor Document::sampleToolColor(int _x, int _y) const
{
    if (_x < 0 || _y < 0 || _x >= m_size.width() || _y >= m_size.height())
        return {};
    QImage image;
    if (m_eyedropperSource == 1) {
        if (m_active < 0 || m_active >= m_layers.size())
            return {};
        image = m_editingMask ? m_layers[m_active].mask : m_layers[m_active].image;
    } else {
        // Sample the latest finished colour render. Channel visualizations and
        // selection/cursor overlays are deliberately excluded. No full render
        // is launched on mouse movement while a preview is computing.
        QMutexLocker lock(&m_previewImageMutex);
        image = !m_cacheDirty && m_composite.size() == m_size ? m_composite
                : m_previewComposite.isNull()                 ? m_composite
                                                              : m_previewComposite;
        if (!m_previewRenderActive.load() && m_cacheDirty && m_previewComposite.isNull()) {
            lock.unlock();
            image = composite();
        }
    }
    if (image.isNull())
        return {};
    const int    radius = m_eyedropperSize / 2;
    const QRect  requested(_x - radius, _y - radius, m_eyedropperSize, m_eyedropperSize);
    const QRect  clipped = requested.intersected(QRect(QPoint{}, m_size));
    const qreal  sx      = qreal(image.width()) / m_size.width(),
                 sy      = qreal(image.height()) / m_size.height();
    const QRect  area(QPoint(qFloor(clipped.left() * sx), qFloor(clipped.top() * sy)),
                      QPoint(qFloor(clipped.right() * sx), qFloor(clipped.bottom() * sy)));
    const QImage sample = image.copy(area.intersected(image.rect()))
                              .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (sample.isNull())
        return {};
    quint64 red = 0, green = 0, blue = 0, alpha = 0;
    for (int y = 0; y < sample.height(); ++y) {
        const auto *pixels = reinterpret_cast<const QRgb *>(sample.constScanLine(y));
        for (int x = 0; x < sample.width(); ++x) {
            const QRgb pixel = pixels[x];
            red += qRed(pixel);
            green += qGreen(pixel);
            blue += qBlue(pixel);
            alpha += qAlpha(pixel);
        }
    }
    const quint64 count = quint64(sample.width()) * sample.height();
    if (!alpha)
        return QColor(0, 0, 0, 0);
    return QColor(int((red * 255 + alpha / 2) / alpha), int((green * 255 + alpha / 2) / alpha),
                  int((blue * 255 + alpha / 2) / alpha), int((alpha + count / 2) / count));
}
void Document::pickToolColor(int _x, int _y, bool _background)
{
    const QColor sample = sampleToolColor(_x, _y);
    if (!sample.isValid())
        return;
    if (_background)
        setSecondaryColor(sample);
    else
        setColor(sample);
}
