#include "PsdReader.h"
#include "DocumentStorage.h"
#include "filters/SelectiveColorFilter.h"
#include "filters/ColorLookupFilter.h"
#include <bit>
#include <cmath>
#include <QColorSpace>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cstring>
#include <zlib.h>

namespace PhotoCrap::Psd
{
namespace
{
QString tr(const char *message)
{
    return QCoreApplication::translate("PsdReader", message);
}

void require(bool ok)
{
    if (!ok)
        throw tr("Données PSD invalides ou incomplètes.");
}

// Each length-prefixed section gets its own bounds, including compressed rows.
struct Cursor {
    QByteArrayView bytes;

    QByteArrayView take(qsizetype n)
    {
        require(n >= 0 && n <= bytes.size());
        auto part = bytes.first(n);
        bytes     = bytes.sliced(n);
        return part;
    }

    int u8()
    {
        return quint8(take(1)[0]);
    }
    int u16()
    {
        return qFromBigEndian<quint16>(take(2).data());
    }
    quint32 u32()
    {
        return qFromBigEndian<quint32>(take(4).data());
    }
    Cursor section()
    {
        return {take(u32())};
    }
};

QRect rectangle(Cursor &data)
{
    const qint64 top = qint32(data.u32()), left = qint32(data.u32());
    const qint64 bottom = qint32(data.u32()), right = qint32(data.u32());
    require(bottom >= top && right >= left);
    if (bottom == top || right == left)
        return {};
    if (!DocumentStorage::validDimensions(right - left, bottom - top))
        throw tr("Le calque PSD dépasse les dimensions prises en charge.");
    return QRect(QPoint(int(left), int(top)), QPoint(int(right - 1), int(bottom - 1)));
}

void unpackRow(Cursor row, char *out, int width)
{
    int x = 0;
    while (!row.bytes.empty()) {
        const int control = qint8(row.u8());
        if (control == -128)
            continue;
        const int count = control < 0 ? 1 - control : 1 + control;
        require(count <= width - x);
        if (control < 0)
            std::memset(out + x, row.u8(), count);
        else
            std::memcpy(out + x, row.take(count).data(), count);
        x += count;
    }
    require(x == width);
}

// A merged image uses one compression header and row table for all its planes.
QByteArray samples(Cursor data, int width, int rows, int bits)
{
    const int       rowBytes = width * (bits / 8);
    const qsizetype length   = qsizetype(rowBytes) * rows;
    if (length > DocumentStorage::MaxLayerBytes / 2)
        throw tr("Les données PSD dépassent la limite mémoire.");
    const int compression = data.u16();
    if (compression == 0)
        return QByteArray::fromRawData(data.take(length).data(), length);
    QByteArray out(length, Qt::Uninitialized);
    if (compression == 1) {
        Cursor lengths{data.take(2LL * rows)};
        for (int y = 0; y < rows; ++y)
            unpackRow({data.take(lengths.u16())}, out.data() + qsizetype(y) * rowBytes, rowBytes);
    } else if (compression == 2 || compression == 3) {
        uLongf written = uLongf(length);
        require(uncompress(reinterpret_cast<Bytef *>(out.data()), &written,
                           reinterpret_cast<const Bytef *>(data.bytes.data()),
                           uLong(data.bytes.size())) == Z_OK &&
                written == uLongf(length));
        if (compression == 3) {
            for (int y = 0; y < rows; ++y) {
                auto *row = reinterpret_cast<uchar *>(out.data() + qsizetype(y) * rowBytes);
                if (bits == 8) {
                    for (int x = 1; x < width; ++x)
                        row[x] += row[x - 1];
                } else {
                    quint16 previous = 0;
                    for (int x = 0; x < width; ++x) {
                        previous += qFromBigEndian<quint16>(row + x * 2);
                        qToBigEndian(previous, row + x * 2);
                    }
                }
            }
        }
    } else {
        throw tr("Compression PSD non prise en charge.");
    }
    return out;
}

struct Record {
    QRect                          bounds, maskBounds;
    Layer                          layer;
    int                            maskDefault      = 255;
    bool                           hasMask          = false;
    bool                           relevantPixels   = true;
    bool                           effectiveVisible = true;
    bool                           keep             = true;
    bool                           adjustmentBase   = false;
    int                            divider          = 0;
    QString                        skipReason;
    QStringList                    notes;
    QList<std::pair<int, quint32>> channels;
};

QByteArrayView descriptorId(Cursor &data)
{
    const auto size = data.u32();
    return data.take(size ? size : 4);
}

QString unicodeString(Cursor &data)
{
    const quint32 n = data.u32();
    require(n <= 65536);
    const auto raw = data.take(2LL * n);
    QString    s(n, Qt::Uninitialized);
    qFromBigEndian<quint16>(raw.data(), n, s.data());
    while (s.endsWith(QChar::Null))
        s.chop(1);
    return s;
}

// Adjustment descriptors contain typed, big-endian values, including nested RGB colors.
QVariantMap descriptor(Cursor &data, int depth = 0)
{
    require(depth < 5);
    unicodeString(data);
    descriptorId(data);
    const quint32 count = data.u32();
    require(count <= 128);
    QVariantMap result;
    for (quint32 i = 0; i < count; ++i) {
        const auto id = descriptorId(data);
        require(id.size() <= 256);
        const auto kind = data.take(4);
        QVariant   value;
        if (kind == "long")
            value = qint32(data.u32());
        else if (kind == "bool")
            value = data.u8() != 0;
        else if (kind == "doub") {
            const double d = std::bit_cast<double>(qFromBigEndian<quint64>(data.take(8).data()));
            require(std::isfinite(d));
            value = d;
        } else if (kind == "TEXT")
            value = unicodeString(data);
        else if (kind == "enum") {
            descriptorId(data);
            value = QString::fromLatin1(descriptorId(data));
        } else if (kind == "Objc")
            value = descriptor(data, depth + 1);
        else if (kind == "tdta") {
            const auto bytes = data.section().bytes;
            require(bytes.size() <= 16 * 1024 * 1024);
            value = bytes.toByteArray();
        } else
            throw tr("Type de donnée de réglage Photoshop non pris en charge.");
        result.insert(QString::fromLatin1(id), value);
    }
    return result;
}

QVariantMap versionedDescriptor(Cursor &data)
{
    require(data.u32() == 16);
    return descriptor(data);
}

QColor psdColor(Cursor &data)
{
    const int space = data.u16();
    const int a = data.u16(), b = data.u16(), c = data.u16();
    data.u16();
    if (space == 0)
        return QColor::fromRgba64(a, b, c);
    if (space != 7)
        return {};
    // Photoshop Lab uses D50, with signed a* and b* in hundredths.
    const double fy  = (a / 100. + 16.) / 116.;
    const auto   xyz = [](double f) {
        return f > 6. / 29. ? f * f * f : (f - 4. / 29.) * 108. / 841.;
    };
    const double x    = .96422 * xyz(fy + qint16(b) / 50000.);
    const double y    = xyz(fy);
    const double z    = .82521 * xyz(fy - qint16(c) / 20000.);
    const auto   srgb = [](double v) {
        return qBound(0., v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1. / 2.4) - .055, 1.);
    };
    return QColor::fromRgbF(srgb(3.1338561 * x - 1.6168667 * y - .4906146 * z),
                            srgb(-.9787684 * x + 1.9161415 * y + .033454 * z),
                            srgb(.0719453 * x - .2289914 * y + 1.4052427 * z));
}

bool readGradient(Cursor &data, QVariantMap &params, QStringList &notes)
{
    const int version = data.u16();
    if (version != 1 && version != 3)
        return false;
    const bool reverse     = data.u8() != 0;
    bool       approximate = data.u8() != 0;
    if (version == 3) {
        const auto method = data.take(4);
        params["gradientMethod"] = QString::fromLatin1(method);
        approximate |= method != "Lnr " && method != "Smoo" && method != "Gcls";
    }
    unicodeString(data);
    const int count = data.u16();
    if (count < 2)
        return false;
    approximate |= count != 2;
    for (int i = 0; i < count; ++i) {
        const auto position = data.u32();
        const auto midpoint = data.u32();
        const auto color    = psdColor(data);
        data.u16(); // reserved
        if (!color.isValid())
            return false;
        approximate |=
            midpoint != 50 || (i == 0 && position != 0) || (i == count - 1 && position != 4096);
        if (i == 0 || i == count - 1)
            params[(i == 0) != reverse ? "shadowColor" : "highlightColor"] = color.name();
    }
    const int opacityStops = data.u16();
    for (int i = 0; i < opacityStops; ++i) {
        data.take(8);
        approximate |= data.u16() != 255;
    }
    if (data.u16() != 2)
        return false;
    const int interpolation = data.u16();
    if (data.u16() != 32 || data.u16() != 0) // noise gradients are a different model
        return false;
    if (approximate || interpolation != 4096)
        notes << tr("dégradé approché avec les deux couleurs extrêmes");
    return true;
}

// Read the typed adjustment payload independently of the layer's name and pixel channels.
bool readAdjustment(QByteArrayView key, Cursor data, Record &rec)
{
    QString     type;
    QVariantMap params;
    if (key == "vibA" || key == "clrd" || key == "grnA" || key == "blwh") {
        const auto d = versionedDescriptor(data);
        if (key == "vibA") {
            params = {{"vibrance", d.value("vibrance", 0)}, {"saturation", d.value("Strt", 0)}};
            type   = "vibrance";
            if (d.contains("temperature") || d.contains("tint")) {
                type                  = "color";
                params["temperature"] = d.value("temperature", 0);
                params["tint"]        = d.value("tint", 0);
            }
        } else if (key == "clrd") {
            type   = "clarityDehaze";
            params = {{"clarity", d.value("clarity", 0)}, {"dehaze", d.value("dehaze", 0)}, {"radius", 32}};
        } else if (key == "grnA") {
            type   = "grain";
            params = {{"amount", d.value("grainAmount", 0)},
                      {"size", 1. + .07 * d.value("grainSize", 25).toInt()},
                      {"roughness", d.value("grainRoughness", 50)},
                      {"seed", d.value("grainSeed", 0)}};
        } else {
            type            = "blackWhite";
            params          = {{"reds", d.value("Rd  ", 40)},
                               {"yellows", d.value("Yllw", 60)},
                               {"greens", d.value("Grn ", 40)},
                               {"cyans", d.value("Cyn ", 60)},
                               {"blues", d.value("Bl  ", 20)},
                               {"magentas", d.value("Mgnt", 80)},
                               {"tintEnabled", d.value("useTint", false)}};
            const auto tint = d.value("tintColor").toMap();
            if (tint.contains("Rd  ") && tint.contains("Grn ") && tint.contains("Bl  ")) {
                auto channel = [&](const char *id) {
                    return qRound(qBound(0., tint[id].toDouble(), 255.));
                };
                params["tintColor"] =
                    QColor(channel("Rd  "), channel("Grn "), channel("Bl  ")).name();
            } else if (params["tintEnabled"].toBool())
                return false;
        }
    } else if (key == "brit") {
        type                 = "brightnessContrast";
        params["brightness"] = qint16(data.u16());
        params["contrast"]   = qint16(data.u16());
    } else if (key == "expA") {
        if (data.u16() != 1)
            return false;
        type = "exposure";
        params["photoshopExposure"] = true;
        for (const auto *name : {"exposure", "offset", "gamma"}) {
            const float value = std::bit_cast<float>(data.u32());
            require(std::isfinite(value));
            params[name] = double(value);
        }
    } else if (key == "mixr") {
        if (data.u16() != 1)
            return false;
        type                 = "channelMixer";
        const bool mono      = data.u16() != 0;
        params["monochrome"] = mono;
        for (const auto *output : {"red", "green", "blue"}) {
            const QString prefix = mono ? QStringLiteral("mono") : QString::fromLatin1(output);
            for (const auto *axis : {"Red", "Green", "Blue"})
                params[prefix + QLatin1String(axis)] = qint16(data.u16());
            data.u16(); // black input, only used for CMYK
            params[prefix + "Constant"] = qint16(data.u16());
            if (mono)
                break;
        }
    } else if (key == "phfl") {
        if (data.u16() != 2)
            return false;
        const auto color = psdColor(data);
        if (!color.isValid())
            return false;
        type                         = "photoFilter";
        params                       = {{"useCustomColor", true}, {"color", color.name()}};
        params["density"]            = int(data.u32());
        params["preserveLuminosity"] = data.u8() != 0;
    } else if (key == "grdm") {
        type = "gradientMap";
        if (!readGradient(data, params, rec.notes))
            return false;
    } else if (key == "clrL") {
        if (data.u16() != 1)
            return false;
        const auto d = versionedDescriptor(data);
        if (d.value("lookupType") != "3DLUT" || d.value("LUTFormat") != "LUTFormatLOOK")
            return false;
        type = "colorLookup";
        if (!Filters::readLookTable(d.value("LUT3DFileData").toByteArray(), params))
            return false;
        params["lookupName"] = QFileInfo(d.value("LUT3DFileName").toString()).fileName();
        params["lookupDither"] = d.value("Dthr").toBool();
    } else if (key == "post" || key == "thrs") {
        const bool posterize = key == "post";
        type                 = posterize ? "posterize" : "threshold";
        const int value      = data.u16();
        if (value > 255 || (posterize && value < 2))
            return false;
        params[posterize ? "levels" : "level"] = value;
    } else if (key == "nvrt") {
        type = "invert";
    } else if (key == "curv") {
        if (data.u8() != 0 || data.u16() != 1)
            return false;
        const auto channels = data.u32();
        if (channels & ~15u)
            return false;
        constexpr std::array keys{"points", "redPoints", "greenPoints", "bluePoints"};
        for (unsigned i = 0; i < keys.size(); ++i) {
            if (!(channels & (1u << i)))
                continue;
            const int count = data.u16();
            if (count < 2 || count > 256)
                return false;
            QVariantList points;
            for (int p = 0; p < count; ++p) {
                const int y = data.u16();
                const int x = data.u16();
                if (x > 255 || y > 255)
                    return false;
                points.append(QVariantMap{{"x", x}, {"y", y}});
            }
            params[keys[i]] = points;
        }
        type = "curves";
    } else if (key == "levl") {
        if (data.u16() != 2)
            return false;
        constexpr std::array keys{"inputBlack", "inputWhite", "outputBlack", "outputWhite",
                                  "gamma"};
        constexpr std::array defaults{0, 255, 0, 255, 100};
        bool                 separateChannels = false;
        for (int channel = 0; channel < 29; ++channel) {
            for (size_t i = 0; i < keys.size(); ++i) {
                const int value = data.u16();
                if (channel == 0)
                    params[keys[i]] = i == 4 ? value / 100. : value;
                else if (channel <= 3 && value != defaults[i])
                    separateChannels = true;
            }
        }
        if (separateChannels)
            rec.notes << tr("réglage global des niveaux importé, niveaux par couleur ignorés");
        if (params["gamma"].toDouble() > 3. || params["gamma"].toDouble() < .1 ||
            params["outputBlack"].toDouble() > params["outputWhite"].toDouble())
            rec.notes << tr("valeurs de niveaux limitées à la plage de PhotoCrap");
        type = "levels";
    } else if (key == "hue2") {
        if (data.u16() != 2)
            return false;
        const bool colorize = data.u8() != 0;
        data.u8();
        // Colorize stores absolute hue/saturation; the master stores relative shifts.
        constexpr std::array keys{"hue", "saturation", "lightness"};
        for (bool absolute : {true, false}) {
            for (const char *name : keys) {
                int value = qint16(data.u16());
                if (absolute == colorize)
                    params[name] = absolute && QByteArrayView(name) == "hue" && value > 180
                                       ? value - 360
                                       : value;
            }
        }
        if (colorize)
            params["colorize"] = true;
        bool selective = false;
        for (int range = 0; range < 6; ++range) {
            data.take(8); // four hue boundaries
            for (int i = 0; i < 3; ++i)
                if (data.u16())
                    selective = true;
        }
        if (selective && !colorize)
            rec.notes << tr("réglage global de teinte importé, plages de couleurs ignorées");
        type = "hueSaturation";
    } else if (key == "selc") {
        if (data.bytes.size() != 84 || data.u16() != 1)
            return false;
        const int method = data.u16();
        if (method > 1)
            return false;
        params["relative"] = method == 0;
        data.take(8); // The first CMYK record is reserved.
        for (const char *range : Filters::SelectiveColorRangeNames) {
            const QString prefix = QLatin1String(range);
            for (const char *axis : {"CyanRed", "MagentaGreen", "YellowBlue", "Black"}) {
                const int ink = qint16(data.u16());
                if (ink < -100 || ink > 100)
                    return false;
                // The first three editor axes increase toward the additive RGB primary.
                params[prefix + QLatin1String(axis)] = QByteArrayView(axis) == "Black" ? ink : -ink;
            }
        }
        type = "selectiveColor";
    } else if (key == "blnc") {
        for (const auto &tone : {"shadow", "", "highlight"}) {
            for (const auto &axis : {"cyanRed", "magentaGreen", "yellowBlue"}) {
                QString name = QString::fromLatin1(axis);
                if (*tone) {
                    name[0] = name[0].toUpper();
                    name.prepend(QLatin1String(tone));
                }
                params[name] = qint16(data.u16());
            }
        }
        params["preserveLuminosity"] = data.u8() != 0;
        type                         = "colorBalance";
    } else {
        return false;
    }
    // These native controls have a distinct Photoshop transfer function.
    static const QStringList compatibleTypes{"color", "curves", "levels", "grain", "clarityDehaze",
        "blackWhite", "colorBalance", "photoFilter", "selectiveColor", "posterize", "gradientMap",
        "threshold", "hueSaturation"};
    if (compatibleTypes.contains(type))
        params["photoshopAdjustment"] = true;
    rec.layer.kind           = "adjustment";
    rec.layer.adjustmentType = type;
    rec.layer.parameters     = params;
    return true;
}

Record layerRecord(Cursor &data)
{
    Record rec;
    rec.bounds         = rectangle(data);
    const int channels = data.u16();
    require(channels <= 56);
    for (int i = 0; i < channels; ++i) {
        const int id = qint16(data.u16());
        rec.channels.append({id, data.u32()});
    }
    require(data.take(4) == "8BIM");
    static const QList<QByteArray> modes = {"norm", "diss", "dark", "mul ", "idiv", "lbrn", "dkCl",
                                            "lite", "scrn", "div ", "lddg", "lgCl", "over", "sLit",
                                            "hLit", "vLit", "lLit", "pLit", "hMix", "diff", "smud",
                                            "fsub", "fdiv", "hue ", "sat ", "colr", "lum "};
    const auto                     mode  = data.take(4);
    // Some ImageMagick versions write the normal blend key in host byte order.
    rec.layer.blend        = mode == "mron" ? 0 : int(modes.indexOf(mode.toByteArray()));
    rec.layer.opacity      = data.u8() / 255.;
    rec.layer.clippingMask = data.u8() != 0;
    const int flags        = data.u8();
    rec.layer.visible      = !(flags & 2);
    rec.relevantPixels     = !(flags & 16);
    if (rec.layer.blend < 0) {
        rec.layer.blend = 0;
        if (mode != "pass")
            rec.notes
                << tr("mode de fusion « %1 » remplacé par Normal").arg(QString::fromLatin1(mode));
    }
    data.take(1);
    auto extra = data.section();
    auto mask  = extra.section();
    if (!mask.bytes.empty()) {
        rec.maskBounds        = rectangle(mask);
        rec.maskDefault       = mask.u8();
        const int maskFlags   = mask.u8();
        rec.hasMask           = true;
        rec.layer.maskEnabled = !(maskFlags & 2);
        if (maskFlags & ~3) {
            rec.hasMask = false;
            rec.notes << tr("masque complexe ignoré");
        }
    }
    auto ranges = extra.section();
    while (ranges.bytes.size() >= 4)
        if (ranges.u32() != 0x0000ffff) {
            rec.notes << tr("plages de fusion ignorées");
            break;
        }
    const int length = extra.u8();
    rec.layer.name   = QString::fromLatin1(extra.take(length));
    extra.take((3 - length % 4) % 4);
    QByteArrayView modernSettings;
    while (extra.bytes.size() >= 12) {
        const auto signature = extra.take(4);
        require(signature == "8BIM" || signature == "8B64");
        const auto key   = extra.take(4);
        auto       value = extra.section();
        if (value.bytes.size() % 2)
            extra.take(1);
        if (key == "luni") {
            const quint32 count = value.u32();
            require(count <= quint32(value.bytes.size() / 2) && count <= 65536);
            QString name(count, Qt::Uninitialized);
            qFromBigEndian<quint16>(value.take(count * 2).data(), count, name.data());
            while (name.endsWith(QChar::Null))
                name.chop(1);
            if (!name.isEmpty())
                rec.layer.name = name;
        } else if (key == "CgEd") {
            modernSettings = value.bytes;
        } else if (key == "lspf") {
            rec.layer.locked = (value.u32() & 7) == 7;
        } else if (key == "knko") {
            if (value.u8() != 0)
                rec.notes << tr("perçage ignoré");
        } else if (key == "lsct" || key == "lsdk") {
            rec.divider = int(value.u32());
            if ((rec.divider == 1 || rec.divider == 2) && value.bytes.size() >= 8) {
                value.take(4);
                if (value.take(4) != "pass")
                    rec.notes << tr("fusion isolée du groupe ignorée");
            }
        } else if (key == "iOpa") {
            const int fill = value.u8();
            if (fill != 255) {
                rec.layer.opacity *= fill / 255.;
                rec.notes << tr("opacité du fond convertie en opacité du calque");
            }
        } else {
            static const QList<QByteArray> adjustments = {
                "brit", "levl", "curv", "expA", "vibA", "hue ", "hue2", "blnc", "blwh",
                "phfl", "mixr", "clrL", "nvrt", "post", "thrs", "grdm", "selc", "clrd", "grnA"};
            if (adjustments.contains(key.toByteArray())) {
                try {
                    if (!readAdjustment(key, value, rec))
                        rec.skipReason =
                            tr("type ou variante de réglage Photoshop non pris en charge (%1)")
                                .arg(QString::fromLatin1(key));
                } catch (const QString &error) {
                    rec.skipReason = error;
                }
            } else if (key == "lrFX" || key == "lfx2" || key == "lmfx" || key == "lfxs")
                rec.notes << tr("effets de calque ignorés");
            else if (key == "vmsk" || key == "vsms")
                rec.notes << tr("masque vectoriel ignoré");
            else if (key == "TySh" || key == "tySh")
                rec.notes << tr("texte importé en pixels");
            else if (key == "SoLd" || key == "SoLE" || key == "PlLd" || key == "plLd")
                rec.notes << tr("objet dynamique importé en pixels");
            else if (key == "SoCo" || key == "GdFl" || key == "PtFl")
                rec.skipReason = tr("calque de remplissage Photoshop non pris en charge");
        }
    }
    if (rec.layer.adjustmentType == "brightnessContrast" && !modernSettings.empty()) {
        try {
            Cursor     settings{modernSettings};
            const auto d = versionedDescriptor(settings);
            if (d.value("brightnessMode") == "brightnessModeLight") {
                rec.layer.adjustmentType = "light";
                rec.layer.parameters     = {{"photoshopLight", true},
                                            {"exposure", d.value("brightnessExposure", 0.)},
                                            {"contrast", d.value("Cntr", 0)},
                                            {"whites", d.value("brightnessWhites", 0)},
                                            {"blacks", d.value("brightnessBlacks", 0)},
                                            {"highlights", d.value("brightnessHighlights", 0)},
                                            {"shadows", d.value("brightnessShadows", 0)}};
            } else {
                for (const auto &[id, name] :
                     {std::pair{"Brgh", "brightness"}, {"Cntr", "contrast"}})
                    if (d.contains(id))
                        rec.layer.parameters[name] = d[id];
            }
        } catch (const QString &error) {
            rec.notes << tr("réglages de luminosité historiques utilisés") + " (" + error + ")";
        }
    }
    return rec;
}

// PSD stores group footers before their children, so resolve parents from the top down.
void planLayers(QList<Record> &records, QSize canvas, int bits, bool gray)
{
    QList<int> groups;
    int        hiddenDepth = 0;
    for (qsizetype i = records.size(); i-- > 0;) {
        auto &r = records[i];
        if (r.layer.name.isEmpty())
            r.layer.name = tr("Calque %1").arg(i + 1);
        if (r.divider == 3) {
            r.keep = false;
            require(!groups.empty());
            if (!records[groups.takeLast()].layer.visible)
                --hiddenDepth;
            continue;
        }
        if (r.divider == 1 || r.divider == 2) {
            r.layer.kind           = QStringLiteral("group");
            r.layer.groupId        = r.layer.historyId;
            r.layer.groupCollapsed = r.divider == 2;
            if (!groups.empty())
                r.layer.parentGroupId = records[groups.last()].layer.groupId;
            if (!r.layer.visible)
                ++hiddenDepth;
            if (r.layer.blend != 0)
                r.notes << tr("mode de fusion du groupe ignoré");
            groups.append(int(i));
        } else {
            require(r.divider == 0);
            r.effectiveVisible = r.layer.visible && hiddenDepth == 0;
            if (!groups.empty()) {
                r.layer.groupId = records[groups.last()].layer.groupId;
            }
            if (r.skipReason.isEmpty() && r.layer.kind == "pixel") {
                const auto hasChannel = [&](int id) {
                    return std::any_of(r.channels.cbegin(), r.channels.cend(),
                                       [id](const auto &channel) { return channel.first == id; });
                };
                if (!r.relevantPixels || !hasChannel(0) ||
                    (!gray && (!hasChannel(1) || !hasChannel(2))))
                    r.skipReason = tr("aucune représentation en pixels exploitable");
            }
        }
        r.notes.removeDuplicates();
    }
    require(groups.empty());

    // Keep the pixel base of a clipped adjustment ahead of unrelated hidden copies.
    int base = -1;
    for (int i = 0; i < records.size(); ++i) {
        const auto &r = records[i];
        if (r.divider || !r.layer.clippingMask)
            base = !r.divider && r.layer.kind == "pixel" ? i : -1;
        else if (base >= 0 && r.layer.kind == "adjustment")
            records[base].adjustmentBase = true;
    }
    // Reserve the same logical plane budget used when reopening a PhotoCrap project.
    // Visible layers take precedence. Keep the original stacking order during decoding.
    qint64 used = 0;
    int    kept = 0;
    for (bool visible : {true, false}) {
        for (int priority = 0; priority < 3; ++priority) {
            for (auto &r : records) {
                const int rank = r.adjustmentBase ? 0 : r.layer.kind == "adjustment" ? 1 : 2;
                if (!r.keep || r.effectiveVisible != visible || rank != priority)
                    continue;
                const QRect  clip  = r.bounds.intersected(QRect(QPoint{}, canvas));
                const int    edge  = TiledImage::TileSize;
                const qint64 tiles = clip.isEmpty()
                                         ? 0
                                         : (clip.right() / edge - clip.left() / edge + 1) *
                                               (clip.bottom() / edge - clip.top() / edge + 1);
                const auto   cost =
                    (r.layer.kind == "pixel" ? DocumentStorage::tiledColorBytes(
                                                   canvas.width(), canvas.height(), bits, tiles)
                                             : 0) +
                    DocumentStorage::estimateBytes(canvas.width(), canvas.height(), 0,
                                                   r.hasMask && !r.maskBounds.isEmpty(), bits);
                if (r.skipReason.isEmpty()) {
                    if (kept >= DocumentStorage::MaxLayers)
                        r.skipReason =
                            tr("limite de %1 calques atteinte").arg(DocumentStorage::MaxLayers);
                    else if (cost > DocumentStorage::MaxLayerBytes - used)
                        r.skipReason = tr("budget de %1 Mio pour les calques dépassé")
                                           .arg(DocumentStorage::MaxLayerBytes / (1024 * 1024));
                }
                r.keep = r.skipReason.isEmpty();
                if (r.keep) {
                    used += cost;
                    ++kept;
                }
            }
        }
    }
    // Reparent descendants of an omitted header, retaining inherited visibility.
    QHash<QString, int> omitted;
    for (int i = 0; i < records.size(); ++i)
        if (records[i].layer.kind == "group" && !records[i].keep)
            omitted.insert(records[i].layer.groupId, i);
    for (auto &r : records) {
        QString &parent = r.layer.kind == "group" ? r.layer.parentGroupId : r.layer.groupId;
        while (omitted.contains(parent)) {
            const auto &group = records[omitted.value(parent)].layer;
            r.layer.visible &= group.visible;
            parent = group.parentGroupId;
        }
    }
    // Keep a clipped layer editable without accidentally clipping it to an unrelated base.
    bool hasBase = false;
    for (auto &r : records) {
        if (r.divider || !r.layer.clippingMask)
            hasBase = !r.divider && r.keep;
        else if (r.keep && !hasBase) {
            r.layer.clippingMask = false;
            r.layer.visible      = false;
            r.notes << tr("calque masqué car sa base d’écrêtage n’a pas été importée");
        }
    }
}

using Planes = std::array<QByteArrayView, 4>;

QImage pixels(QSize canvas, QRect bounds, const Planes &planes, int bits, bool gray,
              const QColorSpace &profile, bool whiteMatte = false)
{
    const int       bps      = bits / 8;
    const qsizetype expected = qsizetype(bounds.width()) * bounds.height() * bps;
    require(planes[0].size() == expected);
    if (!gray)
        require(planes[1].size() == expected && planes[2].size() == expected);
    require(planes[3].empty() || planes[3].size() == expected);
    QImage image(canvas, bits == 16 ? QImage::Format_RGBA64 : QImage::Format_RGBA8888);
    if (image.isNull())
        throw tr("Allocation des pixels impossible.");
    image.fill(Qt::transparent);
    const QRect clip = bounds.intersected(image.rect());
    for (int y = clip.top(); !clip.isEmpty() && y <= clip.bottom(); ++y) {
        auto     *dst = image.scanLine(y) + clip.x() * 4 * bps;
        qsizetype pos = (qsizetype(y - bounds.y()) * bounds.width() + clip.x() - bounds.x()) * bps;
        for (int x = 0; x < clip.width(); ++x, pos += bps) {
            const quint32 maximum = bits == 8 ? 255 : 65535;
            const auto    sample  = [&](int c) -> quint32 {
                const auto &plane = planes[gray && c < 3 ? 0 : c];
                if (plane.empty())
                    return maximum;
                return bits == 8 ? uchar(plane[pos]) : qFromBigEndian<quint16>(plane.data() + pos);
            };
            const quint32 alpha = sample(3);
            for (int c = 0; c < 4; ++c) {
                quint32 value = c < 3 ? sample(c) : alpha;
                // Photoshop stores merged RGB against white when it includes transparency.
                if (whiteMatte && c < 3 && alpha && alpha != maximum)
                    value =
                        maximum - qMin(maximum, ((maximum - value) * maximum + alpha / 2) / alpha);
                if (bits == 8) {
                    *dst++ = uchar(value);
                } else {
                    const quint16 native = quint16(value);
                    std::memcpy(dst, &native, 2);
                    dst += 2;
                }
            }
        }
    }
    if (profile.isValid() && !gray) {
        image.setColorSpace(profile);
        image.convertToColorSpace(QColorSpace::SRgb);
    } else {
        image.setColorSpace(QColorSpace::SRgb);
    }
    image.convertTo(bits == 16 ? QImage::Format_RGBA64_Premultiplied
                               : QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
        throw tr("Allocation des pixels impossible.");
    return image;
}

QImage maskImage(QSize size, const Record &rec, QByteArrayView bytes, int bits)
{
    QImage image(size, QImage::Format_Grayscale8);
    if (image.isNull())
        throw tr("Allocation du masque impossible.");
    image.fill(rec.hasMask ? rec.maskDefault : 255);
    const auto bounds = rec.hasMask ? rec.maskBounds : QRect{};
    const auto clip   = bounds.intersected(image.rect());
    require(bytes.size() == qsizetype(bounds.width()) * bounds.height() * (bits / 8));
    for (int y = clip.top(); !clip.isEmpty() && y <= clip.bottom(); ++y) {
        auto *dst = image.scanLine(y) + clip.x();
        auto  pos =
            (qsizetype(y - bounds.y()) * bounds.width() + clip.x() - bounds.x()) * (bits / 8);
        for (int x = 0; x < clip.width(); ++x, pos += bits / 8)
            dst[x] = uchar(bytes[pos]);
    }
    return image;
}

struct Resources {
    QColorSpace profile;
    bool        hasMergedImage = true;
};

Resources readResources(Cursor data)
{
    Resources result;
    while (data.bytes.size() >= 12) {
        require(data.take(4) == "8BIM");
        const int id = data.u16();
        const int n  = data.u8();
        data.take(n + ((n + 1) % 2));
        auto resource = data.section();
        if (resource.bytes.size() % 2)
            data.take(1);
        if (id == 1039 && resource.bytes.size() <= 16 * 1024 * 1024)
            result.profile = QColorSpace::fromIccProfile(resource.bytes.toByteArray());
        if (id == 1057) { // Version Info: hasRealMergedData
            resource.u32();
            result.hasMergedImage = resource.u8() != 0;
        }
    }
    return result;
}

Cursor layerInfo(Cursor section, int bits, bool &mergedAlpha)
{
    if (section.bytes.empty())
        return {};
    auto info = section.section();
    if (section.bytes.size() >= 4) {
        section.section(); // Global mask
        while (section.bytes.size() >= 12) {
            const auto signature = section.take(4);
            if (signature != "8BIM" && signature != "8B64")
                break;
            auto key   = section.take(4);
            auto block = section.section();
            if (bits == 16 && key == "Lr16")
                info = block;
            if ((bits == 16 && key == "Mt16") || (bits == 8 && key == "Mtrn"))
                mergedAlpha = true;
            section.take((4 - block.bytes.size() % 4) % 4);
        }
    }
    return info;
}

Result decode(Cursor data, const QString &name, bool compositeOnly,
              const std::function<bool(int, int)> &progress)
{
    require(data.take(4) == "8BPS");
    if (data.u16() != 1)
        throw tr("Seuls les fichiers PSD version 1 sont pris en charge.");
    data.take(6);
    const int     channels = data.u16();
    const quint32 height = data.u32(), width = data.u32();
    if (!DocumentStorage::validDimensions(width, height))
        throw tr(
            "Ce PSD mesure %1 × %2 px. PhotoCrap accepte jusqu’à %3 px par côté et %4 mégapixels.")
            .arg(width)
            .arg(height)
            .arg(DocumentStorage::MaxDimension)
            .arg(DocumentStorage::MaxPixels / 1000000);
    Result result;
    result.size     = QSize(int(width), int(height));
    result.bits     = data.u16();
    const int  mode = data.u16();
    const bool gray = mode == 1;
    if ((!gray && mode != 3) || (result.bits != 8 && result.bits != 16))
        throw tr("L’import PSD accepte les documents RVB et niveaux de gris en 8 ou 16 bits.");
    require(channels >= (gray ? 1 : 3) && channels <= 56);
    data.section(); // Color mode data (empty for RGB/grayscale)
    const auto    resources   = readResources(data.section());
    bool          mergedAlpha = false;
    auto          info        = layerInfo(data.section(), result.bits, mergedAlpha);
    const int     signedCount = info.bytes.empty() ? 0 : qint16(info.u16());
    const int     count       = qAbs(signedCount);
    QList<Record> records;
    if (!compositeOnly) {
        for (int i = 0; i < count; ++i)
            records.append(layerRecord(info));
        planLayers(records, result.size, result.bits, gray);
    }
    const bool merged = std::none_of(records.cbegin(), records.cend(), [](const Record &r) {
        return r.keep && r.layer.kind == "pixel";
    });
    for (const auto &r : std::as_const(records)) {
        if (!r.skipReason.isEmpty())
            result.notes << tr("%1 — ignoré : %2").arg(r.layer.name, r.skipReason);
        else if (!r.notes.isEmpty())
            result.notes << tr("%1 — %2").arg(r.layer.name, r.notes.join(", "));
    }
    if (merged && count && !compositeOnly)
        result.notes << tr(
            "Aucun calque de pixels récupérable. Affichage du composite enregistré dans le PSD.");
    if (merged) {
        if (!resources.hasMergedImage)
            throw tr("Ce PSD nécessite une image fusionnée. Dans Photoshop, activez « Maximiser la "
                     "compatibilité » puis réenregistrez-le.");
        if (!progress(0, 1))
            return {};
        const auto      raw    = samples(data, width, int(height) * channels, result.bits);
        const qsizetype stride = qsizetype(width) * height * (result.bits / 8);
        Planes          planes{};
        const int       colors = gray ? 1 : 3;
        for (int c = 0; c < colors; ++c)
            planes[c] = QByteArrayView(raw).sliced(c * stride, stride);
        if ((signedCount < 0 || mergedAlpha) && channels > colors)
            planes[3] = QByteArrayView(raw).sliced(colors * stride, stride);
        result.layers.append(
            Layer{count ? tr("Composite PSD — %1").arg(name) : name,
                  pixels(result.size, QRect(QPoint{}, result.size), planes, result.bits, gray,
                         resources.profile, !planes[3].empty())});
    } else {
        for (int i = 0; i < count; ++i) {
            if (!progress(i, count))
                return {};
            auto                     &rec = records[i];
            std::array<QByteArray, 4> storage;
            QByteArray                mask;
            for (auto [id, len] : rec.channels) {
                Cursor channel{info.take(len)};
                if (!rec.keep || (rec.divider && id != -2) || (id == -2 && !rec.hasMask) ||
                    (rec.layer.kind == "adjustment" && id != -2))
                    continue;
                const QRect area = id == -2 ? rec.maskBounds : rec.bounds;
                if (area.isEmpty() || id < -2 || id >= (gray ? 1 : 3))
                    continue;
                auto bytes = samples(channel, area.width(), area.height(), result.bits);
                if (id == -2)
                    mask = std::move(bytes);
                else
                    storage[id == -1 ? 3 : id] = std::move(bytes);
            }
            if (!rec.keep)
                continue;
            if (rec.layer.kind == "pixel") {
                Planes      planes{storage[0], storage[1], storage[2], storage[3]};
                const QRect canvas(QPoint{}, result.size);
                const QRect clip = canvas.intersected(rec.bounds);
                if (clip == canvas) {
                    rec.layer.image = pixels(result.size, rec.bounds, planes, result.bits, gray,
                                             resources.profile);
                } else {
                    QImage background(1, 1,
                                      result.bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                                        : QImage::Format_ARGB32_Premultiplied);
                    background.fill(Qt::transparent);
                    background.setColorSpace(QColorSpace::SRgb);
                    TiledImage tiles(result.size, background);
                    if (!clip.isEmpty()) {
                        const auto patch =
                            pixels(clip.size(), rec.bounds.translated(-clip.topLeft()), planes,
                                   result.bits, gray, resources.profile);
                        if (!tiles.writeRegion(clip.topLeft(), patch))
                            throw tr("Allocation des pixels impossible.");
                    }
                    rec.layer.image = PixelPlane::fromTiles(std::move(tiles));
                }
            }
            if (rec.hasMask || rec.layer.kind == "adjustment")
                rec.layer.mask =
                    maskImage(rec.maskBounds.isEmpty() || !rec.hasMask ? QSize(1, 1) : result.size,
                              rec, mask, result.bits);
            if (!rec.layer.image.freeze() || !rec.layer.mask.freeze())
                throw tr("Allocation des pixels impossible.");
            result.layers.append(std::move(rec.layer));
        }
    }
    return progress(1, 1) ? result : Result{};
}
} // namespace

bool isFile(const QString &path)
{
    return QFileInfo(path).suffix().compare("psd", Qt::CaseInsensitive) == 0;
}

Result read(const QString &path, bool compositeOnly, const std::function<bool(int, int)> &progress)
{
    QFile  file(path);
    Result result;
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    const auto size = file.size();
    if (size < 26 || size > qint64(2) * 1024 * 1024 * 1024) {
        result.error = tr("Taille de fichier PSD invalide.");
        return result;
    }
    auto *mapped = file.map(0, size);
    if (!mapped) {
        result.error = file.errorString();
        return result;
    }
    try {
        return decode({QByteArrayView(reinterpret_cast<const char *>(mapped), size)},
                      QFileInfo(path).completeBaseName(), compositeOnly, progress);
    } catch (const QString &message) {
        result.error = message;
    } catch (const std::bad_alloc &) {
        result.error = tr("Mémoire insuffisante pour ouvrir ce PSD.");
    }
    return result;
}
} // namespace PhotoCrap::Psd
