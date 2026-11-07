#include "GradientRaster.h"
#include "Document.h"
#include "ImageUtils.h"
#include <QPainter>
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace
{
using Color = std::array<float, 4>;

float linear(float value)
{
    return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
}

float srgb(float value)
{
    value = qBound(0.f, value, 1.f);
    return value <= .0031308f ? 12.92f * value : 1.055f * std::pow(value, 1.f / 2.4f) - .055f;
}

Color encodeColor(const QColor &color, int method)
{
    if (method == 0)
        return {float(color.redF()), float(color.greenF()), float(color.blueF()), float(color.alphaF())};
    const float r = linear(color.redF()), g = linear(color.greenF()), b = linear(color.blueF());
    if (method == 1)
        return {r, g, b, float(color.alphaF())};
    const float l = std::cbrt(.4122214708f * r + .5363325363f * g + .0514459929f * b);
    const float m = std::cbrt(.2119034982f * r + .6806995451f * g + .1073969566f * b);
    const float s = std::cbrt(.0883024619f * r + .2817188376f * g + .6299787005f * b);
    return {.2104542553f * l + .7936177850f * m - .0040720468f * s,
            1.9779984951f * l - 2.4285922050f * m + .4505937099f * s,
            .0259040371f * l + .7827717662f * m - .8086757660f * s, float(color.alphaF())};
}

Color decodeColor(Color value, int method)
{
    if (method == 2) {
        const float l0 = value[0] + .3963377774f * value[1] + .2158037573f * value[2];
        const float m0 = value[0] - .1055613458f * value[1] - .0638541728f * value[2];
        const float s0 = value[0] - .0894841775f * value[1] - 1.2914855480f * value[2];
        const float l = l0 * l0 * l0, m = m0 * m0 * m0, s = s0 * s0 * s0;
        value[0] = 4.0767416621f * l - 3.3077115913f * m + .2309699292f * s;
        value[1] = -1.2684380046f * l + 2.6097574011f * m - .3413193965f * s;
        value[2] = -.0041960863f * l - .7034186147f * m + 1.7076147010f * s;
    }
    for (int channel = 0; channel < 3; ++channel)
        value[channel] = (method == 0 ? value[channel] : srgb(value[channel])) * value[3];
    return value;
}

Color mix(const Color &a, const Color &b, float t)
{
    Color result;
    for (size_t channel = 0; channel < result.size(); ++channel)
        result[channel] = std::lerp(a[channel], b[channel], t);
    return result;
}

float noise(int x, int y)
{
    quint32 hash = (quint32(x) * 73856093u) ^ (quint32(y) * 19349663u);
    hash = (hash ^ (hash >> 16)) * 0x7feb352du;
    return float((hash ^ (hash >> 15)) & 65535u) / 65536.f - .5f;
}
}

namespace PhotoCrap::Tools
{
constexpr int GradientResolution = 4096;

void GradientRaster::prepareRamp() const
{
    constexpr int resolution = GradientResolution;
    if (ramp.empty()) {
        ramp.resize(resolution + 1);
        QList<Color> encoded;
        for (const auto &stop : stops)
            encoded.append(encodeColor(stop.second, method));
        qsizetype next = 1;
        for (int i = 0; i <= resolution; ++i) {
            const float t = float(i) / resolution;
            while (next < stops.size() - 1 && t > stops[next].first)
                ++next;
            const float startPosition = stops[next - 1].first, endPosition = stops[next].first;
            const float weight = qBound(0.f, (t - startPosition) / (endPosition - startPosition), 1.f);
            ramp[i] = decodeColor(mix(encoded[next - 1], encoded[next], weight), method);
        }
    }
}

bool GradientRaster::paint(QPainter &painter, const QRect &area, bool wide) const
{
    if (shape == 0 && method == 0 && !dither) {
        QLinearGradient gradient(start, end);
        gradient.setStops(stops);
        painter.fillRect(area, gradient);
        return true;
    }
    const QImage pixels = image(area, wide);
    if (pixels.isNull())
        return false;
    painter.drawImage(area.topLeft(), pixels);
    return true;
}

QImage GradientRaster::image(const QRect &area, bool wide) const
{
    prepareRamp();
    constexpr int resolution = GradientResolution;
    const QPointF axis = end - start;
    const double lengthSquared = qMax(1.e-12, QPointF::dotProduct(axis, axis));
    const auto position = [&](int x, int y) {
        const double dx = x + .5 - start.x(), dy = y + .5 - start.y();
        const double along = (dx * axis.x() + dy * axis.y()) / lengthSquared;
        const double across = (dx * axis.y() - dy * axis.x()) / lengthSquared;
        switch (shape) {
        case 1: return std::hypot(along, across);
        case 2: {
            const double turn = std::atan2(across, along) / (2. * std::numbers::pi);
            return turn < 0 ? turn + 1. : turn;
        }
        case 3: return std::abs(along);
        case 4: return std::abs(along) + std::abs(across);
        default: return along;
        }
    };
    QImage image(area.size(), wide ? QImage::Format_RGBA64_Premultiplied
                                   : QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        return {};
    const int maximum = wide ? 65535 : 255;
    for (int y = 0; y < image.height(); ++y) {
        auto *row = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const int px = area.x() + x, py = area.y() + y;
            const float index = float(qBound(0., position(px, py), 1.)) * resolution;
            const int low = qMin(resolution - 1, int(index));
            const auto value = mix(ramp[low], ramp[low + 1], index - low);
            const int alpha = qRound(value[3] * maximum);
            const float offset = dither && index > 0 && index < resolution ? noise(px, py) : 0.f;
            const auto channel = [&](int component) {
                return qBound(0, qRound(value[component] * maximum + offset), alpha);
            };
            if (wide)
                reinterpret_cast<QRgba64 *>(row)[x] =
                    QRgba64::fromRgba64(channel(0), channel(1), channel(2), alpha);
            else
                reinterpret_cast<QRgb *>(row)[x] =
                    qRgba(channel(0), channel(1), channel(2), alpha);
        }
    }
    return image;
}

QImage GradientRaster::palette() const
{
    prepareRamp();
    QImage image(256, GradientResolution / 256 + 1, QImage::Format_RGBA32FPx4);
    if (image.isNull())
        return {};
    for (int row = 0; row < image.height(); ++row) {
        auto *output = reinterpret_cast<float *>(image.scanLine(row));
        for (int col = 0; col < image.width(); ++col) {
            const auto &value = ramp[qMin(row * 256 + col, GradientResolution)];
            std::copy(value.cbegin(), value.cend(), output + col * 4);
        }
    }
    return image;
}

QVariantMap GradientRaster::encode(const QSize &size) const
{
    QVariantList entries;
    for (const auto &[position, color] : stops)
        entries.append(QVariant(QVariantList{position, color.redF(), color.greenF(), color.blueF(), color.alphaF()}));
    return {{"x0", start.x() / size.width()}, {"y0", start.y() / size.height()},
            {"x1", end.x() / size.width()}, {"y1", end.y() / size.height()},
            {"stops", entries}, {"shape", shape}, {"method", method}, {"dither", dither}};
}

bool GradientRaster::decode(const QVariantMap &data, const QSize &size, GradientRaster &result)
{
    if (size.isEmpty())
        return false;
    GradientRaster value;
    const auto coordinate = [&](const char *key, int scale) {
        bool ok = false;
        const double number = data.value(QLatin1String(key)).toDouble(&ok);
        return ok && std::isfinite(number) && qAbs(number) <= 32768.
                   ? number * scale : qQNaN();
    };
    value.start = {coordinate("x0", size.width()), coordinate("y0", size.height())};
    value.end = {coordinate("x1", size.width()), coordinate("y1", size.height())};
    if (!std::isfinite(value.start.x()) || !std::isfinite(value.start.y()) ||
        !std::isfinite(value.end.x()) || !std::isfinite(value.end.y()))
        return false;
    value.shape = data.value("shape").toInt();
    value.method = data.value("method").toInt();
    value.dither = data.value("dither").toBool();
    if (value.shape < 0 || value.shape > 4 || value.method < 0 || value.method > 2)
        return false;
    const auto entries = data.value("stops").toList();
    if (entries.size() < 2 || entries.size() > 64)
        return false;
    qreal previous = -1;
    for (const auto &entry : entries) {
        const auto components = entry.toList();
        if (components.size() != 5)
            return false;
        std::array<double, 5> parts;
        for (int i = 0; i < 5; ++i) {
            bool ok = false;
            parts[i] = components[i].toDouble(&ok);
            if (!ok || !std::isfinite(parts[i]) || parts[i] < 0 || parts[i] > 1)
                return false;
        }
        if (parts[0] <= previous)
            return false;
        value.stops.append({parts[0], QColor::fromRgbF(parts[1], parts[2], parts[3], parts[4])});
        previous = parts[0];
    }
    if (value.stops.first().first != 0 || value.stops.last().first != 1)
        return false;
    result = std::move(value);
    return true;
}
}

QGradientStops Document::gradientStops(int preset) const
{
    auto clear = m_color;
    clear.setAlpha(0);
    QGradientStops stops;
    switch (preset) {
    case 1: stops = {{0., m_color}, {1., clear}}; break;
    case 2: stops = {{0., Qt::black}, {1., Qt::white}}; break;
    case 3: stops = {{0., QColor("#372368")}, {.55, QColor("#bf4867")}, {1., QColor("#ffc77d")}}; break;
    case 4: stops = {{0., QColor("#ea4335")}, {.25, QColor("#ffda48")},
                     {.5, QColor("#38bd83")}, {.75, QColor("#3e8ed9")}, {1., QColor("#913ab4")}}; break;
    default: stops = {{0., m_color}, {1., m_secondary}}; break;
    }
    if (m_gradientReverse) {
        std::reverse(stops.begin(), stops.end());
        for (auto &stop : stops)
            stop.first = 1. - stop.first;
    }
    return stops;
}

QVariantList Document::gradientPreview() const
{
    QVariantList result;
    for (const auto &[position, color] : gradientStops(m_gradientPreset))
        result.append(QVariantMap{{"position", position}, {"color", color}});
    return result;
}

using PhotoCrap::Tools::GradientRaster;

bool Document::activeGradientLayer() const
{
    return m_active >= 0 && m_active < m_layers.size() &&
           m_layers[m_active].adjustmentType == "gradientFill";
}

bool Document::dynamicGradientActive() const
{
    return m_tool == "gradient" && !m_editingMask && activeGradientLayer() &&
           m_layers[m_active].visible && !m_layers[m_active].locked &&
           (m_gradientGesture != CreateGradient || m_gradientGestureChanged);
}

bool Document::gradientDragging() const
{
    return m_tool == "gradient" && (m_drawing || m_gradientGesture != NoGradientGesture);
}

QPointF Document::gradientStart() const
{
    GradientRaster gradient;
    if (dynamicGradientActive() &&
        GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                               gradient))
        return gradient.start;
    return m_anchor;
}

QPointF Document::gradientEnd() const
{
    GradientRaster gradient;
    if (dynamicGradientActive() &&
        GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                               gradient))
        return gradient.end;
    return m_last;
}

QVariantList Document::dynamicGradientStops() const
{
    QVariantList result;
    GradientRaster gradient;
    if (dynamicGradientActive() &&
        GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                               gradient))
        for (const auto &[position, color] : gradient.stops)
            result.append(QVariantMap{{"position", position}, {"color", color}});
    return result;
}

void Document::beginDynamicGradient(const QPointF &_point)
{
    if (m_size.isEmpty() || !QRectF(QPointF{}, m_size).contains(_point))
        return;
    m_anchor = m_last = _point;
    m_gradientGesture = CreateGradient;
    m_gradientGestureChanged = false;
    emit gradientGuideChanged();
}

bool Document::beginGradientHandle(int _handle)
{
    if (!dynamicGradientActive() || _handle < GradientStartHandle)
        return false;
    endStroke();
    if (_handle >= dynamicGradientStops().size())
        return false;
    finishLayerPreview(tr("Modifier les propriétés du calque"));
    acceptLayerPropertiesSession();
    cancelOperation();
    m_beforeStroke = snapshot(tr("Modifier le dégradé"));
    m_gradientGesture = _handle;
    m_gradientGestureChanged = false;
    return true;
}

void Document::applyGradientParameters(const QVariantMap &_parameters)
{
    auto &parameters = m_layers[m_active].parameters;
    if (parameters.value("gradient").toMap() == _parameters)
        return;
    parameters.insert("gradient", _parameters);
    m_gradientGestureChanged = true;
    m_fullCompositeDirty = true;
    m_cacheDirty = true;
    requestPreviewRender(true, false, false);
    emit gradientGuideChanged();
}

void Document::updateGradientHandle(qreal _x, qreal _y)
{
    if (m_gradientGesture == NoGradientGesture || !std::isfinite(_x) || !std::isfinite(_y))
        return;
    const QPointF point(qBound(-32768., _x, 32768.), qBound(-32768., _y, 32768.));
    GradientRaster gradient;
    if (m_gradientGesture == CreateGradient && !m_gradientGestureChanged) {
        if (QLineF(m_anchor, point).length() < .1)
            return;
        const auto mask = hasSelection() ? selectionMask() : ImageUtils::uniformMask(255);
        // Creating a tool layer finalizes any earlier edit, so suspend this gesture temporarily.
        m_gradientGesture = NoGradientGesture;
        if (!validLayerStorage(width(), height(), 0, hasSelection() ? 1 : 0) ||
            !startToolLayer(tr("Dégradé"))) {
            emit gradientGuideChanged();
            return;
        }
        m_gradientGesture = CreateGradient;
        auto &layer = m_layers[m_active];
        layer.kind = "adjustment";
        layer.adjustmentType = "gradientFill";
        layer.mask = mask;
        gradient.start = m_anchor;
        gradient.end = point;
        gradient.stops = gradientStops(m_gradientPreset);
        gradient.shape = qBound(0, m_gradientShape, 4);
        gradient.method = qBound(0, m_gradientMethod, 2);
        gradient.dither = m_gradientDither;
        applyGradientParameters(gradient.encode(m_size));
        emit layersChanged();
        return;
    }
    if (!activeGradientLayer() ||
        !GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                                gradient))
        return;
    if (m_gradientGesture == GradientStartHandle)
        gradient.start = point;
    else if (m_gradientGesture < 0)
        gradient.end = point;
    else if (m_gradientGesture > 0 && m_gradientGesture < gradient.stops.size() - 1) {
        const auto axis = gradient.end - gradient.start;
        const double extent = QPointF::dotProduct(axis, axis);
        const double low = gradient.stops[m_gradientGesture - 1].first;
        const double high = gradient.stops[m_gradientGesture + 1].first;
        const double spacing = qMin(.002, (high - low) / 4.);
        if (extent > 1.e-12)
            gradient.stops[m_gradientGesture].first =
                qBound(low + spacing, QPointF::dotProduct(point - gradient.start, axis) / extent,
                       high - spacing);
    }
    applyGradientParameters(gradient.encode(m_size));
}

void Document::finishDynamicGradient(bool _cancel)
{
    const bool creation = m_gradientGesture == CreateGradient;
    const bool changed = m_gradientGestureChanged;
    m_gradientGesture = NoGradientGesture;
    m_gradientGestureChanged = false;
    if (changed) {
        if (_cancel) {
            restore(m_beforeStroke);
            requestPreviewRender(true, true, false);
        } else {
            commit(m_beforeStroke, creation ? tr("Créer un dégradé") : tr("Modifier le dégradé"),
                   true, false);
            requestPreviewRender(false, false, false);
        }
    }
    m_beforeStroke = {};
    m_strokeCreatedLayer = false;
    emit gradientGuideChanged();
}

void Document::addGradientStop(qreal _position)
{
    GradientRaster gradient;
    if (!dynamicGradientActive() || !std::isfinite(_position) ||
        !GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                                gradient) || gradient.stops.size() >= 64)
        return;
    const auto next = std::upper_bound(gradient.stops.cbegin(), gradient.stops.cend(), _position,
                                      [](double t, const QGradientStop &stop) { return t < stop.first; });
    const auto index = next - gradient.stops.cbegin();
    if (index < 1 || index >= gradient.stops.size() || _position <= (next - 1)->first + .002 ||
        _position >= next->first - .002)
        return;
    const auto &a = *(next - 1);
    const auto &b = *next;
    const auto color = decodeColor(
        mix(encodeColor(a.second, gradient.method), encodeColor(b.second, gradient.method),
            (_position - a.first) / (b.first - a.first)), gradient.method);
    const QColor sampled = color[3] > 0
                              ? QColor::fromRgbF(qBound(0.f, color[0] / color[3], 1.f),
                                                 qBound(0.f, color[1] / color[3], 1.f),
                                                 qBound(0.f, color[2] / color[3], 1.f), color[3])
                              : QColor(Qt::transparent);
    gradient.stops.insert(index, {_position, sampled});
    if (beginGradientHandle(GradientEndHandle)) {
        applyGradientParameters(gradient.encode(m_size));
        finishDynamicGradient(false);
    }
}

void Document::removeGradientStop(int _index)
{
    GradientRaster gradient;
    if (dynamicGradientActive() &&
        GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                               gradient) &&
        _index > 0 && _index < gradient.stops.size() - 1 && beginGradientHandle(GradientEndHandle)) {
        gradient.stops.removeAt(_index);
        applyGradientParameters(gradient.encode(m_size));
        finishDynamicGradient(false);
    }
}

void Document::setGradientStopColor(int _index, const QColor &_color)
{
    GradientRaster gradient;
    if (_color.isValid() && dynamicGradientActive() &&
        GradientRaster::decode(m_layers[m_active].parameters.value("gradient").toMap(), m_size,
                               gradient) &&
        _index >= 0 && _index < gradient.stops.size() && beginGradientHandle(GradientEndHandle)) {
        gradient.stops[_index].second = _color;
        applyGradientParameters(gradient.encode(m_size));
        finishDynamicGradient(false);
    }
}
