#pragma once
#include <QColor>
#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPainter>
#include <QPolygonF>
#include <QRect>
#include <QRegion>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>

struct Layer {
    Layer() = default;
    Layer(const QString &_layerName, const QImage &_layerImage, bool _layerVisible = true,
          bool _layerLocked = false, qreal _layerOpacity = 1, int _layerBlend = 0)
        : name(_layerName), image(_layerImage), visible(_layerVisible), locked(_layerLocked),
          opacity(_layerOpacity), blend(_layerBlend)
    {
    }
    QString     name;
    QImage      image;
    bool        visible = true;
    bool        locked  = false;
    qreal       opacity = 1;
    int         blend   = 0;
    QString     kind    = QStringLiteral("pixel");
    QString     adjustmentType;
    QVariantMap parameters;
    QImage      mask;
    bool        maskEnabled  = true;
    bool        clippingMask = false;
    QString     groupId;
    bool        groupCollapsed = false;
    QImage      source;
};

class Document : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int width READ width NOTIFY canvasSizeChanged)
    Q_PROPERTY(int height READ height NOTIFY canvasSizeChanged)
    Q_PROPERTY(QString name READ name NOTIFY documentChanged)
    Q_PROPERTY(QUrl projectUrl READ projectUrl NOTIFY documentChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY historyChanged)
    Q_PROPERTY(QVariantList layers READ layers NOTIFY layersChanged)
    Q_PROPERTY(int activeLayer READ activeLayer WRITE setActiveLayer NOTIFY layersChanged)
    Q_PROPERTY(int selectedLayerCount READ selectedLayerCount NOTIFY layersChanged)
    Q_PROPERTY(bool canGroupSelected READ canGroupSelected NOTIFY layersChanged)
    Q_PROPERTY(bool canUngroupActive READ canUngroupActive NOTIFY layersChanged)
    Q_PROPERTY(int revision READ revision NOTIFY imageChanged)
    Q_PROPERTY(int thumbnailRevision READ thumbnailRevision NOTIFY thumbnailsChanged)
    Q_PROPERTY(
        int displayChannel READ displayChannel WRITE setDisplayChannel NOTIFY displayChannelChanged)
    Q_PROPERTY(int maskOnlyLayer READ maskOnlyLayer NOTIFY displayChannelChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY settingsChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY settingsChanged)
    Q_PROPERTY(
        QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushHardness READ brushHardness WRITE setBrushHardness NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushFlow READ brushFlow WRITE setBrushFlow NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushSpacing READ brushSpacing WRITE setBrushSpacing NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushAngle READ brushAngle WRITE setBrushAngle NOTIFY settingsChanged)
    Q_PROPERTY(
        qreal brushRoundness READ brushRoundness WRITE setBrushRoundness NOTIFY settingsChanged)
    Q_PROPERTY(
        qreal brushSmoothing READ brushSmoothing WRITE setBrushSmoothing NOTIFY settingsChanged)
    Q_PROPERTY(int brushBlend READ brushBlend WRITE setBrushBlend NOTIFY settingsChanged)
    Q_PROPERTY(
        bool pencilAutoErase READ pencilAutoErase WRITE setPencilAutoErase NOTIFY settingsChanged)
    Q_PROPERTY(qreal toolStrength READ toolStrength WRITE setToolStrength NOTIFY settingsChanged)
    Q_PROPERTY(
        bool spongeSaturate READ spongeSaturate WRITE setSpongeSaturate NOTIFY settingsChanged)
    Q_PROPERTY(int retouchRange READ retouchRange WRITE setRetouchRange NOTIFY settingsChanged)
    Q_PROPERTY(bool protectTones READ protectTones WRITE setProtectTones NOTIFY settingsChanged)
    Q_PROPERTY(bool smudgeSampleAllLayers READ smudgeSampleAllLayers WRITE setSmudgeSampleAllLayers
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool smudgeFingerPainting READ smudgeFingerPainting WRITE setSmudgeFingerPainting
                   NOTIFY settingsChanged)
    Q_PROPERTY(
        int replacementMode READ replacementMode WRITE setReplacementMode NOTIFY settingsChanged)
    Q_PROPERTY(int replacementSampling READ replacementSampling WRITE setReplacementSampling NOTIFY
                   settingsChanged)
    Q_PROPERTY(int replacementLimits READ replacementLimits WRITE setReplacementLimits NOTIFY
                   settingsChanged)
    Q_PROPERTY(int replacementTolerance READ replacementTolerance WRITE setReplacementTolerance
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool replacementAntialias READ replacementAntialias WRITE setReplacementAntialias
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool stampAligned READ stampAligned WRITE setStampAligned NOTIFY settingsChanged)
    Q_PROPERTY(
        int cloneSampleMode READ cloneSampleMode WRITE setCloneSampleMode NOTIFY settingsChanged)
    Q_PROPERTY(bool cloneSourceValid READ cloneSourceValid NOTIFY settingsChanged)
    Q_PROPERTY(QPointF cloneSource READ cloneSource NOTIFY settingsChanged)
    Q_PROPERTY(QPointF cloneOffset READ cloneOffset NOTIFY settingsChanged)
    Q_PROPERTY(int stampPattern READ stampPattern WRITE setStampPattern NOTIFY settingsChanged)
    Q_PROPERTY(bool hasCustomStampPattern READ hasCustomStampPattern NOTIFY settingsChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY settingsChanged)
    Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY settingsChanged)
    Q_PROPERTY(bool filledShapes READ filledShapes WRITE setFilledShapes NOTIFY settingsChanged)
    Q_PROPERTY(
        QColor shapeFillColor READ shapeFillColor WRITE setShapeFillColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor shapeStrokeColor READ shapeStrokeColor WRITE setShapeStrokeColor NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool shapeStrokeEnabled READ shapeStrokeEnabled WRITE setShapeStrokeEnabled NOTIFY
                   settingsChanged)
    Q_PROPERTY(qreal shapeStrokeWidth READ shapeStrokeWidth WRITE setShapeStrokeWidth NOTIFY
                   settingsChanged)
    Q_PROPERTY(
        int shapeStrokeStyle READ shapeStrokeStyle WRITE setShapeStrokeStyle NOTIFY settingsChanged)
    Q_PROPERTY(qreal shapeCornerRadius READ shapeCornerRadius WRITE setShapeCornerRadius NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool shapeAlignPixels READ shapeAlignPixels WRITE setShapeAlignPixels NOTIFY
                   settingsChanged)
    Q_PROPERTY(int shapeSides READ shapeSides WRITE setShapeSides NOTIFY settingsChanged)
    Q_PROPERTY(
        qreal shapeStarRatio READ shapeStarRatio WRITE setShapeStarRatio NOTIFY settingsChanged)
    Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(int selectionShape READ selectionShape NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectionPath READ selectionPath NOTIFY selectionChanged)
    Q_PROPERTY(bool polygonLassoActive READ polygonLassoActive NOTIFY selectionChanged)
    Q_PROPERTY(int selectionMode READ selectionMode WRITE setSelectionMode NOTIFY settingsChanged)
    Q_PROPERTY(int selectionTolerance READ selectionTolerance WRITE setSelectionTolerance NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool selectionContiguous READ selectionContiguous WRITE setSelectionContiguous NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool selectionSampleAllLayers READ selectionSampleAllLayers WRITE
                   setSelectionSampleAllLayers NOTIFY settingsChanged)
    Q_PROPERTY(bool selectionEnhanceEdge READ selectionEnhanceEdge WRITE setSelectionEnhanceEdge
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
    Q_PROPERTY(bool editingMask READ editingMask NOTIFY layersChanged)
public:
    explicit Document(QObject *_parent = nullptr);
    int width() const
    {
        return m_size.width();
    }
    int height() const
    {
        return m_size.height();
    }
    QString name() const
    {
        return m_name;
    }
    QUrl projectUrl() const
    {
        return m_projectUrl;
    }
    bool modified() const
    {
        return m_stateId != m_savedId;
    }
    QVariantList             layers() const;
    Q_INVOKABLE QVariantList histogram() const;
    Q_INVOKABLE QVariantMap  colorHistogram() const;
    int                      activeLayer() const
    {
        return m_active;
    }
    int  selectedLayerCount() const;
    bool canGroupSelected() const;
    bool canUngroupActive() const;
    int  revision() const
    {
        return m_revision;
    }
    int thumbnailRevision() const
    {
        return m_thumbnailRevision;
    }
    int displayChannel() const
    {
        return m_displayChannel;
    }
    QString tool() const
    {
        return m_tool;
    }
    QColor color() const
    {
        return m_color;
    }
    QColor secondaryColor() const
    {
        return m_secondary;
    }
    qreal brushSize() const
    {
        return m_brushSize;
    }
    qreal brushOpacity() const
    {
        return m_brushOpacity;
    }
    qreal brushHardness() const
    {
        return m_brushHardness;
    }
    qreal brushFlow() const
    {
        return m_brushFlow;
    }
    qreal brushSpacing() const
    {
        return m_brushSpacing;
    }
    qreal brushAngle() const
    {
        return m_brushAngle;
    }
    qreal brushRoundness() const
    {
        return m_brushRoundness;
    }
    qreal brushSmoothing() const
    {
        return m_brushSmoothing;
    }
    int brushBlend() const
    {
        return m_brushBlend;
    }
    bool pencilAutoErase() const
    {
        return m_pencilAutoErase;
    }
    qreal toolStrength() const
    {
        return m_toolStrength;
    }
    bool spongeSaturate() const
    {
        return m_spongeSaturate;
    }
    int retouchRange() const
    {
        return m_retouchRange;
    }
    bool protectTones() const
    {
        return m_protectTones;
    }
    QString text() const
    {
        return m_text;
    }
    int textSize() const
    {
        return m_textSize;
    }
    bool filledShapes() const
    {
        return m_filledShapes;
    }
    QColor shapeFillColor() const
    {
        return m_shapeFillColor.isValid() ? m_shapeFillColor : m_color;
    }
    QColor shapeStrokeColor() const
    {
        return m_shapeStrokeColor.isValid() ? m_shapeStrokeColor : m_color;
    }
    bool shapeStrokeEnabled() const
    {
        return m_shapeStrokeEnabled;
    }
    qreal shapeStrokeWidth() const
    {
        return m_shapeStrokeWidth;
    }
    int shapeStrokeStyle() const
    {
        return m_shapeStrokeStyle;
    }
    qreal shapeCornerRadius() const
    {
        return m_shapeCornerRadius;
    }
    bool shapeAlignPixels() const
    {
        return m_shapeAlignPixels;
    }
    int shapeSides() const
    {
        return m_shapeSides;
    }
    qreal shapeStarRatio() const
    {
        return m_shapeStarRatio;
    }
    QRect selection() const
    {
        return m_selection;
    }
    bool hasSelection() const
    {
        return !m_selectionRegion.isEmpty();
    }
    int selectionShape() const
    {
        return m_selectionShape;
    }
    QVariantList selectionPath() const;
    bool         polygonLassoActive() const
    {
        return m_polygonLassoActive;
    }
    int selectionMode() const
    {
        return m_selectionMode;
    }
    int selectionTolerance() const
    {
        return m_selectionTolerance;
    }
    bool selectionContiguous() const
    {
        return m_selectionContiguous;
    }
    bool selectionSampleAllLayers() const
    {
        return m_selectionSampleAllLayers;
    }
    bool selectionEnhanceEdge() const
    {
        return m_selectionEnhanceEdge;
    }
    const QRegion &selectionRegion() const
    {
        return m_selectionRegion;
    }
    bool canUndo() const
    {
        return !m_undo.isEmpty();
    }
    bool canRedo() const
    {
        return !m_redo.isEmpty();
    }
    QStringList history() const;
    int         historyIndex() const
    {
        return int(m_undo.size());
    }
    bool editingMask() const
    {
        return m_editingMask;
    }
    int maskOnlyLayer() const
    {
        return m_maskOnlyView ? m_active : -1;
    }
    bool smudgeSampleAllLayers() const
    {
        return m_smudgeSampleAllLayers;
    }
    bool smudgeFingerPainting() const
    {
        return m_smudgeFingerPainting;
    }
    void setSmudgeSampleAllLayers(bool _value);
    void setSmudgeFingerPainting(bool _value);
    int  replacementMode() const
    {
        return m_replacementMode;
    }
    int replacementSampling() const
    {
        return m_replacementSampling;
    }
    int replacementLimits() const
    {
        return m_replacementLimits;
    }
    int replacementTolerance() const
    {
        return m_replacementTolerance;
    }
    bool replacementAntialias() const
    {
        return m_replacementAntialias;
    }
    void setReplacementMode(int _value);
    void setReplacementSampling(int _value);
    void setReplacementLimits(int _value);
    void setReplacementTolerance(int _value);
    void setReplacementAntialias(bool _value);
    bool stampAligned() const
    {
        return m_stampAligned;
    }
    int cloneSampleMode() const
    {
        return m_cloneSampleMode;
    }
    bool cloneSourceValid() const
    {
        return m_cloneSourceValid;
    }
    QPointF cloneSource() const
    {
        return m_cloneSource;
    }
    QPointF cloneOffset() const
    {
        return m_cloneOffset;
    }
    int stampPattern() const
    {
        return m_stampPattern;
    }
    bool hasCustomStampPattern() const
    {
        return !m_customStampPattern.isNull();
    }
    void             setStampAligned(bool _value);
    void             setCloneSampleMode(int _value);
    void             setStampPattern(int _value);
    Q_INVOKABLE void setCloneSource(qreal _x, qreal _y);
    Q_INVOKABLE bool defineStampPattern();
    const QImage    &composite() const;
    QImage           displayComposite() const;
    QImage           layerImage(int _index) const;
    QImage           maskImage(int _index) const;
    void             setActiveLayer(int _index);
    void             setDisplayChannel(int _channel);
    Q_INVOKABLE void toggleMaskOnlyView(int _index);
    Q_INVOKABLE void showCompositeView();
    Q_INVOKABLE void selectLayer(int _index, bool _toggle = false, bool _range = false);
    void             setTool(const QString &_value);
    void             setColor(const QColor &_value);
    void             setSecondaryColor(const QColor &_value);
    void             setBrushSize(qreal _value);
    void             setBrushOpacity(qreal _value);
    void             setBrushHardness(qreal _value);
    void             setBrushFlow(qreal _value);
    void             setBrushSpacing(qreal _value);
    void             setBrushAngle(qreal _value);
    void             setBrushRoundness(qreal _value);
    void             setBrushSmoothing(qreal _value);
    void             setBrushBlend(int _value);
    void             setPencilAutoErase(bool _value);
    void             setToolStrength(qreal _value);
    void             setSpongeSaturate(bool _value);
    void             setRetouchRange(int _value);
    void             setProtectTones(bool _value);
    void             setText(const QString &_value);
    void             setTextSize(int _value);
    void             setFilledShapes(bool _value);
    void             setShapeFillColor(const QColor &_value);
    void             setShapeStrokeColor(const QColor &_value);
    void             setShapeStrokeEnabled(bool _value);
    void             setShapeStrokeWidth(qreal _value);
    void             setShapeStrokeStyle(int _value);
    void             setShapeCornerRadius(qreal _value);
    void             setShapeAlignPixels(bool _value);
    void             setShapeSides(int _value);
    void             setShapeStarRatio(qreal _value);
    void             setSelectionMode(int _value);
    void             setSelectionTolerance(int _value);
    void             setSelectionContiguous(bool _value);
    void             setSelectionSampleAllLayers(bool _value);
    void             setSelectionEnhanceEdge(bool _value);

    Q_INVOKABLE bool newDocument(int _width, int _height, bool _transparent = false);
    Q_INVOKABLE void setPreviewSize(int _width, int _height);
    Q_INVOKABLE bool openFile(const QUrl &_url);
    Q_INVOKABLE bool importLayer(const QUrl &_url);
    Q_INVOKABLE bool saveProject(const QUrl &_url);
    Q_INVOKABLE bool exportImage(const QUrl &_url);
    Q_INVOKABLE void addLayer();
    Q_INVOKABLE void duplicateLayer();
    Q_INVOKABLE void removeLayer();
    Q_INVOKABLE void moveLayer(int _direction);
    Q_INVOKABLE void mergeDown();
    Q_INVOKABLE void groupSelectedLayers();
    Q_INVOKABLE void ungroupActiveLayers();
    Q_INVOKABLE void toggleGroupCollapsed(int _index);
    Q_INVOKABLE void renameLayer(int _index, const QString &_name);
    Q_INVOKABLE void setLayerVisible(int _index, bool _visible);
    Q_INVOKABLE void toggleLayerIsolation(int _index);
    Q_INVOKABLE void setLayerLocked(int _index, bool _locked);
    Q_INVOKABLE void setLayerOpacity(int _index, qreal _opacity);
    Q_INVOKABLE void setLayerBlend(int _index, int _blend);
    Q_INVOKABLE void previewLayerBlend(int _index, int _blend);
    Q_INVOKABLE void setClippingMask(bool _enabled);
    Q_INVOKABLE void addLayerMask(bool _revealAll = true);
    Q_INVOKABLE void copyLayerMask(int _sourceIndex, int _targetIndex);
    Q_INVOKABLE void removeLayerMask();
    Q_INVOKABLE void invertLayerMask();
    Q_INVOKABLE void setMaskEnabled(bool _enabled);
    Q_INVOKABLE void setEditingMask(bool _enabled);
    Q_INVOKABLE void addAdjustmentLayer(const QString &_type);
    Q_INVOKABLE void addFrequencySeparation(qreal _radius = 8);
    Q_INVOKABLE void beginLayerPreview();
    Q_INVOKABLE void previewLayerParameter(const QString &_key, const QVariant &_value);
    Q_INVOKABLE void
    finishLayerPreview(const QString &_label = QStringLiteral("Modifier les propriétés du calque"));
    Q_INVOKABLE void         cancelLayerPreview();
    Q_INVOKABLE void         beginLayerPropertiesSession();
    Q_INVOKABLE void         acceptLayerPropertiesSession();
    Q_INVOKABLE void         cancelLayerPropertiesSession();
    Q_INVOKABLE QColor       sampleCurvePoint(const QString &_mode, int _x, int _y);
    Q_INVOKABLE QColor       sampleCompositeColor(int _x, int _y) const;
    Q_INVOKABLE QVariantList autoBlackWhiteValues() const;
    Q_INVOKABLE void         resetAdjustmentLayer();
    Q_INVOKABLE void         resetCurveAdjustment();
    Q_INVOKABLE void         beginStroke(qreal _x, qreal _y, bool _alternate = false);
    Q_INVOKABLE void         continueStroke(qreal _x, qreal _y);
    Q_INVOKABLE void         endStroke();
    Q_INVOKABLE void         cancelStroke();
    Q_INVOKABLE void         addPolygonLassoPoint(qreal _x, qreal _y);
    Q_INVOKABLE void         finishPolygonLasso();
    Q_INVOKABLE void         removeLastPolygonLassoPoint();
    Q_INVOKABLE void         cancelPolygonLasso();
    Q_INVOKABLE void         resetColors();
    Q_INVOKABLE void         swapColors();
    Q_INVOKABLE void         undo();
    Q_INVOKABLE void         redo();
    Q_INVOKABLE void         selectAll();
    Q_INVOKABLE void         deselect();
    Q_INVOKABLE void         clearSelected();
    Q_INVOKABLE void         fillWithColor(bool _foreground, bool _preserveTransparency = false);
    Q_INVOKABLE void         fillMaskWithColor(const QColor &_color);
    Q_INVOKABLE void         copy();
    Q_INVOKABLE void         paste();
    Q_INVOKABLE void         cropToSelection();
    Q_INVOKABLE void         resizeImage(int _width, int _height);
    Q_INVOKABLE void         transformImage(const QString &_operation);
    Q_INVOKABLE void         applyFilter(const QString &_filter, qreal _amount = 0);
    Q_INVOKABLE void         createDemo();
signals:
    void canvasSizeChanged();
    void documentChanged();
    void layersChanged();
    void imageChanged();
    void thumbnailsChanged();
    void displayChannelChanged();
    void settingsChanged();
    void selectionChanged();
    void historyChanged();
    void errorOccurred(const QString &_message);
    void statusMessage(const QString &_message);

private:
    struct Snapshot {
        QVector<Layer> layers;
        QSize          size;
        int            active;
        QRect          selection;
        QRegion        selectionRegion;
        int            selectionShape;
        QPolygonF      selectionPath;
        QSet<int>      selectedLayers;
        int            layerSelectionAnchor;
        bool           editingMask;
        quint64        id;
        QString        label;
    };
    QSize          m_size{1280, 800};
    QVector<Layer> m_layers;
    int            m_active = 0;
    QSet<int>      m_selectedLayers{0};
    int            m_layerSelectionAnchor = 0;
    QVector<bool>  m_visibilityBeforeIsolation;
    int            m_isolatedLayer = -1;
    QString        m_name          = QStringLiteral("Sans titre");
    QUrl           m_projectUrl;
    QString        m_tool = QStringLiteral("brush");
    QColor         m_color{Qt::black}, m_secondary{Qt::white};
    qreal          m_brushSize = 24, m_brushOpacity = 1, m_brushHardness = 1, m_brushFlow = 1;
    qreal   m_brushSpacing = .25, m_brushAngle = 0, m_brushRoundness = 1, m_brushSmoothing = .1;
    int     m_brushBlend       = 0;
    bool    m_pencilAutoErase  = false;
    bool    m_stampAligned     = true;
    int     m_cloneSampleMode  = 0;
    bool    m_cloneSourceValid = false, m_cloneOffsetValid = false;
    QPointF m_cloneSource, m_cloneOffset, m_patternOrigin;
    bool    m_patternOriginValid = false;
    int     m_stampPattern       = 0;
    QImage  m_customStampPattern, m_stampSource;

    bool   m_smudgeSampleAllLayers = false, m_smudgeFingerPainting = false;
    QImage m_smudgePickup, m_smudgeMask, m_smudgeSample;

    int    m_replacementMode = 0, m_replacementSampling = 0, m_replacementLimits = 0;
    int    m_replacementTolerance = 30;
    bool   m_replacementAntialias = true;
    QColor m_replacementSample;

    qreal                          m_toolStrength   = .5;
    bool                           m_spongeSaturate = false;
    int                            m_retouchRange   = 1;
    bool                           m_protectTones   = true;
    QString                        m_text           = QStringLiteral("Votre texte");
    int                            m_textSize       = 64;
    bool                           m_filledShapes   = true;
    QColor                         m_shapeFillColor;
    QColor                         m_shapeStrokeColor;
    bool                           m_shapeStrokeEnabled = true;
    qreal                          m_shapeStrokeWidth   = 1;
    int                            m_shapeStrokeStyle   = 0;
    qreal                          m_shapeCornerRadius  = 0;
    bool                           m_shapeAlignPixels   = true;
    int                            m_shapeSides         = 5;
    qreal                          m_shapeStarRatio     = .5;
    QRect                          m_selection;
    QRegion                        m_selectionRegion;
    int                            m_selectionShape = 0;
    QPolygonF                      m_selectionPath;
    bool                           m_polygonLassoActive       = false;
    int                            m_selectionMode            = 0;
    int                            m_selectionTolerance       = 32;
    bool                           m_selectionContiguous      = true;
    bool                           m_selectionSampleAllLayers = false;
    bool                           m_selectionEnhanceEdge     = true;
    QRegion                        m_selectionRegionBeforeStroke;
    QRect                          m_selectionBoundsBeforeStroke;
    int                            m_selectionShapeBeforeStroke = 0;
    QPolygonF                      m_selectionPathBeforeStroke;
    bool                           m_quickSelectionFirstDab = false;
    QVector<Snapshot>              m_undo, m_redo;
    quint64                        m_stateId = 1, m_savedId = 1, m_nextId = 2;
    int                            m_revision          = 0;
    int                            m_thumbnailRevision = 0;
    int                            m_displayChannel    = 0;
    bool                           m_maskOnlyView      = false;
    mutable QImage                 m_channelDisplayCache;
    mutable qint64                 m_channelDisplaySourceKey  = 0;
    mutable int                    m_channelDisplayCacheIndex = 0;
    mutable QImage                 m_composite;
    mutable bool                   m_cacheDirty         = true;
    mutable bool                   m_fullCompositeDirty = true;
    mutable QRect                  m_dirtyRegion;
    bool                           m_drawing = false, m_strokeChanged = false;
    bool                           m_editingMask           = false;
    bool                           m_previewingLayer       = false;
    bool                           m_layerPreviewChanged   = false;
    bool                           m_frequencyPreviewDraft = false;
    QPointF                        m_anchor, m_last;
    QPointF                        m_smoothedLast;
    qreal                          m_distanceToNextDab = 0;
    QImage                         m_strokeBase, m_strokeCoverage;
    QColor                         m_strokeInkColor;
    QRectF                         m_strokeBounds;
    mutable QHash<QString, QImage> m_frequencyCache;
    QFutureWatcher<QImage>         m_previewRenderWatcher;
    QVector<Layer>                 m_pendingPreviewLayers;
    QSize                          m_pendingPreviewSize;
    QSize                          m_pendingPreviewTargetSize;
    QSize                          m_previewTargetSize;
    QImage                         m_previewComposite;
    bool                           m_pendingPreviewDraft  = false;
    bool                           m_pendingPreviewRefine = false;
    bool                           m_runningPreviewDraft  = false;
    bool                           m_runningPreviewRefine = false;
    QSize                          m_runningPreviewTargetSize;
    quint64                        m_previewRequestGeneration = 0;
    quint64                        m_runningPreviewGeneration = 0;
    bool                           m_previewRenderPending     = false;
    std::atomic_bool               m_previewRenderActive{false};
    mutable QMutex                 m_previewImageMutex;
    Snapshot                       m_beforeStroke;
    Snapshot                       m_beforeLayerPreview;
    QImage                         m_beforeLayerPreviewComposite;
    bool                           m_layerPropertiesSession = false;
    Snapshot                       m_beforeLayerProperties;
    QVector<Snapshot>              m_undoBeforeLayerProperties;
    QVector<Snapshot>              m_redoBeforeLayerProperties;
    Snapshot                       snapshot(const QString &_label) const;
    void                           restore(const Snapshot &_state);
    void    commit(const Snapshot &_before, const QString &_label, bool _invalidateFull = true,
                   bool _notifyImage = true);
    void    trimHistory();
    void    beginLasso(qreal _x, qreal _y);
    void    continueLasso(qreal _x, qreal _y);
    void    finishLasso();
    void    applyMagicWand(const QPoint &_point);
    void    applySelectionCandidate(const QRegion &_candidate, bool _firstDab);
    void    beginQuickSelection(const QPointF &_point);
    void    continueQuickSelection(const QPointF &_point);
    void    finishQuickSelection();
    void    cancelQuickSelection();
    void    selectOnly(int _index);
    void    changed(bool _metadata = true, bool _notifyImage = true);
    void    resetHistory(bool _clean);
    bool    validSize(int _width, int _height, int _layerCount = 1);
    bool    editable();
    bool    fail(const QString &_message);
    void    edit(const QString &_label, const std::function<void()> &_operation);
    void    paintSegment(const QPointF &_from, const QPointF &_to);
    void    stampBrush(const QPointF &_point);
    void    renderBrushStroke();
    void    renderRetouchStroke();
    void    prepareSmudgeStroke(bool _alternate);
    void    smudgeDab(const QPointF &_point);
    void    colorReplacementDab(const QPointF &_point);
    bool    prepareStampStroke();
    void    renderStampStroke();
    QImage  stampPatternImage() const;
    void    renderShape(const QPointF &_to);
    void    floodFill(QPoint _point);
    QImage *paintTarget();
    QImage  readImage(const QString &_path);
    bool    loadProject(const QString &_path);
    static QImage applyMask(const QImage &_source, const QImage &_mask, qreal _density = 1,
                            qreal _feather = 0);
    static QImage blendImages(const QImage &_base, const QImage &_source, int _mode,
                              qreal _opacity);
    static QImage blurImage(const QImage &_source, int _radius);
    static QImage gaussianBlurImage(const QImage &_source, qreal _radius);
    static QImage motionBlurImage(const QImage &_source, qreal _distance, qreal _angle);
    static QImage unsharpMaskImage(const QImage &_source, qreal _radius, qreal _amount,
                                   int _threshold);
    static QImage highPassImage(const QImage &_source, qreal _radius);
    static QImage mosaicImage(const QImage &_source, int _cellSize);
    static QImage noiseImage(const QImage &_source, qreal _amount);
    static QImage adjustedImage(const QImage &_source, const QString &_type,
                                const QVariantMap &_parameters);
    QImage        frequencyImage(const Layer &_layer) const;
    void          requestPreviewRender(bool _draft, bool _refine = false);
    void          startPendingPreviewRender();
    static QImage renderPreviewSnapshot(QVector<Layer> _layers, const QSize &_size,
                                        const QSize &_targetSize, bool _draft);
};
