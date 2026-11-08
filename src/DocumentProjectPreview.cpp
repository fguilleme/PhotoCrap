#include "Document.h"
#include "ImageUtils.h"
#include <cmath>

QImage Document::projectPreview() const
{
    QSize target = m_size.scaled(1280, 1280, Qt::KeepAspectRatio).boundedTo(m_size);
    if (target.isEmpty())
        return {};
    if (!m_cacheDirty && !m_fullCompositeDirty && !m_composite.isNull())
        return m_composite.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // Bound additional dense proxy planes; uniform backgrounds remain logical.
    qint64 bytesPerPixel = 0;
    for (const auto &layer : m_layers)
        for (const auto *plane : {&layer.image, &layer.mask, &layer.source})
            if (!plane->isNull() && !ImageUtils::compact(*plane))
                bytesPerPixel += plane->format() == QImage::Format_Grayscale8 ? 1 : 8;
    constexpr qint64 budget = 64 * 1024 * 1024;
    if (bytesPerPixel && qint64(target.width()) * target.height() > budget / bytesPerPixel) {
        const double scale =
            std::sqrt(double(budget) / bytesPerPixel / target.width() / target.height());
        target = QSize(qMax(1, int(target.width() * scale)), qMax(1, int(target.height() * scale)));
    }
    auto layers = m_layers;
    for (auto &layer : layers)
        for (auto *plane : {&layer.image, &layer.mask, &layer.source})
            if (!plane->isNull() && !ImageUtils::compact(*plane))
                *plane = plane->scaled(target, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    return renderPreviewSnapshot(std::move(layers), m_size, target, true, m_scratchCache);
}
