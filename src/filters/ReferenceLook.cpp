#include "AdjustmentRecipe.h"

#include <QCoreApplication>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace PhotoCrap::Look
{
namespace
{
using Histogram = std::array<qint64, 256>;
struct Statistics {
    std::array<Histogram, 4> channels{};
    qint64                   weight = 0;
    qint64                   chroma = 0;
    double                   hueX = 0., hueY = 0., saturation = 0.;
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
            stats.hueX += weight * (2 * r - g - b);
            stats.hueY += weight * (g - b) * std::sqrt(3.);
            stats.weight += weight;
            const int high = std::max({r, g, b}), low = std::min({r, g, b});
            stats.chroma += weight * (high - low);
            // Saturation in the same HSL space as the editable layer.
            stats.saturation +=
                double(weight) * (high - low) / qMax(1, 255 - qAbs(high + low - 255));
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

QImage overview(const QImage &image)
{
    QSize size = image.size();
    if (qMax(size.width(), size.height()) > 384)
        size = size.scaled(384, 384, Qt::KeepAspectRatio).expandedTo({1, 1});
    return image.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}

// Uniformly sample the frame, keeping neighboring pixels at their original spacing.
QImage detailSample(const QImage &image)
{
    constexpr int tile = 64;
    const int     w = qMin(tile, image.width()), h = qMin(tile, image.height());
    const int cols = qBound(1, image.width() / tile, 4), rows = qBound(1, image.height() / tile, 4);
    QImage    sample(w * cols, h * rows, QImage::Format_ARGB32_Premultiplied);
    QPainter  painter(&sample);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const int left = (2 * x + 1) * image.width() / (2 * cols) - w / 2;
            const int top  = (2 * y + 1) * image.height() / (2 * rows) - h / 2;
            painter.drawImage(QPoint(x * w, y * h), image, QRect(left, top, w, h));
        }
    }
    return sample;
}

struct Detail {
    double                contrast = 0.;
    std::array<double, 3> noise{};
    int                   blocks = 0;
    int                   noiseBlocks = 0;
};

Detail measureDetail(const QImage &image)
{
    const QImage                 pixels = image.convertToFormat(QImage::Format_ARGB32);
    std::array<QList<double>, 3> residuals;
    Detail                       result;
    // Each block stays within a sampled tile. Second differences reject linear gradients.
    for (int y = 0; y + 16 <= pixels.height(); y += 8) {
        if (y % 64 > 48)
            continue;
        for (int x = 0; x + 16 <= pixels.width(); x += 8) {
            if (x % 64 > 48)
                continue;
            double                luma[16][16];
            std::array<double, 4> quadrant{};
            bool                  visible = true;
            for (int j = 0; j < 16 && visible; ++j) {
                const auto *row = reinterpret_cast<const QRgb *>(pixels.constScanLine(y + j));
                for (int i = 0; i < 16; ++i) {
                    const QRgb p = row[x + i];
                    if (qAlpha(p) < 32) {
                        visible = false;
                        break;
                    }
                    luma[j][i] = .2126 * qRed(p) + .7152 * qGreen(p) + .0722 * qBlue(p);
                    quadrant[2 * (j / 8) + i / 8] += luma[j][i] / 64.;
                }
            }
            if (!visible)
                continue;
            const double mean =
                (quadrant[0] + quadrant[1] + quadrant[2] + quadrant[3]) / (4. * 255.);
            if (mean < .08 || mean > .92)
                continue;
            const auto [low, high] = std::minmax_element(quadrant.begin(), quadrant.end());
            result.contrast += qMin(65., *high - *low);
            ++result.blocks;
            for (int k = 0; k < 3; ++k) {
                const int lag    = 1 + 2 * k;
                double horizontal = 0., vertical = 0., cross = 0.;
                for (int j = 5; j < 11; ++j)
                    for (int i = 5; i < 11; ++i) {
                        const double dx = luma[j][i - lag] + luma[j][i + lag] - 2. * luma[j][i];
                        const double dy = luma[j - lag][i] + luma[j + lag][i] - 2. * luma[j][i];
                        horizontal += dx * dx;
                        vertical += dy * dy;
                        cross += dx * dy;
                    }
                // Stripes, edges and repeating checker patterns have directional or
                // highly correlated second differences. Keep them out of the grain fit.
                if (k == 0) {
                    if (horizontal + vertical > 72. &&
                        (qMin(horizontal, vertical) < .15 * qMax(horizontal, vertical) ||
                         cross * cross > .94 * horizontal * vertical))
                        break;
                    ++result.noiseBlocks;
                }
                residuals[k].append(std::sqrt((horizontal + vertical) / 72.) / (4. * mean * (1. - mean)));
            }
        }
    }
    if (result.blocks)
        result.contrast /= result.blocks;
    if (result.noiseBlocks) {
        for (int k = 0; k < 3; ++k) {
            auto &samples = residuals[k];
            auto  it      = samples.begin() + samples.size() / 4;
            std::nth_element(samples.begin(), it, samples.end());
            result.noise[k] = *it;
        }
    }
    return result;
}

// Fit exposure and a white veil to tonal percentiles. Prefer exposure alone
// unless a change in the shape of the tonal response clearly supports dehaze.
QPair<double, int> exposureAndVeil(const Statistics &a, const Statistics &b)
{
    constexpr std::array<int, 5> ranks{10, 25, 50, 75, 90};
    std::array<int, 5>           input{}, target{};
    for (int i = 0; i < 5; ++i) {
        input[i]  = percentile(a.channels[0], a.weight, ranks[i]);
        target[i] = percentile(b.channels[0], b.weight, ranks[i]);
    }
    double     best = std::numeric_limits<double>::max(), exposure = 0.;
    int        veil     = 0;
    const bool hasRange = input.back() - input.front() >= 32;
    for (int d = -100; d <= 100; d += 5) {
        if (d && !hasRange)
            continue;
        for (int step = -100; step <= 100; ++step) {
            const double ev = step * .05, factor = std::exp2(ev);
            double       error = std::abs(ev) * .3 + std::abs(d) * .015 + (d ? 1.5 : 0.);
            for (int i = 0; i < 5; ++i) {
                const double v    = qMin(255., input[i] * factor);
                const double haze = .65 * d / 100. * v / 255.;
                const double gain = haze > 0. ? 1. / (1. - haze) : 1. + haze;
                error += std::abs(qBound(0., 255. + (v - 255.) * gain, 255.) - target[i]) / 5.;
            }
            if (error < best) {
                best     = error;
                exposure = ev;
                veil     = d;
            }
        }
    }
    return {exposure, veil};
}

QVariantMap matchGrain(const Detail &source, const Detail &target, const RenderAdjustment &render)
{
    if (source.noiseBlocks < qMax(4, source.blocks / 5) ||
        target.noiseBlocks < qMax(4, target.blocks / 5))
        return {};
    std::array<double, 3> missing{};
    for (int i = 0; i < 3; ++i)
        missing[i] = std::sqrt(
            qMax(0., target.noise[i] * target.noise[i] - source.noise[i] * source.noise[i]));
    if (target.noise[0] < source.noise[0] + .3 || missing[2] < 1.5)
        return {};

    // Calibrate the three spatial frequencies with the same grain generator used by the layer.
    QImage gray(128, 128, QImage::Format_RGB32);
    gray.fill(qRgb(128, 128, 128));
    double      best = std::numeric_limits<double>::max();
    QVariantMap params;
    for (double size : {1., 1.5, 2., 3., 4., 6., 8.}) {
        for (int roughness : {0, 25, 50, 75, 100}) {
            QVariantMap trial{{"size", size}, {"roughness", roughness}, {"amount", 50}};
            const auto  model = measureDetail(render(gray, "grain", trial)).noise;
            double      dot = 0., norm = 0.;
            for (int i = 0; i < 3; ++i) {
                dot += model[i] * missing[i];
                norm += model[i] * model[i];
            }
            const double scale = qBound(0., dot / qMax(.001, norm), 2.);
            double       error = 0.;
            for (int i = 0; i < 3; ++i)
                error += std::pow(scale * model[i] - missing[i], 2);
            if (error < best) {
                best             = error;
                params           = trial;
                params["amount"] = qRound(50. * scale);
            }
        }
    }
    // When the residual spectrum is unlike any grain this layer can produce,
    // applying the nearest candidate would transfer scene texture as noise.
    const double energy = missing[0] * missing[0] + missing[1] * missing[1] + missing[2] * missing[2];
    return best < .04 * energy && params.value("amount").toInt() >= 3 ? params : QVariantMap{};
}
} // namespace

Recipe matchReference(const QImage &source, const QImage &reference, const RenderAdjustment &render)
{
    Recipe     result;
    auto       current = overview(source);
    const auto ref     = overview(reference);
    auto       stats   = measure(current);
    const auto target  = measure(ref);
    if (!stats.weight || !target.weight) {
        result.error = QCoreApplication::translate(
            "PhotoLook", "La photo et la référence doivent contenir des pixels visibles.");
        return result;
    }
    auto       patches    = detailSample(source);
    const auto refPatches = detailSample(reference);
    if (current == ref && patches == refPatches)
        return result;
    const auto targetDetail = measureDetail(refPatches);
    const auto apply        = [&](const QString &type, const QVariantMap &params,
                           const QString &name = {}) {
        result.layers.append({type, params, name});
        patches          = render(patches, type, params);
        auto proxyParams = params;
        if (type == "clarityDehaze")
            proxyParams["radius"] = qMax(1, qRound(18. * qMax(current.width(), current.height()) /
                                                          qMax(source.width(), source.height())));
        current = render(current, type, proxyParams);
        stats   = measure(current);
    };
    if (stats.chroma > stats.weight) {
        const double ratio =
            (target.saturation / target.weight) / (stats.saturation / stats.weight);
        const int   saturation = qBound(-100, qRound(100 * (ratio - 1)), 100);
        QVariantMap color;
        if (qAbs(saturation) > 1)
            color["saturation"] = saturation;
        if (std::hypot(stats.hueX, stats.hueY) > .6 * stats.chroma &&
            std::hypot(target.hueX, target.hueY) > .6 * target.chroma &&
            target.chroma > target.weight) {
            const double angle =
                std::atan2(target.hueY, target.hueX) - std::atan2(stats.hueY, stats.hueX);
            const int hue = qRound(std::remainder(qRadiansToDegrees(angle), 360.));
            if (qAbs(hue) > 2)
                color["hue"] = hue;
        }
        if (!color.isEmpty())
            apply("hueSaturation", color);
    }
    const auto [ev, veil] = exposureAndVeil(stats, target);
    if (std::abs(ev) >= .05)
        apply("exposure", {{"exposure", ev}});
    QVariantMap presence;
    if (veil)
        presence["dehaze"] = veil;
    // Compare local contrast after accounting for haze and grain, both of which also
    // change the apparent contrast of a textured region.
    const auto basePatches  = veil ? render(patches, "clarityDehaze", presence) : patches;
    auto       detail       = measureDetail(basePatches);
    const auto initialGrain = matchGrain(detail, targetDetail, render);
    if (!initialGrain.isEmpty())
        detail = measureDetail(render(basePatches, "grain", initialGrain));
    if (detail.blocks >= 4 && targetDetail.blocks >= 4 && detail.contrast > 3.) {
        const double difference = targetDetail.contrast - detail.contrast;
        if (std::abs(difference) > 1.5)
            presence["clarity"] = qBound(-100, qRound(120. * difference / detail.contrast), 100);
    }
    if (!presence.isEmpty())
        apply("clarityDehaze", presence);
    // Model the final grain in the histogram so curves do not introduce the same contrast twice.
    const auto predictedTone = [&] {
        return initialGrain.isEmpty() ? stats : measure(render(current, "grain", initialGrain));
    };
    auto       predicted = predictedTone();
    const auto tone      = matchedPoints(predicted, target, 0);
    if (!tone.isEmpty()) {
        const QVariantMap params{{"points", tone}};
        apply("curves", params, QCoreApplication::translate("PhotoLook", "Tonalité"));
        predicted = predictedTone();
    }
    QVariantMap      channels;
    const std::array keys{"redPoints", "greenPoints", "bluePoints"};
    for (int c = 0; c < 3; ++c) {
        const auto points = matchedPoints(predicted, target, c + 1);
        if (!points.isEmpty())
            channels.insert(keys[c], points);
    }
    if (!channels.isEmpty())
        apply("curves", channels, QCoreApplication::translate("PhotoLook", "Courbes couleur"));
    const auto grain = matchGrain(measureDetail(patches), targetDetail, render);
    if (!grain.isEmpty())
        result.layers.append({"grain", grain, {}});
    return result;
}
} // namespace PhotoCrap::Look
