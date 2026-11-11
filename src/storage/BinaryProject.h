#pragma once
#include "PixelPlane.h"
#include "ProjectBinaryHeader.h"
#include <QCborArray>
#include <QIODevice>
#include <QJsonObject>
#include <functional>

namespace PhotoCrap::Storage
{
constexpr qint64 MaxOriginalBytes = 512LL * 1024 * 1024;
using Continue = std::function<bool()>;
class BinaryProjectWriter
{
public:
    BinaryProjectWriter(QIODevice &file, QByteArray preview, Continue progress);
    int  plane(const PixelPlane &plane);
    int  original(const QByteArray &bytes);
    bool finish(QJsonObject metadata);

private:
    int                    block(const QByteArray &raw, int width, int height, int bpp);
    QIODevice &            m_file;
    Continue               m_continue;
    BinaryHeader           m_header;
    QCborArray             m_planes, m_blocks;
    QHash<qint64, int>     m_versions;
    QHash<QByteArray, int> m_hashes;
    QHash<QByteArray, int> m_planeHashes;
    bool                   m_ok = true;
};
class BinaryProjectReader
{
public:
    bool        read(QIODevice &file, Continue progress = {});
    QJsonObject metadata;
    PixelPlane  plane(int id, QSize expected, QImage::Format format);
    QByteArray  original(int id);
    bool        isOriginal(int id) const;
    int         planeCount() const
    {
        return int(m_planes.size());
    }

private:
    QByteArray                 block(int id, int width, int height, int bpp);
    QIODevice *                m_file = nullptr;
    Continue                   m_continue;
    BinaryHeader               m_header;
    QCborArray                 m_planes, m_blocks;
    QHash<QString, PixelPlane> m_decoded;
};
QByteArray packProjectBytes(const QByteArray &bytes);
QByteArray unpackProjectBytes(const QByteArray &bytes, qint64 size);
} // namespace PhotoCrap::Storage
