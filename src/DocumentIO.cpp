#include "Document.h"
#include "ImageUtils.h"

#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSaveFile>

namespace
{
constexpr qint64 MaxProjectBytes = 256LL * 1024 * 1024;

QString localPath(const QUrl &_url)
{
    return _url.isLocalFile() ? _url.toLocalFile() : QString{};
}
} // namespace

bool Document::newDocument(int _w, int _h, bool _transparent)
{
    if (!validSize(_w, _h))
        return false;
    m_size       = {_w, _h};
    QImage image = ImageUtils::blank(m_size);
    if (!_transparent)
        image.fill(Qt::white);
    m_layers     = {{tr("Arrière-plan"), image}};
    m_active     = 0;
    m_name       = tr("Sans titre");
    m_projectUrl = QUrl{};
    resetHistory(true);
    return true;
}
QImage Document::readImage(const QString &_path)
{
    QImageReader reader(_path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || !validSize(size.width(), size.height())) {
        if (!size.isValid())
            fail(tr("Format non reconnu ou image illisible."));
        return {};
    }
    QImage image = reader.read();
    if (image.isNull()) {
        fail(reader.errorString());
        return {};
    }
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    else
        image.setColorSpace(QColorSpace::SRgb);
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
bool Document::openFile(const QUrl &_url)
{
    const QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Choisissez un fichier local."));
    if (QFileInfo(path).suffix().compare("pcr", Qt::CaseInsensitive) == 0)
        return loadProject(path);
    auto image = readImage(path);
    if (image.isNull())
        return false;
    m_size       = image.size();
    m_layers     = {{QFileInfo(path).completeBaseName(), image}};
    m_active     = 0;
    m_name       = QFileInfo(path).completeBaseName();
    m_projectUrl = QUrl{};
    resetHistory(false);
    return true;
}
bool Document::importLayer(const QUrl &_url)
{
    if (!validSize(width(), height(), int(m_layers.size()) + 1))
        return false;
    auto image = readImage(localPath(_url));
    if (image.isNull())
        return false;
    edit(tr("Importer un calque"), [&] {
        QImage   target = ImageUtils::blank(m_size);
        QPainter painter(&target);
        painter.drawImage((width() - image.width()) / 2, (height() - image.height()) / 2, image);
        painter.end();
        m_layers.insert(++m_active, {QFileInfo(_url.toLocalFile()).completeBaseName(), target});
        selectOnly(m_active);
    });
    return true;
}
bool Document::saveProject(const QUrl &_url)
{
    endStroke();
    QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Chemin de sauvegarde invalide."));
    if (!path.endsWith(".pcr", Qt::CaseInsensitive))
        path += ".pcr";
    QJsonArray layers;
    for (const auto &layer : m_layers) {
        auto encode = [&](const QImage &_image) -> QString {
            if (_image.isNull())
                return {};
            QByteArray bytes;
            QBuffer    buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            return _image.save(&buffer, "PNG") ? QString::fromLatin1(bytes.toBase64()) : QString{};
        };
        const QString png = encode(layer.image), mask = encode(layer.mask),
                      source = encode(layer.source);
        if ((!layer.image.isNull() && png.isEmpty()) || (!layer.mask.isNull() && mask.isEmpty()) ||
            (!layer.source.isNull() && source.isEmpty()))
            return fail(tr("Impossible d’encoder les données du calque."));
        layers.append(QJsonObject{{"name", layer.name},
                                  {"visible", layer.visible},
                                  {"locked", layer.locked},
                                  {"opacity", layer.opacity},
                                  {"blend", layer.blend},
                                  {"kind", layer.kind},
                                  {"adjustmentType", layer.adjustmentType},
                                  {"parameters", QJsonObject::fromVariantMap(layer.parameters)},
                                  {"png", png},
                                  {"mask", mask},
                                  {"maskEnabled", layer.maskEnabled},
                                  {"clippingMask", layer.clippingMask},
                                  {"groupId", layer.groupId},
                                  {"groupCollapsed", layer.groupCollapsed},
                                  {"source", source}});
    }
    QJsonObject      root{{"format", "PhotoCrap"},   {"version", 5},
                          {"width", width()},        {"height", height()},
                          {"activeLayer", m_active}, {"layers", layers}};
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (data.size() > MaxProjectBytes)
        return fail(tr("Le projet dépasse 256 Mio. Réduisez ses dimensions ou ses calques."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return fail(file.errorString());
    m_savedId    = m_stateId;
    m_projectUrl = QUrl::fromLocalFile(path);
    m_name       = QFileInfo(path).completeBaseName();
    emit documentChanged();
    emit historyChanged();
    emit statusMessage(tr("Projet enregistré"));
    return true;
}
bool Document::loadProject(const QString &_path)
{
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    if (file.size() > MaxProjectBytes)
        return fail(tr("Projet trop volumineux (256 Mio maximum)."));
    QJsonParseError error;
    const auto      json    = QJsonDocument::fromJson(file.readAll(), &error);
    const auto      root    = json.object();
    const int       version = root["version"].toInt();
    if (error.error != QJsonParseError::NoError || root["format"] != "PhotoCrap" ||
        (version < 1 || version > 5))
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    const int  w = root["width"].toInt(), h = root["height"].toInt();
    const auto array = root["layers"].toArray();
    if (!validSize(w, h, int(array.size())))
        return false;
    QVector<Layer> layers;
    for (const auto &value : array) {
        auto object = value.toObject();
        auto decode = [&](const QString &_encoded, QImage::Format _format) -> QImage {
            if (_encoded.isEmpty())
                return {};
            QByteArray bytes = QByteArray::fromBase64(_encoded.toLatin1(),
                                                      QByteArray::AbortOnBase64DecodingErrors);
            QBuffer    buffer(&bytes);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "PNG");
            if (reader.size() != QSize(w, h))
                return {};
            return reader.read().convertToFormat(_format);
        };
        const QString kind =
            version == 1 ? QStringLiteral("pixel") : object["kind"].toString("pixel");
        if (kind != "pixel" && kind != "adjustment" && kind != "group" && kind != "frequency" &&
            kind != "frequencyLow" && kind != "frequencyHigh")
            return fail(tr("Type de calque inconnu."));
        auto image  = decode(object["png"].toString(), QImage::Format_ARGB32_Premultiplied);
        auto mask   = decode(object["mask"].toString(), QImage::Format_Grayscale8);
        auto source = decode(object["source"].toString(), QImage::Format_ARGB32_Premultiplied);
        if ((kind == "pixel" && image.isNull()) ||
            (kind.startsWith("frequency") && source.isNull()) ||
            (!object["mask"].toString().isEmpty() && mask.isNull()))
            return fail(tr("Calque corrompu ou dimensions incohérentes."));
        if (!image.isNull())
            image.setColorSpace(QColorSpace::SRgb);
        if (!source.isNull())
            source.setColorSpace(QColorSpace::SRgb);
        const double     opacity    = object["opacity"].toDouble(-1);
        int              blend      = object["blend"].toInt(-1);
        static const int legacyV1[] = {0, 3, 8, 12, 2, 7, 19};
        static const int legacyV2[] = {0, 3, 8, 12, 2, 7, 19, 13, 14, 9, 4, 20, 10};
        if (version == 1 && blend >= 0 && blend < 7)
            blend = legacyV1[blend];
        else if (version == 2 && blend >= 0 && blend < 13)
            blend = legacyV2[blend];
        if (opacity < 0 || opacity > 1 || blend < 0 || blend > 26)
            return fail(tr("Propriétés de calque invalides."));
        Layer layer{object["name"].toString().left(256), image,   object["visible"].toBool(true),
                    object["locked"].toBool(),           opacity, blend};
        layer.kind           = kind;
        layer.adjustmentType = object["adjustmentType"].toString();
        layer.parameters     = object["parameters"].toObject().toVariantMap();
        layer.mask           = mask;
        layer.maskEnabled    = object["maskEnabled"].toBool(true);
        layer.clippingMask   = object["clippingMask"].toBool(false);
        layer.groupId        = object["groupId"].toString().left(64);
        layer.groupCollapsed = object["groupCollapsed"].toBool(false);
        layer.source         = source;
        layers.append(layer);
        if (version == 2 && kind == "frequency") {
            layers.last().kind       = "pixel";
            layers.last().image      = source;
            layers.last().source     = {};
            layers.last().parameters = {};
        }
    }
    // Only replace the current document after validating every layer.
    m_layers     = layers;
    m_size       = {w, h};
    m_active     = qBound(0, root["activeLayer"].toInt(), int(layers.size()) - 1);
    m_projectUrl = QUrl::fromLocalFile(_path);
    m_name       = QFileInfo(_path).completeBaseName();
    resetHistory(true);
    return true;
}
bool Document::exportImage(const QUrl &_url)
{
    endStroke();
    QString path = localPath(_url);
    if (path.isEmpty())
        return fail(tr("Chemin d’export invalide."));
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        path += ".png";
        suffix = "png";
    }
    if (suffix != "png" && suffix != "jpg" && suffix != "jpeg" && suffix != "bmp" &&
        suffix != "webp")
        return fail(
            tr("Export disponible en PNG, JPEG, BMP ou WebP (si le module Qt est installé)."));
    QImage result = composite();
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "bmp") {
        QImage white(result.size(), QImage::Format_RGB32);
        white.fill(Qt::white);
        QPainter p(&white);
        p.drawImage(0, 0, result);
        p.end();
        result = white;
    }
    result.setColorSpace(QColorSpace::SRgb);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    QImageWriter writer(&file, suffix.toLatin1());
    writer.setQuality(95);
    if (!writer.write(result))
        return fail(writer.errorString());
    if (!file.commit())
        return fail(file.errorString());
    emit statusMessage(tr("Image exportée"));
    return true;
}
