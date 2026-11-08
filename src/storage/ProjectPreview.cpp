#include "ProjectPreview.h"
#include "ProjectPreviewScan.h"
#include <QBuffer>
#include <QColorSpace>
#include <QImageReader>
#include <QJsonDocument>
#include <QPainter>
#include <filesystem>
#include <fstream>

namespace PhotoCrap::Preview
{
QJsonObject encode(QImage image, const QSize &documentSize, int bits)
{
    if (image.isNull())
        return {};
    image = image.scaled(QSize(1280, 1280).boundedTo(image.size()), Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    // JPEG keeps the additional project data small. Transparency is shown on
    // a checkerboard; editable pixels and their alpha are saved separately.
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.setColorSpace(QColorSpace::SRgb);
    QPainter painter(&flat);
    for (int y = 0; y < flat.height(); y += 12)
        for (int x = 0; x < flat.width(); x += 12)
            painter.fillRect(x, y, 12, 12,
                             ((x / 12 + y / 12) % 2) ? QColor(214, 214, 214)
                                                     : QColor(238, 238, 238));
    painter.drawImage(0, 0, image);
    painter.end();
    QByteArray bytes;
    for (int attempt = 0; attempt < 8; ++attempt) {
        bytes.clear();
        QBuffer output(&bytes);
        output.open(QIODevice::WriteOnly);
        if (!flat.save(&output, "JPEG", attempt == 0 ? 82 : 65))
            return {};
        if (bytes.size() <= 512 * 1024)
            break;
        flat = flat.scaled(flat.size() * .75, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (bytes.size() > 512 * 1024)
        return {};
    return {{"version", 1},
            {"encoding", "jpeg"},
            {"width", flat.width()},
            {"height", flat.height()},
            {"documentWidth", documentSize.width()},
            {"documentHeight", documentSize.height()},
            {"channelBits", bits},
            {"image", QString::fromLatin1(bytes.toBase64())}};
}
QImage read(const QString &path, const QSize &bounds)
{
    std::ifstream input(std::filesystem::path(path.toStdU16String()), std::ios::binary);
    if (!input)
        return {};
    const auto entry = Scan(input).read();
    if (entry.json.empty())
        return {};
    const auto object = QJsonDocument::fromJson(QByteArray::fromStdString(entry.json)).object();
    if (object.value("version").toInt() != 1 || object.value("encoding") != "jpeg")
        return {};
    const auto bytes = QByteArray::fromBase64(object.value("image").toString().toLatin1(),
                                              QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty() || bytes.size() > 512 * 1024)
        return {};
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "JPEG");
    const auto   size = reader.size();
    if (size.isEmpty() || size.width() > 1280 || size.height() > 1280)
        return {};
    if (!bounds.isEmpty())
        reader.setScaledSize(size.scaled(bounds, Qt::KeepAspectRatio).boundedTo(size));
    return reader.read();
}
} // namespace PhotoCrap::Preview
