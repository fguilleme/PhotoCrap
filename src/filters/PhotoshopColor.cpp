#include "PhotoshopColor.h"
#include "PhotoFilterFilter.h"
#include "PhotoshopPixels.h"
#include <QColorSpace>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

namespace PhotoCrap::Filters
{
namespace
{
using namespace Photoshop;
RGB mix(RGB a, RGB b, float t)
{
    return a + t * (b - a);
}

double decode(double x)
{
    x /= 255.;
    return x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4);
}

double encode(double x)
{
    x = std::clamp(x, 0., 1.);
    return 255. * (x <= .0031308 ? 12.92 * x : 1.055 * std::pow(x, 1. / 2.4) - .055);
}

// Each slab in the atlas is a 17^3 RGB step chart, blue varying fastest.
// Nine equally spaced positions per control include its neutral response.
constexpr int edge = 17;
using Cube         = std::array<RGB, edge * edge * edge>;
RGB sampleCube(const Cube &cube, RGB rgb)
{
    int base = 0;
    RGB t;
    for (int c = 0; c < 3; ++c) {
        const float v = std::clamp(rgb[c], 0.f, 255.f);
        int         i = std::min(int(v * (edge - 1) / 255.f), edge - 2);
        if (v < std::nearbyint(i * 255. / (edge - 1)))
            --i;
        else if (i < edge - 2 && v > std::nearbyint((i + 1) * 255. / (edge - 1)))
            ++i;
        // The chart's input coordinates were quantized to whole RGB values.
        const int lo = int(std::nearbyint(i * 255. / (edge - 1)));
        const int hi = int(std::nearbyint((i + 1) * 255. / (edge - 1)));
        base         = base * edge + i;
        t[c]         = (v - lo) / (hi - lo);
    }
    const auto line = [&](int r, int g) {
        const int k = base + (r * edge + g) * edge;
        return mix(cube[k], cube[k + 1], t.z());
    };
    return mix(mix(line(0, 0), line(0, 1), t.y()), mix(line(1, 0), line(1, 1), t.y()), t.x());
}

RGB unpack(QRgb p)
{
    return RGB(qRed(p), qGreen(p), qBlue(p));
}

RGB fromColor(QColor color)
{
    return RGB(color.red(), color.green(), color.blue());
}

RGB lmsRoot(RGB c)
{
    const double r = decode(c.x()), g = decode(c.y()), b = decode(c.z());
    return RGB(std::cbrt(.4122214708 * r + .5363325363 * g + .0514459929 * b),
               std::cbrt(.2119034982 * r + .6806995451 * g + .1073969566 * b),
               std::cbrt(.0883024619 * r + .2817188376 * g + .6299787005 * b));
}

RGB fromLmsRoot(RGB c)
{
    const double l = c.x() * c.x() * c.x(), m = c.y() * c.y() * c.y(), s = c.z() * c.z() * c.z();
    return RGB(encode(4.0767416621 * l - 3.3077115913 * m + .2309699292 * s),
               encode(-1.2684380046 * l + 2.6097574011 * m - .3413193965 * s),
               encode(-.0041960863 * l - .7034186147 * m + 1.707614701 * s));
}
} // namespace

QImage photoshopColorControls(const QImage &source, const QVariantMap &p)
{
    static const QImage atlas =
        QImage(":/psd/color-controls.png").convertToFormat(QImage::Format_RGB32);
    if (atlas.size() != QSize(edge * edge, 36 * edge)) {
        qWarning("Missing Photoshop color calibration");
        return source;
    }
    constexpr std::array keys{"temperature", "tint", "vibrance", "saturation"};
    std::vector<Cube>    cubes;
    for (int control = 0; control < 4; ++control) {
        const float setting = std::clamp(p.value(keys[control]).toFloat(), -100.f, 100.f);
        if (setting == 0.f)
            continue;
        const float pos  = (setting + 100.f) / 25.f;
        const int   lo   = std::min(int(pos), 7);
        const int   y    = edge * (control * 9 + lo);
        auto       &cube = cubes.emplace_back();
        for (int r = 0; r < edge; ++r) {
            const auto *a = reinterpret_cast<const QRgb *>(atlas.constScanLine(y + r));
            const auto *b = reinterpret_cast<const QRgb *>(atlas.constScanLine(y + r + edge));
            for (int gb = 0; gb < edge * edge; ++gb)
                cube[r * edge * edge + gb] = mix(unpack(a[gb]), unpack(b[gb]), pos - lo);
        }
    }
    if (cubes.empty())
        return source;
    // Compose once per call so rendering cost per pixel is independent of active controls.
    Cube combined;
    for (int i = 0; i < int(combined.size()); ++i) {
        RGB rgb(std::nearbyint((i / (edge * edge)) * 255. / 16),
                std::nearbyint(((i / edge) % edge) * 255. / 16),
                std::nearbyint((i % edge) * 255. / 16));
        for (const auto &cube : cubes)
            rgb = sampleCube(cube, rgb);
        combined[i] = rgb;
    }
    return mapColors(source, [&](RGB p) { return sampleCube(combined, p); });
}

QImage photoshopColorBalance(const QImage &source, const QVariantMap &p)
{
    static const QImage ramps =
        QImage(":/psd/color-balance.png").convertToFormat(QImage::Format_Grayscale8);
    if (ramps.size() != QSize(256, 27)) {
        qWarning("Missing Photoshop balance calibration");
        return source;
    }
    constexpr const char *keys[3][3] = {
        {"shadowCyanRed", "shadowMagentaGreen", "shadowYellowBlue"},
        {"cyanRed", "magentaGreen", "yellowBlue"},
        {"highlightCyanRed", "highlightMagentaGreen", "highlightYellowBlue"}};
    std::array<std::array<float, 256>, 3> tables;
    for (auto &t : tables)
        std::iota(t.begin(), t.end(), 0.f);
    for (int tone = 0; tone < 3; ++tone) {
        RGB         shift(p.value(keys[tone][0]).toFloat(), p.value(keys[tone][1]).toFloat(),
                          p.value(keys[tone][2]).toFloat());
        const float common = p.value("preserveLuminosity").toBool() ? luminosity(shift) : 0.f;
        for (int c = 0; c < 3; ++c) {
            const double setting = std::clamp(double(shift[c] - common), -100., 100.);
            if (setting == 0.)
                continue;
            const double pos = (setting + 100.) / 25.;
            const int    lo  = std::min(int(pos), 7);
            const auto  *a   = ramps.constScanLine(tone * 9 + lo),
                       *b    = ramps.constScanLine(tone * 9 + lo + 1);
            std::array<float, 256> curve;
            for (int x = 0; x < 256; ++x)
                curve[x] = std::lerp(float(a[x]), float(b[x]), float(pos - lo));
            for (auto &v : tables[c])
                v = interpolate(curve, v);
        }
    }
    return mapColors(source, [&](RGB v) {
        for (int c = 0; c < 3; ++c)
            v[c] = interpolate(tables[c], v[c]);
        return v;
    });
}

QImage photoshopPhotoFilter(const QImage &source, const QVariantMap &p)
{
    const RGB    filter  = fromColor(photoFilterColor(p));
    const double density = std::clamp(p.value("density", 25).toDouble() / 100., 0., 1.);
    if (!density)
        return source;
    // Linear-light transmission matrices measured with red and green filters.
    // The three primary transmissions sum to identity (a white filter).
    constexpr double red[3][3] = {
        {.538, .2872, .1748}, {-.0982, .1352, -.037}, {.0031, -.022, .0189}};
    constexpr double green[3][3] = {
        {.2871, -.3548, .0677}, {.1351, .8348, .0301}, {-.0217, -.0908, .1125}};
    std::array<RGB, 3> matrix;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const double identity = i == j ? 1. : 0.;
            matrix[i][j] =
                std::lerp(identity,
                          decode(filter.x()) * red[i][j] + decode(filter.y()) * green[i][j] +
                              decode(filter.z()) * (identity - red[i][j] - green[i][j]),
                          density);
        }
    const bool preserve = p.value("preserveLuminosity", true).toBool();
    return mapColors(source, [&](RGB rgb) {
        const RGB linear(decode(rgb.x()), decode(rgb.y()), decode(rgb.z()));
        RGB       out;
        for (int i = 0; i < 3; ++i)
            out[i] = encode(QVector3D::dotProduct(matrix[i], linear));
        if (preserve) {
            const float l = luminosity(rgb), delta = l - luminosity(out);
            out += RGB(delta, delta, delta);
            const float low  = std::min({out.x(), out.y(), out.z()}),
                        high = std::max({out.x(), out.y(), out.z()});
            if (low < 0)
                out = mix(RGB(l, l, l), out, l / (l - low));
            if (high > 255)
                out = mix(RGB(l, l, l), out, (255 - l) / (high - l));
        }
        return out;
    });
}

QImage photoshopGradientMap(const QImage &source, const QVariantMap &p)
{
    const RGB     dark   = fromColor(QColor(p.value("shadowColor", "#000000").toString()));
    const RGB     bright = fromColor(QColor(p.value("highlightColor", "#ffffff").toString()));
    const QString method = p.value("gradientMethod").toString();
    const RGB     a = lmsRoot(dark), b = lmsRoot(bright);
    // Interpolation weights measured on neutral and two-colour ramps in perceptual mode.
    constexpr std::array<float, 33> perceptual{
        0,     .022,  .0464, .075,  .1042, .135,  .1677, .2004, .2344, .2683, .301,
        .3364, .3706, .405,  .4377, .4712, .5027, .5339, .5662, .6001, .6333, .6692,
        .7024, .7375, .7703, .8042, .8381, .8699, .9001, .9302, .957,  .9814, 1};
    std::array<RGB, 1025> palette;
    for (int i = 0; i < int(palette.size()); ++i) {
        const float t = float(i) / (palette.size() - 1);
        if (method == "Smoo") {
            palette[i] = fromLmsRoot(mix(a, b, interpolate(perceptual, t * 255.)));
        } else if (method == "Lnr ") {
            const float weight = .5f * t * (1 + t * (3 - 2 * t));
            for (int c = 0; c < 3; ++c)
                palette[i][c] = encode(std::lerp(decode(dark[c]), decode(bright[c]), weight));
        } else
            palette[i] = mix(dark, bright, t);
    }
    return mapColors(source, [&](RGB p) { return interpolate(palette, luminosity(p)); });
}
} // namespace PhotoCrap::Filters
