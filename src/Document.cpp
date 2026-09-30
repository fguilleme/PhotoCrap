#include "Document.h"
#include <QBuffer>
#include <QClipboard>
#include <QColorSpace>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 MaxPixels = 32LL * 1024 * 1024;
constexpr qint64 MaxDocumentBytes = 512LL * 1024 * 1024;
constexpr qint64 MaxProjectBytes = 256LL * 1024 * 1024;
constexpr int MaxLayers = 64;
QImage blank(QSize size) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    image.setColorSpace(QColorSpace::SRgb);
    return image;
}
QString localPath(const QUrl &url) { return url.isLocalFile() ? url.toLocalFile() : QString{}; }
QString toolLabel(const QString &tool) {
    static const QMap<QString, QString> labels{{"brush","Pinceau"},{"eraser","Gomme"},
        {"fill","Remplissage"},{"move","Déplacement"},{"rectangle","Rectangle"},
        {"ellipse","Ellipse"},{"line","Ligne"},{"gradient","Dégradé"},{"text","Texte"}};
    return labels.value(tool, tool);
}
}

Document::Document(QObject *parent) : QObject(parent) { newDocument(1280, 800); }
bool Document::fail(const QString &message) { emit errorOccurred(message); return false; }
bool Document::validSize(int w, int h, int count) {
    if (w < 1 || h < 1 || w > 16384 || h > 16384 || qint64(w)*h > MaxPixels)
        return fail(QStringLiteral("Dimensions invalides : 1 à 16 384 px, 32 mégapixels maximum."));
    if (count < 1 || count > MaxLayers || qint64(w)*h*4*count > MaxDocumentBytes)
        return fail(QStringLiteral("Limite atteinte : 64 calques et 512 Mio de pixels par document."));
    return true;
}
bool Document::editable() {
    if (m_active < 0 || m_active >= m_layers.size()) return false;
    if (m_layers[m_active].locked) return fail(QStringLiteral("Ce calque est verrouillé."));
    return true;
}
Document::Snapshot Document::snapshot(const QString &label) const {
    return {m_layers, m_size, m_active, m_selection, m_stateId, label};
}
void Document::restore(const Snapshot &state) {
    m_layers = state.layers; m_size = state.size; m_active = state.active;
    m_selection = state.selection; m_stateId = state.id;
    changed(); emit documentChanged(); emit selectionChanged(); emit historyChanged();
}
void Document::trimHistory() {
    // QImage snapshots share their storage until an edit detaches a layer.
    // Count unique storage, not theoretical copies, to bound retained history.
    while (m_undo.size() > 1) {
        QSet<qint64> seen;
        qint64 bytes = 0;
        for (const auto &state : m_undo) for (const auto &layer : state.layers) {
            if (!seen.contains(layer.image.cacheKey())) {
                seen.insert(layer.image.cacheKey()); bytes += layer.image.sizeInBytes();
            }
        }
        if (m_undo.size() <= 40 && bytes <= 256LL*1024*1024) break;
        m_undo.removeFirst();
    }
}
void Document::commit(const Snapshot &before, const QString &label) {
    Snapshot state = before; state.label = label;
    m_undo.append(state); m_redo.clear(); m_stateId = m_nextId++;
    trimHistory(); changed(); emit historyChanged();
}
void Document::edit(const QString &label, const std::function<void()> &operation) {
    endStroke(); auto before = snapshot(label); operation(); commit(before, label);
}
void Document::changed(bool metadata) {
    m_cacheDirty = true; ++m_revision;
    if (metadata) emit layersChanged();
    emit imageChanged();
}
void Document::resetHistory(bool clean) {
    m_undo.clear(); m_redo.clear(); m_stateId = m_nextId++;
    m_savedId = clean ? m_stateId : 0;
    m_selection = {}; m_drawing = false; m_beforeStroke = {};
    changed(); emit documentChanged(); emit selectionChanged(); emit historyChanged();
}
QVariantList Document::layers() const {
    QVariantList result;
    // UI order is top to bottom, storage order is bottom to top.
    for (int i = int(m_layers.size())-1; i >= 0; --i) {
        const auto &l = m_layers[i];
        result.append(QVariantMap{{"layerIndex",i},{"name",l.name},{"visible",l.visible},
            {"locked",l.locked},{"opacity",l.opacity},{"blend",l.blend}});
    }
    return result;
}
QStringList Document::history() const {
    QStringList result{QStringLiteral("État initial")};
    for (const auto &state : m_undo) result.append(state.label);
    for (auto it = m_redo.crbegin(); it != m_redo.crend(); ++it) result.append(it->label);
    return result;
}
QPainter::CompositionMode Document::blendMode(int mode) {
    static const QPainter::CompositionMode modes[] = {QPainter::CompositionMode_SourceOver,
        QPainter::CompositionMode_Multiply, QPainter::CompositionMode_Screen,
        QPainter::CompositionMode_Overlay, QPainter::CompositionMode_Darken,
        QPainter::CompositionMode_Lighten, QPainter::CompositionMode_Difference};
    return modes[qBound(0,mode,6)];
}
const QImage &Document::composite() const {
    if (!m_cacheDirty) return m_composite;
    m_composite = blank(m_size);
    QPainter painter(&m_composite);
    for (const auto &layer : m_layers) if (layer.visible) {
        painter.setOpacity(layer.opacity); painter.setCompositionMode(blendMode(layer.blend));
        painter.drawImage(0,0,layer.image);
    }
    painter.end(); m_cacheDirty = false; return m_composite;
}
QImage Document::layerImage(int index) const {
    return index >= 0 && index < m_layers.size() ? m_layers[index].image : QImage{};
}
void Document::setActiveLayer(int index) {
    if (index < 0 || index >= m_layers.size() || m_active == index) return;
    endStroke(); m_active = index; emit layersChanged();
}
void Document::setTool(const QString &value) {
    static const QStringList tools{"brush","eraser","fill","picker","move","rectangle","ellipse","line","gradient","select","crop","text","hand"};
    if (!tools.contains(value)) return;
    endStroke(); m_tool = value; emit settingsChanged();
}
void Document::setColor(const QColor &value) { if(value.isValid()) {m_color=value; emit settingsChanged();} }
void Document::setSecondaryColor(const QColor &value) { if(value.isValid()) {m_secondary=value; emit settingsChanged();} }
void Document::setBrushSize(qreal value) { if(std::isfinite(value)) {m_brushSize=qBound(1.,value,500.); emit settingsChanged();} }
void Document::setBrushOpacity(qreal value) { if(std::isfinite(value)) {m_brushOpacity=qBound(0.,value,1.); emit settingsChanged();} }
void Document::setText(const QString &value) { m_text=value.left(4096); emit settingsChanged(); }
void Document::setTextSize(int value) { m_textSize=qBound(6,value,512); emit settingsChanged(); }
void Document::setFilledShapes(bool value) { m_filledShapes=value; emit settingsChanged(); }

bool Document::newDocument(int w, int h, bool transparent) {
    if (!validSize(w,h)) return false;
    m_size={w,h}; QImage image=blank(m_size);
    if (!transparent) image.fill(Qt::white);
    m_layers={{QStringLiteral("Arrière-plan"),image}}; m_active=0;
    m_name=QStringLiteral("Sans titre"); m_projectUrl=QUrl{}; resetHistory(true); return true;
}
QImage Document::readImage(const QString &path) {
    QImageReader reader(path); reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || !validSize(size.width(),size.height())) {
        if (!size.isValid()) fail(QStringLiteral("Format non reconnu ou image illisible."));
        return {};
    }
    QImage image=reader.read();
    if (image.isNull()) { fail(reader.errorString()); return {}; }
    if (image.colorSpace().isValid()) image.convertToColorSpace(QColorSpace::SRgb);
    else image.setColorSpace(QColorSpace::SRgb);
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
bool Document::openFile(const QUrl &url) {
    const QString path=localPath(url);
    if (path.isEmpty()) return fail(QStringLiteral("Choisissez un fichier local."));
    if (QFileInfo(path).suffix().compare("pcr",Qt::CaseInsensitive)==0) return loadProject(path);
    auto image=readImage(path); if(image.isNull()) return false;
    m_size=image.size(); m_layers={{QFileInfo(path).completeBaseName(),image}}; m_active=0;
    m_name=QFileInfo(path).completeBaseName(); m_projectUrl=QUrl{}; resetHistory(false); return true;
}
bool Document::importLayer(const QUrl &url) {
    if (!validSize(width(),height(),int(m_layers.size())+1)) return false;
    auto image=readImage(localPath(url)); if(image.isNull()) return false;
    edit(QStringLiteral("Importer un calque"),[&] {
        QImage target=blank(m_size); QPainter painter(&target);
        painter.drawImage((width()-image.width())/2,(height()-image.height())/2,image);
        painter.end(); m_layers.insert(++m_active,{QFileInfo(url.toLocalFile()).completeBaseName(),target});
    }); return true;
}
bool Document::saveProject(const QUrl &url) {
    endStroke(); QString path=localPath(url);
    if(path.isEmpty()) return fail(QStringLiteral("Chemin de sauvegarde invalide."));
    if(!path.endsWith(".pcr",Qt::CaseInsensitive)) path+=".pcr";
    QJsonArray layers;
    for(const auto &layer:m_layers) {
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        if(!layer.image.save(&buffer,"PNG")) return fail(QStringLiteral("Impossible d’encoder le calque."));
        layers.append(QJsonObject{{"name",layer.name},{"visible",layer.visible},{"locked",layer.locked},
            {"opacity",layer.opacity},{"blend",layer.blend},{"png",QString::fromLatin1(bytes.toBase64())}});
    }
    QJsonObject root{{"format","PhotoCrap"},{"version",1},{"width",width()},{"height",height()},
        {"activeLayer",m_active},{"layers",layers}};
    const QByteArray data=QJsonDocument(root).toJson(QJsonDocument::Compact);
    if(data.size()>MaxProjectBytes) return fail(QStringLiteral("Le projet dépasse 256 Mio. Réduisez ses dimensions ou ses calques."));
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) return fail(file.errorString());
    m_savedId=m_stateId; m_projectUrl=QUrl::fromLocalFile(path); m_name=QFileInfo(path).completeBaseName();
    emit documentChanged(); emit historyChanged(); emit statusMessage(QStringLiteral("Projet enregistré")); return true;
}
bool Document::loadProject(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    if(file.size()>MaxProjectBytes) return fail(QStringLiteral("Projet trop volumineux (256 Mio maximum)."));
    QJsonParseError error;
    const auto json=QJsonDocument::fromJson(file.readAll(),&error);
    const auto root=json.object();
    if(error.error!=QJsonParseError::NoError || root["format"]!="PhotoCrap" || root["version"].toInt()!=1)
        return fail(QStringLiteral("Projet PhotoCrap invalide ou version non prise en charge."));
    const int w=root["width"].toInt(), h=root["height"].toInt();
    const auto array=root["layers"].toArray();
    if(!validSize(w,h,int(array.size()))) return false;
    QVector<Layer> layers;
    for(const auto &value:array) {
        auto object=value.toObject();
        QByteArray png=QByteArray::fromBase64(object["png"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        QBuffer buffer(&png); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer,"PNG");
        if(reader.size()!=QSize(w,h)) return fail(QStringLiteral("Dimensions de calque incohérentes."));
        auto image=reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        if(image.isNull()) return fail(QStringLiteral("Calque corrompu."));
        image.setColorSpace(QColorSpace::SRgb);
        const double opacity=object["opacity"].toDouble(-1);
        const int blend=object["blend"].toInt(-1);
        if(opacity<0 || opacity>1 || blend<0 || blend>6) return fail(QStringLiteral("Propriétés de calque invalides."));
        layers.append({object["name"].toString().left(256),image,object["visible"].toBool(true),
            object["locked"].toBool(),opacity,blend});
    }
    // Only replace the current document after validating every layer.
    m_layers=layers; m_size={w,h}; m_active=qBound(0,root["activeLayer"].toInt(),int(layers.size())-1);
    m_projectUrl=QUrl::fromLocalFile(path); m_name=QFileInfo(path).completeBaseName(); resetHistory(true); return true;
}
bool Document::exportImage(const QUrl &url) {
    endStroke(); QString path=localPath(url); if(path.isEmpty()) return fail(QStringLiteral("Chemin d’export invalide."));
    QString suffix=QFileInfo(path).suffix().toLower(); if(suffix.isEmpty()) {path+=".png"; suffix="png";}
    if(suffix!="png" && suffix!="jpg" && suffix!="jpeg" && suffix!="bmp" && suffix!="webp")
        return fail(QStringLiteral("Export disponible en PNG, JPEG, BMP ou WebP (si le module Qt est installé)."));
    QImage result=composite();
    if(suffix=="jpg" || suffix=="jpeg" || suffix=="bmp") {
        QImage white(result.size(),QImage::Format_RGB32); white.fill(Qt::white);
        QPainter p(&white); p.drawImage(0,0,result); p.end(); result=white;
    }
    result.setColorSpace(QColorSpace::SRgb);
    QSaveFile file(path); if(!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    QImageWriter writer(&file,suffix.toLatin1()); writer.setQuality(95);
    if(!writer.write(result)) return fail(writer.errorString());
    if(!file.commit()) return fail(file.errorString());
    emit statusMessage(QStringLiteral("Image exportée")); return true;
}

void Document::addLayer() {
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    edit(QStringLiteral("Nouveau calque"),[&]{m_layers.insert(++m_active,{QStringLiteral("Calque %1").arg(m_layers.size()+1),blank(m_size)});});
}
void Document::duplicateLayer() {
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    edit(QStringLiteral("Dupliquer le calque"),[&]{auto layer=m_layers[m_active]; layer.name+=" — copie"; m_layers.insert(++m_active,layer);});
}
void Document::removeLayer() {
    if(!editable() || m_layers.size()==1) return;
    edit(QStringLiteral("Supprimer le calque"),[&]{m_layers.removeAt(m_active); m_active=qMin(m_active,int(m_layers.size())-1);});
}
void Document::moveLayer(int direction) {
    const int to=m_active+(direction>0?1:-1); if(to<0 || to>=m_layers.size()) return;
    edit(QStringLiteral("Réordonner le calque"),[&]{m_layers.swapItemsAt(m_active,to); m_active=to;});
}
void Document::mergeDown() {
    if(!editable() || m_active<1) return;
    const auto &lower=m_layers[m_active-1]; const auto &upper=m_layers[m_active];
    if(lower.locked || !lower.visible || !upper.visible || lower.blend!=0 || upper.blend!=0)
        {fail(QStringLiteral("La fusion nécessite deux calques visibles en mode Normal, non verrouillés.")); return;}
    edit(QStringLiteral("Fusionner vers le bas"),[&]{
        auto &bottom=m_layers[m_active-1]; const auto &top=m_layers[m_active];
        QImage image=blank(m_size); QPainter painter(&image);
        painter.setOpacity(bottom.opacity); painter.drawImage(0,0,bottom.image);
        painter.setOpacity(top.opacity); painter.drawImage(0,0,top.image); painter.end();
        bottom.image=image; bottom.opacity=1; m_layers.removeAt(m_active--);
    });
}
void Document::renameLayer(int index,const QString &name) {
    if(index<0 || index>=m_layers.size() || name.trimmed().isEmpty() || m_layers[index].name==name.trimmed()) return;
    edit(QStringLiteral("Renommer le calque"),[&]{m_layers[index].name=name.trimmed().left(256);});
}
void Document::setLayerVisible(int i,bool visible) {
    if(i<0 || i>=m_layers.size() || m_layers[i].visible==visible) return;
    edit(QStringLiteral("Visibilité du calque"),[&]{m_layers[i].visible=visible;});
}
void Document::setLayerLocked(int i,bool locked) {
    if(i<0 || i>=m_layers.size() || m_layers[i].locked==locked) return;
    edit(QStringLiteral("Verrouillage du calque"),[&]{m_layers[i].locked=locked;});
}
void Document::setLayerOpacity(int i,qreal opacity) {
    if(i<0 || i>=m_layers.size() || !std::isfinite(opacity)) return;
    opacity=qBound(0.,opacity,1.); if(qFuzzyCompare(m_layers[i].opacity,opacity)) return;
    edit(QStringLiteral("Opacité du calque"),[&]{m_layers[i].opacity=opacity;});
}
void Document::setLayerBlend(int i,int blend) {
    if(i<0 || i>=m_layers.size() || blend<0 || blend>6 || m_layers[i].blend==blend) return;
    edit(QStringLiteral("Mode de fusion"),[&]{m_layers[i].blend=blend;});
}

void Document::beginStroke(qreal x,qreal y) {
    if(!std::isfinite(x) || !std::isfinite(y)) return;
    endStroke(); m_anchor=m_last={x,y};
    if(m_tool=="hand") return;
    if(m_tool=="picker") { if(composite().rect().contains(QPoint(int(x),int(y)))) setColor(composite().pixelColor(int(x),int(y))); return; }
    if(m_tool=="select" || m_tool=="crop") {m_drawing=true; m_selection={}; emit selectionChanged(); return;}
    if(!editable()) return;
    m_beforeStroke=snapshot(toolLabel(m_tool)); m_drawing=true; m_strokeChanged=false;
    if(m_tool=="brush" || m_tool=="eraser") paintSegment(m_anchor,m_anchor);
    else if(m_tool=="fill") {floodFill(QPoint(int(x),int(y))); endStroke();}
    else if(m_tool=="text") {
        if(!m_text.isEmpty()) {
            QPainter painter(&m_layers[m_active].image); painter.setRenderHint(QPainter::TextAntialiasing);
            if(hasSelection()) painter.setClipRect(m_selection);
            QFont font("sans-serif"); font.setPixelSize(m_textSize); font.setWeight(QFont::DemiBold);
            painter.setFont(font); painter.setPen(m_color); painter.setOpacity(m_brushOpacity);
            painter.drawText(m_anchor+QPointF(0,m_textSize),m_text); painter.end(); m_strokeChanged=true;
        }
        endStroke();
    }
}
void Document::paintSegment(const QPointF &from,const QPointF &to) {
    QPainter painter(&m_layers[m_active].image); painter.setRenderHint(QPainter::Antialiasing);
    if(hasSelection()) painter.setClipRect(m_selection);
    painter.setOpacity(m_brushOpacity);
    if(m_tool=="eraser") painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    QColor ink=m_tool=="eraser"?QColor(Qt::black):m_color;
    painter.setPen(QPen(ink,m_brushSize,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    if(from==to) {painter.setPen(Qt::NoPen); painter.setBrush(ink); painter.drawEllipse(to,m_brushSize/2,m_brushSize/2);}
    else painter.drawLine(from,to);
    painter.end(); m_strokeChanged=true; changed(false);
}
void Document::renderShape(const QPointF &to) {
    m_layers[m_active].image=m_beforeStroke.layers[m_active].image;
    if(m_tool=="move") {
        const QImage source=m_beforeStroke.layers[m_active].image;
        QImage result=hasSelection()?source:blank(m_size);
        QPainter painter(&result);
        const QPoint offset=(to-m_anchor).toPoint();
        if(hasSelection()) {
            painter.setCompositionMode(QPainter::CompositionMode_Clear); painter.fillRect(m_selection,Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(m_selection.topLeft()+offset,source.copy(m_selection));
        } else painter.drawImage(offset,source);
        painter.end(); m_layers[m_active].image=result;
    } else {
        QPainter painter(&m_layers[m_active].image); painter.setRenderHint(QPainter::Antialiasing);
        if(hasSelection()) painter.setClipRect(m_selection);
        painter.setOpacity(m_brushOpacity);
        painter.setPen(QPen(m_color,m_brushSize,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        painter.setBrush(m_filledShapes?QBrush(m_color):Qt::NoBrush);
        const QRectF rect=QRectF(m_anchor,to).normalized();
        if(m_tool=="rectangle") painter.drawRect(rect);
        else if(m_tool=="ellipse") painter.drawEllipse(rect);
        else if(m_tool=="line") painter.drawLine(m_anchor,to);
        else if(m_tool=="gradient") {
            QLinearGradient gradient(m_anchor,to); gradient.setColorAt(0,m_color); gradient.setColorAt(1,m_secondary);
            painter.fillRect(QRect(QPoint{},m_size),gradient);
        }
    }
    m_strokeChanged=true; changed(false);
}
void Document::continueStroke(qreal x,qreal y) {
    if(!m_drawing || !std::isfinite(x) || !std::isfinite(y)) return;
    const QPointF point(qBound(-32768.,x,32768.),qBound(-32768.,y,32768.));
    if(m_tool=="select" || m_tool=="crop") {
        m_selection=QRect(m_anchor.toPoint(),point.toPoint()).normalized().intersected(QRect(QPoint{},m_size));
        emit selectionChanged();
    } else if(m_tool=="brush" || m_tool=="eraser") paintSegment(m_last,point);
    else renderShape(point);
    m_last=point;
}
void Document::endStroke() {
    if(!m_drawing) return;
    m_drawing=false;
    if(m_tool=="select" || m_tool=="crop") return;
    if(m_strokeChanged) {
        if(m_tool=="move" && hasSelection()) {
            m_selection=m_selection.translated((m_last-m_anchor).toPoint()).intersected(QRect(QPoint{},m_size));
            emit selectionChanged();
        }
        commit(m_beforeStroke,toolLabel(m_tool));
    }
    m_beforeStroke={};
}
void Document::cancelStroke() {
    if(!m_drawing) return;
    m_drawing=false;
    if(m_tool=="select" || m_tool=="crop") deselect(); else restore(m_beforeStroke);
    m_beforeStroke={};
}
void Document::floodFill(QPoint point) {
    auto &image=m_layers[m_active].image;
    const QRect bounds=hasSelection()?m_selection:image.rect();
    if(!bounds.contains(point)) return;
    const QRgb target=image.pixel(point);
    QImage solid(1,1,QImage::Format_ARGB32_Premultiplied); solid.fill(m_color);
    const QRgb replacement=solid.pixel(0,0);
    if(target==replacement && qFuzzyCompare(m_brushOpacity,1.)) return;
    // Scanline flood fill: visited pixels are kept separate so semi-transparent
    // replacement colors cannot cause pixels to be queued repeatedly.
    QImage mask(image.size(),QImage::Format_Grayscale8); mask.fill(0);
    QVector<QPoint> stack{point};
    auto matches=[&](int x,int y){return !mask.constScanLine(y)[x] && image.pixel(x,y)==target;};
    while(!stack.isEmpty()) {
        auto seed=stack.takeLast(); if(!matches(seed.x(),seed.y())) continue;
        int left=seed.x(),right=seed.x();
        while(left>bounds.left() && matches(left-1,seed.y())) --left;
        while(right<bounds.right() && matches(right+1,seed.y())) ++right;
        auto row=mask.scanLine(seed.y()); for(int x=left;x<=right;++x) row[x]=255;
        for(int y:{seed.y()-1,seed.y()+1}) if(y>=bounds.top() && y<=bounds.bottom()) {
            bool run=false;
            for(int x=left;x<=right;++x) {
                bool match=matches(x,y); if(match && !run) stack.append({x,y}); run=match;
            }
        }
    }
    QImage overlay=blank(m_size);
    for(int y=bounds.top();y<=bounds.bottom();++y) {
        auto row=reinterpret_cast<QRgb*>(overlay.scanLine(y)); auto selected=mask.constScanLine(y);
        for(int x=bounds.left();x<=bounds.right();++x) if(selected[x]) row[x]=replacement;
    }
    QPainter painter(&image); painter.setOpacity(m_brushOpacity); painter.drawImage(0,0,overlay);
    painter.end(); m_strokeChanged=true;
}

void Document::undo() {
    endStroke(); if(m_undo.isEmpty()) return;
    auto state=m_undo.takeLast(); m_redo.append(snapshot(state.label)); restore(state);
}
void Document::redo() {
    endStroke(); if(m_redo.isEmpty()) return;
    auto state=m_redo.takeLast(); m_undo.append(snapshot(state.label)); restore(state);
}
void Document::selectAll() {endStroke(); m_selection=QRect(QPoint{},m_size); emit selectionChanged();}
void Document::deselect() {endStroke(); m_selection={}; emit selectionChanged();}
void Document::clearSelected() {
    if(!editable()) return;
    edit(QStringLiteral("Effacer"),[&]{QPainter p(&m_layers[m_active].image); p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(hasSelection()?m_selection:QRect(QPoint{},m_size),Qt::transparent);});
}
void Document::copy() {
    endStroke(); QGuiApplication::clipboard()->setImage(hasSelection()?composite().copy(m_selection):composite());
    emit statusMessage(QStringLiteral("Copie de l’image visible dans le presse-papiers"));
}
void Document::paste() {
    auto source=QGuiApplication::clipboard()->image(); if(source.isNull()) return;
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    if(source.colorSpace().isValid()) source.convertToColorSpace(QColorSpace::SRgb);
    edit(QStringLiteral("Coller"),[&]{auto image=blank(m_size); QPainter p(&image);
        p.drawImage((width()-source.width())/2,(height()-source.height())/2,source); p.end();
        m_layers.insert(++m_active,{QStringLiteral("Image collée"),image});});
}
void Document::cropToSelection() {
    if(!hasSelection()) return;
    edit(QStringLiteral("Recadrer"),[&]{for(auto &layer:m_layers) layer.image=layer.image.copy(m_selection);
        m_size=m_selection.size(); m_selection={};});
    emit documentChanged(); emit selectionChanged();
}
void Document::resizeImage(int w,int h) {
    if(!validSize(w,h,int(m_layers.size())) || m_size==QSize(w,h)) return;
    edit(QStringLiteral("Redimensionner l’image"),[&]{for(auto &layer:m_layers)
        layer.image=layer.image.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        m_size={w,h}; m_selection={};}); emit documentChanged(); emit selectionChanged();
}
void Document::transformImage(const QString &operation) {
    if(operation!="rotateLeft" && operation!="rotateRight" && operation!="flipH" && operation!="flipV") return;
    edit(QStringLiteral("Transformer l’image"),[&]{for(auto &layer:m_layers) {
        if(operation=="flipH" || operation=="flipV") {
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
            layer.image=layer.image.flipped(operation=="flipH"?Qt::Horizontal:Qt::Vertical);
#else
            layer.image=layer.image.mirrored(operation=="flipH",operation=="flipV");
#endif
        }
        else layer.image=layer.image.transformed(QTransform().rotate(operation=="rotateRight"?90:-90));
    }
    if(operation.startsWith("rotate")) m_size.transpose(); m_selection={};});
    emit documentChanged(); emit selectionChanged();
}
void Document::applyFilter(const QString &filter,qreal amount) {
    static const QStringList supported{"grayscale","invert","sepia","brightness","contrast","saturation","blur","sharpen"};
    if(!supported.contains(filter) || !std::isfinite(amount) || !editable()) return;
    amount=qBound(-100.,amount,100.);
    edit(QStringLiteral("Filtre : %1").arg(filter),[&]{
        const QImage original=m_layers[m_active].image;
        QImage result=original.convertToFormat(QImage::Format_ARGB32);
        const QRect area=hasSelection()?m_selection:result.rect();
        if(filter=="blur" || filter=="sharpen") {
            // Separable box blur on premultiplied pixels avoids dark alpha fringes.
            const int radius=filter=="sharpen"?1:qBound(1,int(amount),30);
            QImage horizontal=blank(m_size), blurred=blank(m_size);
            auto pass=[&](const QImage &src,QImage &dst,bool vertical){
                const int major=vertical?width():height(), minor=vertical?height():width(), window=radius*2+1;
                for(int a=0;a<major;++a) {
                    qint64 r=0,g=0,b=0,alpha=0;
                    auto sample=[&](int pos,int sign){const int p=qBound(0,pos,minor-1);
                        QRgb c=src.pixel(vertical?a:p,vertical?p:a);
                        r+=sign*qRed(c); g+=sign*qGreen(c); b+=sign*qBlue(c); alpha+=sign*qAlpha(c);};
                    for(int k=-radius;k<=radius;++k) sample(k,1);
                    for(int n=0;n<minor;++n) {
                        dst.setPixel(vertical?a:n,vertical?n:a,qRgba(int(r/window),int(g/window),int(b/window),int(alpha/window)));
                        sample(n-radius,-1); sample(n+radius+1,1);
                    }
                }
            };
            pass(original,horizontal,false); pass(horizontal,blurred,true);
            if(filter=="blur") {
                result=original; QPainter p(&result); p.setCompositionMode(QPainter::CompositionMode_Source);
                p.setClipRect(area); p.drawImage(0,0,blurred); p.end();
                m_layers[m_active].image=result; return;
            }
            const QImage smooth=blurred.convertToFormat(QImage::Format_ARGB32);
            for(int y=area.top();y<=area.bottom();++y) for(int x=area.left();x<=area.right();++x) {
                const QColor c=result.pixelColor(x,y), b=smooth.pixelColor(x,y);
                result.setPixelColor(x,y,QColor(qBound(0,2*c.red()-b.red(),255),qBound(0,2*c.green()-b.green(),255),
                    qBound(0,2*c.blue()-b.blue(),255),c.alpha()));
            }
        } else for(int y=area.top();y<=area.bottom();++y) {
            auto row=reinterpret_cast<QRgb*>(result.scanLine(y));
            for(int x=area.left();x<=area.right();++x) {
                QRgb pixel=row[x]; double r=qRed(pixel),g=qGreen(pixel),b=qBlue(pixel);
                const double gray=.2126*r+.7152*g+.0722*b;
                if(filter=="invert") {r=255-r;g=255-g;b=255-b;}
                else if(filter=="grayscale") r=g=b=gray;
                else if(filter=="sepia") {double nr=.393*r+.769*g+.189*b,ng=.349*r+.686*g+.168*b; b=.272*r+.534*g+.131*b;r=nr;g=ng;}
                else if(filter=="brightness") {r+=amount*2.55;g+=amount*2.55;b+=amount*2.55;}
                else if(filter=="contrast") {const double f=std::pow((100+amount)/100,2);r=(r-128)*f+128;g=(g-128)*f+128;b=(b-128)*f+128;}
                else if(filter=="saturation") {const double f=1+amount/100;r=gray+(r-gray)*f;g=gray+(g-gray)*f;b=gray+(b-gray)*f;}
                row[x]=qRgba(qBound(0,qRound(r),255),qBound(0,qRound(g),255),qBound(0,qRound(b),255),qAlpha(pixel));
            }
        }
        m_layers[m_active].image=result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    });
}

void Document::createDemo() {
    newDocument(1440,1000);
    auto &background=m_layers[0]; background.name=QStringLiteral("Fond · bleu nuit");
    QPainter bg(&background.image);
    QLinearGradient gradient(0,0,1440,1000); gradient.setColorAt(0,QColor("#0e2632")); gradient.setColorAt(1,QColor("#30424c"));
    bg.fillRect(background.image.rect(),gradient); bg.end();
    QImage orb=blank(m_size); QPainter p(&orb); p.setRenderHint(QPainter::Antialiasing);
    QRadialGradient sphere(QPointF(1010,405),350,QPointF(910,270));
    sphere.setColorAt(0,QColor("#edffe0")); sphere.setColorAt(.36,QColor("#b3ecaf"));
    sphere.setColorAt(.7,QColor("#70bda5")); sphere.setColorAt(1,QColor("#28595c"));
    p.setBrush(sphere); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(1010,405),330,330);
    p.setPen(QPen(QColor(213,255,224,75),1.5)); p.setBrush(Qt::NoBrush);
    for(int i=0;i<8;++i) p.drawEllipse(QPointF(1010,405),350+i*17,350+i*17);
    p.end(); m_layers.append({QStringLiteral("Sphère · lumière"),orb});
    QImage typography=blank(m_size); QPainter t(&typography); t.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans-serif"); font.setPixelSize(19); font.setLetterSpacing(QFont::AbsoluteSpacing,5);
    t.setFont(font); t.setPen(QColor("#bee8ce")); t.drawText(QPointF(88,106),QStringLiteral("PHOTOGRAPHIE  /  DESIGN  /  EXPLORATION"));
    font.setPixelSize(146); font.setLetterSpacing(QFont::AbsoluteSpacing,-6); font.setWeight(QFont::Bold);
    t.setFont(font); t.setPen(QColor("#f2f2e7")); t.drawText(QPointF(78,390),QStringLiteral("Make room"));
    t.drawText(QPointF(78,552),QStringLiteral("for wonder."));
    font.setPixelSize(25); font.setWeight(QFont::Normal); font.setLetterSpacing(QFont::AbsoluteSpacing,0);
    t.setFont(font); t.setPen(QColor("#afc4c8")); t.drawText(QPointF(88,646),QStringLiteral("Une idée. Quelques calques. Des possibilités infinies."));
    t.setPen(QPen(QColor("#91b8a7"),2)); t.drawLine(QPointF(88,856),QPointF(1352,856));
    font.setPixelSize(19); font.setLetterSpacing(QFont::AbsoluteSpacing,2); t.setFont(font);
    t.drawText(QPointF(88,912),QStringLiteral("STUDIO EXPÉRIMENTAL")); t.drawText(QPointF(1162,912),QStringLiteral("VOL. 001"));
    t.end(); m_layers.append({QStringLiteral("Composition · typographie"),typography});
    m_layers.append({QStringLiteral("Vos retouches"),blank(m_size)}); m_active=3;
    m_name=QStringLiteral("Exploration créative"); resetHistory(false);
}
