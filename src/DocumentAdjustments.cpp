#include "Document.h"
#include "ImagePixels.h"
#include "filters/SelectiveColorFilter.h"

#include <QPainter>
#include <QtMath>
#include <cmath>

void Document::addAdjustmentLayer(const QString &_type)
{
    const QMap<QString, QString> names{{"light", tr("Lumière")},
                                       {"color", tr("Couleur")},
                                       {"solidColor", tr("Couleur unie")},
                                       {"brightnessContrast", tr("Luminosité / Contraste")},
                                       {"levels", tr("Niveaux")},
                                       {"curves", tr("Courbes")},
                                       {"exposure", tr("Exposition")},
                                       {"vibrance", tr("Vibrance")},
                                       {"hueSaturation", tr("Teinte / Saturation")},
                                       {"colorBalance", tr("Balance des couleurs")},
                                       {"blackWhite", tr("Noir et blanc")},
                                       {"photoFilter", tr("Filtre photo")},
                                       {"channelMixer", tr("Mélangeur de couches")},
                                       {"colorLookup", tr("Style colorimétrique (LUT)")},
                                       {"invert", tr("Négatif")},
                                       {"posterize", tr("Isohélie")},
                                       {"threshold", tr("Seuil")},
                                       {"gradientMap", tr("Courbe de transfert de dégradé")},
                                       {"selectiveColor", tr("Correction sélective")},
                                       {"gaussianBlur", tr("Flou gaussien")},
                                       {"motionBlur", tr("Flou directionnel")},
                                       {"boxBlur", tr("Flou de boîte")},
                                       {"unsharpMask", tr("Netteté optimisée")},
                                       {"highPass", tr("Passe-haut")},
                                       {"mosaic", tr("Mosaïque")},
                                       {"noise", tr("Ajout de bruit")}};
    if (!names.contains(_type) || !validSize(width(), height(), int(m_layers.size()) + 1) ||
        !validLayerStorage(width(), height(), 0, 1))
        return;
    QVariantMap params;
    if (_type == "solidColor")
        params = {{"color", QStringLiteral("#000000")}};
    else if (_type == "color")
        params = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (_type == "light")
        params = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                  {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (_type == "brightnessContrast")
        params = {{"brightness", 0}, {"contrast", 0}};
    else if (_type == "levels")
        params = {{"inputBlack", 0},
                  {"inputWhite", 255},
                  {"gamma", 1.},
                  {"outputBlack", 0},
                  {"outputWhite", 255}};
    else if (_type == "curves")
        params = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                          QVariantMap{{"x", 255}, {"y", 255}}}}};
    else if (_type == "exposure")
        params = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (_type == "vibrance")
        params = {{"vibrance", 0}, {"saturation", 0}};
    else if (_type == "hueSaturation")
        params = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (_type == "colorBalance")
        params = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (_type == "selectiveColor")
        params = PhotoCrap::Filters::defaultSelectiveColorParameters();
    else if (_type == "blackWhite")
        params = {{"reds", 40},           {"yellows", 60},
                  {"greens", 40},         {"cyans", 60},
                  {"blues", 20},          {"magentas", 80},
                  {"tintEnabled", false}, {"tintColor", QStringLiteral("#d6aa78")}};
    else if (_type == "photoFilter")
        params = {{"preset", 0},
                  {"color", QStringLiteral("#ec8a00")},
                  {"useCustomColor", false},
                  {"density", 25},
                  {"preserveLuminosity", true}};
    else if (_type == "channelMixer")
        params = {{"redRed", 100},       {"redGreen", 0},      {"redBlue", 0},
                  {"greenRed", 0},       {"greenGreen", 100},  {"greenBlue", 0},
                  {"blueRed", 0},        {"blueGreen", 0},     {"blueBlue", 100},
                  {"redConstant", 0},    {"greenConstant", 0}, {"blueConstant", 0},
                  {"monochrome", false}, {"monoRed", 40},      {"monoGreen", 40},
                  {"monoBlue", 20},      {"monoConstant", 0}};
    else if (_type == "colorLookup")
        params = {{"preset", 0}, {"strength", 100}, {"preserveLuminosity", false}};
    else if (_type == "posterize")
        params = {{"levels", 4}};
    else if (_type == "threshold")
        params = {{"level", 128}};
    else if (_type == "gradientMap")
        params = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (_type == "gaussianBlur" || _type == "boxBlur")
        params = {{"radius", 8.}};
    else if (_type == "motionBlur")
        params = {{"distance", 20.}, {"angle", 0.}};
    else if (_type == "unsharpMask")
        params = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (_type == "highPass")
        params = {{"radius", 4.}};
    else if (_type == "mosaic")
        params = {{"cellSize", 12}};
    else if (_type == "noise")
        params = {{"amount", 8.}};
    endStroke();
    const auto before = snapshot(tr("Nouveau calque de réglage"));
    {
        Layer layer;
        layer.name           = names.value(_type);
        layer.kind           = "adjustment";
        layer.adjustmentType = _type;
        layer.parameters     = params;
        layer.mask           = QImage(m_size, QImage::Format_Grayscale8);
        layer.mask.fill(hasSelection() ? 0 : 255);
        layer.parameters.insert("_maskDensity", 100.);
        layer.parameters.insert("_maskFeather", 0.);
        if (hasSelection()) {
            QPainter painter(&layer.mask);
            painter.setClipRegion(m_selectionRegion);
            painter.fillRect(QRect(QPoint{}, m_size), Qt::white);
        }
        m_layers.insert(++m_active, layer);
        m_editingMask = false;
        selectOnly(m_active);
    }
    // Publish the new layer metadata without prompting the canvas to compose
    // the full document synchronously on the UI thread.
    commit(before, tr("Nouveau calque de réglage"), true, false);
    // Present a screen-sized result first, then refine the document pixels in
    // the background. Adding a costly filter must never block the QML thread.
    requestPreviewRender(true, true);
    emit imageChanged();
}
void Document::addFrequencySeparation(qreal _radius)
{
    if (!std::isfinite(_radius) || !validSize(width(), height(), int(m_layers.size()) + 2) ||
        !validLayerStorage(width(), height(), 2))
        return;
    QImage source = m_layers[m_active].kind == "pixel" ? m_layers[m_active].image : composite();
    if (m_layers[m_active].kind == "pixel" && m_layers[m_active].maskEnabled &&
        !m_layers[m_active].mask.isNull())
        source =
            applyMask(source, m_layers[m_active].mask,
                      m_layers[m_active].parameters.value("_maskDensity", 100.).toDouble() / 100.,
                      m_layers[m_active].parameters.value("_maskFeather", 0.).toDouble());
    const QString pairId = QString::number(m_nextId);
    edit(tr("Séparation de fréquences"), [&] {
        const QVariantMap parameters{{"radius", qBound(1., _radius, 100.)}, {"pairId", pairId}};
        Layer             low;
        low.name       = tr("Basses fréquences");
        low.kind       = "frequencyLow";
        low.source     = source;
        low.parameters = parameters;
        Layer high;
        high.name       = tr("Hautes fréquences");
        high.kind       = "frequencyHigh";
        high.source     = source;
        high.parameters = parameters;
        high.blend      = 16;
        m_layers.insert(++m_active, low);
        m_layers.insert(++m_active, high);
        m_editingMask = false;
        selectOnly(m_active);
    });
}
void Document::resetAdjustmentLayer()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].kind != QStringLiteral("adjustment"))
        return;

    const QString type = m_layers[m_active].adjustmentType;
    QVariantMap   parameters;
    if (type == QStringLiteral("solidColor"))
        parameters = {{"color", QStringLiteral("#000000")}};
    else if (type == QStringLiteral("color"))
        parameters = {{"temperature", 0}, {"tint", 0}, {"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("light"))
        parameters = {{"exposure", 0.}, {"contrast", 0}, {"highlights", 0},
                      {"shadows", 0},   {"whites", 0},   {"blacks", 0}};
    else if (type == QStringLiteral("brightnessContrast"))
        parameters = {{"brightness", 0}, {"contrast", 0}};
    else if (type == QStringLiteral("levels"))
        parameters = {{"inputBlack", 0},
                      {"inputWhite", 255},
                      {"gamma", 1.},
                      {"outputBlack", 0},
                      {"outputWhite", 255}};
    else if (type == QStringLiteral("curves"))
        parameters = {{"points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                              QVariantMap{{"x", 255}, {"y", 255}}}},
                      {"blackSample", QVariantList{}},
                      {"graySample", QVariantList{}},
                      {"whiteSample", QVariantList{}}};
    else if (type == QStringLiteral("exposure"))
        parameters = {{"exposure", 0.}, {"offset", 0.}, {"gamma", 1.}};
    else if (type == QStringLiteral("vibrance"))
        parameters = {{"vibrance", 0}, {"saturation", 0}};
    else if (type == QStringLiteral("hueSaturation"))
        parameters = {{"hue", 0}, {"saturation", 0}, {"lightness", 0}};
    else if (type == QStringLiteral("colorBalance"))
        parameters = {{"cyanRed", 0}, {"magentaGreen", 0}, {"yellowBlue", 0}, {"black", 0}};
    else if (type == QStringLiteral("selectiveColor"))
        parameters = PhotoCrap::Filters::defaultSelectiveColorParameters();
    else if (type == QStringLiteral("blackWhite"))
        parameters = {{"reds", 40},           {"yellows", 60},
                      {"greens", 40},         {"cyans", 60},
                      {"blues", 20},          {"magentas", 80},
                      {"tintEnabled", false}, {"tintColor", QStringLiteral("#d6aa78")}};
    else if (type == QStringLiteral("photoFilter"))
        parameters = {{"preset", 0},
                      {"color", QStringLiteral("#ec8a00")},
                      {"useCustomColor", false},
                      {"density", 25},
                      {"preserveLuminosity", true}};
    else if (type == QStringLiteral("channelMixer"))
        parameters = {{"redRed", 100},       {"redGreen", 0},      {"redBlue", 0},
                      {"greenRed", 0},       {"greenGreen", 100},  {"greenBlue", 0},
                      {"blueRed", 0},        {"blueGreen", 0},     {"blueBlue", 100},
                      {"redConstant", 0},    {"greenConstant", 0}, {"blueConstant", 0},
                      {"monochrome", false}, {"monoRed", 40},      {"monoGreen", 40},
                      {"monoBlue", 20},      {"monoConstant", 0}};
    else if (type == QStringLiteral("colorLookup"))
        parameters = {{"preset", 0}, {"strength", 100}, {"preserveLuminosity", false}};
    else if (type == QStringLiteral("posterize"))
        parameters = {{"levels", 4}};
    else if (type == QStringLiteral("threshold"))
        parameters = {{"level", 128}};
    else if (type == QStringLiteral("gradientMap"))
        parameters = {{"shadowColor", "#101828"}, {"highlightColor", "#f5d9a8"}};
    else if (type == QStringLiteral("gaussianBlur") || type == QStringLiteral("boxBlur"))
        parameters = {{"radius", 8.}};
    else if (type == QStringLiteral("motionBlur"))
        parameters = {{"distance", 20.}, {"angle", 0.}};
    else if (type == QStringLiteral("unsharpMask"))
        parameters = {{"amount", 100.}, {"radius", 2.}, {"threshold", 0}};
    else if (type == QStringLiteral("highPass"))
        parameters = {{"radius", 4.}};
    else if (type == QStringLiteral("mosaic"))
        parameters = {{"cellSize", 12}};
    else if (type == QStringLiteral("noise"))
        parameters = {{"amount", 8.}};

    parameters.insert(QStringLiteral("_maskDensity"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskDensity"), 100.));
    parameters.insert(QStringLiteral("_maskFeather"),
                      m_layers[m_active].parameters.value(QStringLiteral("_maskFeather"), 0.));
    beginLayerPreview();
    m_layers[m_active].parameters = parameters;
    m_layerPreviewChanged         = true;
    m_fullCompositeDirty          = true;
    m_cacheDirty                  = true;
    requestPreviewRender(true, true);
    emit layersChanged();
    finishLayerPreview(tr("Réinitialiser le réglage"));
}

void Document::resetCurveAdjustment()
{
    if (m_active < 0 || m_active >= m_layers.size() ||
        m_layers[m_active].adjustmentType != QStringLiteral("curves"))
        return;
    beginLayerPreview();
    previewLayerParameter(
        QStringLiteral("points"),
        QVariantList{QVariantMap{{"x", 0}, {"y", 0}}, QVariantMap{{"x", 255}, {"y", 255}}});
    for (const QString &key : {QStringLiteral("blackSample"), QStringLiteral("graySample"),
                               QStringLiteral("whiteSample")})
        previewLayerParameter(key, QVariantList{});
    finishLayerPreview(tr("Réinitialiser les courbes"));
}

void Document::applyFilter(const QString &_filter, qreal _amount)
{
    static const QStringList supported{"grayscale", "invert",     "sepia", "brightness",
                                       "contrast",  "saturation", "blur",  "sharpen"};
    if (!supported.contains(_filter) || !std::isfinite(_amount) || !editable())
        return;
    if (m_editingMask || m_layers[m_active].kind != "pixel") {
        fail(tr("Ce filtre destructif nécessite un calque de pixels."));
        return;
    }
    _amount = qBound(-100., _amount, 100.);
    edit(tr("Filtre : %1").arg(_filter), [&] {
        const auto render = [&]<typename Pixel>() {
            using P               = ImageUtils::PixelAccess<Pixel>;
            const QImage original = m_layers[m_active].image;
            QImage       result   = original.convertToFormat(P::format);
            const QRect  area     = hasSelection() ? m_selection : result.rect();
            if (_filter == "blur" || _filter == "sharpen") {
                const int    radius  = _filter == "sharpen" ? 1 : qBound(1, int(_amount), 30);
                const QImage blurred = blurImage(original, radius);
                if (_filter == "blur") {
                    result = original;
                    QPainter p(&result);
                    p.setCompositionMode(QPainter::CompositionMode_Source);
                    if (hasSelection())
                        p.setClipRegion(m_selectionRegion);
                    else
                        p.setClipRect(area);
                    p.drawImage(0, 0, blurred);
                    p.end();
                    m_layers[m_active].image = result;
                    return;
                }
                const QImage smooth = blurred.convertToFormat(P::format);
                for (int y = area.top(); y <= area.bottom(); ++y)
                    for (int x = area.left(); x <= area.right(); ++x) {
                        if (hasSelection() && !m_selectionRegion.contains(QPoint(x, y)))
                            continue;
                        const QColor c = result.pixelColor(x, y), b = smooth.pixelColor(x, y);
                        const auto   value = [](double _a, double _b) {
                            return qBound(0., 2 * _a - _b, 1.);
                        };
                        result.setPixelColor(x, y,
                                             QColor::fromRgbF(value(c.redF(), b.redF()),
                                                              value(c.greenF(), b.greenF()),
                                                              value(c.blueF(), b.blueF()),
                                                              c.alphaF()));
                    }
            } else
                for (int y = area.top(); y <= area.bottom(); ++y) {
                    auto row = reinterpret_cast<Pixel *>(result.scanLine(y));
                    for (int x = area.left(); x <= area.right(); ++x) {
                        if (hasSelection() && !m_selectionRegion.contains(QPoint(x, y)))
                            continue;
                        Pixel        pixel = row[x];
                        double       r = P::red(pixel), g = P::green(pixel), b = P::blue(pixel);
                        const double gray = .2126 * r + .7152 * g + .0722 * b;
                        if (_filter == "invert") {
                            r = 255 - r;
                            g = 255 - g;
                            b = 255 - b;
                        } else if (_filter == "grayscale")
                            r = g = b = gray;
                        else if (_filter == "sepia") {
                            double nr = .393 * r + .769 * g + .189 * b,
                                   ng = .349 * r + .686 * g + .168 * b;
                            b         = .272 * r + .534 * g + .131 * b;
                            r         = nr;
                            g         = ng;
                        } else if (_filter == "brightness") {
                            r += _amount * 2.55;
                            g += _amount * 2.55;
                            b += _amount * 2.55;
                        } else if (_filter == "contrast") {
                            const double f = std::pow((100 + _amount) / 100, 2);
                            r              = (r - 128) * f + 128;
                            g              = (g - 128) * f + 128;
                            b              = (b - 128) * f + 128;
                        } else if (_filter == "saturation") {
                            const double f = 1 + _amount / 100;
                            r              = gray + (r - gray) * f;
                            g              = gray + (g - gray) * f;
                            b              = gray + (b - gray) * f;
                        }
                        row[x] =
                            P::rgba(qBound(0., P::round(r), 255.), qBound(0., P::round(g), 255.),
                                    qBound(0., P::round(b), 255.), P::alpha(pixel));
                    }
                }
            m_layers[m_active].image = result.convertToFormat(P::premultiplied);
        };
        if (m_channelBits == 16)
            render.template operator()<QRgba64>();
        else
            render.template operator()<QRgb>();
    });
}
