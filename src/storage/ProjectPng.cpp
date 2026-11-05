#include "ProjectPng.h"
#include "PixelPlane.h"
#include "ProjectPngCodec.h"

#include <QIODevice>

namespace PhotoCrap::Storage
{
bool writeProjectPng(QIODevice &_output, const PixelPlane &_plane,
                     const std::function<bool()> &_keepGoing)
{
    const auto format = _plane.format();
    const bool gray   = format == QImage::Format_Grayscale8;
    const bool wide   = format == QImage::Format_RGBA64_Premultiplied;
    if (_plane.isNull() || (!gray && !wide && format != QImage::Format_ARGB32_Premultiplied))
        return false;
    const int bandHeight = qBound(1,
                                  4 * 1024 * 1024 /
                                      (_plane.width() * (gray   ? 1
                                                         : wide ? 8
                                                                : 4)),
                                  TiledImage::TileSize);
    struct State {
        QIODevice                   &output;
        const PixelPlane            &plane;
        const std::function<bool()> &keepGoing;
        int                          bandHeight;
        QImage::Format               format;
        QImage                       band;
        int                          top = -1;

        bool load(int _top)
        {
            if (keepGoing && !keepGoing())
                return false;
            const QRect region(0, _top, plane.width(), qMin(bandHeight, plane.height() - _top));
            band = plane.region(region).convertToFormat(format);
            top  = _top;
            return band.size() == region.size();
        }
    } state{_output, _plane, _keepGoing, bandHeight,
            gray   ? QImage::Format_Grayscale8
            : wide ? QImage::Format_RGBA64
                   : QImage::Format_RGBA8888};
    if (!state.load(0))
        return false;
    const ProjectPngImage image{unsigned(_plane.width()),
                                unsigned(_plane.height()),
                                wide ? 16 : 8,
                                gray,
                                Q_BYTE_ORDER == Q_LITTLE_ENDIAN,
                                _plane.colorSpace() == QColorSpace(QColorSpace::SRgb),
                                unsigned(qMax(0, state.band.dotsPerMeterX())),
                                unsigned(qMax(0, state.band.dotsPerMeterY()))};
    return projectPngWrite(
        &image, &state,
        [](void *_context, unsigned _row) -> const unsigned char * {
            auto     &state = *static_cast<State *>(_context);
            const int y     = int(_row);
            if (y >= state.top + state.band.height() && !state.load(y))
                return nullptr;
            return state.band.constScanLine(y - state.top);
        },
        [](void *_context, const unsigned char *_bytes, size_t _size) {
            auto &state = *static_cast<State *>(_context);
            while (_size) {
                const auto written =
                    state.output.write(reinterpret_cast<const char *>(_bytes), qint64(_size));
                if (written <= 0)
                    return 0;
                _bytes += written;
                _size -= size_t(written);
            }
            return 1;
        });
}
} // namespace PhotoCrap::Storage
