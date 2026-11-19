#include "AdjustmentRecipe.h"
#include "ColorFilter.h"
#include "CurvesFilter.h"

#include <QCoreApplication>
#include <algorithm>
#include <array>

namespace PhotoCrap::Look
{
namespace
{
using Histogram = std::array<qint64, 256>;
struct Statistics {
    std::array<Histogram, 4> channels{};
    qint64                   weight = 0;
    qint64                   chroma = 0;
};

Statistics measure(const QImage &image)
{
    const QImage pixels = image.convertToFormat(QImage::Format_ARGB32);
    Statistics   stats;
    for (int y = 0; y < pixels.height(); ++y) {
        const auto *row = reinterpret_cast<const QRgb *>(pixels.constScanLine(y));
        for (int x = 0; x < pixels.width(); ++x) {
            const auto p = row[x];
            const int  r = qRed(p), g = qGreen(p), b = qBlue(p), weight = qAlpha(p);
            stats.weight += weight;
            stats.chroma += weight * (std::max({r, g, b}) - std::min({r, g, b}));
            stats.channels[0][qRound(.2126 * r + .7152 * g + .0722 * b)] += weight;
            stats.channels[1][r] += weight;
            stats.channels[2][g] += weight;
            stats.channels[3][b] += weight;
        }
    }
    return stats;
}

int percentile(const Histogram &histogram, qint64 weight, int percent)
{
    qint64 cumulative = 0;
    for (int i = 0; i < 255; ++i) {
        cumulative += histogram[i];
        if (100 * cumulative >= percent * weight)
            return i;
    }
    return 255;
}

QVariantList matchedPoints(const Statistics &source, const Statistics &reference, int channel)
{
    // Repeated percentiles in a flat area must produce a single control point.
    QMap<int, QList<int>> targets;
    for (const int percent : {2, 10, 25, 50, 75, 90, 98}) {
        const int x = percentile(source.channels[channel], source.weight, percent);
        targets[x].append(percentile(reference.channels[channel], reference.weight, percent));
    }
    if (!targets.contains(0))
        targets[0] = {0};
    if (!targets.contains(255))
        targets[255] = {255};

    QVariantList points;
    bool         changed = false;
    for (const auto &[x, values] : targets.asKeyValueRange()) {
        const auto n = values.size();
        const int  y = (values[(n - 1) / 2] + values[n / 2]) / 2;
        changed |= qAbs(x - y) > 1;
        points.append(QVariantMap{{"x", x}, {"y", y}});
    }
    return changed ? points : QVariantList{};
}
} // namespace

Recipe matchReference(const QImage &source, const QImage &reference)
{
    Recipe     result;
    auto       current = source;
    auto       stats   = measure(current);
    const auto target  = measure(reference);
    if (!stats.weight || !target.weight) {
        result.error = QCoreApplication::translate(
            "PhotoLook", "La photo et la référence doivent contenir des pixels visibles.");
        return result;
    }
    const auto tone = matchedPoints(stats, target, 0);
    if (!tone.isEmpty()) {
        const QVariantMap params{{"points", tone}};
        result.layers.append(
            {"curves", params, QCoreApplication::translate("PhotoLook", "Tonalité")});
        current = Filters::applyCurves(current, params);
        stats   = measure(current);
    }
    if (stats.chroma > stats.weight) {
        const double ratio =
            (double(target.chroma) / target.weight) / (double(stats.chroma) / stats.weight);
        const int saturation = qBound(-100, qRound(100 * (ratio - 1)), 100);
        if (qAbs(saturation) > 1) {
            const QVariantMap params{{"saturation", saturation}};
            result.layers.append(
                {"color", params, QCoreApplication::translate("PhotoLook", "Saturation")});
            stats = measure(Filters::applyColor(current, params));
        }
    }
    QVariantMap      channels;
    const std::array keys{"redPoints", "greenPoints", "bluePoints"};
    for (int c = 0; c < 3; ++c) {
        const auto points = matchedPoints(stats, target, c + 1);
        if (!points.isEmpty())
            channels.insert(keys[c], points);
    }
    if (!channels.isEmpty())
        result.layers.append(
            {"curves", channels, QCoreApplication::translate("PhotoLook", "Courbes couleur")});
    return result;
}
} // namespace PhotoCrap::Look
