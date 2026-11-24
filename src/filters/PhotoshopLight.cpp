// Approximate Photoshop Light in display RGB. Each row is the output of a
// grayscale step chart at one slider setting. Interpolate between measurements
// before composing the six controls so the response stays monotone as they move.
#include "PhotoshopLight.h"
#include "PhotoshopPixels.h"
#include <numeric>

#include <algorithm>
#include <array>
#include <cmath>

namespace PhotoCrap::Filters
{
namespace
{
using Samples = std::array<int, 13>;
constexpr Samples input{0, 8, 16, 32, 48, 64, 96, 128, 160, 192, 224, 240, 255};
struct Response {
    double  setting;
    Samples output;
};

constexpr Response exposure[]{
    {-5, {0, 0, 1, 1, 2, 3, 6, 9, 13, 19, 29, 38, 56}},
    {-2, {0, 2, 4, 10, 16, 21, 35, 50, 68, 92, 128, 155, 198}},
    {-1, {0, 4, 9, 18, 27, 38, 60, 83, 108, 141, 185, 213, 255}},
    {0, input},
    {1, {0, 15, 28, 55, 79, 102, 146, 181, 207, 229, 245, 251, 255}},
    {2, {0, 26, 49, 90, 123, 153, 196, 220, 235, 246, 252, 254, 255}},
    {5, {0, 119, 178, 227, 242, 249, 253, 254, 255, 255, 255, 255, 255}},
};

constexpr Response contrast[]{
    {-100, {0, 21, 33, 54, 72, 89, 116, 137, 154, 175, 207, 229, 255}},
    {-50, {0, 14, 24, 42, 60, 77, 107, 133, 157, 183, 216, 235, 255}},
    {0, input},
    {50, {0, 4, 8, 21, 35, 50, 83, 122, 164, 202, 232, 245, 255}},
    {100, {0, 1, 2, 8, 20, 34, 70, 116, 169, 212, 241, 250, 255}},
};

constexpr Response blacks[]{
    {-100, {0, 0, 0, 0, 0, 5, 49, 91, 134, 178, 219, 238, 255}},
    {-50, {0, 0, 0, 17, 35, 52, 87, 121, 155, 190, 223, 240, 255}},
    {0, input},
    {50, {0, 11, 20, 40, 58, 74, 107, 138, 167, 196, 225, 240, 255}},
    {100, {0, 14, 26, 49, 68, 85, 117, 146, 172, 198, 226, 241, 255}},
};

constexpr Response whites[]{
    {-100, {0, 8, 16, 32, 47, 62, 92, 119, 147, 173, 198, 216, 255}},
    {-50, {0, 8, 16, 32, 47, 62, 92, 121, 150, 180, 212, 231, 255}},
    {0, input},
    {50, {0, 8, 16, 33, 49, 66, 102, 138, 174, 209, 241, 254, 255}},
    {100, {0, 8, 17, 36, 57, 79, 131, 180, 219, 249, 255, 255, 255}},
};

constexpr Response highlights[]{
    {-100, {0, 7, 15, 28, 41, 55, 82, 108, 122, 143, 164, 183, 225}},
    {-50, {0, 8, 15, 30, 45, 59, 89, 118, 141, 168, 197, 215, 244}},
    {0, input},
    {50, {0, 8, 17, 34, 52, 69, 103, 139, 179, 214, 246, 253, 255}},
    {100, {0, 9, 18, 37, 55, 74, 111, 150, 196, 232, 251, 254, 255}},
};

constexpr Response shadows[]{
    {-100, {0, 1, 2, 11, 20, 35, 71, 109, 144, 181, 219, 238, 255}},
    {-50, {0, 2, 6, 19, 31, 48, 83, 118, 152, 187, 221, 239, 255}},
    {0, input},
    {50, {0, 24, 34, 52, 70, 84, 111, 138, 168, 197, 227, 241, 255}},
    {100, {0, 62, 69, 80, 99, 107, 128, 148, 176, 202, 229, 242, 255}},
};

template <size_t N>
void applyResponse(std::array<double, 256> &curve, const Response (&responses)[N], double setting,
                   double extent = 1.)
{
    if (!std::isfinite(setting) || setting == 0.)
        return;
    setting = std::clamp(setting, responses[0].setting, responses[N - 1].setting);
    const Response *high =
        std::lower_bound(responses + 1, responses + N, setting,
                         [](const Response &point, double v) { return point.setting < v; });
    const Response &low      = high[-1];
    const double    fraction = (setting - low.setting) / (high->setting - low.setting);
    std::array<double, input.size()> output;
    for (size_t i = 0; i < input.size(); ++i)
        output[i] = std::lerp(double(low.output[i]), double(high->output[i]), fraction);

    // Shadows and highlights in Photoshop also respond to surrounding pixels.
    // Approximate their tonal range globally so tiled and full-image renders agree.
    size_t right = 1;
    for (double &value : curve) {
        const double x = std::min(value / extent, 255.);
        while (right + 1 < input.size() && input[right] < x)
            ++right;
        const size_t left = right - 1;
        const double t    = (x - input[left]) / (input[right] - input[left]);
        value =
            std::clamp(value + extent * (std::lerp(output[left], output[right], t) - x), 0., 255.);
    }
}
} // namespace

QImage applyPhotoshopLight(const QImage &source, const QVariantMap &params)
{
    std::array<double, 256> curve;
    for (size_t i = 0; i < curve.size(); ++i)
        curve[i] = double(i);
    auto shadowCurve = curve, highlightCurve = curve;
    applyResponse(shadowCurve, shadows, params.value("shadows").toDouble(), .28);
    applyResponse(highlightCurve, highlights, params.value("highlights").toDouble(), .75);
    applyResponse(curve, exposure, params.value("exposure").toDouble());
    applyResponse(curve, blacks, params.value("blacks").toDouble());
    applyResponse(curve, contrast, params.value("contrast").toDouble());
    applyResponse(curve, whites, params.value("whites").toDouble());

    // The local tone controls primarily follow brightness. Lifting shadows per
    // channel independently produces a hue shift on saturated input.
    return Photoshop::mapColors(source, [&](Photoshop::RGB rgb) {
        const float l     = Photoshop::luminosity(rgb);
        const float delta = Photoshop::interpolate(shadowCurve, l) - l;
        for (int c = 0; c < 3; ++c)
            rgb[c] = std::clamp(
                float(.75 * (rgb[c] + delta) + .25 * Photoshop::interpolate(shadowCurve, rgb[c])),
                0.f, 255.f);
        const float next = Photoshop::luminosity(rgb);
        const float hi   = Photoshop::interpolate(highlightCurve, next) - next;
        for (int c = 0; c < 3; ++c)
            rgb[c] = Photoshop::interpolate(curve, rgb[c] + hi);
        return rgb;
    });
}
} // namespace PhotoCrap::Filters
