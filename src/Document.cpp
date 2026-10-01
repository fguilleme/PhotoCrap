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
#include <QThreadPool>
#include <QtConcurrent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#ifdef PHOTOCRAP_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

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
    static const QMap<QString, const char*> labels{{"brush",QT_TRANSLATE_NOOP("Document","Pinceau")},{"eraser",QT_TRANSLATE_NOOP("Document","Gomme")},
        {"fill",QT_TRANSLATE_NOOP("Document","Remplissage")},{"move",QT_TRANSLATE_NOOP("Document","Déplacement")},{"rectangle",QT_TRANSLATE_NOOP("Document","Rectangle")},
        {"ellipse",QT_TRANSLATE_NOOP("Document","Ellipse")},{"line",QT_TRANSLATE_NOOP("Document","Ligne")},{"gradient",QT_TRANSLATE_NOOP("Document","Dégradé")},{"text",QT_TRANSLATE_NOOP("Document","Texte")}};
    return labels.contains(tool)?QCoreApplication::translate("Document",labels.value(tool)):tool;
}
bool requiresFullComposite(const Layer &layer) {
    static const QStringList filters{QStringLiteral("gaussianBlur"),QStringLiteral("motionBlur"),QStringLiteral("boxBlur"),
        QStringLiteral("unsharpMask"),QStringLiteral("highPass"),QStringLiteral("mosaic"),QStringLiteral("noise")};
    return layer.visible&&layer.kind==QStringLiteral("adjustment")&&filters.contains(layer.adjustmentType);
}
}

Document::Document(QObject *parent) : QObject(parent) { newDocument(1280, 800); }
bool Document::fail(const QString &message) { emit errorOccurred(message); return false; }
bool Document::validSize(int w, int h, int count) {
    if (w < 1 || h < 1 || w > 16384 || h > 16384 || qint64(w)*h > MaxPixels)
        return fail(tr("Dimensions invalides : 1 à 16 384 px, 32 mégapixels maximum."));
    if (count < 1 || count > MaxLayers || qint64(w)*h*4*count > MaxDocumentBytes)
        return fail(tr("Limite atteinte : 64 calques et 512 Mio de pixels par document."));
    return true;
}
bool Document::editable() {
    if (m_active < 0 || m_active >= m_layers.size()) return false;
    if (m_layers[m_active].locked) return fail(tr("Ce calque est verrouillé."));
    if (m_layers[m_active].kind != QStringLiteral("pixel") && !m_editingMask)
        return fail(tr("Sélectionnez le masque de ce calque pour peindre dessus."));
    return true;
}
Document::Snapshot Document::snapshot(const QString &label) const {
    return {m_layers, m_size, m_active, m_selection, m_editingMask, m_stateId, label};
}
void Document::restore(const Snapshot &state) {
    m_layers = state.layers; m_size = state.size; m_active = state.active;
    m_selection = state.selection; m_editingMask = state.editingMask; m_stateId = state.id;
    changed(); emit documentChanged(); emit selectionChanged(); emit historyChanged();
}
void Document::trimHistory() {
    // QImage snapshots share their storage until an edit detaches a layer.
    // Count unique storage, not theoretical copies, to bound retained history.
    while (m_undo.size() > 1) {
        QSet<qint64> seen;
        qint64 bytes = 0;
        for (const auto &state : m_undo) for (const auto &layer : state.layers) {
            for (const QImage *image : {&layer.image, &layer.mask, &layer.source})
                if (!image->isNull() && !seen.contains(image->cacheKey())) {
                    seen.insert(image->cacheKey()); bytes += image->sizeInBytes();
                }
        }
        if (m_undo.size() <= 40 && bytes <= 256LL*1024*1024) break;
        m_undo.removeFirst();
    }
}
void Document::commit(const Snapshot &before, const QString &label,bool invalidateFull) {
    Snapshot state = before; state.label = label;
    m_undo.append(state); m_redo.clear(); m_stateId = m_nextId++;
    trimHistory(); changed(invalidateFull); emit historyChanged();
}
void Document::edit(const QString &label, const std::function<void()> &operation) {
    endStroke(); auto before = snapshot(label); operation(); commit(before, label);
}
void Document::changed(bool metadata) {
    m_cacheDirty = true;if(metadata)m_fullCompositeDirty=true;++m_revision;
    if (metadata) {++m_thumbnailRevision;emit thumbnailsChanged();emit layersChanged();}
    emit imageChanged();
}
void Document::resetHistory(bool clean) {
    m_undo.clear(); m_redo.clear(); m_stateId = m_nextId++;
    m_savedId = clean ? m_stateId : 0;
    m_selection = {}; m_drawing = false; m_editingMask = false; m_previewingLayer = false; m_frequencyPreviewDraft = false;
    m_beforeStroke = {}; m_beforeLayerPreview = {}; m_strokeBase={};m_strokeCoverage={};m_strokeBounds={};m_frequencyCache.clear();
    m_fullCompositeDirty=true;m_dirtyRegion={};
    changed(); emit documentChanged(); emit selectionChanged(); emit historyChanged();
}
QVariantList Document::layers() const {
    QVariantList result;
    // UI order is top to bottom, storage order is bottom to top.
    for (int i = int(m_layers.size())-1; i >= 0; --i) {
        const auto &l = m_layers[i];
        result.append(QVariantMap{{"layerIndex",i},{"name",l.name},{"visible",l.visible},
            {"locked",l.locked},{"opacity",l.opacity},{"blend",l.blend},
            {"kind",l.kind},{"adjustmentType",l.adjustmentType},{"parameters",l.parameters},
            {"hasMask",!l.mask.isNull()},{"maskEnabled",l.maskEnabled},
            {"editingMask",m_editingMask && i==m_active}});
    }
    return result;
}
QStringList Document::history() const {
    QStringList result{tr("État initial")};
    for (const auto &state : m_undo) result.append(state.label);
    for (auto it = m_redo.crbegin(); it != m_redo.crend(); ++it) result.append(it->label);
    return result;
}
QImage Document::applyMask(const QImage &source,const QImage &mask,qreal density,qreal feather) {
    if(source.isNull() || mask.isNull()) return source;
    QImage result=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    density=qBound(0.,density,1.);feather=qBound(0.,feather,100.);
    const QImage coverage=(feather>=.5?blurImage(mask,qRound(feather)):mask).convertToFormat(QImage::Format_Grayscale8);
    const int h=qMin(result.height(),coverage.height()), w=qMin(result.width(),coverage.width());
    for(int y=0;y<h;++y) {
        auto pixels=reinterpret_cast<QRgb*>(result.scanLine(y)); const uchar *m=coverage.constScanLine(y);
        for(int x=0;x<w;++x) {
            const int a=qRound(255-(255-m[x])*density);
            pixels[x]=qRgba(qRed(pixels[x])*a/255,qGreen(pixels[x])*a/255,qBlue(pixels[x])*a/255,qAlpha(pixels[x])*a/255);
        }
    }
    return result;
}
QImage Document::blendImages(const QImage &base,const QImage &source,int mode,qreal opacity) {
    if(source.isNull())return base;
    opacity=qBound(0.,opacity,1.);mode=qBound(0,mode,26);
    if(mode==0) {
        QImage background=base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&background);painter.setOpacity(opacity);painter.drawImage(0,0,source);painter.end();
        background.setColorSpace(QColorSpace::SRgb);return background;
    }
    QImage background=base.convertToFormat(QImage::Format_ARGB32);
    const QImage foreground=source.convertToFormat(QImage::Format_ARGB32);
    auto channel=[](double b,double s,int m) {
        auto burn=[](double baseValue,double blendValue){return blendValue<=0?0.:1.-qMin(1.,(1.-baseValue)/blendValue);};
        auto dodge=[](double baseValue,double blendValue){return blendValue>=1?1.:qMin(1.,baseValue/(1.-blendValue));};
        switch(m) {
        case 2:return qMin(b,s); case 3:return b*s; case 4:return burn(b,s);
        case 5:return qMax(0.,b+s-1.); case 7:return qMax(b,s); case 8:return b+s-b*s;
        case 9:return dodge(b,s); case 10:return qMin(1.,b+s);
        case 12:return b<=.5?2*b*s:1-2*(1-b)*(1-s);
        case 13:return s<=.5?b-(1-2*s)*b*(1-b):b+(2*s-1)*(b<=.25?((16*b-12)*b+4)*b:std::sqrt(b)-b);
        case 14:return s<=.5?2*b*s:1-2*(1-b)*(1-s);
        case 15:return s<=.5?burn(b,2*s):dodge(b,2*s-1);
        case 16:return qBound(0.,b+2*s-1.,1.);
        case 17:return s<=.5?qMin(b,2*s):qMax(b,2*s-1);
        case 18:return (s<=.5?burn(b,2*s):dodge(b,2*s-1))<.5?0.:1.;
        case 19:return std::abs(b-s); case 20:return b+s-2*b*s;
        case 21:return qMax(0.,b-s); case 22:return s<=0?1.:qMin(1.,b/s);
        default:return s;
        }
    };
    for(int y=0;y<background.height();++y) for(int x=0;x<background.width();++x) {
        const QColor bc=background.pixelColor(x,y),sc=foreground.pixelColor(x,y);
        double ab=bc.alphaF(),as=sc.alphaF()*opacity;
        if(mode==1) {
            const quint32 hash=quint32(x*73856093u)^quint32(y*19349663u);
            as=(hash&0xffffu)<quint32(as*65535.)?1.:0.;
        }
        if(as<=0)continue;
        QColor blended=sc;
        if(mode==6 || mode==11) {
            const int baseTotal=bc.red()+bc.green()+bc.blue(),sourceTotal=sc.red()+sc.green()+sc.blue();
            blended=(mode==6 ? sourceTotal<baseTotal : sourceTotal>baseTotal)?sc:bc;
        } else if(mode>=23) {
            float bh,bs,bl,ba,sh,ss,sl,sa;bc.getHslF(&bh,&bs,&bl,&ba);sc.getHslF(&sh,&ss,&sl,&sa);
            if(bh<0)bh=0;if(sh<0)sh=0;
            if(mode==23)blended.setHslF(sh,bs,bl);
            else if(mode==24)blended.setHslF(bh,ss,bl);
            else if(mode==25)blended.setHslF(sh,ss,bl);
            else blended.setHslF(bh,bs,sl);
        } else if(mode>=2) {
            blended=QColor::fromRgbF(channel(bc.redF(),sc.redF(),mode),channel(bc.greenF(),sc.greenF(),mode),channel(bc.blueF(),sc.blueF(),mode));
        }
        const double ao=as+ab*(1-as);
        auto compose=[&](double b,double s,double mix){return ao<=0?0.:((1-as)*ab*b+(1-ab)*as*s+ab*as*mix)/ao;};
        background.setPixelColor(x,y,QColor::fromRgbF(
            qBound(0.,compose(bc.redF(),sc.redF(),blended.redF()),1.),
            qBound(0.,compose(bc.greenF(),sc.greenF(),blended.greenF()),1.),
            qBound(0.,compose(bc.blueF(),sc.blueF(),blended.blueF()),1.),qBound(0.,ao,1.)));
    }
    background.setColorSpace(QColorSpace::SRgb);
    return background.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::blurImage(const QImage &source,int radius) {
    if(source.isNull() || radius<=0) return source;
    radius=qBound(1,radius,100); const int w=source.width(),h=source.height(),window=radius*2+1;
    const QImage input=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
#ifdef PHOTOCRAP_USE_ACCELERATE
    QImage accelerated=blank(source.size());
    vImage_Buffer sourceBuffer{const_cast<uchar*>(input.constBits()),vImagePixelCount(h),vImagePixelCount(w),size_t(input.bytesPerLine())};
    vImage_Buffer destinationBuffer{accelerated.bits(),vImagePixelCount(h),vImagePixelCount(w),size_t(accelerated.bytesPerLine())};
    const vImage_Error error=vImageBoxConvolve_ARGB8888(&sourceBuffer,&destinationBuffer,nullptr,0,0,
        uint32_t(window),uint32_t(window),nullptr,kvImageEdgeExtend);
    if(error==kvImageNoError)return accelerated;
#endif
    QImage horizontal=blank(source.size()),result=blank(source.size());
    auto pass=[&](const QImage &src,QImage &dst,bool vertical) {
        const int major=vertical?w:h,minor=vertical?h:w;
        const int jobs=qMin(major,qMax(1,QThreadPool::globalInstance()->maxThreadCount()*2));
        QVector<int> blocks(jobs);std::iota(blocks.begin(),blocks.end(),0);
        QtConcurrent::blockingMap(blocks,[&](int block){
          const int first=major*block/jobs,last=major*(block+1)/jobs;
          for(int a=first;a<last;++a) {
            qint64 r=0,g=0,b=0,alpha=0;
            auto sample=[&](int pos,int sign){const int p=qBound(0,pos,minor-1);
                const auto row=reinterpret_cast<const QRgb*>(src.constScanLine(vertical?p:a));const QRgb c=row[vertical?a:p];
                r+=sign*qRed(c);g+=sign*qGreen(c);b+=sign*qBlue(c);alpha+=sign*qAlpha(c);};
            for(int k=-radius;k<=radius;++k) sample(k,1);
            for(int n=0;n<minor;++n) {
                auto row=reinterpret_cast<QRgb*>(dst.scanLine(vertical?n:a));row[vertical?a:n]=qRgba(int(r/window),int(g/window),int(b/window),int(alpha/window));
                sample(n-radius,-1);sample(n+radius+1,1);
            }
          }
        });
    };
    pass(input,horizontal,false);pass(horizontal,result,true);return result;
}
QImage Document::gaussianBlurImage(const QImage &source,qreal radius) {
    if(source.isNull()||radius<=0)return source;
    // Three box convolutions closely approximate a Gaussian while retaining
    // the platform accelerated O(n) implementation of each pass.
    const int boxRadius=qBound(1,qRound(radius),100);
    QImage result=source;
    for(int pass=0;pass<3;++pass)result=blurImage(result,boxRadius);
    return result;
}
QImage Document::motionBlurImage(const QImage &source,qreal distance,qreal angle) {
    if(source.isNull()||distance<=0)return source;
    const QImage input=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage result(input.size(),QImage::Format_ARGB32_Premultiplied);result.setColorSpace(QColorSpace::SRgb);
    distance=qBound(0.,distance,200.);angle=qDegreesToRadians(angle);
    const int samples=qBound(3,qRound(distance/4.)*2+1,33),half=samples/2;
    const qreal dx=std::cos(angle)*distance/qMax(1,samples-1),dy=std::sin(angle)*distance/qMax(1,samples-1);
    QVector<int> rows(input.height());std::iota(rows.begin(),rows.end(),0);
    QtConcurrent::blockingMap(rows,[&](int y){
        auto output=reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<input.width();++x) {
            int a=0,r=0,g=0,b=0;
            for(int sample=-half;sample<=half;++sample) {
                const int sx=qBound(0,qRound(x+sample*dx),input.width()-1);
                const int sy=qBound(0,qRound(y+sample*dy),input.height()-1);
                const auto row=reinterpret_cast<const QRgb*>(input.constScanLine(sy));const QRgb pixel=row[sx];
                a+=qAlpha(pixel);r+=qRed(pixel);g+=qGreen(pixel);b+=qBlue(pixel);
            }
            output[x]=qRgba(r/samples,g/samples,b/samples,a/samples);
        }
    });
    return result;
}
QImage Document::unsharpMaskImage(const QImage &source,qreal radius,qreal amount,int threshold) {
    if(source.isNull()||amount<=0)return source;
    const QImage input=source.convertToFormat(QImage::Format_ARGB32);
    const QImage smooth=gaussianBlurImage(source,qMax(.1,radius)).convertToFormat(QImage::Format_ARGB32);
    QImage result(input.size(),QImage::Format_ARGB32);const qreal gain=qBound(0.,amount,300.)/100.;threshold=qBound(0,threshold,255);
    QVector<int> rows(input.height());std::iota(rows.begin(),rows.end(),0);
    QtConcurrent::blockingMap(rows,[&](int y){
        const auto src=reinterpret_cast<const QRgb*>(input.constScanLine(y));const auto blur=reinterpret_cast<const QRgb*>(smooth.constScanLine(y));
        auto dst=reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<input.width();++x) {
            auto sharpen=[&](int value,int soft){const int delta=value-soft;return qBound(0,value+(qAbs(delta)>=threshold?qRound(delta*gain):0),255);};
            dst[x]=qRgba(sharpen(qRed(src[x]),qRed(blur[x])),sharpen(qGreen(src[x]),qGreen(blur[x])),
                sharpen(qBlue(src[x]),qBlue(blur[x])),qAlpha(src[x]));
        }
    });
    result.setColorSpace(QColorSpace::SRgb);return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::highPassImage(const QImage &source,qreal radius) {
    if(source.isNull())return source;
    const QImage input=source.convertToFormat(QImage::Format_ARGB32),smooth=gaussianBlurImage(source,qMax(.1,radius)).convertToFormat(QImage::Format_ARGB32);
    QImage result(input.size(),QImage::Format_ARGB32);QVector<int> rows(input.height());std::iota(rows.begin(),rows.end(),0);
    QtConcurrent::blockingMap(rows,[&](int y){
        const auto src=reinterpret_cast<const QRgb*>(input.constScanLine(y));const auto blur=reinterpret_cast<const QRgb*>(smooth.constScanLine(y));
        auto dst=reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<input.width();++x)dst[x]=qRgba(qBound(0,qRed(src[x])-qRed(blur[x])+128,255),
            qBound(0,qGreen(src[x])-qGreen(blur[x])+128,255),qBound(0,qBlue(src[x])-qBlue(blur[x])+128,255),qAlpha(src[x]));
    });
    result.setColorSpace(QColorSpace::SRgb);return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::mosaicImage(const QImage &source,int cellSize) {
    if(source.isNull()||cellSize<=1)return source;
    cellSize=qBound(2,cellSize,200);const QSize reduced(qMax(1,(source.width()+cellSize-1)/cellSize),qMax(1,(source.height()+cellSize-1)/cellSize));
    return source.scaled(reduced,Qt::IgnoreAspectRatio,Qt::FastTransformation).scaled(source.size(),Qt::IgnoreAspectRatio,Qt::FastTransformation);
}
QImage Document::noiseImage(const QImage &source,qreal amount) {
    if(source.isNull()||amount<=0)return source;
    QImage result=source.convertToFormat(QImage::Format_ARGB32);const int amplitude=qRound(qBound(0.,amount,100.)*1.275);
    QVector<int> rows(result.height());std::iota(rows.begin(),rows.end(),0);
    QtConcurrent::blockingMap(rows,[&](int y){auto row=reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<result.width();++x){const QRgb pixel=row[x];quint32 hash=quint32(x)*0x9e3779b1u^quint32(y)*0x85ebca6bu;
            auto delta=[&](){hash^=hash<<13;hash^=hash>>17;hash^=hash<<5;return int(hash%quint32(amplitude*2+1))-amplitude;};
            row[x]=qRgba(qBound(0,qRed(pixel)+delta(),255),qBound(0,qGreen(pixel)+delta(),255),qBound(0,qBlue(pixel)+delta(),255),qAlpha(pixel));}}
    );result.setColorSpace(QColorSpace::SRgb);return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
QImage Document::frequencyImage(const Layer &layer) const {
    if(layer.source.isNull()) return {};
    const int radius=qBound(1,layer.parameters.value("radius",8).toInt(),100);
    QImage source=layer.source.convertToFormat(QImage::Format_ARGB32);
    qreal previewScale=1.;
    if(m_frequencyPreviewDraft && qMax(source.width(),source.height())>960) {
        previewScale=960./qreal(qMax(source.width(),source.height()));
        source=source.scaled(qMax(1,qRound(source.width()*previewScale)),qMax(1,qRound(source.height()*previewScale)),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }
    const QString cacheBase=QString::number(layer.source.cacheKey())+QLatin1Char('/')+QString::number(radius)
        +(previewScale<1?QStringLiteral("/draft/")+QString::number(source.width()):QStringLiteral("/full"));
    const QString lowKey=QStringLiteral("low/")+cacheBase;
    QImage low=m_frequencyCache.value(lowKey);
    if(low.isNull()) {
        if(m_frequencyCache.size()>12)m_frequencyCache.clear();
        low=blurImage(source,qMax(1,qRound(radius*previewScale))).convertToFormat(QImage::Format_ARGB32);
        if(previewScale<1)low=low.scaled(layer.source.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        m_frequencyCache.insert(lowKey,low);
    }
    if(layer.kind==QStringLiteral("frequencyLow")) return low;
    const QString highKey=QStringLiteral("high/")+cacheBase;
    if(m_frequencyCache.contains(highKey))return m_frequencyCache.value(highKey);
    const QImage fullSource=layer.source.convertToFormat(QImage::Format_ARGB32);
    QImage result(fullSource.size(),QImage::Format_ARGB32);
    QVector<int> rows(fullSource.height());std::iota(rows.begin(),rows.end(),0);
    QtConcurrent::blockingMap(rows,[&](int y){
        const auto s=reinterpret_cast<const QRgb*>(fullSource.constScanLine(y));
        const auto l=reinterpret_cast<const QRgb*>(low.constScanLine(y));
        auto out=reinterpret_cast<QRgb*>(result.scanLine(y));
        for(int x=0;x<fullSource.width();++x)out[x]=qRgba((qRed(s[x])-qRed(l[x])+256)/2,
            (qGreen(s[x])-qGreen(l[x])+256)/2,(qBlue(s[x])-qBlue(l[x])+256)/2,qAlpha(s[x]));
    });
    result=result.convertToFormat(QImage::Format_ARGB32_Premultiplied);m_frequencyCache.insert(highKey,result);return result;
}
QImage Document::adjustedImage(const QImage &source,const QString &type,const QVariantMap &p) {
    if(source.isNull()) return source;
    if(type=="gaussianBlur")return gaussianBlurImage(source,p.value("radius",8.).toDouble());
    if(type=="motionBlur")return motionBlurImage(source,p.value("distance",20.).toDouble(),p.value("angle",0.).toDouble());
    if(type=="boxBlur")return blurImage(source,qRound(p.value("radius",8.).toDouble()));
    if(type=="unsharpMask")return unsharpMaskImage(source,p.value("radius",2.).toDouble(),p.value("amount",100.).toDouble(),p.value("threshold",0).toInt());
    if(type=="highPass")return highPassImage(source,p.value("radius",4.).toDouble());
    if(type=="mosaic")return mosaicImage(source,p.value("cellSize",12).toInt());
    if(type=="noise")return noiseImage(source,p.value("amount",8.).toDouble());
    QImage result=source.convertToFormat(QImage::Format_ARGB32);
    auto value=[&](const char *key,double fallback){return p.value(QLatin1String(key),fallback).toDouble();};
    auto clamp=[](double v){return qBound(0,qRound(v),255);};
    const QColor shadowColor(p.value("shadowColor",QStringLiteral("#101828")).toString());
    const QColor highlightColor(p.value("highlightColor",QStringLiteral("#f5d9a8")).toString());
    for(int y=0;y<result.height();++y) for(int x=0;x<result.width();++x) {
        QColor c=result.pixelColor(x,y); double r=c.red(),g=c.green(),b=c.blue(); const int alpha=c.alpha();
        const double lum=.2126*r+.7152*g+.0722*b;
        if(type=="brightnessContrast") {
            const double brightness=value("brightness",0)*2.55, contrast=value("contrast",0);
            const double f=std::pow((100+contrast)/100,2); r=(r-128)*f+128+brightness;g=(g-128)*f+128+brightness;b=(b-128)*f+128+brightness;
        } else if(type=="levels") {
            const double inB=value("inputBlack",0),inW=qMax(inB+1.,value("inputWhite",255)),gamma=qMax(.1,value("gamma",1));
            const double outB=value("outputBlack",0),outW=value("outputWhite",255);
            auto level=[&](double v){return outB+std::pow(qBound(0.,(v-inB)/(inW-inB),1.),1/gamma)*(outW-outB);};r=level(r);g=level(g);b=level(b);
        } else if(type=="curves") {
            const double t=lum/255., shadows=value("shadows",0),mid=value("midtones",0),high=value("highlights",0);
            const double delta=shadows*std::pow(1-t,2)+mid*4*t*(1-t)+high*t*t;r+=delta;g+=delta;b+=delta;
        } else if(type=="exposure") {
            const double exposure=std::pow(2.,value("exposure",0)),offset=value("offset",0)*255,gamma=qMax(.1,value("gamma",1));
            auto expose=[&](double v){return std::pow(qMax(0.,v*exposure+offset)/255.,1/gamma)*255.;};r=expose(r);g=expose(g);b=expose(b);
        } else if(type=="vibrance") {
            const double mx=qMax(r,qMax(g,b)),mn=qMin(r,qMin(g,b)),sat=(mx-mn)/255.;
            const double factor=1+value("saturation",0)/100.+value("vibrance",0)/100.*(1-sat);
            r=lum+(r-lum)*factor;g=lum+(g-lum)*factor;b=lum+(b-lum)*factor;
        } else if(type=="hueSaturation") {
            float h,s,l,a;c.getHslF(&h,&s,&l,&a);h=std::fmod(h+value("hue",0)/360.+1.,1.);
            s=qBound(0.,s*(1+value("saturation",0)/100.),1.);l=qBound(0.,l+value("lightness",0)/100.,1.);c.setHslF(h,s,l,a);r=c.red();g=c.green();b=c.blue();
        } else if(type=="colorBalance") {
            r+=value("cyanRed",0)*2.55;g+=value("magentaGreen",0)*2.55;b+=value("yellowBlue",0)*2.55;
        } else if(type=="blackWhite") {
            const double total=qMax(1.,value("reds",40)+value("greens",40)+value("blues",20));
            const double gray=(r*value("reds",40)+g*value("greens",40)+b*value("blues",20))/total;r=g=b=gray;
        } else if(type=="photoFilter") {
            QColor filter;filter.setHslF(std::fmod(value("hue",35)/360.+1.,1.),.8,.55);
            const double d=qBound(0.,value("density",25)/100.,1.);r=r*(1-d)+filter.red()*d;g=g*(1-d)+filter.green()*d;b=b*(1-d)+filter.blue()*d;
        } else if(type=="channelMixer") {
            const double nr=r*value("redRed",100)/100.+g*value("redGreen",0)/100.+b*value("redBlue",0)/100.;
            const double ng=r*value("greenRed",0)/100.+g*value("greenGreen",100)/100.+b*value("greenBlue",0)/100.;
            const double nb=r*value("blueRed",0)/100.+g*value("blueGreen",0)/100.+b*value("blueBlue",100)/100.;r=nr;g=ng;b=nb;
        } else if(type=="colorLookup") {
            const int preset=p.value("preset",0).toInt();
            if(preset==0){r=r*1.05+8;g=g*.98;b=b*.88;}else if(preset==1){r=r*1.08;g=g*1.01;b=b*.92+8;}
            else if(preset==2){r=r*.9;g=g*1.02;b=b*1.12;}else {r=128+(r-128)*.82;g=128+(g-128)*.82;b=128+(b-128)*.82;}
        } else if(type=="invert") {r=255-r;g=255-g;b=255-b;
        } else if(type=="posterize") {
            const int levels=qBound(2,p.value("levels",4).toInt(),32);const double step=255./(levels-1);r=qRound(r/step)*step;g=qRound(g/step)*step;b=qRound(b/step)*step;
        } else if(type=="threshold") {r=g=b=lum>=value("level",128)?255:0;
        } else if(type=="gradientMap") {
            const double t=lum/255.;r=shadowColor.red()*(1-t)+highlightColor.red()*t;g=shadowColor.green()*(1-t)+highlightColor.green()*t;b=shadowColor.blue()*(1-t)+highlightColor.blue()*t;
        } else if(type=="selectiveColor") {
            const double strength=(qMax(r,qMax(g,b))-qMin(r,qMin(g,b)))/255.;
            r+=value("cyanRed",0)*2.55*strength;g+=value("magentaGreen",0)*2.55*strength;b+=value("yellowBlue",0)*2.55*strength;
            const double black=value("black",0)*2.55;r-=black;g-=black;b-=black;
        }
        result.setPixelColor(x,y,QColor(clamp(r),clamp(g),clamp(b),alpha));
    }
    return result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
const QImage &Document::composite() const {
    if (!m_cacheDirty) return m_composite;
    const QRect documentRect(QPoint{},m_size);
    const bool fullEffect=std::any_of(m_layers.cbegin(),m_layers.cend(),requiresFullComposite);
    const QRect region=(m_fullCompositeDirty||fullEffect||m_composite.size()!=m_size?documentRect:m_dirtyRegion.intersected(documentRect));
    if(region.isEmpty()){m_cacheDirty=false;return m_composite;}
    QImage regionComposite=blank(region.size());
    for (int layerIndex=0;layerIndex<m_layers.size();++layerIndex) {
        const auto &layer=m_layers[layerIndex];if(!layer.visible)continue;
        // An untouched low/high pair reconstructs its source independently of
        // the radius. Collapsing it avoids two full-image filters per slider tick.
        if(layer.kind==QStringLiteral("frequencyLow") && layerIndex+1<m_layers.size()) {
            const auto &high=m_layers[layerIndex+1];const QString pairId=layer.parameters.value("pairId").toString();
            const bool plainMasks=(!layer.maskEnabled||layer.mask.isNull())&&(!high.maskEnabled||high.mask.isNull());
            if(high.visible&&high.kind==QStringLiteral("frequencyHigh")&&!pairId.isEmpty()
                &&high.parameters.value("pairId").toString()==pairId&&layer.source.cacheKey()==high.source.cacheKey()
                &&qFuzzyCompare(layer.opacity,1.)&&qFuzzyCompare(high.opacity,1.)&&layer.blend==0&&high.blend==16&&plainMasks) {
                regionComposite=blendImages(regionComposite,layer.source.copy(region),0,1.);++layerIndex;continue;
            }
        }
        QImage rendered;
        if (layer.kind == QStringLiteral("adjustment")) {
            static const QStringList scalableFilters{QStringLiteral("gaussianBlur"),QStringLiteral("motionBlur"),QStringLiteral("boxBlur"),
                QStringLiteral("unsharpMask"),QStringLiteral("highPass"),QStringLiteral("mosaic")};
            const bool draft=m_previewingLayer&&layerIndex==m_active&&scalableFilters.contains(layer.adjustmentType)
                &&qMax(regionComposite.width(),regionComposite.height())>960;
            if(draft) {
                const qreal scale=960./qreal(qMax(regionComposite.width(),regionComposite.height()));
                const QSize draftSize(qMax(1,qRound(regionComposite.width()*scale)),qMax(1,qRound(regionComposite.height()*scale)));
                QVariantMap parameters=layer.parameters;
                for(const auto &key:{QStringLiteral("radius"),QStringLiteral("distance"),QStringLiteral("cellSize")})
                    if(parameters.contains(key))parameters.insert(key,qMax(1.,parameters.value(key).toDouble()*scale));
                rendered=adjustedImage(regionComposite.scaled(draftSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation),layer.adjustmentType,parameters)
                    .scaled(regionComposite.size(),Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
            } else rendered = adjustedImage(regionComposite, layer.adjustmentType, layer.parameters);
        }
        else if (layer.kind.startsWith(QStringLiteral("frequency")))
            rendered = frequencyImage(layer).copy(region);
        else
            rendered = layer.image.copy(region);
        if (layer.maskEnabled && !layer.mask.isNull()) {
            const qreal density=layer.parameters.value("_maskDensity",100.).toDouble()/100.;
            const qreal feather=layer.parameters.value("_maskFeather",0.).toDouble();
            const QImage maskRegion=(feather>=.5?blurImage(layer.mask,qRound(feather)):layer.mask).copy(region);
            rendered=applyMask(rendered,maskRegion,density,0);
        }
        regionComposite=blendImages(regionComposite,rendered,layer.blend,layer.opacity);
    }
    if(region==documentRect)m_composite=regionComposite;
    else {QPainter painter(&m_composite);painter.setCompositionMode(QPainter::CompositionMode_Source);painter.drawImage(region.topLeft(),regionComposite);}
    m_cacheDirty=false;m_fullCompositeDirty=false;m_dirtyRegion={};return m_composite;
}
QImage Document::layerImage(int index) const {
    if (index < 0 || index >= m_layers.size()) return {};
    const auto &layer=m_layers[index];
    if(layer.kind.startsWith(QStringLiteral("frequency"))) return frequencyImage(layer);
    if(layer.kind==QStringLiteral("adjustment")) {
        QImage icon(96,72,QImage::Format_ARGB32_Premultiplied); icon.fill(QColor("#232b32"));
        QPainter p(&icon); QLinearGradient g(0,72,96,0); g.setColorAt(0,QColor("#273845")); g.setColorAt(1,QColor("#c5f2cf"));
        p.fillRect(icon.rect().adjusted(8,8,-8,-8),g); p.setPen(QColor("#ffffff")); p.drawText(icon.rect(),Qt::AlignCenter,QStringLiteral("A")); return icon;
    }
    return layer.image;
}
QImage Document::maskImage(int index) const {
    return index >= 0 && index < m_layers.size() ? m_layers[index].mask : QImage{};
}
void Document::setActiveLayer(int index) {
    if (index < 0 || index >= m_layers.size() || m_active == index) return;
    endStroke(); cancelLayerPreview(); m_active = index; m_editingMask = false; emit layersChanged();
}
void Document::setTool(const QString &value) {
    static const QStringList tools{"brush","eraser","fill","picker","move","rectangle","ellipse","line","gradient","select","crop","text","hand"};
    if (!tools.contains(value)) return;
    endStroke(); m_tool = value; emit settingsChanged();
}
void Document::setColor(const QColor &value) { if(value.isValid()) {m_color=value; emit settingsChanged();} }
void Document::setSecondaryColor(const QColor &value) { if(value.isValid()) {m_secondary=value; emit settingsChanged();} }
void Document::resetColors() {
    if(m_color==Qt::black && m_secondary==Qt::white) return;
    m_color=Qt::black;m_secondary=Qt::white;emit settingsChanged();
}
void Document::swapColors() {
    qSwap(m_color,m_secondary);emit settingsChanged();
}
void Document::setBrushSize(qreal value) { if(std::isfinite(value)) {m_brushSize=qBound(1.,value,500.); emit settingsChanged();} }
void Document::setBrushOpacity(qreal value) { if(std::isfinite(value)) {m_brushOpacity=qBound(0.,value,1.); emit settingsChanged();} }
void Document::setBrushHardness(qreal value) {if(std::isfinite(value)){m_brushHardness=qBound(0.,value,1.);emit settingsChanged();}}
void Document::setBrushFlow(qreal value) {if(std::isfinite(value)){m_brushFlow=qBound(.01,value,1.);emit settingsChanged();}}
void Document::setBrushSpacing(qreal value) {if(std::isfinite(value)){m_brushSpacing=qBound(.01,value,10.);emit settingsChanged();}}
void Document::setBrushAngle(qreal value) {if(std::isfinite(value)){m_brushAngle=std::fmod(value,360.);emit settingsChanged();}}
void Document::setBrushRoundness(qreal value) {if(std::isfinite(value)){m_brushRoundness=qBound(.01,value,1.);emit settingsChanged();}}
void Document::setBrushSmoothing(qreal value) {if(std::isfinite(value)){m_brushSmoothing=qBound(0.,value,1.);emit settingsChanged();}}
void Document::setBrushBlend(int value) {if(value>=0&&value<=26&&m_brushBlend!=value){m_brushBlend=value;emit settingsChanged();}}
void Document::setText(const QString &value) { m_text=value.left(4096); emit settingsChanged(); }
void Document::setTextSize(int value) { m_textSize=qBound(6,value,512); emit settingsChanged(); }
void Document::setFilledShapes(bool value) { m_filledShapes=value; emit settingsChanged(); }

bool Document::newDocument(int w, int h, bool transparent) {
    if (!validSize(w,h)) return false;
    m_size={w,h}; QImage image=blank(m_size);
    if (!transparent) image.fill(Qt::white);
    m_layers={{tr("Arrière-plan"),image}}; m_active=0;
    m_name=tr("Sans titre"); m_projectUrl=QUrl{}; resetHistory(true); return true;
}
QImage Document::readImage(const QString &path) {
    QImageReader reader(path); reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (!size.isValid() || !validSize(size.width(),size.height())) {
        if (!size.isValid()) fail(tr("Format non reconnu ou image illisible."));
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
    if (path.isEmpty()) return fail(tr("Choisissez un fichier local."));
    if (QFileInfo(path).suffix().compare("pcr",Qt::CaseInsensitive)==0) return loadProject(path);
    auto image=readImage(path); if(image.isNull()) return false;
    m_size=image.size(); m_layers={{QFileInfo(path).completeBaseName(),image}}; m_active=0;
    m_name=QFileInfo(path).completeBaseName(); m_projectUrl=QUrl{}; resetHistory(false); return true;
}
bool Document::importLayer(const QUrl &url) {
    if (!validSize(width(),height(),int(m_layers.size())+1)) return false;
    auto image=readImage(localPath(url)); if(image.isNull()) return false;
    edit(tr("Importer un calque"),[&] {
        QImage target=blank(m_size); QPainter painter(&target);
        painter.drawImage((width()-image.width())/2,(height()-image.height())/2,image);
        painter.end(); m_layers.insert(++m_active,{QFileInfo(url.toLocalFile()).completeBaseName(),target});
    }); return true;
}
bool Document::saveProject(const QUrl &url) {
    endStroke(); QString path=localPath(url);
    if(path.isEmpty()) return fail(tr("Chemin de sauvegarde invalide."));
    if(!path.endsWith(".pcr",Qt::CaseInsensitive)) path+=".pcr";
    QJsonArray layers;
    for(const auto &layer:m_layers) {
        auto encode=[&](const QImage &image)->QString {if(image.isNull())return {};QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);
            return image.save(&buffer,"PNG")?QString::fromLatin1(bytes.toBase64()):QString{};};
        const QString png=encode(layer.image),mask=encode(layer.mask),source=encode(layer.source);
        if((!layer.image.isNull()&&png.isEmpty())||(!layer.mask.isNull()&&mask.isEmpty())||(!layer.source.isNull()&&source.isEmpty()))
            return fail(tr("Impossible d’encoder les données du calque."));
        layers.append(QJsonObject{{"name",layer.name},{"visible",layer.visible},{"locked",layer.locked},
            {"opacity",layer.opacity},{"blend",layer.blend},{"kind",layer.kind},{"adjustmentType",layer.adjustmentType},
            {"parameters",QJsonObject::fromVariantMap(layer.parameters)},{"png",png},{"mask",mask},
            {"maskEnabled",layer.maskEnabled},{"source",source}});
    }
    QJsonObject root{{"format","PhotoCrap"},{"version",3},{"width",width()},{"height",height()},
        {"activeLayer",m_active},{"layers",layers}};
    const QByteArray data=QJsonDocument(root).toJson(QJsonDocument::Compact);
    if(data.size()>MaxProjectBytes) return fail(tr("Le projet dépasse 256 Mio. Réduisez ses dimensions ou ses calques."));
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) return fail(file.errorString());
    m_savedId=m_stateId; m_projectUrl=QUrl::fromLocalFile(path); m_name=QFileInfo(path).completeBaseName();
    emit documentChanged(); emit historyChanged(); emit statusMessage(tr("Projet enregistré")); return true;
}
bool Document::loadProject(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    if(file.size()>MaxProjectBytes) return fail(tr("Projet trop volumineux (256 Mio maximum)."));
    QJsonParseError error;
    const auto json=QJsonDocument::fromJson(file.readAll(),&error);
    const auto root=json.object();
    const int version=root["version"].toInt();
    if(error.error!=QJsonParseError::NoError || root["format"]!="PhotoCrap" || (version<1||version>3))
        return fail(tr("Projet PhotoCrap invalide ou version non prise en charge."));
    const int w=root["width"].toInt(), h=root["height"].toInt();
    const auto array=root["layers"].toArray();
    if(!validSize(w,h,int(array.size()))) return false;
    QVector<Layer> layers;
    for(const auto &value:array) {
        auto object=value.toObject();
        auto decode=[&](const QString &encoded,QImage::Format format)->QImage {if(encoded.isEmpty())return {};QByteArray bytes=QByteArray::fromBase64(encoded.toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
            QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"PNG");
            if(reader.size()!=QSize(w,h))return {};return reader.read().convertToFormat(format);};
        const QString kind=version==1?QStringLiteral("pixel"):object["kind"].toString("pixel");
        if(kind!="pixel"&&kind!="adjustment"&&kind!="frequency"&&kind!="frequencyLow"&&kind!="frequencyHigh")return fail(tr("Type de calque inconnu."));
        auto image=decode(object["png"].toString(),QImage::Format_ARGB32_Premultiplied);
        auto mask=decode(object["mask"].toString(),QImage::Format_Grayscale8);
        auto source=decode(object["source"].toString(),QImage::Format_ARGB32_Premultiplied);
        if((kind=="pixel"&&image.isNull())||(kind.startsWith("frequency")&&source.isNull())||(!object["mask"].toString().isEmpty()&&mask.isNull()))
            return fail(tr("Calque corrompu ou dimensions incohérentes."));
        if(!image.isNull())image.setColorSpace(QColorSpace::SRgb);if(!source.isNull())source.setColorSpace(QColorSpace::SRgb);
        const double opacity=object["opacity"].toDouble(-1);
        int blend=object["blend"].toInt(-1);
        static const int legacyV1[]={0,3,8,12,2,7,19};
        static const int legacyV2[]={0,3,8,12,2,7,19,13,14,9,4,20,10};
        if(version==1 && blend>=0 && blend<7)blend=legacyV1[blend];
        else if(version==2 && blend>=0 && blend<13)blend=legacyV2[blend];
        if(opacity<0 || opacity>1 || blend<0 || blend>26) return fail(tr("Propriétés de calque invalides."));
        Layer layer{object["name"].toString().left(256),image,object["visible"].toBool(true),object["locked"].toBool(),opacity,blend};
        layer.kind=kind;layer.adjustmentType=object["adjustmentType"].toString();layer.parameters=object["parameters"].toObject().toVariantMap();
        layer.mask=mask;layer.maskEnabled=object["maskEnabled"].toBool(true);layer.source=source;layers.append(layer);
        if(version==2 && kind=="frequency") {layers.last().kind="pixel";layers.last().image=source;layers.last().source={};layers.last().parameters={};}
    }
    // Only replace the current document after validating every layer.
    m_layers=layers; m_size={w,h}; m_active=qBound(0,root["activeLayer"].toInt(),int(layers.size())-1);
    m_projectUrl=QUrl::fromLocalFile(path); m_name=QFileInfo(path).completeBaseName(); resetHistory(true); return true;
}
bool Document::exportImage(const QUrl &url) {
    endStroke(); QString path=localPath(url); if(path.isEmpty()) return fail(tr("Chemin d’export invalide."));
    QString suffix=QFileInfo(path).suffix().toLower(); if(suffix.isEmpty()) {path+=".png"; suffix="png";}
    if(suffix!="png" && suffix!="jpg" && suffix!="jpeg" && suffix!="bmp" && suffix!="webp")
        return fail(tr("Export disponible en PNG, JPEG, BMP ou WebP (si le module Qt est installé)."));
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
    emit statusMessage(tr("Image exportée")); return true;
}

void Document::addLayer() {
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    edit(tr("Nouveau calque"),[&]{m_layers.insert(++m_active,{tr("Calque %1").arg(m_layers.size()+1),blank(m_size)});});
}
void Document::duplicateLayer() {
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    edit(tr("Dupliquer le calque"),[&]{auto layer=m_layers[m_active]; layer.name+=tr(" — copie"); m_layers.insert(++m_active,layer);});
}
void Document::removeLayer() {
    if(m_active<0 || m_active>=m_layers.size() || m_layers[m_active].locked || m_layers.size()==1) return;
    edit(tr("Supprimer le calque"),[&]{m_layers.removeAt(m_active); m_active=qMin(m_active,int(m_layers.size())-1);m_editingMask=false;});
}
void Document::moveLayer(int direction) {
    const int to=m_active+(direction>0?1:-1); if(to<0 || to>=m_layers.size()) return;
    edit(tr("Réordonner le calque"),[&]{m_layers.swapItemsAt(m_active,to); m_active=to;});
}
void Document::mergeDown() {
    if(m_active<1 || m_active>=m_layers.size()) return;
    const auto &lower=m_layers[m_active-1]; const auto &upper=m_layers[m_active];
    if(lower.locked || upper.locked || lower.kind!="pixel" || upper.kind!="pixel" ||
       (lower.maskEnabled&&!lower.mask.isNull()) || (upper.maskEnabled&&!upper.mask.isNull()) ||
       !lower.visible || !upper.visible || lower.blend!=0 || upper.blend!=0)
        {fail(tr("La fusion nécessite deux calques visibles en mode Normal, non verrouillés.")); return;}
    edit(tr("Fusionner vers le bas"),[&]{
        auto &bottom=m_layers[m_active-1]; const auto &top=m_layers[m_active];
        QImage image=blank(m_size); QPainter painter(&image);
        painter.setOpacity(bottom.opacity); painter.drawImage(0,0,bottom.image);
        painter.setOpacity(top.opacity); painter.drawImage(0,0,top.image); painter.end();
        bottom.image=image; bottom.opacity=1; m_layers.removeAt(m_active--);
    });
}
void Document::renameLayer(int index,const QString &name) {
    if(index<0 || index>=m_layers.size() || name.trimmed().isEmpty() || m_layers[index].name==name.trimmed()) return;
    edit(tr("Renommer le calque"),[&]{m_layers[index].name=name.trimmed().left(256);});
}
void Document::setLayerVisible(int i,bool visible) {
    if(i<0 || i>=m_layers.size() || m_layers[i].visible==visible) return;
    edit(tr("Visibilité du calque"),[&]{m_layers[i].visible=visible;});
}
void Document::setLayerLocked(int i,bool locked) {
    if(i<0 || i>=m_layers.size() || m_layers[i].locked==locked) return;
    edit(tr("Verrouillage du calque"),[&]{m_layers[i].locked=locked;});
}
void Document::setLayerOpacity(int i,qreal opacity) {
    if(i<0 || i>=m_layers.size() || !std::isfinite(opacity)) return;
    opacity=qBound(0.,opacity,1.); if(qFuzzyCompare(m_layers[i].opacity,opacity)) return;
    edit(tr("Opacité du calque"),[&]{m_layers[i].opacity=opacity;});
}
void Document::setLayerBlend(int i,int blend) {
    if(i<0 || i>=m_layers.size() || blend<0 || blend>26 || m_layers[i].blend==blend) return;
    edit(tr("Mode de fusion"),[&]{m_layers[i].blend=blend;});
}
void Document::addLayerMask(bool revealAll) {
    if(m_active<0 || m_active>=m_layers.size()) return;
    if(!m_layers[m_active].mask.isNull()) {m_editingMask=true;emit layersChanged();return;}
    edit(tr("Ajouter un masque de fusion"),[&]{
        QImage mask(m_size,QImage::Format_Grayscale8);mask.fill(hasSelection()?(revealAll?0:255):(revealAll?255:0));
        if(hasSelection()) {QPainter painter(&mask);painter.fillRect(m_selection,revealAll?Qt::white:Qt::black);}
        m_layers[m_active].mask=mask;m_layers[m_active].maskEnabled=true;
        m_layers[m_active].parameters.insert("_maskDensity",100.);m_layers[m_active].parameters.insert("_maskFeather",0.);m_editingMask=true;
    });
}
void Document::removeLayerMask() {
    if(m_active<0 || m_active>=m_layers.size() || m_layers[m_active].mask.isNull()) return;
    edit(tr("Supprimer le masque de fusion"),[&]{m_layers[m_active].mask={};m_editingMask=false;});
}
void Document::invertLayerMask() {
    if(m_active<0 || m_active>=m_layers.size() || m_layers[m_active].mask.isNull()) return;
    edit(tr("Inverser le masque de fusion"),[&]{m_layers[m_active].mask.invertPixels();});
}
void Document::setMaskEnabled(bool enabled) {
    if(m_active<0 || m_active>=m_layers.size() || m_layers[m_active].mask.isNull() || m_layers[m_active].maskEnabled==enabled) return;
    edit(tr("Activer le masque de fusion"),[&]{m_layers[m_active].maskEnabled=enabled;});
}
void Document::setEditingMask(bool enabled) {
    if(m_active<0 || m_active>=m_layers.size()) return;
    enabled=enabled&&!m_layers[m_active].mask.isNull();
    if(m_editingMask==enabled)return;endStroke();m_editingMask=enabled;emit layersChanged();
}
void Document::addAdjustmentLayer(const QString &type) {
    const QMap<QString,QString> names{{"brightnessContrast",tr("Luminosité / Contraste")},{"levels",tr("Niveaux")},
        {"curves",tr("Courbes")},{"exposure",tr("Exposition")},{"vibrance",tr("Vibrance")},{"hueSaturation",tr("Teinte / Saturation")},
        {"colorBalance",tr("Balance des couleurs")},{"blackWhite",tr("Noir et blanc")},{"photoFilter",tr("Filtre photo")},
        {"channelMixer",tr("Mélangeur de couches")},{"colorLookup",tr("Recherche de couleurs")},{"invert",tr("Négatif")},
        {"posterize",tr("Isohélie")},{"threshold",tr("Seuil")},{"gradientMap",tr("Courbe de transfert de dégradé")},
        {"selectiveColor",tr("Correction sélective")},{"gaussianBlur",tr("Flou gaussien")},{"motionBlur",tr("Flou directionnel")},
        {"boxBlur",tr("Flou de boîte")},{"unsharpMask",tr("Netteté optimisée")},{"highPass",tr("Passe-haut")},
        {"mosaic",tr("Mosaïque")},{"noise",tr("Ajout de bruit")}};
    if(!names.contains(type) || !validSize(width(),height(),int(m_layers.size())+1)) return;
    QVariantMap params;
    if(type=="brightnessContrast")params={{"brightness",0},{"contrast",0}};
    else if(type=="levels")params={{"inputBlack",0},{"inputWhite",255},{"gamma",1.},{"outputBlack",0},{"outputWhite",255}};
    else if(type=="curves")params={{"shadows",0},{"midtones",0},{"highlights",0}};
    else if(type=="exposure")params={{"exposure",0.},{"offset",0.},{"gamma",1.}};
    else if(type=="vibrance")params={{"vibrance",0},{"saturation",0}};
    else if(type=="hueSaturation")params={{"hue",0},{"saturation",0},{"lightness",0}};
    else if(type=="colorBalance"||type=="selectiveColor")params={{"cyanRed",0},{"magentaGreen",0},{"yellowBlue",0},{"black",0}};
    else if(type=="blackWhite")params={{"reds",40},{"greens",40},{"blues",20}};
    else if(type=="photoFilter")params={{"hue",35},{"density",25}};
    else if(type=="channelMixer")params={{"redRed",100},{"redGreen",0},{"redBlue",0},{"greenRed",0},{"greenGreen",100},{"greenBlue",0},{"blueRed",0},{"blueGreen",0},{"blueBlue",100}};
    else if(type=="colorLookup")params={{"preset",0}};
    else if(type=="posterize")params={{"levels",4}};
    else if(type=="threshold")params={{"level",128}};
    else if(type=="gradientMap")params={{"shadowColor","#101828"},{"highlightColor","#f5d9a8"}};
    else if(type=="gaussianBlur"||type=="boxBlur")params={{"radius",8.}};
    else if(type=="motionBlur")params={{"distance",20.},{"angle",0.}};
    else if(type=="unsharpMask")params={{"amount",100.},{"radius",2.},{"threshold",0}};
    else if(type=="highPass")params={{"radius",4.}};
    else if(type=="mosaic")params={{"cellSize",12}};
    else if(type=="noise")params={{"amount",8.}};
    edit(tr("Nouveau calque de réglage"),[&]{
        Layer layer;layer.name=names.value(type);layer.kind="adjustment";layer.adjustmentType=type;layer.parameters=params;
        layer.mask=QImage(m_size,QImage::Format_Grayscale8);layer.mask.fill(hasSelection()?0:255);
        layer.parameters.insert("_maskDensity",100.);layer.parameters.insert("_maskFeather",0.);
        if(hasSelection()){QPainter painter(&layer.mask);painter.fillRect(m_selection,Qt::white);}
        m_layers.insert(++m_active,layer);m_editingMask=false;
    });
}
void Document::addFrequencySeparation(qreal radius) {
    if(!std::isfinite(radius)||!validSize(width(),height(),int(m_layers.size())+2))return;
    QImage source=m_layers[m_active].kind=="pixel"?m_layers[m_active].image:composite();
    if(m_layers[m_active].kind=="pixel"&&m_layers[m_active].maskEnabled&&!m_layers[m_active].mask.isNull())source=applyMask(source,m_layers[m_active].mask,
        m_layers[m_active].parameters.value("_maskDensity",100.).toDouble()/100.,m_layers[m_active].parameters.value("_maskFeather",0.).toDouble());
    const QString pairId=QString::number(m_nextId);
    edit(tr("Séparation de fréquences"),[&]{
        const QVariantMap parameters{{"radius",qBound(1.,radius,100.)},{"pairId",pairId}};
        Layer low;low.name=tr("Basses fréquences");low.kind="frequencyLow";low.source=source;low.parameters=parameters;
        Layer high;high.name=tr("Hautes fréquences");high.kind="frequencyHigh";high.source=source;high.parameters=parameters;high.blend=16;
        m_layers.insert(++m_active,low);m_layers.insert(++m_active,high);m_editingMask=false;
    });
}
void Document::beginLayerPreview() {
    if(m_previewingLayer||m_active<0||m_active>=m_layers.size())return;
    endStroke();m_beforeLayerPreview=snapshot(tr("Aperçu"));m_previewingLayer=true;m_layerPreviewChanged=false;m_frequencyPreviewDraft=false;
}
void Document::previewLayerParameter(const QString &key,const QVariant &value) {
    if(m_active<0||m_active>=m_layers.size()||key.isEmpty())return;
    if(!m_previewingLayer)beginLayerPreview();
    if(m_layers[m_active].parameters.value(key)==value)return;
    const QString pairId=m_layers[m_active].parameters.value("pairId").toString();
    if(key=="radius"&&!pairId.isEmpty()) {
        m_frequencyPreviewDraft=true;m_frequencyCache.clear();
        for(auto &layer:m_layers)if(layer.parameters.value("pairId").toString()==pairId)layer.parameters.insert(key,value);
    } else m_layers[m_active].parameters.insert(key,value);
    // The property panel already owns the live slider value. Avoid rebuilding
    // the layer model and its thumbnails for every pointer event.
    m_layerPreviewChanged=true;m_fullCompositeDirty=true;changed(false);
}
void Document::finishLayerPreview(const QString &label) {
    if(!m_previewingLayer)return;m_previewingLayer=false;m_frequencyPreviewDraft=false;
    if(m_layerPreviewChanged)commit(m_beforeLayerPreview,label);m_beforeLayerPreview={};m_layerPreviewChanged=false;
}
void Document::cancelLayerPreview() {
    if(!m_previewingLayer)return;const bool changedPreview=m_layerPreviewChanged;m_previewingLayer=false;m_layerPreviewChanged=false;m_frequencyPreviewDraft=false;
    if(changedPreview)restore(m_beforeLayerPreview);m_beforeLayerPreview={};
}

void Document::beginStroke(qreal x,qreal y) {
    if(!std::isfinite(x) || !std::isfinite(y)) return;
    endStroke(); m_anchor=m_last={x,y};
    if(m_tool=="hand") return;
    if(m_tool=="picker") { if(composite().rect().contains(QPoint(int(x),int(y)))) setColor(composite().pixelColor(int(x),int(y))); return; }
    if(m_tool=="select" || m_tool=="crop") {m_drawing=true; m_selection={}; emit selectionChanged(); return;}
    if(!editable()) return;
    if(m_editingMask && m_tool!="brush" && m_tool!="eraser" && m_tool!="fill") {
        fail(tr("Utilisez le pinceau, la gomme ou le remplissage pour modifier un masque."));return;
    }
    m_beforeStroke=snapshot(toolLabel(m_tool)); m_drawing=true; m_strokeChanged=false;
    if(m_tool=="brush" || m_tool=="eraser") {
        QImage *target=paintTarget();if(!target){m_drawing=false;return;}
        m_strokeBase=*target;m_strokeCoverage=blank(m_size);m_strokeBounds={};m_smoothedLast=m_anchor;
        m_distanceToNextDab=qMax(1.,m_brushSize*m_brushSpacing);
        paintSegment(m_anchor,m_anchor);
    }
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
    Q_UNUSED(from)
    if(m_strokeCoverage.isNull())return;
    m_strokeBounds={};
    if(m_anchor==to && !m_strokeChanged) stampBrush(to);
    else {
        const qreal response=1.-m_brushSmoothing*.85;
        const QPointF target=m_smoothedLast+(to-m_smoothedLast)*response;
        QPointF cursor=m_smoothedLast; qreal distance=QLineF(cursor,target).length();
        const qreal step=qMax(1.,m_brushSize*m_brushSpacing);
        while(distance>=m_distanceToNextDab && distance>0) {
            const QPointF direction=(target-cursor)/distance;
            cursor+=direction*m_distanceToNextDab;stampBrush(cursor);
            distance=QLineF(cursor,target).length();m_distanceToNextDab=step;
        }
        m_distanceToNextDab-=distance;m_smoothedLast=target;
    }
    renderBrushStroke();m_strokeChanged=true;changed(false);
}
void Document::stampBrush(const QPointF &point) {
    QPainter painter(&m_strokeCoverage);painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(point);painter.rotate(m_brushAngle);painter.scale(1.,m_brushRoundness);
    const qreal radius=m_brushSize/2.;const QColor paint(255,255,255,qRound(m_brushFlow*255));
    if(m_brushHardness>=.999) painter.setBrush(paint);
    else {
        QRadialGradient gradient(QPointF{},radius);
        gradient.setColorAt(qBound(0.,m_brushHardness,.999),paint);
        gradient.setColorAt(1.,QColor(255,255,255,0));painter.setBrush(gradient);
    }
    painter.setPen(Qt::NoPen);painter.drawEllipse(QPointF{},radius,radius);painter.end();
    const QRectF dabBounds(point.x()-radius-2,point.y()-radius-2,radius*2+4,radius*2+4);
    m_strokeBounds=m_strokeBounds.isEmpty()?dabBounds:m_strokeBounds.united(dabBounds);
}
void Document::renderBrushStroke() {
    QImage *target=paintTarget();if(!target||m_strokeBase.isNull())return;
    QRect region=m_strokeBounds.toAlignedRect().intersected(QRect(QPoint{},m_size));
    if(hasSelection())region=region.intersected(m_selection);if(region.isEmpty())return;
    if(!m_fullCompositeDirty)m_dirtyRegion=m_dirtyRegion.isEmpty()?region:m_dirtyRegion.united(region);
    QImage overlay=blank(region.size());
    const QColor ink=m_editingMask?(m_tool=="eraser"?QColor(Qt::white):QColor(Qt::black)):m_color;
    overlay.fill(ink);QPainter maskPainter(&overlay);maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    maskPainter.drawImage(0,0,m_strokeCoverage.copy(region));maskPainter.end();
    QImage canvas=m_strokeBase.copy(region).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if(m_tool=="brush"&&!m_editingMask) {
        canvas=blendImages(canvas,overlay,m_brushBlend,m_brushOpacity);
    } else {
        QPainter painter(&canvas);painter.setOpacity(m_brushOpacity);
        if(m_tool=="eraser"&&!m_editingMask)painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        painter.drawImage(0,0,overlay);painter.end();
    }
    if(m_editingMask) {
        const QImage gray=canvas.convertToFormat(QImage::Format_Grayscale8);
        for(int y=0;y<region.height();++y)memcpy(target->scanLine(region.y()+y)+region.x(),gray.constScanLine(y),size_t(region.width()));
    } else {
        QPainter output(target);output.setCompositionMode(QPainter::CompositionMode_Source);output.drawImage(region.topLeft(),canvas);
    }
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
    m_strokeChanged=true;m_fullCompositeDirty=true;changed(false);
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
    if((m_tool=="brush"||m_tool=="eraser")&&QLineF(m_smoothedLast,m_last).length()>.01) {
        const qreal smoothing=m_brushSmoothing;m_brushSmoothing=0;paintSegment(m_smoothedLast,m_last);m_brushSmoothing=smoothing;
    }
    m_drawing=false;
    if(m_tool=="select" || m_tool=="crop") return;
    if(m_strokeChanged) {
        if(m_tool=="move" && hasSelection()) {
            m_selection=m_selection.translated((m_last-m_anchor).toPoint()).intersected(QRect(QPoint{},m_size));
            emit selectionChanged();
        }
        commit(m_beforeStroke,toolLabel(m_tool),m_tool!="brush"&&m_tool!="eraser");
        if(m_tool=="brush"||m_tool=="eraser") {++m_thumbnailRevision;emit thumbnailsChanged();}
    }
    m_beforeStroke={};m_strokeBase={};m_strokeCoverage={};m_strokeBounds={};
}
void Document::cancelStroke() {
    if(!m_drawing) return;
    m_drawing=false;
    if(m_tool=="select" || m_tool=="crop") deselect(); else restore(m_beforeStroke);
    m_beforeStroke={};m_strokeBase={};m_strokeCoverage={};m_strokeBounds={};
}
void Document::floodFill(QPoint point) {
    QImage *paintImage=paintTarget();if(!paintImage)return;auto &image=*paintImage;
    const QRect bounds=hasSelection()?m_selection:image.rect();
    if(!bounds.contains(point)) return;
    const QRgb targetColor=image.pixel(point);
    QImage solid(1,1,QImage::Format_ARGB32_Premultiplied); solid.fill(m_editingMask?QColor(Qt::black):m_color);
    const QRgb replacement=solid.pixel(0,0);
    if(targetColor==replacement && qFuzzyCompare(m_brushOpacity,1.)) return;
    // Scanline flood fill: visited pixels are kept separate so semi-transparent
    // replacement colors cannot cause pixels to be queued repeatedly.
    QImage mask(image.size(),QImage::Format_Grayscale8); mask.fill(0);
    QVector<QPoint> stack{point};
    auto matches=[&](int x,int y){return !mask.constScanLine(y)[x] && image.pixel(x,y)==targetColor;};
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
QImage *Document::paintTarget() {
    if(m_active<0||m_active>=m_layers.size())return nullptr;
    if(m_editingMask&&!m_layers[m_active].mask.isNull())return &m_layers[m_active].mask;
    if(m_layers[m_active].kind=="pixel")return &m_layers[m_active].image;
    return nullptr;
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
    edit(tr("Effacer"),[&]{QImage *target=paintTarget();if(!target)return;QPainter p(target);
        if(m_editingMask)p.fillRect(hasSelection()?m_selection:QRect(QPoint{},m_size),Qt::black);
        else {p.setCompositionMode(QPainter::CompositionMode_Clear);p.fillRect(hasSelection()?m_selection:QRect(QPoint{},m_size),Qt::transparent);}});
}
void Document::copy() {
    endStroke(); QGuiApplication::clipboard()->setImage(hasSelection()?composite().copy(m_selection):composite());
    emit statusMessage(tr("Copie de l’image visible dans le presse-papiers"));
}
void Document::paste() {
    auto source=QGuiApplication::clipboard()->image(); if(source.isNull()) return;
    if(!validSize(width(),height(),int(m_layers.size())+1)) return;
    if(source.colorSpace().isValid()) source.convertToColorSpace(QColorSpace::SRgb);
    edit(tr("Coller"),[&]{auto image=blank(m_size); QPainter p(&image);
        p.drawImage((width()-source.width())/2,(height()-source.height())/2,source); p.end();
        m_layers.insert(++m_active,{tr("Image collée"),image});});
}
void Document::cropToSelection() {
    if(!hasSelection()) return;
    edit(tr("Recadrer"),[&]{for(auto &layer:m_layers) {
        if(!layer.image.isNull())layer.image=layer.image.copy(m_selection);
        if(!layer.mask.isNull())layer.mask=layer.mask.copy(m_selection);
        if(!layer.source.isNull())layer.source=layer.source.copy(m_selection);
    }
        m_size=m_selection.size(); m_selection={};});
    emit documentChanged(); emit selectionChanged();
}
void Document::resizeImage(int w,int h) {
    if(!validSize(w,h,int(m_layers.size())) || m_size==QSize(w,h)) return;
    edit(tr("Redimensionner l’image"),[&]{for(auto &layer:m_layers) {
        if(!layer.image.isNull())layer.image=layer.image.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        if(!layer.mask.isNull())layer.mask=layer.mask.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        if(!layer.source.isNull())layer.source=layer.source.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }
        m_size={w,h}; m_selection={};}); emit documentChanged(); emit selectionChanged();
}
void Document::transformImage(const QString &operation) {
    if(operation!="rotateLeft" && operation!="rotateRight" && operation!="flipH" && operation!="flipV") return;
    edit(tr("Transformer l’image"),[&]{for(auto &layer:m_layers) {
        auto transform=[&](QImage &image){if(image.isNull())return;
        if(operation=="flipH" || operation=="flipV") {
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
            image=image.flipped(operation=="flipH"?Qt::Horizontal:Qt::Vertical);
#else
            image=image.mirrored(operation=="flipH",operation=="flipV");
#endif
        } else image=image.transformed(QTransform().rotate(operation=="rotateRight"?90:-90));};
        transform(layer.image);transform(layer.mask);transform(layer.source);
    }
    if(operation.startsWith("rotate")) m_size.transpose(); m_selection={};});
    emit documentChanged(); emit selectionChanged();
}
void Document::applyFilter(const QString &filter,qreal amount) {
    static const QStringList supported{"grayscale","invert","sepia","brightness","contrast","saturation","blur","sharpen"};
    if(!supported.contains(filter) || !std::isfinite(amount) || !editable()) return;
    if(m_editingMask || m_layers[m_active].kind!="pixel") {fail(tr("Ce filtre destructif nécessite un calque de pixels."));return;}
    amount=qBound(-100.,amount,100.);
    edit(tr("Filtre : %1").arg(filter),[&]{
        const QImage original=m_layers[m_active].image;
        QImage result=original.convertToFormat(QImage::Format_ARGB32);
        const QRect area=hasSelection()?m_selection:result.rect();
        if(filter=="blur" || filter=="sharpen") {
            const int radius=filter=="sharpen"?1:qBound(1,int(amount),30);
            const QImage blurred=blurImage(original,radius);
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
    auto &background=m_layers[0]; background.name=tr("Fond · bleu nuit");
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
    p.end(); m_layers.append({tr("Sphère · lumière"),orb});
    QImage typography=blank(m_size); QPainter t(&typography); t.setRenderHint(QPainter::TextAntialiasing);
    QFont font("sans-serif"); font.setPixelSize(19); font.setLetterSpacing(QFont::AbsoluteSpacing,5);
    t.setFont(font); t.setPen(QColor("#bee8ce")); t.drawText(QPointF(88,106),QStringLiteral("PHOTOGRAPHIE  /  DESIGN  /  EXPLORATION"));
    font.setPixelSize(146); font.setLetterSpacing(QFont::AbsoluteSpacing,-6); font.setWeight(QFont::Bold);
    t.setFont(font); t.setPen(QColor("#f2f2e7")); t.drawText(QPointF(78,390),QStringLiteral("Make room"));
    t.drawText(QPointF(78,552),QStringLiteral("for wonder."));
    font.setPixelSize(25); font.setWeight(QFont::Normal); font.setLetterSpacing(QFont::AbsoluteSpacing,0);
    t.setFont(font); t.setPen(QColor("#afc4c8")); t.drawText(QPointF(88,646),tr("Une idée. Quelques calques. Des possibilités infinies."));
    t.setPen(QPen(QColor("#91b8a7"),2)); t.drawLine(QPointF(88,856),QPointF(1352,856));
    font.setPixelSize(19); font.setLetterSpacing(QFont::AbsoluteSpacing,2); t.setFont(font);
    t.drawText(QPointF(88,912),QStringLiteral("STUDIO EXPÉRIMENTAL")); t.drawText(QPointF(1162,912),QStringLiteral("VOL. 001"));
    t.end(); m_layers.append({tr("Composition · typographie"),typography});
    m_layers.append({tr("Vos retouches"),blank(m_size)}); m_active=3;
    m_name=tr("Exploration créative"); resetHistory(false);
}
