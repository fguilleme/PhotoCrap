#include "Document.h"
#include "ImageUtils.h"
#include "filters/BlackWhiteFilter.h"
#include "filters/BrightnessContrastFilter.h"
#include "filters/ColorFilter.h"
#include "filters/CurvesFilter.h"
#include "filters/GradientMapFilter.h"
#include "filters/LevelsFilter.h"
#include "filters/LightFilter.h"

#include <QCache>
#include <QColorSpace>
#include <QJSValue>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QThreadPool>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#ifdef PHOTOCRAP_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

namespace
{
QImage cachedScreenProxy(const QImage &_source, const QSize &_size, QImage::Format _format)
{
    if (_source.isNull() || !_size.isValid())
        return {};
    const QString                  key = QStringLiteral("%1:%2x%3:%4")
                                             .arg(_source.cacheKey())
                                             .arg(_size.width())
                                             .arg(_size.height())
                                             .arg(int(_format));
    static QMutex                  cacheMutex;
    static QCache<QString, QImage> cache(128 * 1024); // Cost is expressed in KiB.
    {
        QMutexLocker locker(&cacheMutex);
        if (const QImage *cached = cache.object(key))
            return *cached;
    }
    QImage    proxy = _source.scaled(_size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                          .convertToFormat(_format);
    const int cost =
        qMax(1, int(qMin<qsizetype>(proxy.sizeInBytes() / 1024, std::numeric_limits<int>::max())));
    {
        QMutexLocker locker(&cacheMutex);
        cache.insert(key, new QImage(proxy), cost);
    }
    return proxy;
}
} // namespace

QImage Document::applyMask(const QImage &_source, const QImage &_mask, qreal _density,
                           qreal _feather)
{
    if (_source.isNull() || _mask.isNull())
        return _source;
    QImage result         = _source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    _density              = qBound(0., _density, 1.);
    _feather              = qBound(0., _feather, 100.);
    const QImage coverage = (_feather >= .5 ? blurImage(_mask, qRound(_feather)) : _mask)
                                .convertToFormat(QImage::Format_Grayscale8);
    const int    h        = qMin(result.height(), coverage.height()),
                 w        = qMin(result.width(), coverage.width());
    for (int y = 0; y < h; ++y) {
        auto         pixels = reinterpret_cast<QRgb *>(result.scanLine(y));
        const uchar *m      = coverage.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int a = qRound(255 - (255 - m[x]) * _density);
            pixels[x]   = qRgba(qRed(pixels[x]) * a / 255, qGreen(pixels[x]) * a / 255,
                                qBlue(pixels[x]) * a / 255, qAlpha(pixels[x]) * a / 255);
        }
    }
    return result;
}
QImage Document::blendImages(const QImage &_base, const QImage &_source, int _mode, qreal _opacity)
{
    if (_source.isNull())
        return _base;
    _opacity = qBound(0., _opacity, 1.);
    _mode    = qBound(0, _mode, 26);
    if (_mode == 0) {
        QImage   background = _base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&background);
        painter.setOpacity(_opacity);
        painter.drawImage(0, 0, _source);
        painter.end();
        background.setColorSpace(QColorSpace::SRgb);
        return background;
    }
    QImage       background = _base.convertToFormat(QImage::Format_ARGB32);
    const QImage foreground = _source.convertToFormat(QImage::Format_ARGB32);
    auto         channel    = [](double _b, double _s, int _m) {
        auto burn = [](double _baseValue, double _blendValue) {
            return _blendValue <= 0 ? 0. : 1. - qMin(1., (1. - _baseValue) / _blendValue);
        };
        auto dodge = [](double _baseValue, double _blendValue) {
            return _blendValue >= 1 ? 1. : qMin(1., _baseValue / (1. - _blendValue));
        };
        switch (_m) {
        case 2:
            return qMin(_b, _s);
        case 3:
            return _b * _s;
        case 4:
            return burn(_b, _s);
        case 5:
            return qMax(0., _b + _s - 1.);
        case 7:
            return qMax(_b, _s);
        case 8:
            return _b + _s - _b * _s;
        case 9:
            return dodge(_b, _s);
        case 10:
            return qMin(1., _b + _s);
        case 12:
            return _b <= .5 ? 2 * _b * _s : 1 - 2 * (1 - _b) * (1 - _s);
        case 13:
            return _s <= .5 ? _b - (1 - 2 * _s) * _b * (1 - _b)
                            : _b + (2 * _s - 1) * (_b <= .25 ? ((16 * _b - 12) * _b + 4) * _b
                                                             : std::sqrt(_b) - _b);
        case 14:
            return _s <= .5 ? 2 * _b * _s : 1 - 2 * (1 - _b) * (1 - _s);
        case 15:
            return _s <= .5 ? burn(_b, 2 * _s) : dodge(_b, 2 * _s - 1);
        case 16:
            return qBound(0., _b + 2 * _s - 1., 1.);
        case 17:
            return _s <= .5 ? qMin(_b, 2 * _s) : qMax(_b, 2 * _s - 1);
        case 18:
            return (_s <= .5 ? burn(_b, 2 * _s) : dodge(_b, 2 * _s - 1)) < .5 ? 0. : 1.;
        case 19:
            return std::abs(_b - _s);
        case 20:
            return _b + _s - 2 * _b * _s;
        case 21:
            return qMax(0., _b - _s);
        case 22:
            return _s <= 0 ? 1. : qMin(1., _b / _s);
        default:
            return _s;
        }
    };
    QVector<int> rows(background.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        auto       *destination = reinterpret_cast<QRgb *>(background.scanLine(_y));
        const auto *source      = reinterpret_cast<const QRgb *>(foreground.constScanLine(_y));
        for (int x = 0; x < background.width(); ++x) {
            const QRgb   bc = destination[x], sc = source[x];
            const double br = qRed(bc) / 255., bg = qGreen(bc) / 255., bb = qBlue(bc) / 255.;
            const double sr = qRed(sc) / 255., sg = qGreen(sc) / 255., sb = qBlue(sc) / 255.;
            const double ab = qAlpha(bc) / 255.;
            double       as = qAlpha(sc) / 255. * _opacity;
            if (_mode == 1) {
                const quint32 hash = quint32(x * 73856093u) ^ quint32(_y * 19349663u);
                as                 = (hash & 0xffffu) < quint32(as * 65535.) ? 1. : 0.;
            }
            if (as <= 0)
                continue;
            double mixedRed = sr, mixedGreen = sg, mixedBlue = sb;
            if (_mode == 6 || _mode == 11) {
                const int  baseTotal   = qRed(bc) + qGreen(bc) + qBlue(bc);
                const int  sourceTotal = qRed(sc) + qGreen(sc) + qBlue(sc);
                const bool useSource =
                    _mode == 6 ? sourceTotal < baseTotal : sourceTotal > baseTotal;
                if (!useSource) {
                    mixedRed   = br;
                    mixedGreen = bg;
                    mixedBlue  = bb;
                }
            } else if (_mode >= 23) {
                float        bh, bs, bl, ba, sh, ss, sl, sa;
                QColor       blended   = QColor::fromRgba(sc);
                const QColor baseColor = QColor::fromRgba(bc);
                baseColor.getHslF(&bh, &bs, &bl, &ba);
                blended.getHslF(&sh, &ss, &sl, &sa);
                if (bh < 0)
                    bh = 0;
                if (sh < 0)
                    sh = 0;
                if (_mode == 23)
                    blended.setHslF(sh, bs, bl);
                else if (_mode == 24)
                    blended.setHslF(bh, ss, bl);
                else if (_mode == 25)
                    blended.setHslF(sh, ss, bl);
                else
                    blended.setHslF(bh, bs, sl);
                mixedRed   = blended.redF();
                mixedGreen = blended.greenF();
                mixedBlue  = blended.blueF();
            } else if (_mode >= 2) {
                mixedRed   = channel(br, sr, _mode);
                mixedGreen = channel(bg, sg, _mode);
                mixedBlue  = channel(bb, sb, _mode);
            }
            const double ao      = as + ab * (1 - as);
            auto         compose = [&](double _b, double _s, double _mix) {
                return ao <= 0 ? 0.
                               : ((1 - as) * ab * _b + (1 - ab) * as * _s + ab * as * _mix) / ao;
            };
            destination[x] = qRgba(qRound(qBound(0., compose(br, sr, mixedRed), 1.) * 255.),
                                   qRound(qBound(0., compose(bg, sg, mixedGreen), 1.) * 255.),
                                   qRound(qBound(0., compose(bb, sb, mixedBlue), 1.) * 255.),
                                   qRound(qBound(0., ao, 1.) * 255.));
        }
    });
    background.setColorSpace(QColorSpace::SRgb);
    return background.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::blurImage(const QImage &_source, int _radius)
{
    if (_source.isNull() || _radius <= 0)
        return _source;
    _radius        = qBound(1, _radius, 100);
    const int    w = _source.width(), h = _source.height(), window = _radius * 2 + 1;
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
#ifdef PHOTOCRAP_USE_ACCELERATE
    QImage        accelerated = ImageUtils::blank(_source.size());
    vImage_Buffer sourceBuffer{const_cast<uchar *>(input.constBits()), vImagePixelCount(h),
                               vImagePixelCount(w), size_t(input.bytesPerLine())};
    vImage_Buffer destinationBuffer{accelerated.bits(), vImagePixelCount(h), vImagePixelCount(w),
                                    size_t(accelerated.bytesPerLine())};
    const vImage_Error error =
        vImageBoxConvolve_ARGB8888(&sourceBuffer, &destinationBuffer, nullptr, 0, 0,
                                   uint32_t(window), uint32_t(window), nullptr, kvImageEdgeExtend);
    if (error == kvImageNoError)
        return accelerated;
#endif
    QImage horizontal = ImageUtils::blank(_source.size()),
           result     = ImageUtils::blank(_source.size());
    auto   pass       = [&](const QImage &_src, QImage &_dst, bool _vertical) {
        const int major = _vertical ? w : h, minor = _vertical ? h : w;
        const int jobs = qMin(major, qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
        QVector<int> blocks(jobs);
        std::iota(blocks.begin(), blocks.end(), 0);
        ImageUtils::blockingMap(blocks, [&](int _block) {
            const int first = major * _block / jobs, last = major * (_block + 1) / jobs;
            for (int a = first; a < last; ++a) {
                qint64 r = 0, g = 0, b = 0, alpha = 0;
                auto   sample = [&](int _pos, int _sign) {
                    const int  p = qBound(0, _pos, minor - 1);
                    const auto row =
                        reinterpret_cast<const QRgb *>(_src.constScanLine(_vertical ? p : a));
                    const QRgb c = row[_vertical ? a : p];
                    r += _sign * qRed(c);
                    g += _sign * qGreen(c);
                    b += _sign * qBlue(c);
                    alpha += _sign * qAlpha(c);
                };
                for (int k = -_radius; k <= _radius; ++k)
                    sample(k, 1);
                for (int n = 0; n < minor; ++n) {
                    auto row = reinterpret_cast<QRgb *>(_dst.scanLine(_vertical ? n : a));
                    row[_vertical ? a : n] = qRgba(int(r / window), int(g / window),
                                                   int(b / window), int(alpha / window));
                    sample(n - _radius, -1);
                    sample(n + _radius + 1, 1);
                }
            }
        });
    };
    pass(input, horizontal, false);
    pass(horizontal, result, true);
    return result;
}
QImage Document::gaussianBlurImage(const QImage &_source, qreal _radius)
{
    if (_source.isNull() || _radius <= 0)
        return _source;
    // Three box convolutions closely approximate a Gaussian while retaining
    // the platform accelerated O(n) implementation of each pass.
    const int boxRadius = qBound(1, qRound(_radius), 100);
    QImage    result    = _source;
    for (int pass = 0; pass < 3; ++pass)
        result = blurImage(result, boxRadius);
    return result;
}
QImage Document::motionBlurImage(const QImage &_source, qreal _distance, qreal _angle)
{
    if (_source.isNull() || _distance <= 0)
        return _source;
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage       result(input.size(), QImage::Format_ARGB32_Premultiplied);
    result.setColorSpace(QColorSpace::SRgb);
    _distance            = qBound(0., _distance, 200.);
    _angle               = qDegreesToRadians(_angle);
    const int    samples = qBound(3, qRound(_distance / 4.) * 2 + 1, 33), half = samples / 2;
    const qreal  dx = std::cos(_angle) * _distance / qMax(1, samples - 1),
                 dy = std::sin(_angle) * _distance / qMax(1, samples - 1);
    QVector<int> rows(input.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        auto output = reinterpret_cast<QRgb *>(result.scanLine(_y));
        for (int x = 0; x < input.width(); ++x) {
            int a = 0, r = 0, g = 0, b = 0;
            for (int sample = -half; sample <= half; ++sample) {
                const int  sx    = qBound(0, qRound(x + sample * dx), input.width() - 1);
                const int  sy    = qBound(0, qRound(_y + sample * dy), input.height() - 1);
                const auto row   = reinterpret_cast<const QRgb *>(input.constScanLine(sy));
                const QRgb pixel = row[sx];
                a += qAlpha(pixel);
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
            }
            output[x] = qRgba(r / samples, g / samples, b / samples, a / samples);
        }
    });
    return result;
}
QImage Document::unsharpMaskImage(const QImage &_source, qreal _radius, qreal _amount,
                                  int _threshold)
{
    if (_source.isNull() || _amount <= 0)
        return _source;
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    const QImage smooth =
        gaussianBlurImage(_source, qMax(.1, _radius)).convertToFormat(QImage::Format_ARGB32);
    QImage      result(input.size(), QImage::Format_ARGB32);
    const qreal gain = qBound(0., _amount, 300.) / 100.;
    _threshold       = qBound(0, _threshold, 255);
    QVector<int> rows(input.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        const auto src  = reinterpret_cast<const QRgb *>(input.constScanLine(_y));
        const auto blur = reinterpret_cast<const QRgb *>(smooth.constScanLine(_y));
        auto       dst  = reinterpret_cast<QRgb *>(result.scanLine(_y));
        for (int x = 0; x < input.width(); ++x) {
            auto sharpen = [&](int _value, int _soft) {
                const int delta = _value - _soft;
                return qBound(0, _value + (qAbs(delta) >= _threshold ? qRound(delta * gain) : 0),
                              255);
            };
            dst[x] = qRgba(sharpen(qRed(src[x]), qRed(blur[x])),
                           sharpen(qGreen(src[x]), qGreen(blur[x])),
                           sharpen(qBlue(src[x]), qBlue(blur[x])), qAlpha(src[x]));
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::highPassImage(const QImage &_source, qreal _radius)
{
    if (_source.isNull())
        return _source;
    const QImage input  = _source.convertToFormat(QImage::Format_ARGB32),
                 smooth = gaussianBlurImage(_source, qMax(.1, _radius))
                              .convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);
    QVector<int> rows(input.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        const auto src  = reinterpret_cast<const QRgb *>(input.constScanLine(_y));
        const auto blur = reinterpret_cast<const QRgb *>(smooth.constScanLine(_y));
        auto       dst  = reinterpret_cast<QRgb *>(result.scanLine(_y));
        for (int x = 0; x < input.width(); ++x)
            dst[x] = qRgba(qBound(0, qRed(src[x]) - qRed(blur[x]) + 128, 255),
                           qBound(0, qGreen(src[x]) - qGreen(blur[x]) + 128, 255),
                           qBound(0, qBlue(src[x]) - qBlue(blur[x]) + 128, 255), qAlpha(src[x]));
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::mosaicImage(const QImage &_source, int _cellSize)
{
    if (_source.isNull() || _cellSize <= 1)
        return _source;
    _cellSize = qBound(2, _cellSize, 200);
    const QSize reduced(qMax(1, (_source.width() + _cellSize - 1) / _cellSize),
                        qMax(1, (_source.height() + _cellSize - 1) / _cellSize));
    return _source.scaled(reduced, Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .scaled(_source.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
}
QImage Document::noiseImage(const QImage &_source, qreal _amount)
{
    if (_source.isNull() || _amount <= 0)
        return _source;
    QImage       result    = _source.convertToFormat(QImage::Format_ARGB32);
    const int    amplitude = qRound(qBound(0., _amount, 100.) * 1.275);
    QVector<int> rows(result.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        auto row = reinterpret_cast<QRgb *>(result.scanLine(_y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = row[x];
            quint32    hash  = quint32(x) * 0x9e3779b1u ^ quint32(_y) * 0x85ebca6bu;
            auto       delta = [&]() {
                hash ^= hash << 13;
                hash ^= hash >> 17;
                hash ^= hash << 5;
                return int(hash % quint32(amplitude * 2 + 1)) - amplitude;
            };
            row[x] = qRgba(qBound(0, qRed(pixel) + delta(), 255),
                           qBound(0, qGreen(pixel) + delta(), 255),
                           qBound(0, qBlue(pixel) + delta(), 255), qAlpha(pixel));
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::frequencyImage(const Layer &_layer) const
{
    if (_layer.source.isNull())
        return {};
    const int radius       = qBound(1, _layer.parameters.value("radius", 8).toInt(), 100);
    QImage    source       = _layer.source.convertToFormat(QImage::Format_ARGB32);
    qreal     previewScale = 1.;
    if (m_frequencyPreviewDraft && qMax(source.width(), source.height()) > 960) {
        previewScale = 960. / qreal(qMax(source.width(), source.height()));
        source       = source.scaled(qMax(1, qRound(source.width() * previewScale)),
                                     qMax(1, qRound(source.height() * previewScale)),
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    const QString cacheBase =
        QString::number(_layer.source.cacheKey()) + QLatin1Char('/') + QString::number(radius) +
        (previewScale < 1 ? QStringLiteral("/draft/") + QString::number(source.width())
                          : QStringLiteral("/full"));
    const QString lowKey = QStringLiteral("low/") + cacheBase;
    QImage        low    = m_frequencyCache.value(lowKey);
    if (low.isNull()) {
        if (m_frequencyCache.size() > 12)
            m_frequencyCache.clear();
        low = blurImage(source, qMax(1, qRound(radius * previewScale)))
                  .convertToFormat(QImage::Format_ARGB32);
        if (previewScale < 1)
            low = low.scaled(_layer.source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        m_frequencyCache.insert(lowKey, low);
    }
    if (_layer.kind == QStringLiteral("frequencyLow"))
        return low;
    const QString highKey = QStringLiteral("high/") + cacheBase;
    if (m_frequencyCache.contains(highKey))
        return m_frequencyCache.value(highKey);
    const QImage fullSource = _layer.source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(fullSource.size(), QImage::Format_ARGB32);
    QVector<int> rows(fullSource.height());
    std::iota(rows.begin(), rows.end(), 0);
    ImageUtils::blockingMap(rows, [&](int _y) {
        const auto s   = reinterpret_cast<const QRgb *>(fullSource.constScanLine(_y));
        const auto l   = reinterpret_cast<const QRgb *>(low.constScanLine(_y));
        auto       out = reinterpret_cast<QRgb *>(result.scanLine(_y));
        for (int x = 0; x < fullSource.width(); ++x)
            out[x] =
                qRgba((qRed(s[x]) - qRed(l[x]) + 256) / 2, (qGreen(s[x]) - qGreen(l[x]) + 256) / 2,
                      (qBlue(s[x]) - qBlue(l[x]) + 256) / 2, qAlpha(s[x]));
    });
    result = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_frequencyCache.insert(highKey, result);
    return result;
}
QImage Document::adjustedImage(const QImage &_source, const QString &_type, const QVariantMap &_p)
{
    if (_source.isNull())
        return _source;
    if (_type == "curves")
        return PhotoCrap::Filters::applyCurves(_source, _p);
    if (_type == "levels")
        return PhotoCrap::Filters::applyLevels(_source, _p);
    if (_type == "blackWhite")
        return PhotoCrap::Filters::applyBlackWhite(_source, _p);
    if (_type == "brightnessContrast")
        return PhotoCrap::Filters::applyBrightnessContrast(_source, _p);
    if (_type == "gradientMap")
        return PhotoCrap::Filters::applyGradientMap(_source, _p);
    if (_type == "color")
        return PhotoCrap::Filters::applyColor(_source, _p);
    if (_type == "light")
        return PhotoCrap::Filters::applyLight(_source, _p);
    if (_type == "gaussianBlur")
        return gaussianBlurImage(_source, _p.value("radius", 8.).toDouble());
    if (_type == "motionBlur")
        return motionBlurImage(_source, _p.value("distance", 20.).toDouble(),
                               _p.value("angle", 0.).toDouble());
    if (_type == "boxBlur")
        return blurImage(_source, qRound(_p.value("radius", 8.).toDouble()));
    if (_type == "unsharpMask")
        return unsharpMaskImage(_source, _p.value("radius", 2.).toDouble(),
                                _p.value("amount", 100.).toDouble(),
                                _p.value("threshold", 0).toInt());
    if (_type == "highPass")
        return highPassImage(_source, _p.value("radius", 4.).toDouble());
    if (_type == "mosaic")
        return mosaicImage(_source, _p.value("cellSize", 12).toInt());
    if (_type == "noise")
        return noiseImage(_source, _p.value("amount", 8.).toDouble());
    const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
    QImage       result(input.size(), QImage::Format_ARGB32);
    auto         value = [&](const char *_key, double _fallback) {
        return _p.value(QLatin1String(_key), _fallback).toDouble();
    };
    auto      clamp = [](double _v) { return qBound(0, qRound(_v), 255); };
    const int jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        const int firstRow = input.height() * _job / jobCount;
        const int lastRow  = input.height() * (_job + 1) / jobCount;
        for (int y = firstRow; y < lastRow; ++y) {
            const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                QColor       c = QColor::fromRgba(source[x]);
                double       r = c.red(), g = c.green(), b = c.blue();
                const int    alpha = c.alpha();
                const double lum   = .2126 * r + .7152 * g + .0722 * b;
                if (_type == "exposure") {
                    const double exposure = std::pow(2., value("exposure", 0)),
                                 offset   = value("offset", 0) * 255,
                                 gamma    = qMax(.1, value("gamma", 1));
                    auto         expose   = [&](double _v) {
                        return std::pow(qMax(0., _v * exposure + offset) / 255., 1 / gamma) * 255.;
                    };
                    r = expose(r);
                    g = expose(g);
                    b = expose(b);
                } else if (_type == "vibrance") {
                    const double mx = qMax(r, qMax(g, b)), mn = qMin(r, qMin(g, b)),
                                 sat = (mx - mn) / 255.;
                    const double factor =
                        1 + value("saturation", 0) / 100. + value("vibrance", 0) / 100. * (1 - sat);
                    r = lum + (r - lum) * factor;
                    g = lum + (g - lum) * factor;
                    b = lum + (b - lum) * factor;
                } else if (_type == "hueSaturation") {
                    float h, s, l, a;
                    c.getHslF(&h, &s, &l, &a);
                    h = std::fmod(h + value("hue", 0) / 360. + 1., 1.);
                    s = qBound(0., s * (1 + value("saturation", 0) / 100.), 1.);
                    l = qBound(0., l + value("lightness", 0) / 100., 1.);
                    c.setHslF(h, s, l, a);
                    r = c.red();
                    g = c.green();
                    b = c.blue();
                } else if (_type == "colorBalance") {
                    r += value("cyanRed", 0) * 2.55;
                    g += value("magentaGreen", 0) * 2.55;
                    b += value("yellowBlue", 0) * 2.55;
                } else if (_type == "photoFilter") {
                    QColor filter;
                    filter.setHslF(std::fmod(value("hue", 35) / 360. + 1., 1.), .8, .55);
                    const double d = qBound(0., value("density", 25) / 100., 1.);
                    r              = r * (1 - d) + filter.red() * d;
                    g              = g * (1 - d) + filter.green() * d;
                    b              = b * (1 - d) + filter.blue() * d;
                } else if (_type == "channelMixer") {
                    const double nr = r * value("redRed", 100) / 100. +
                                      g * value("redGreen", 0) / 100. +
                                      b * value("redBlue", 0) / 100.;
                    const double ng = r * value("greenRed", 0) / 100. +
                                      g * value("greenGreen", 100) / 100. +
                                      b * value("greenBlue", 0) / 100.;
                    const double nb = r * value("blueRed", 0) / 100. +
                                      g * value("blueGreen", 0) / 100. +
                                      b * value("blueBlue", 100) / 100.;
                    r               = nr;
                    g               = ng;
                    b               = nb;
                } else if (_type == "colorLookup") {
                    const int preset = _p.value("preset", 0).toInt();
                    if (preset == 0) {
                        r = r * 1.05 + 8;
                        g = g * .98;
                        b = b * .88;
                    } else if (preset == 1) {
                        r = r * 1.08;
                        g = g * 1.01;
                        b = b * .92 + 8;
                    } else if (preset == 2) {
                        r = r * .9;
                        g = g * 1.02;
                        b = b * 1.12;
                    } else {
                        r = 128 + (r - 128) * .82;
                        g = 128 + (g - 128) * .82;
                        b = 128 + (b - 128) * .82;
                    }
                } else if (_type == "invert") {
                    r = 255 - r;
                    g = 255 - g;
                    b = 255 - b;
                } else if (_type == "posterize") {
                    const int    levels = qBound(2, _p.value("levels", 4).toInt(), 32);
                    const double step   = 255. / (levels - 1);
                    r                   = qRound(r / step) * step;
                    g                   = qRound(g / step) * step;
                    b                   = qRound(b / step) * step;
                } else if (_type == "threshold") {
                    r = g = b = lum >= value("level", 128) ? 255 : 0;
                } else if (_type == "selectiveColor") {
                    const double strength = (qMax(r, qMax(g, b)) - qMin(r, qMin(g, b))) / 255.;
                    r += value("cyanRed", 0) * 2.55 * strength;
                    g += value("magentaGreen", 0) * 2.55 * strength;
                    b += value("yellowBlue", 0) * 2.55 * strength;
                    const double black = value("black", 0) * 2.55;
                    r -= black;
                    g -= black;
                    b -= black;
                }
                output[x] = qRgba(clamp(r), clamp(g), clamp(b), alpha);
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
void Document::addAdjustmentLayer(const QString &_type)
{
    const QMap<QString, QString> names{{"light", tr("Lumière")},
                                       {"color", tr("Couleur")},
                                       {"brightnessContrast", tr("Luminosité / Contraste")},
                                       {"levels", tr("Niveaux")},
                                       {"curves", tr("Courbes")},
                                       {"exposure", tr("Exposition")},
                                       {"vibrance", tr("Vibrance")},
                                       {"hueSaturation", tr("Teinte / Saturation")},
                                       {"colorBalance", tr("Balance des couleurs")},
                                       {"blackWhite", tr("Noir et blanc")},
                                       {"photoFilter", tr("Filtre photo")},
                                       {"channelMixer", tr("Mélangeur de couches")},
                                       {"colorLookup", tr("Recherche de couleurs")},
                                       {"invert", tr("Négatif")},
                                       {"posterize", tr("Isohélie")},
                                       {"threshold", tr("Seuil")},
                                       {"gradientMap", tr("Courbe de transfert de dégradé")},
                                       {"selectiveColor", tr("Correction sélective")},
                                       {"gaussianBlur", tr("Flou gaussien")},
                                       {"motionBlur", tr("Flou directionnel")},
                                       {"boxBlur", tr("Flou de boîte")},
                                       {"unsharpMask", tr("Netteté optimisée")},
                                       {"highPass", tr("Passe-haut")},
                                       {"mosaic", tr("Mosaïque")},
                                       {"noise", tr("Ajout de bruit")}};
    if (!names.contains(_type) || !validSize(width(), height(), int(m_layers.size()) + 1))
        return;
    QVariantMap params;
    if (_type == "color")
        params = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (_type == "light")
        params = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                  {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (_type == "brightnessContrast")
        params = {{"brightness", 0}, {"contrast", 0}};
    else if (_type == "levels")
        params = {{"inputBlack", 0},
                  {"inputWhite", 255},
                  {"gamma", 1.},
                  {"outputBlack", 0},
                  {"outputWhite", 255}};
    else if (_type == "curves")
        params = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                          QVariantMap{{"x", 255}, {"y", 255}}}}};
    else if (_type == "exposure")
        params = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (_type == "vibrance")
        params = {{"vibrance", 0}, {"saturation", 0}};
    else if (_type == "hueSaturation")
        params = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (_type == "colorBalance" || _type == "selectiveColor")
        params = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (_type == "blackWhite")
        params = {{"reds", 40}, {"greens", 40}, {"blues", 20}};
    else if (_type == "photoFilter")
        params = {{"hue", 35}, {"density", 25}};
    else if (_type == "channelMixer")
        params = {{"redRed", 100}, {"redGreen", 0},     {"redBlue", 0},
                  {"greenRed", 0}, {"greenGreen", 100}, {"greenBlue", 0},
                  {"blueRed", 0},  {"blueGreen", 0},    {"blueBlue", 100}};
    else if (_type == "colorLookup")
        params = {{"preset", 0}};
    else if (_type == "posterize")
        params = {{"levels", 4}};
    else if (_type == "threshold")
        params = {{"level", 128}};
    else if (_type == "gradientMap")
        params = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (_type == "gaussianBlur" || _type == "boxBlur")
        params = {{"radius", 8.}};
    else if (_type == "motionBlur")
        params = {{"distance", 20.}, {"angle", 0.}};
    else if (_type == "unsharpMask")
        params = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (_type == "highPass")
        params = {{"radius", 4.}};
    else if (_type == "mosaic")
        params = {{"cellSize", 12}};
    else if (_type == "noise")
        params = {{"amount", 8.}};
    edit(tr("Nouveau calque de réglage"), [&] {
        Layer layer;
        layer.name           = names.value(_type);
        layer.kind           = "adjustment";
        layer.adjustmentType = _type;
        layer.parameters     = params;
        layer.mask           = QImage(m_size, QImage::Format_Grayscale8);
        layer.mask.fill(hasSelection() ? 0 : 255);
        layer.parameters.insert("_maskDensity", 100.);
        layer.parameters.insert("_maskFeather", 0.);
        if (hasSelection()) {
            QPainter painter(&layer.mask);
            painter.fillRect(m_selection, Qt::white);
        }
        m_layers.insert(++m_active, layer);
        m_editingMask = false;
        selectOnly(m_active);
    });
    // Present a screen-sized result first, then refine the document pixels in
    // the background. Adding a costly filter must never block the QML thread.
    requestPreviewRender(true, true);
}
void Document::addFrequencySeparation(qreal _radius)
{
    if (!std::isfinite(_radius) || !validSize(width(), height(), int(m_layers.size()) + 2))
        return;
    QImage source = m_layers[m_active].kind == "pixel" ? m_layers[m_active].image : composite();
    if (m_layers[m_active].kind == "pixel" && m_layers[m_active].maskEnabled &&
        !m_layers[m_active].mask.isNull())
        source =
            applyMask(source, m_layers[m_active].mask,
                      m_layers[m_active].parameters.value("_maskDensity", 100.).toDouble() / 100.,
                      m_layers[m_active].parameters.value("_maskFeather", 0.).toDouble());
    const QString pairId = QString::number(m_nextId);
    edit(tr("Séparation de fréquences"), [&] {
        const QVariantMap parameters{{"radius", qBound(1., _radius, 100.)}, {"pairId", pairId}};
        Layer             low;
        low.name       = tr("Basses fréquences");
        low.kind       = "frequencyLow";
        low.source     = source;
        low.parameters = parameters;
        Layer high;
        high.name       = tr("Hautes fréquences");
        high.kind       = "frequencyHigh";
        high.source     = source;
        high.parameters = parameters;
        high.blend      = 16;
        m_layers.insert(++m_active, low);
        m_layers.insert(++m_active, high);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::beginLayerPreview()
{
    if (m_previewingLayer || m_active < 0 || m_active >= m_layers.size())
        return;
    endStroke();
    (void)composite();
    m_beforeLayerPreviewComposite = m_composite;
    m_beforeLayerPreview          = snapshot(tr("Aperçu"));
    m_previewingLayer             = true;
    m_layerPreviewChanged         = false;
    m_frequencyPreviewDraft       = false;
}
void Document::previewLayerParameter(const QString &_key, const QVariant &_value)
{
    if (m_active < 0 || m_active >= m_layers.size() || _key.isEmpty())
        return;
    if (!m_previewingLayer)
        beginLayerPreview();
    const QVariant value = _value.metaType() == QMetaType::fromType<QJSValue>()
                               ? _value.value<QJSValue>().toVariant(QJSValue::ConvertJSObjects)
                               : _value;
    if (m_layers[m_active].parameters.value(_key) == value)
        return;
    const QString pairId = m_layers[m_active].parameters.value("pairId").toString();
    if (_key == "radius" && !pairId.isEmpty()) {
        m_frequencyPreviewDraft = true;
        m_frequencyCache.clear();
        for (auto &layer : m_layers)
            if (layer.parameters.value("pairId").toString() == pairId)
                layer.parameters.insert(_key, value);
    } else
        m_layers[m_active].parameters.insert(_key, value);
    // The property panel already owns the live slider value. Avoid rebuilding
    // the layer model and its thumbnails for every pointer event.
    m_layerPreviewChanged = true;
    m_fullCompositeDirty  = true;
    m_cacheDirty          = true;
    requestPreviewRender(true);
}
void Document::finishLayerPreview(const QString &_label)
{
    if (!m_previewingLayer)
        return;
    m_previewingLayer       = false;
    m_frequencyPreviewDraft = false;
    if (m_layerPreviewChanged) {
        QImage screenPreview;
        {
            QMutexLocker locker(&m_previewImageMutex);
            screenPreview = m_previewComposite;
        }
        commit(m_beforeLayerPreview, _label);
        {
            QMutexLocker locker(&m_previewImageMutex);
            m_previewComposite = screenPreview;
        }
        requestPreviewRender(false);
    }
    m_beforeLayerPreview          = {};
    m_beforeLayerPreviewComposite = {};
    m_layerPreviewChanged         = false;
}

void Document::requestPreviewRender(bool _draft, bool _refine)
{
    m_pendingPreviewLayers     = m_layers;
    m_pendingPreviewSize       = m_size;
    m_pendingPreviewTargetSize = m_previewTargetSize;
    m_pendingPreviewDraft      = _draft;
    m_pendingPreviewRefine     = _refine;
    m_previewRenderPending     = true;
    m_previewRenderActive.store(true);
    ++m_previewRequestGeneration;
    if (!m_previewRenderWatcher.isRunning())
        startPendingPreviewRender();
}

void Document::startPendingPreviewRender()
{
    if (!m_previewRenderPending || m_previewRenderWatcher.isRunning())
        return;
    QVector<Layer> layers      = std::move(m_pendingPreviewLayers);
    const QSize    size        = m_pendingPreviewSize;
    const QSize    targetSize  = m_pendingPreviewTargetSize;
    const bool     draft       = m_pendingPreviewDraft;
    const bool     refine      = m_pendingPreviewRefine;
    m_previewRenderPending     = false;
    m_runningPreviewGeneration = m_previewRequestGeneration;
    m_runningPreviewDraft      = draft;
    m_runningPreviewRefine     = refine;
    m_runningPreviewTargetSize = targetSize;
    m_previewRenderWatcher.setFuture(
        QtConcurrent::run([layers = std::move(layers), size, targetSize, draft]() mutable {
            return renderPreviewSnapshot(std::move(layers), size, targetSize, draft);
        }));
}

QImage Document::renderPreviewSnapshot(QVector<Layer> _layers, const QSize &_size,
                                       const QSize &_targetSize, bool _draft)
{
    QSize renderSize = _size;
    if (_draft) {
        if (_targetSize.isValid())
            renderSize = _targetSize.boundedTo(_size);
        else if (qMax(_size.width(), _size.height()) > 960)
            renderSize.scale(960, 960, Qt::KeepAspectRatio);
    }
    if (renderSize != _size) {
        const qreal scale = qreal(renderSize.width()) / qMax(1, _size.width());
        for (Layer &layer : _layers) {
            if (!layer.image.isNull())
                layer.image =
                    cachedScreenProxy(layer.image, renderSize, QImage::Format_ARGB32_Premultiplied);
            if (!layer.mask.isNull())
                layer.mask = cachedScreenProxy(layer.mask, renderSize, QImage::Format_Grayscale8);
            if (!layer.source.isNull())
                layer.source = cachedScreenProxy(layer.source, renderSize,
                                                 QImage::Format_ARGB32_Premultiplied);
            for (const QString &key :
                 {QStringLiteral("radius"), QStringLiteral("distance"), QStringLiteral("cellSize")})
                if (layer.parameters.contains(key))
                    layer.parameters.insert(
                        key, qMax(1., layer.parameters.value(key).toDouble() * scale));
        }
    }
    Document renderer;
    renderer.m_layers             = std::move(_layers);
    renderer.m_size               = renderSize;
    renderer.m_cacheDirty         = true;
    renderer.m_fullCompositeDirty = true;
    renderer.m_dirtyRegion        = {};
    return renderer.composite().copy();
}
void Document::cancelLayerPreview()
{
    if (!m_previewingLayer)
        return;
    const bool changedPreview = m_layerPreviewChanged;
    m_previewingLayer         = false;
    m_layerPreviewChanged     = false;
    m_frequencyPreviewDraft   = false;
    if (changedPreview) {
        restore(m_beforeLayerPreview);
        m_composite          = m_beforeLayerPreviewComposite;
        m_cacheDirty         = false;
        m_fullCompositeDirty = false;
        m_dirtyRegion        = {};
    }
    m_beforeLayerPreview          = {};
    m_beforeLayerPreviewComposite = {};
}

void Document::beginLayerPropertiesSession()
{
    if (m_layerPropertiesSession)
        return;
    cancelLayerPreview();
    m_beforeLayerProperties     = snapshot(tr("Modifier les propriétés du calque"));
    m_undoBeforeLayerProperties = m_undo;
    m_redoBeforeLayerProperties = m_redo;
    m_layerPropertiesSession    = true;
}

void Document::acceptLayerPropertiesSession()
{
    if (!m_layerPropertiesSession)
        return;
    m_layerPropertiesSession = false;
    m_beforeLayerProperties  = {};
    m_undoBeforeLayerProperties.clear();
    m_redoBeforeLayerProperties.clear();
}

void Document::cancelLayerPropertiesSession()
{
    if (!m_layerPropertiesSession)
        return;
    cancelLayerPreview();
    const Snapshot before    = m_beforeLayerProperties;
    m_undo                   = m_undoBeforeLayerProperties;
    m_redo                   = m_redoBeforeLayerProperties;
    m_layerPropertiesSession = false;
    m_beforeLayerProperties  = {};
    m_undoBeforeLayerProperties.clear();
    m_redoBeforeLayerProperties.clear();
    restore(before);
}

QColor Document::sampleCurvePoint(const QString &_mode, int _x, int _y)
{
    static const QSet<QString> modes{QStringLiteral("black"), QStringLiteral("gray"),
                                     QStringLiteral("white")};
    if (!modes.contains(_mode) || m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].adjustmentType != QStringLiteral("curves") || m_size.isEmpty())
        return {};
    const QColor sampled = composite().pixelColor(qBound(0, _x, m_size.width() - 1),
                                                  qBound(0, _y, m_size.height() - 1));
    beginLayerPreview();
    previewLayerParameter(_mode + QStringLiteral("Sample"),
                          QVariantList{sampled.red(), sampled.green(), sampled.blue()});
    finishLayerPreview(tr("Échantillonner le point de la courbe"));
    return sampled;
}

QColor Document::sampleCompositeColor(int _x, int _y) const
{
    if (m_size.isEmpty())
        return {};
    return composite().pixelColor(qBound(0, _x, m_size.width() - 1),
                                  qBound(0, _y, m_size.height() - 1));
}

void Document::resetAdjustmentLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != QStringLiteral("adjustment"))
        return;

    const QString type = m_layers[m_active].adjustmentType;
    QVariantMap   parameters;
    if (type == QStringLiteral("color"))
        parameters = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("light"))
        parameters = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                      {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (type == QStringLiteral("brightnessContrast"))
        parameters = {{"brightness", 0}, {"contrast", 0}};
    else if (type == QStringLiteral("levels"))
        parameters = {{"inputBlack", 0},
                      {"inputWhite", 255},
                      {"gamma", 1.},
                      {"outputBlack", 0},
                      {"outputWhite", 255}};
    else if (type == QStringLiteral("curves"))
        parameters = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                              QVariantMap{{"x", 255}, {"y", 255}}}},
                      {"blackSample", QVariantList{}},
                      {"graySample", QVariantList{}},
                      {"whiteSample", QVariantList{}}};
    else if (type == QStringLiteral("exposure"))
        parameters = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (type == QStringLiteral("vibrance"))
        parameters = {{"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("hueSaturation"))
        parameters = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (type == QStringLiteral("colorBalance") || type == QStringLiteral("selectiveColor"))
        parameters = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (type == QStringLiteral("blackWhite"))
        parameters = {{"reds", 40}, {"greens", 40}, {"blues", 20}};
    else if (type == QStringLiteral("photoFilter"))
        parameters = {{"hue", 35}, {"density", 25}};
    else if (type == QStringLiteral("channelMixer"))
        parameters = {{"redRed", 100}, {"redGreen", 0},     {"redBlue", 0},
                      {"greenRed", 0}, {"greenGreen", 100}, {"greenBlue", 0},
                      {"blueRed", 0},  {"blueGreen", 0},    {"blueBlue", 100}};
    else if (type == QStringLiteral("colorLookup"))
        parameters = {{"preset", 0}};
    else if (type == QStringLiteral("posterize"))
        parameters = {{"levels", 4}};
    else if (type == QStringLiteral("threshold"))
        parameters = {{"level", 128}};
    else if (type == QStringLiteral("gradientMap"))
        parameters = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (type == QStringLiteral("gaussianBlur") || type == QStringLiteral("boxBlur"))
        parameters = {{"radius", 8.}};
    else if (type == QStringLiteral("motionBlur"))
        parameters = {{"distance", 20.}, {"angle", 0.}};
    else if (type == QStringLiteral("unsharpMask"))
        parameters = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (type == QStringLiteral("highPass"))
        parameters = {{"radius", 4.}};
    else if (type == QStringLiteral("mosaic"))
        parameters = {{"cellSize", 12}};
    else if (type == QStringLiteral("noise"))
        parameters = {{"amount", 8.}};

    parameters.insert(QStringLiteral("_maskDensity"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskDensity"), 100.));
    parameters.insert(QStringLiteral("_maskFeather"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskFeather"), 0.));
    beginLayerPreview();
    m_layers[m_active].parameters = parameters;
    m_layerPreviewChanged         = true;
    m_fullCompositeDirty          = true;
    m_cacheDirty                  = true;
    requestPreviewRender(true, true);
    emit layersChanged();
    finishLayerPreview(tr("Réinitialiser le réglage"));
}

void Document::resetCurveAdjustment()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].adjustmentType != QStringLiteral("curves"))
        return;
    beginLayerPreview();
    previewLayerParameter(
        QStringLiteral("points"),
        QVariantList{QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 255}, {"y", 255}}});
    for (const QString &key : {QStringLiteral("blackSample"), QStringLiteral("graySample"),
                               QStringLiteral("whiteSample")})
        previewLayerParameter(key, QVariantList{});
    finishLayerPreview(tr("Réinitialiser les courbes"));
}

void Document::applyFilter(const QString &_filter, qreal _amount)
{
    static const QStringList supported{"grayscale", "invert",     "sepia", "brightness",
                                       "contrast",  "saturation", "blur",  "sharpen"};
    if (!supported.contains(_filter) || !std::isfinite(_amount) || !editable())
        return;
    if (m_editingMask || m_layers[m_active].kind != "pixel") {
        fail(tr("Ce filtre destructif nécessite un calque de pixels."));
        return;
    }
    _amount = qBound(-100., _amount, 100.);
    edit(tr("Filtre : %1").arg(_filter), [&] {
        const QImage original = m_layers[m_active].image;
        QImage       result   = original.convertToFormat(QImage::Format_ARGB32);
        const QRect  area     = hasSelection() ? m_selection : result.rect();
        if (_filter == "blur" || _filter == "sharpen") {
            const int    radius  = _filter == "sharpen" ? 1 : qBound(1, int(_amount), 30);
            const QImage blurred = blurImage(original, radius);
            if (_filter == "blur") {
                result = original;
                QPainter p(&result);
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.setClipRect(area);
                p.drawImage(0, 0, blurred);
                p.end();
                m_layers[m_active].image = result;
                return;
            }
            const QImage smooth = blurred.convertToFormat(QImage::Format_ARGB32);
            for (int y = area.top(); y <= area.bottom(); ++y)
                for (int x = area.left(); x <= area.right(); ++x) {
                    const QColor c = result.pixelColor(x, y), b = smooth.pixelColor(x, y);
                    result.setPixelColor(x, y,
                                         QColor(qBound(0, 2 * c.red() - b.red(), 255),
                                                qBound(0, 2 * c.green() - b.green(), 255),
                                                qBound(0, 2 * c.blue() - b.blue(), 255),
                                                c.alpha()));
                }
        } else
            for (int y = area.top(); y <= area.bottom(); ++y) {
                auto row = reinterpret_cast<QRgb *>(result.scanLine(y));
                for (int x = area.left(); x <= area.right(); ++x) {
                    QRgb         pixel = row[x];
                    double       r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
                    const double gray = .2126 * r + .7152 * g + .0722 * b;
                    if (_filter == "invert") {
                        r = 255 - r;
                        g = 255 - g;
                        b = 255 - b;
                    } else if (_filter == "grayscale")
                        r = g = b = gray;
                    else if (_filter == "sepia") {
                        double nr = .393 * r + .769 * g + .189 * b,
                               ng = .349 * r + .686 * g + .168 * b;
                        b         = .272 * r + .534 * g + .131 * b;
                        r         = nr;
                        g         = ng;
                    } else if (_filter == "brightness") {
                        r += _amount * 2.55;
                        g += _amount * 2.55;
                        b += _amount * 2.55;
                    } else if (_filter == "contrast") {
                        const double f = std::pow((100 + _amount) / 100, 2);
                        r              = (r - 128) * f + 128;
                        g              = (g - 128) * f + 128;
                        b              = (b - 128) * f + 128;
                    } else if (_filter == "saturation") {
                        const double f = 1 + _amount / 100;
                        r              = gray + (r - gray) * f;
                        g              = gray + (g - gray) * f;
                        b              = gray + (b - gray) * f;
                    }
                    row[x] = qRgba(qBound(0, qRound(r), 255), qBound(0, qRound(g), 255),
                                   qBound(0, qRound(b), 255), qAlpha(pixel));
                }
            }
        m_layers[m_active].image = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    });
}
