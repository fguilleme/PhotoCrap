#include "ColorLookupFilter.h"
#include "ImagePixels.h"
#include "NoiseMath.h"

#include <QColorSpace>
#include <QThreadPool>
#include <QVector>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <numeric>
#include <vector>

namespace
{
struct LookTable {
    int                size = 0;
    std::vector<float> samples;

    bool load(int edge, const QByteArray &bytes)
    {
        if (edge < 2 || edge > 64 || bytes.size() != edge * edge * edge * 12)
            return false;
        samples.resize(bytes.size() / 4);
        for (size_t i = 0; i < samples.size(); ++i) {
            samples[i] =
                std::bit_cast<float>(qFromLittleEndian<quint32>(bytes.constData() + 4 * i));
            if (!std::isfinite(samples[i]) || std::abs(samples[i]) > 65536)
                return false;
        }
        size = edge;
        return true;
    }

    void apply(double &red, double &green, double &blue) const
    {
        const double x = red * (size - 1) / 255., y = green * (size - 1) / 255.,
                     z  = blue * (size - 1) / 255.;
        const int    ix = std::min(int(x), size - 2), iy = std::min(int(y), size - 2),
                     iz      = std::min(int(z), size - 2);
        const int    first   = 3 * ((iz * size + iy) * size + ix);
        // Follow the tetrahedron containing the sample inside this lattice cell.
        struct Axis { double fraction; int stride; };
        std::array<Axis,3> axes{{{x-ix,3},{y-iy,3*size},{z-iz,3*size*size}}};
        std::sort(axes.begin(),axes.end(),[](Axis a,Axis b){ return a.fraction>b.fraction; });
        const auto channel = [&](int c) {
            int vertex = first+c;
            double result = samples[vertex];
            for (const auto axis : axes) {
                const int next=vertex+axis.stride;
                result+=axis.fraction*(samples[next]-samples[vertex]);
                vertex=next;
            }
            return 255.*result;
        };
        red   = channel(0);
        green = channel(1);
        blue  = channel(2);
    }
};

double filmCurve(double _value)
{
    const double normalized = _value / 255.;
    return (normalized * normalized * (3. - 2. * normalized)) * 255.;
}

void applyStyle(int _preset, double &_red, double &_green, double &_blue)
{
    switch (_preset) {
    case 0: // Ambre doux
        _red   = filmCurve(_red) * 1.04 + 7.;
        _green = filmCurve(_green) * 1.01 + 2.;
        _blue  = filmCurve(_blue) * .91;
        break;
    case 1: // Cinéma chaud
        _red   = filmCurve(_red) * 1.06 + 4.;
        _green = filmCurve(_green) * .99 + 3.;
        _blue  = filmCurve(_blue) * .93 + 8.;
        break;
    case 2: // Bleu arctique
        _red   = filmCurve(_red) * .9;
        _green = filmCurve(_green) * 1.01 + 3.;
        _blue  = filmCurve(_blue) * 1.1 + 7.;
        break;
    case 3: // Mat
        _red   = 24. + _red * .8;
        _green = 25. + _green * .79;
        _blue  = 28. + _blue * .77;
        break;
    case 4: { // Sarcelle et orange
        const double luminance = .2126 * _red + .7152 * _green + .0722 * _blue;
        const double shadow    = 1. - luminance / 255.;
        const double light     = luminance / 255.;
        _red += 22. * light - 12. * shadow;
        _green += 11. * shadow;
        _blue += 24. * shadow - 16. * light;
        break;
    }
    default: // Pellicule passée
        _red   = 18. + filmCurve(_red) * .86 + _green * .04;
        _green = 15. + filmCurve(_green) * .84 + _blue * .04;
        _blue  = 20. + filmCurve(_blue) * .79 + _red * .03;
        break;
    }
}
} // namespace

namespace PhotoCrap::Filters
{
// A SpeedGrade .look carries a baked RGB lattice. Its hex triplets are little-endian
// floats with red changing fastest. No external preset file is needed.
bool readLookTable(const QByteArray &xml, QVariantMap &parameters)
{
    QXmlStreamReader reader(xml);
    if (!reader.readNextStartElement() || reader.name() != u"look")
        return false;
    int        edge = 0;
    QByteArray encoded;
    while (reader.readNextStartElement()) {
        if (reader.name() != u"LUT") {
            reader.skipCurrentElement();
            continue;
        }
        while (reader.readNextStartElement()) {
            if (reader.name() == u"size")
                edge = reader.readElementText().remove('"').trimmed().toInt();
            else if (reader.name() == u"data")
                encoded = reader.readElementText().toLatin1();
            else
                reader.skipCurrentElement();
        }
    }
    if (reader.hasError() || edge < 2 || edge > 64)
        return false;
    QByteArray hex;
    hex.reserve(encoded.size());
    for (char c : encoded) {
        if (c == '"' || c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return false;
        hex.append(c);
    }
    if (hex.size() != edge * edge * edge * 24)
        return false;
    const auto bytes = QByteArray::fromHex(hex);
    LookTable  table;
    if (!table.load(edge, bytes))
        return false;
    parameters = {{"preset", -1},
                  {"lookupSize", edge},
                  {"strength", 100},
                  {"lookupData", QString::fromLatin1(bytes.toBase64())}};
    return true;
}

template <typename Pixel>
QImage applyColorLookupPixels(const QImage &_source, const QVariantMap &_parameters, QPoint origin)
{
    using P = ImageUtils::PixelAccess<Pixel>;
    if (_source.isNull())
        return _source;
    const int    preset   = _parameters.value("preset", 0).toInt();
    const double strength = qBound(0., _parameters.value("strength", 100.).toDouble() / 100., 1.);
    const bool   preserveLuminosity = _parameters.value("preserveLuminosity", false).toBool();
    if (strength <= 0.)
        return _source;

    const bool dither = _parameters.value("lookupDither").toBool();
    LookTable table;
    if (preset == -1 &&
        !table.load(_parameters.value("lookupSize").toInt(),
                    QByteArray::fromBase64(_parameters.value("lookupData").toByteArray())))
        return _source;

    const QImage input = _source.convertToFormat(P::format);
    QImage       result(input.size(), P::format);
    const int    jobCount =
        qMin(input.height(), qMax(1, QThreadPool::globalInstance()->maxThreadCount() * 2));
    QVector<int> jobs(jobCount);
    std::iota(jobs.begin(), jobs.end(), 0);
    ImageUtils::blockingMap(jobs, [&](int _job) {
        for (int y = input.height() * _job / jobCount; y < input.height() * (_job + 1) / jobCount;
             ++y) {
            const auto *source = reinterpret_cast<const Pixel *>(input.constScanLine(y));
            auto       *output = reinterpret_cast<Pixel *>(result.scanLine(y));
            for (int x = 0; x < input.width(); ++x) {
                const Pixel  pixel     = source[x];
                const double sourceRed = P::red(pixel), sourceGreen = P::green(pixel),
                             sourceBlue = P::blue(pixel);
                double       red = sourceRed, green = sourceGreen, blue = sourceBlue;
                if (table.size)
                    table.apply(red, green, blue);
                else
                    applyStyle(qBound(0, preset, 5), red, green, blue);
                if (preserveLuminosity) {
                    const double sourceLum =
                        .2126 * sourceRed + .7152 * sourceGreen + .0722 * sourceBlue;
                    const double styledLum = .2126 * red + .7152 * green + .0722 * blue;
                    red += sourceLum - styledLum;
                    green += sourceLum - styledLum;
                    blue += sourceLum - styledLum;
                }
                red   = sourceRed + (red - sourceRed) * strength;
                green = sourceGreen + (green - sourceGreen) * strength;
                blue  = sourceBlue + (blue - sourceBlue) * strength;
                if (dither && table.size) {
                    quint32 hash = noiseSeed(origin.x()+x,origin.y()+y);
                    const double noise = noiseDelta(hash,32767) / (65534.*(P::wide ? 257. : 1.));
                    red += strength*noise;
                    green += strength*noise;
                    blue += strength*noise;
                }
                output[x] =
                    P::rgba(qBound(0., P::round(red), 255.), qBound(0., P::round(green), 255.),
                            qBound(0., P::round(blue), 255.), P::alpha(pixel));
            }
        }
    });
    result.setColorSpace(QColorSpace::SRgb);
    return result.convertToFormat(P::premultiplied);
}
QImage applyColorLookup(const QImage &_source, const QVariantMap &_parameters, QPoint origin)
{
    return ImageUtils::is16Bit(_source) ? applyColorLookupPixels<QRgba64>(_source, _parameters, origin)
                                        : applyColorLookupPixels<QRgb>(_source, _parameters, origin);
}
} // namespace PhotoCrap::Filters
