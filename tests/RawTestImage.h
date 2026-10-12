#pragma once
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QVector>
#include <algorithm>

namespace RawTestImage
{
// An original, uncompressed Bayer DNG. No embedded JPEG or external camera
// sample: a decoder must develop the sensor values to produce an image.
inline QByteArray dng(int _orientation = 1)
{
    struct Entry {
        quint16    tag, type;
        quint32    count;
        QByteArray data;
    };
    auto shorts = [](std::initializer_list<quint16> _values) {
        QByteArray  bytes;
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        for (auto value : _values)
            out << value;
        return bytes;
    };
    auto longs = [](std::initializer_list<quint32> _values) {
        QByteArray  bytes;
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        for (auto value : _values)
            out << value;
        return bytes;
    };
    const int      width = 128, height = 96;
    QVector<Entry> entries{{256, 4, 1, longs({width})},
                           {257, 4, 1, longs({height})},
                           {258, 3, 1, shorts({16})},
                           {259, 3, 1, shorts({1})},
                           {262, 3, 1, shorts({32803})},
                           {273, 4, 1, longs({0})},
                           {274, 3, 1, shorts({quint16(_orientation)})},
                           {277, 3, 1, shorts({1})},
                           {278, 4, 1, longs({height})},
                           {279, 4, 1, longs({width * height * 2})},
                           {284, 3, 1, shorts({1})},
                           {33421, 3, 2, shorts({2, 2})},
                           {33422, 1, 4, QByteArray::fromHex("00010102")},
                           {50706, 1, 4, QByteArray::fromHex("01040000")},
                           {50707, 1, 4, QByteArray::fromHex("01010000")},
                           {50708, 2, 17, QByteArray("PhotoCrap Sensor", 16) + char(0)},
                           {50710, 1, 3, QByteArray::fromHex("000102")},
                           {50711, 3, 1, shorts({1})},
                           {50714, 4, 1, longs({64})},
                           {50717, 4, 1, longs({65535})},
                           {50721, 10, 9,
                            longs({4360747, 10000000, 3850649, 10000000, 1430804, 10000000, 2225045,
                                   10000000, 7168786, 10000000, 606169, 10000000, 139322, 10000000,
                                   971045, 10000000, 7141733, 10000000})},
                           {50728, 5, 3, longs({1, 1, 1, 1, 1, 1})},
                           {50778, 3, 1, shorts({21})}};
    std::sort(entries.begin(), entries.end(),
              [](const Entry &_a, const Entry &_b) { return _a.tag < _b.tag; });
    const quint32 directoryEnd = 8 + 2 + entries.size() * 12 + 4;
    QByteArray    extra;
    for (const auto &entry : entries)
        if (entry.data.size() > 4) {
            extra += entry.data;
            if (extra.size() % 2)
                extra += char(0);
        }
    const quint32 sensorOffset = directoryEnd + extra.size();
    QByteArray    bytes;
    QDataStream   out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("II", 2);
    out << quint16(42) << quint32(8) << quint16(entries.size());
    quint32 offset = directoryEnd;
    for (const auto &entry : entries) {
        out << entry.tag << entry.type << entry.count;
        if (entry.tag == 273)
            out << sensorOffset;
        else if (entry.data.size() > 4) {
            out << offset;
            offset += (entry.data.size() + 1) & ~1;
        } else {
            QByteArray data = entry.data;
            data.append(4 - data.size(), char(0));
            out.writeRawData(data.constData(), 4);
        }
    }
    out << quint32(0);
    out.writeRawData(extra.constData(), extra.size());
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            out << quint16(1000 + x * 300 + y * 130 + ((x + y) % 2 ? 5000 : y % 2 ? 2000 : 10000));
    return bytes;
}
} // namespace RawTestImage
