#pragma once

#include <QtMath>

namespace PhotoCrap::Filters
{
inline int noiseAmplitude(double _amount, bool _wide)
{
    const double amplitude = qBound(0., _amount, 100.) * 1.275;
    return _wide ? int(amplitude) : qRound(amplitude);
}

inline quint32 noiseSeed(int _x, int _y)
{
    return quint32(_x) * 0x9e3779b1u ^ quint32(_y) * 0x85ebca6bu;
}

inline int noiseDelta(quint32 &_hash, int _amplitude)
{
    _hash ^= _hash << 13;
    _hash ^= _hash >> 17;
    _hash ^= _hash << 5;
    return int(_hash % quint32(_amplitude * 2 + 1)) - _amplitude;
}
} // namespace PhotoCrap::Filters
