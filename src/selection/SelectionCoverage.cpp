#include "SelectionCoverage.h"
#include "ImagePixels.h"
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>

namespace
{
QRegion contour(const QImage &_mask, int _threshold,
                const std::shared_ptr<std::atomic_bool> &_cancel, bool &_valid)
{
    QVector<QRect> bands, row;
    qsizetype      start = 0, count = 0;
    for (int y = 0; y < _mask.height(); ++y) {
        if (_cancel->load()) {
            _valid = false;
            return {};
        }
        row.clear();
        const auto *data = _mask.constScanLine(y);
        for (int x = 0; x < _mask.width();) {
            while (x < _mask.width() && data[x] < _threshold)
                ++x;
            int left = x;
            while (x < _mask.width() && data[x] >= _threshold)
                ++x;
            if (left < x)
                row.append(QRect(left, y, x - left, 1));
        }
        if (bands.size() + row.size() > 1000000) {
            _valid = false;
            return {};
        }
        bool same = !row.isEmpty() && row.size() == count;
        for (qsizetype i = 0; same && i < count; ++i)
            same = bands[start + i].x() == row[i].x() &&
                   bands[start + i].width() == row[i].width() && bands[start + i].bottom() + 1 == y;
        if (same)
            for (qsizetype i = 0; i < count; ++i)
                bands[start + i].setBottom(y);
        else {
            start = bands.size();
            count = row.size();
            bands += row;
        }
    }
    QRegion result;
    if (!bands.isEmpty())
        result.setRects(bands.constData(), int(bands.size()));
    return result;
}
QImage box(const QImage &_input, int _radius, bool _vertical,
           const std::shared_ptr<std::atomic_bool> &_cancel)
{
    QImage output(_input.size(), QImage::Format_Grayscale16);
    if (output.isNull())
        return {};
    const int lines  = _vertical ? _input.width() : _input.height(),
              length = _vertical ? _input.height() : _input.width();
    for (int line = 0; line < lines; ++line) {
        if (_cancel->load())
            return {};
        auto read = [&](int _i) {
            return _vertical ? reinterpret_cast<const quint16 *>(_input.constScanLine(_i))[line]
                             : reinterpret_cast<const quint16 *>(_input.constScanLine(line))[_i];
        };
        int sum = 0;
        for (int i = 0; i <= qMin(_radius, length - 1); ++i)
            sum += read(i);
        for (int i = 0; i < length; ++i) {
            if (i > 0) {
                if (i - _radius - 1 >= 0)
                    sum -= read(i - _radius - 1);
                if (i + _radius < length)
                    sum += read(i + _radius);
            }
            const int n     = qMin(length - 1, i + _radius) - qMax(0, i - _radius) + 1;
            const int value = (sum + n / 2) / n;
            if (_vertical)
                reinterpret_cast<quint16 *>(output.scanLine(i))[line] = quint16(value);
            else
                reinterpret_cast<quint16 *>(output.scanLine(line))[i] = quint16(value);
        }
    }
    return output;
}
} // namespace
namespace
{
QImage extremum(const QImage &_image, int _radius, bool _maximum, bool _vertical,
                const std::shared_ptr<std::atomic_bool> &_cancel)
{
    QImage output(_image.size(), QImage::Format_Grayscale8);
    if (output.isNull())
        return {};
    const int lines  = _vertical ? _image.width() : _image.height(),
              length = _vertical ? _image.height() : _image.width();
    for (int line = 0; line < lines; ++line) {
        if (_cancel->load())
            return {};
        std::deque<std::pair<int, int>> queue;
        int                             next = 0;
        for (int i = 0; i < length; ++i) {
            const int end = qMin(length - 1, i + _radius);
            while (next <= end) {
                const int value =
                    _vertical ? _image.constScanLine(next)[line] : _image.constScanLine(line)[next];
                while (!queue.empty() &&
                       (_maximum ? queue.back().second <= value : queue.back().second >= value))
                    queue.pop_back();
                queue.emplace_back(next++, value);
            }
            while (!queue.empty() && queue.front().first < i - _radius)
                queue.pop_front();
            if (_vertical)
                output.scanLine(i)[line] = uchar(queue.front().second);
            else
                output.scanLine(line)[i] = uchar(queue.front().second);
        }
    }
    return output;
}
} // namespace
SelectionCoverage::Result
SelectionCoverage::modify(const QImage &_mask, const QString &_operation, int _radius,
                          const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_mask.isNull() || !_cancel || _cancel->load())
        return {};
    const auto morph = [&](int _r, bool _expand) {
        const auto horizontal = extremum(_mask, _r, _expand, false, _cancel);
        return horizontal.isNull() ? QImage{} : extremum(horizontal, _r, _expand, true, _cancel);
    };
    QImage result;
    if (_operation == "smooth") {
        result = box(_mask.convertToFormat(QImage::Format_Grayscale16), _radius, false, _cancel);
        if (!result.isNull())
            result = box(result, _radius, true, _cancel).convertToFormat(QImage::Format_Grayscale8);
    } else if (_operation == "border") {
        result           = morph(_radius / 2, true);
        const auto inner = morph((_radius + 1) / 2, false);
        if (result.isNull() || inner.isNull())
            return {};
        for (int y = 0; y < result.height(); ++y) {
            if (_cancel->load())
                return {};
            auto       *row = result.scanLine(y);
            const auto *in  = inner.constScanLine(y);
            for (int x = 0; x < result.width(); ++x)
                row[x] = uchar(qMax(0, int(row[x]) - in[x]));
        }
    } else
        result = morph(_radius, _operation == "expand");
    return describe(result, _cancel);
}
QImage SelectionCoverage::raster(const QRegion &_region, const QSize &_size)
{
    QImage mask(_size, QImage::Format_Grayscale8);
    if (mask.isNull())
        return {};
    mask.fill(0);
    QPainter p(&mask);
    p.setClipRegion(_region);
    p.fillRect(mask.rect(), Qt::white);
    return mask;
}
SelectionCoverage::Result
SelectionCoverage::describe(QImage _mask, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_mask.isNull() || !_cancel || _cancel->load())
        return {};
    bool valid   = true;
    auto support = contour(_mask, 1, _cancel, valid);
    auto outline = valid ? contour(_mask, 128, _cancel, valid) : QRegion{};
    if (!valid)
        return {{}, {}, {}, false, _cancel->load() ? QString{} : QString("complexity")};
    return {std::move(_mask), support, outline, true, {}};
}
SelectionCoverage::Result
SelectionCoverage::feather(const QRegion &_region, const QImage &_mask, const QSize &_size,
                           int _radius, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (!_cancel || _cancel->load() || _radius < 1 || _radius > 1000)
        return {};
    // Three separable box passes approximate a Gaussian in linear time.
    const int   radius = qMax(1, qRound(_radius - .5));
    const QRect bounds = _region.boundingRect()
                             .adjusted(-3 * radius, -3 * radius, 3 * radius, 3 * radius)
                             .intersected(QRect(QPoint{}, _size));
    if (bounds.isEmpty())
        return {};
    QImage patch = _mask.isNull() ? raster(_region.translated(-bounds.topLeft()), bounds.size())
                                  : _mask.copy(bounds);
    patch        = patch.convertToFormat(QImage::Format_Grayscale16);
    for (int pass = 0; pass < 3 && !patch.isNull(); ++pass) {
        patch = box(patch, radius, false, _cancel);
        if (patch.isNull())
            break;
        patch = box(patch, radius, true, _cancel);
    }
    if (patch.isNull())
        return {{}, {}, {}, false, _cancel->load() ? QString{} : QString("memory")};
    patch = patch.convertToFormat(QImage::Format_Grayscale8);
    QImage output(_size, QImage::Format_Grayscale8);
    if (output.isNull())
        return {{}, {}, {}, false, "memory"};
    output.fill(0);
    for (int y = 0; y < bounds.height(); ++y) {
        if (_cancel->load())
            return {};
        std::memcpy(output.scanLine(y + bounds.y()) + bounds.x(), patch.constScanLine(y),
                    size_t(bounds.width()));
    }
    return describe(std::move(output), _cancel);
}
namespace
{
bool keepConnected(QImage &_mask, const QVector<SelectionCoverage::Sample> &_samples,
                   bool _subtract, const std::shared_ptr<std::atomic_bool> &_cancel)
{
    QImage connected(_mask.size(), QImage::Format_Grayscale8);
    if (connected.isNull())
        return false;
    connected.fill(0);
    QVector<QPoint> pending;
    for (const auto &sample : _samples)
        if (sample.subtract == _subtract && _mask.rect().contains(sample.position.toPoint()))
            pending.append(sample.position.toPoint());
    while (!pending.isEmpty()) {
        if (_cancel->load())
            return false;
        const QPoint seed = pending.takeLast();
        const auto  *row  = _mask.constScanLine(seed.y());
        auto        *out  = connected.scanLine(seed.y());
        if (!row[seed.x()] || out[seed.x()])
            continue;
        int left = seed.x(), right = seed.x();
        while (left > 0 && row[left - 1] && !out[left - 1])
            --left;
        while (right + 1 < _mask.width() && row[right + 1] && !out[right + 1])
            ++right;
        std::memcpy(out + left, row + left, size_t(right - left + 1));
        for (int y : {seed.y() - 1, seed.y() + 1}) {
            if (y < 0 || y >= _mask.height())
                continue;
            const auto *neighbor = _mask.constScanLine(y), *visited = connected.constScanLine(y);
            bool        run = false;
            for (int x = left; x <= right; ++x) {
                const bool match = neighbor[x] && !visited[x];
                if (match && !run)
                    pending.append(QPoint(x, y));
                run = match;
            }
        }
    }
    _mask = connected;
    return true;
}
} // namespace
SelectionCoverage::Result
SelectionCoverage::colors(const QImage &_source, const QVector<Sample> &_samples,
                          const QVariantMap                       &_options,
                          const std::shared_ptr<std::atomic_bool> &_cancel)
{
    if (_source.isNull() || !_cancel || _cancel->load())
        return {};
    QImage mask(_source.size(), QImage::Format_Grayscale8);
    if (mask.isNull())
        return {{}, {}, {}, false, "memory"};
    const bool    contiguous = _options.value("contiguous").toBool();
    const double  fuzz       = qBound(0., _options.value("fuzziness", 40.).toDouble(), 200.);
    const bool    localized  = _options.value("localized").toBool(),
                  inverse    = _options.value("invert").toBool();
    const double  range      = qBound(1., _options.value("range", 100.).toDouble(), 100.) * .01 *
                               std::hypot(_source.width(), _source.height());
    const QString type       = _options.value("type", "sampled").toString();
    QImage        removed;
    if (std::any_of(_samples.cbegin(), _samples.cend(),
                    [](const Sample &_s) { return _s.subtract; })) {
        removed = QImage(_source.size(), QImage::Format_Grayscale8);
        if (removed.isNull())
            return {{}, {}, {}, false, "memory"};
        removed.fill(0);
    }
    const auto render = [&]<typename Pixel>() {
        using P             = ImageUtils::PixelAccess<Pixel>;
        const QImage source = _source.convertToFormat(P::format);
        if (source.isNull())
            return false;
        for (int y = 0; y < source.height(); ++y) {
            if (_cancel->load())
                return false;
            const auto *row = reinterpret_cast<const Pixel *>(source.constScanLine(y));
            auto       *dst = mask.scanLine(y);
            for (int x = 0; x < source.width(); ++x) {
                const double r = P::red(row[x]), g = P::green(row[x]), b = P::blue(row[x]);
                double       value = 0, remove = 0;
                if (type == "sampled") {
                    for (const auto &sample : _samples) {
                        const double distance = qMax(qAbs(r - sample.color.redF() * 255.),
                                                     qMax(qAbs(g - sample.color.greenF() * 255.),
                                                          qAbs(b - sample.color.blueF() * 255.)));
                        double       v        = fuzz <= 0 ? (distance < .001 ? 1. : 0.)
                                                          : qBound(0., 1. - distance / fuzz, 1.);
                        if (localized)
                            v *= qBound(
                                0.,
                                1. - std::hypot(x - sample.position.x(), y - sample.position.y()) /
                                         range,
                                1.);
                        if (sample.subtract)
                            remove = qMax(remove, v);
                        else
                            value = qMax(value, v);
                    }

                } else if (type == "highlights" || type == "shadows" || type == "midtones") {
                    const double light = (.2126 * r + .7152 * g + .0722 * b) / 255.;
                    value = type == "highlights" ? qBound(0., (light - .5) * 2., 1.)
                            : type == "shadows"  ? qBound(0., (0.5 - light) * 2., 1.)
                                                 : qBound(0., 1. - qAbs(2. * light - 1.), 1.);
                } else {
                    double max = qMax(r, qMax(g, b)), min = qMin(r, qMin(g, b)), delta = max - min;
                    double hue = 0;
                    if (delta > 0)
                        hue = max == r   ? 60. * std::fmod((g - b) / delta + 6., 6.)
                              : max == g ? 60. * ((b - r) / delta + 2.)
                                         : 60. * ((r - g) / delta + 4.);
                    const double center = type == "reds"      ? 0
                                          : type == "yellows" ? 60
                                          : type == "greens"  ? 120
                                          : type == "cyans"   ? 180
                                          : type == "blues"   ? 240
                                                              : 300;
                    double       d      = qAbs(hue - center);
                    d                   = qMin(d, 360. - d);
                    value =
                        qBound(0., 1. - d / (30. + fuzz * .3), 1.) * (max > 0 ? delta / max : 0);
                }
                dst[x] = uchar(qBound(0, qRound(value * P::alpha(row[x])), 255));
                if (!removed.isNull())
                    removed.scanLine(y)[x] =
                        P::alpha(row[x]) > 0 ? uchar(qBound(0, qRound(remove * 255), 255)) : 0;
            }
        }
        if (contiguous) {
            if (!keepConnected(mask, _samples, false, _cancel))
                return false;
            if (!removed.isNull() && !keepConnected(removed, _samples, true, _cancel))
                return false;
        }
        for (int y = 0; y < source.height(); ++y) {
            if (_cancel->load())
                return false;
            auto       *row      = mask.scanLine(y);
            const auto *src      = reinterpret_cast<const Pixel *>(source.constScanLine(y));
            const auto *negative = removed.isNull() ? nullptr : removed.constScanLine(y);
            for (int x = 0; x < source.width(); ++x) {
                const int amount = negative ? (row[x] * (255 - negative[x]) + 127) / 255 : row[x];
                row[x] =
                    uchar(inverse ? qBound(0, qRound(P::alpha(src[x])) - amount, 255) : amount);
            }
        }
        return true;
    };
    const bool done = ImageUtils::is16Bit(_source) ? render.template operator()<QRgba64>()
                                                   : render.template operator()<QRgb>();
    return done ? describe(std::move(mask), _cancel)
                : Result{{}, {}, {}, false, _cancel->load() ? QString{} : QString("memory")};
}
void SelectionCoverage::attenuate(QImage &_image, const QImage &_mask, const QPoint &_origin)
{
    if (_mask.isNull() || _image.isNull())
        return;
    const auto run = [&]<typename Pixel>() {
        using P = ImageUtils::PixelAccess<Pixel>;
        _image  = _image.convertToFormat(P::premultiplied);
        for (int y = 0; y < _image.height(); ++y) {
            auto *row = reinterpret_cast<Pixel *>(_image.scanLine(y));
            for (int x = 0; x < _image.width(); ++x) {
                const QPoint p = _origin + QPoint(x, y);
                const int    a = _mask.rect().contains(p) ? _mask.constScanLine(p.y())[p.x()] : 0;
                const Pixel  v = row[x];
                row[x]         = P::rgba(P::red(v) * a / 255., P::green(v) * a / 255.,
                                         P::blue(v) * a / 255., P::alpha(v) * a / 255.);
            }
        }
    };
    if (ImageUtils::is16Bit(_image))
        run.template operator()<QRgba64>();
    else
        run.template operator()<QRgb>();
}
void SelectionCoverage::blend(QImage &_image, const QImage &_before, const QImage &_mask,
                              const QRect &_area)
{
    if (_mask.isNull() || _image.isNull() || _before.isNull())
        return;
    const QRect area    = _area.intersected(_image.rect()).intersected(_mask.rect());
    const bool  uniform = ImageUtils::compact(_before);
    if (_image.format() == QImage::Format_Grayscale8) {
        for (int y = area.top(); y <= area.bottom(); ++y) {
            auto       *row  = _image.scanLine(y);
            const auto *old  = _before.constScanLine(uniform ? 0 : y),
                       *mask = _mask.constScanLine(y);
            for (int x = area.left(); x <= area.right(); ++x)
                row[x] =
                    uchar((old[uniform ? 0 : x] * (255 - mask[x]) + row[x] * mask[x] + 127) / 255);
        }
        return;
    }
    const auto run = [&]<typename Pixel>() {
        using P           = ImageUtils::PixelAccess<Pixel>;
        _image            = _image.convertToFormat(P::premultiplied);
        const auto before = _before.convertToFormat(P::premultiplied);
        for (int y = area.top(); y <= area.bottom(); ++y) {
            auto       *row = reinterpret_cast<Pixel *>(_image.scanLine(y));
            const auto *old =
                reinterpret_cast<const Pixel *>(before.constScanLine(uniform ? 0 : y));
            const auto *mask = _mask.constScanLine(y);
            for (int x = area.left(); x <= area.right(); ++x) {
                const Pixel  a = old[uniform ? 0 : x], b = row[x];
                const double f = mask[x] / 255.;
                row[x]         = P::rgba(P::red(a) + (P::red(b) - P::red(a)) * f,
                                         P::green(a) + (P::green(b) - P::green(a)) * f,
                                         P::blue(a) + (P::blue(b) - P::blue(a)) * f,
                                         P::alpha(a) + (P::alpha(b) - P::alpha(a)) * f);
            }
        }
    };
    if (ImageUtils::is16Bit(_image))
        run.template operator()<QRgba64>();
    else
        run.template operator()<QRgb>();
}
