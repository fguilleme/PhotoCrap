#pragma once
#include "storage/ProjectPayloadDevice.h"
#include "storage/ProjectReadIndex.h"
#include <QBuffer>
#include <QJsonArray>

// Materialize a legacy fixture for tests which deliberately inspect or corrupt
// old PNG/JSON fields. New-format integrity and native pixels have separate tests.
inline QJsonObject legacyProjectFixture(QIODevice &file)
{
    file.seek(0);
    if (file.peek(8) != QByteArray(PhotoCrap::Storage::BinaryHeader::Magic, 8))
        return QJsonDocument::fromJson(file.readAll()).object();
    ProjectReadIndex index;
    if (!index.read(file))
        return {};
    QJsonObject root = index.metadata;
    const QSize size(root["width"].toInt(), root["height"].toInt());
    const auto  format = root["channelBits"].toInt() == 16 ? QImage::Format_RGBA64_Premultiplied
                                                           : QImage::Format_ARGB32_Premultiplied;
    QJsonArray  layers;
    for (int i = 0; i < index.layers.size(); ++i) {
        bool       valid = false;
        const auto entry = index.layer(i, valid);
        if (!valid)
            return {};
        auto layer = entry.metadata;
        for (const QString &field :
             {QStringLiteral("png"), QStringLiteral("mask"), QStringLiteral("source")}) {
            const auto payload = entry.payload(field);
            QByteArray bytes;
            if (payload.size) {
                if (index.archive.isOriginal(payload.plane))
                    bytes = index.archive.original(payload.plane);
                else {
                    const bool compact = field == "png"
                                             ? layer["compactPixels"].toBool()
                                             : field == "mask" && layer["compactMask"].toBool();
                    const auto plane =
                        index.archive.plane(payload.plane, compact ? QSize(1, 1) : size,
                                            field == "mask" ? QImage::Format_Grayscale8 : format);
                    QBuffer buffer(&bytes);
                    buffer.open(QIODevice::WriteOnly);
                    if (plane.isNull() || !QImage(plane).save(&buffer, "PNG"))
                        return {};
                }
            }
            layer[field] = QString::fromLatin1(bytes.toBase64());
        }
        layers.append(layer);
    }
    root["layers"] = layers;
    file.seek(0);
    const auto                       headerBytes = file.read(64);
    PhotoCrap::Storage::BinaryHeader header;
    if (header.read(headerBytes.constData()) && header.preview) {
        const auto bytes = file.read(header.preview);
        root["preview"]  = QJsonObject{{"version", 1},
                                      {"encoding", "jpeg"},
                                      {"channelBits", root["channelBits"]},
                                      {"image", QString::fromLatin1(bytes.toBase64())}};
    }
    return root;
}
