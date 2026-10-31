#pragma once
#include <QtGlobal>
#include <cmath>

namespace PhotoCrap::Filters
{
// Shared native reference, including floating-point decisions at hard-mix boundaries.
inline double blendChannel(double _b, double _s, int _m)
{
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
}
} // namespace PhotoCrap::Filters
