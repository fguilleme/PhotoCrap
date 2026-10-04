#include "Document.h"
#include "ImageUtils.h"
#include "filters/BlackWhiteFilter.h"
#include "filters/BrightnessContrastFilter.h"
#include "filters/ChannelMixerFilter.h"
#include "filters/ColorFilter.h"
#include "filters/ColorLookupFilter.h"
#include "filters/CurvesFilter.h"
#include "filters/GradientMapFilter.h"
#include "filters/LevelsFilter.h"
#include "filters/LightFilter.h"
#include "filters/PhotoFilterFilter.h"
#include "filters/PosterizeFilter.h"
#include "filters/SelectiveColorFilter.h"
#include "filters/ThresholdFilter.h"

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
    if (_type == "solidColor") {
        QImage result(_source.size(), QImage::Format_ARGB32_Premultiplied);
        result.fill(QColor(_p.value("color", QStringLiteral("#000000")).toString()));
        result.setColorSpace(QColorSpace::SRgb);
        return result;
    }
    if (_type == "curves")
        return PhotoCrap::Filters::applyCurves(_source, _p);
    if (_type == "levels")
        return PhotoCrap::Filters::applyLevels(_source, _p);
    if (_type == "blackWhite")
        return PhotoCrap::Filters::applyBlackWhite(_source, _p);
    if (_type == "brightnessContrast")
        return PhotoCrap::Filters::applyBrightnessContrast(_source, _p);
    if (_type == "channelMixer")
        return PhotoCrap::Filters::applyChannelMixer(_source, _p);
    if (_type == "gradientMap")
        return PhotoCrap::Filters::applyGradientMap(_source, _p);
    if (_type == "color")
        return PhotoCrap::Filters::applyColor(_source, _p);
    if (_type == "colorLookup")
        return PhotoCrap::Filters::applyColorLookup(_source, _p);
    if (_type == "light")
        return PhotoCrap::Filters::applyLight(_source, _p);
    if (_type == "photoFilter")
        return PhotoCrap::Filters::applyPhotoFilter(_source, _p);
    if (_type == "posterize")
        return PhotoCrap::Filters::applyPosterize(_source, _p);
    if (_type == "selectiveColor")
        return PhotoCrap::Filters::applySelectiveColor(_source, _p);
    if (_type == "threshold")
        return PhotoCrap::Filters::applyThreshold(_source, _p);
    if (_type == "colorBalance") {
        const QImage input = _source.convertToFormat(QImage::Format_ARGB32);
        QImage       result(input.size(), QImage::Format_ARGB32);
        const int    redShift   = qRound(_p.value("cyanRed").toDouble() * 2.55);
        const int    greenShift = qRound(_p.value("magentaGreen").toDouble() * 2.55);
        const int    blueShift  = qRound(_p.value("yellowBlue").toDouble() * 2.55);
        const int    jobCount =
            qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
        QVector<int> jobs(jobCount);
        std::iota(jobs.begin(), jobs.end(), 0);
        ImageUtils::blockingMap(jobs, [&](int _job) {
            for (int y = input.height() * _job / jobCount;
                 y < input.height() * (_job + 1) / jobCount; ++y) {
                const auto *source = reinterpret_cast<const QRgb *>(input.constScanLine(y));
                auto       *output = reinterpret_cast<QRgb *>(result.scanLine(y));
                for (int x = 0; x < input.width(); ++x) {
                    const QRgb pixel = source[x];
                    output[x] = qRgba(qBound(0, qRed(pixel) + redShift, 255),
                                      qBound(0, qGreen(pixel) + greenShift, 255),
                                      qBound(0, qBlue(pixel) + blueShift, 255), qAlpha(pixel));
                }
            }
        });
        result.setColorSpace(QColorSpace::SRgb);
        return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
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
                } else if (_type == "invert") {
                    r = 255 - r;
                    g = 255 - g;
                    b = 255 - b;
                }
                output[x] = qRgba(clamp(r), clamp(g), clamp(b), alpha);
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
