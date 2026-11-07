#include "ImageClipboard.h"
#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QUuid>

namespace PhotoCrap::Clipboard
{
namespace
{
const QString metadataType = QStringLiteral("application/x-photocrap-selection");
const QString nativeType   = QStringLiteral("application/x-photocrap-native-png");
Pixels        cached;
QString       cachedToken;
quint64       changes   = 0;
bool          connected = false;
bool          validSize(const QSize &_size)
{
    return _size.width() > 0 && _size.height() > 0 && _size.width() <= 16384 &&
           _size.height() <= 16384 && qint64(_size.width()) * _size.height() <= 32LL * 1024 * 1024;
}
QClipboard *clipboard()
{
    auto *board = QGuiApplication::clipboard();
    if (!connected) {
        connected = true;
        QObject::connect(board, &QClipboard::dataChanged, board, [] {
            ++changes;
            const auto *mime = QGuiApplication::clipboard()->mimeData();
            const auto  data =
                mime ? QJsonDocument::fromJson(mime->data(metadataType)).object() : QJsonObject{};
            if (data.value("token").toString() != cachedToken) {
                cached = {};
                cachedToken.clear();
            }
        });
    }
    return board;
}
// PNG is generated only if another process requests PhotoCrap's native format.
// Local pastes retain the exact QImage, including fractional 16-bit alpha.
class MimeData final : public QMimeData
{
public:
    explicit MimeData(const QImage &_image) : m_image(_image)
    {
        setImageData(_image);
    }
    QStringList formats() const override
    {
        auto names = QMimeData::formats();
        names.append(nativeType);
        return names;
    }

protected:
    QVariant retrieveData(const QString &_type, QMetaType _preferred) const override
    {
        if (_type != nativeType)
            return QMimeData::retrieveData(_type, _preferred);
        if (m_png.isEmpty()) {
            QBuffer buffer(&m_png);
            buffer.open(QIODevice::WriteOnly);
            m_image.save(&buffer, "PNG");
        }
        return m_png;
    }

private:
    QImage             m_image;
    mutable QByteArray m_png;
};
} // namespace
quint64 revision()
{
    clipboard();
    return changes;
}
bool hasImage()
{
    const auto *mime = clipboard()->mimeData();
    return mime && (mime->hasImage() || mime->hasFormat(nativeType));
}
void write(const Pixels &_pixels)
{
    auto *board            = clipboard();
    cached                 = _pixels;
    cachedToken            = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *            mime = new MimeData(_pixels.image);
    const QJsonObject data{{"version", 1},
                           {"token", cachedToken},
                           {"positioned", _pixels.positioned},
                           {"x", _pixels.origin.x()},
                           {"y", _pixels.origin.y()},
                           {"width", _pixels.canvas.width()},
                           {"height", _pixels.canvas.height()}};
    mime->setData(metadataType, QJsonDocument(data).toJson(QJsonDocument::Compact));
    board->setMimeData(mime);
}
Pixels read()
{
    const auto *mime = clipboard()->mimeData();
    if (!mime)
        return {};
    const auto data = QJsonDocument::fromJson(mime->data(metadataType)).object();
    if (!cachedToken.isEmpty() && data.value("token").toString() == cachedToken)
        return cached;
    Pixels result;
    if (mime->hasFormat(nativeType)) {
        auto bytes = mime->data(nativeType);
        if (bytes.size() > 512LL * 1024 * 1024)
            return {};
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "PNG");
        if (!validSize(reader.size()))
            return {};
        result.image = reader.read();
    } else
        result.image = qvariant_cast<QImage>(mime->imageData());
    if (!validSize(result.image.size()))
        return {};
    result.image.setDevicePixelRatio(1.);
    const QSize  size(data.value("width").toInt(), data.value("height").toInt());
    const QPoint point(data.value("x").toInt(), data.value("y").toInt());
    if (data.value("version").toInt() == 1 && data.value("positioned").toBool() &&
        validSize(size) && qAbs(qint64(point.x())) <= 16384 && qAbs(qint64(point.y())) <= 16384) {
        result.positioned = true;
        result.origin     = point;
        result.canvas     = size;
    }
    return result;
}
} // namespace PhotoCrap::Clipboard
