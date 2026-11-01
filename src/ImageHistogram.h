#pragma once
#include <QImage>
#include <QVariantList>
#include <QVariantMap>

namespace PhotoCrap
{
// Gather the exact legacy grid before asking Qt to convert pixel formats.
QImage histogramSamples(const QImage &_source, int _stride);

// GUI-thread cache: retain only normalized bins, never the source image.
class ImageHistogramCache
{
public:
    QVariantList luminance(const QImage &_source);
    QVariantMap  color(const QImage &_source);

private:
    struct Key {
        qint64         pixels = 0;
        QSize          size;
        QImage::Format format                        = QImage::Format_Invalid;
        bool           operator==(const Key &) const = default;
    };
    static Key   key(const QImage &_source);
    bool         m_hasLuminance = false;
    bool         m_hasColor     = false;
    Key          m_luminanceKey;
    Key          m_colorKey;
    QVariantList m_luminance;
    QVariantMap  m_color;
};
} // namespace PhotoCrap
