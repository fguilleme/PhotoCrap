#pragma once
#include "RawImageReader.h"
#include "selection/SelectionCoverage.h"
#include "tools/PenPath.h"
#include <QCache>
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
#include <QUuid>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <memory>

class ScratchImageCache;

struct Layer {
    Layer() = default;
    Layer(const QString &_layerName, const QImage &_layerImage, bool _layerVisible = true,
          bool _layerLocked = false, qreal _layerOpacity = 1, int _layerBlend = 0)
        : name(_layerName), image(_layerImage), visible(_layerVisible), locked(_layerLocked),
          opacity(_layerOpacity), blend(_layerBlend)
    {
    }
    // Runtime identity follows a layer through reordering and undo.
    QString     historyId = QUuid::createUuid().toString(QUuid::WithoutBraces);
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
    Q_PROPERTY(quint64 documentSession READ documentSession NOTIFY documentChanged)
    Q_PROPERTY(bool rawLoading READ rawLoading NOTIFY rawLoadingChanged)
    Q_PROPERTY(
        bool frequencyRetouchBusy READ frequencyRetouchBusy NOTIFY frequencyRetouchBusyChanged)
    Q_PROPERTY(int selectedLayerCount READ selectedLayerCount NOTIFY layersChanged)
    Q_PROPERTY(bool canGroupSelected READ canGroupSelected NOTIFY layersChanged)
    Q_PROPERTY(bool canUngroupActive READ canUngroupActive NOTIFY layersChanged)
    Q_PROPERTY(int revision READ revision NOTIFY imageChanged)
    Q_PROPERTY(int thumbnailRevision READ thumbnailRevision NOTIFY thumbnailsChanged)
    Q_PROPERTY(
        int displayChannel READ displayChannel WRITE setDisplayChannel NOTIFY displayChannelChanged)
    Q_PROPERTY(int maskOnlyLayer READ maskOnlyLayer NOTIFY displayChannelChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList penPaths READ penPaths NOTIFY penPathsChanged)
    Q_PROPERTY(int activePenPath READ activePenPath WRITE setActivePenPath NOTIFY penPathsChanged)
    Q_PROPERTY(bool penDrawing READ penDrawing NOTIFY penPathsChanged)
    Q_PROPERTY(QPointF penHover READ penHover NOTIFY penHoverChanged)
    Q_PROPERTY(
        bool penRubberBand READ penRubberBand WRITE setPenRubberBand NOTIFY penOptionsChanged)
    Q_PROPERTY(bool penAutoAddDelete READ penAutoAddDelete WRITE setPenAutoAddDelete NOTIFY
                   penOptionsChanged)
    Q_PROPERTY(QVariantList slices READ slices NOTIFY slicesChanged)
    Q_PROPERTY(int activeSlice READ activeSlice NOTIFY slicesChanged)
    Q_PROPERTY(QRectF sliceDraft READ sliceDraft NOTIFY slicesChanged)
    Q_PROPERTY(bool sliceBusy READ sliceBusy NOTIFY slicesChanged)
    Q_PROPERTY(bool sliceShowAuto READ sliceShowAuto WRITE setSliceShowAuto NOTIFY slicesChanged)
    Q_PROPERTY(QVariantList cropPoints READ cropPoints NOTIFY cropChanged)
    Q_PROPERTY(bool hasCrop READ hasCrop NOTIFY cropChanged)
    Q_PROPERTY(bool cropBusy READ cropBusy NOTIFY cropChanged)
    Q_PROPERTY(qreal cropRatio READ cropRatio WRITE setCropRatio NOTIFY cropChanged)
    Q_PROPERTY(QSize cropOutputSize READ cropOutputSize WRITE setCropOutputSize NOTIFY cropChanged)
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
        int eyedropperSize READ eyedropperSize WRITE setEyedropperSize NOTIFY settingsChanged)
    Q_PROPERTY(
        int eyedropperSource READ eyedropperSource WRITE setEyedropperSource NOTIFY settingsChanged)
    Q_PROPERTY(
        bool eyedropperRing READ eyedropperRing WRITE setEyedropperRing NOTIFY settingsChanged)
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
    Q_PROPERTY(
        qreal redEyePupilSize READ redEyePupilSize WRITE setRedEyePupilSize NOTIFY settingsChanged)
    Q_PROPERTY(qreal redEyeDarken READ redEyeDarken WRITE setRedEyeDarken NOTIFY settingsChanged)
    Q_PROPERTY(QRectF redEyeRegion READ redEyeRegion NOTIFY redEyeRegionChanged)
    Q_PROPERTY(qreal mixerWet READ mixerWet WRITE setMixerWet NOTIFY settingsChanged)
    Q_PROPERTY(qreal mixerLoad READ mixerLoad WRITE setMixerLoad NOTIFY settingsChanged)
    Q_PROPERTY(qreal mixerMix READ mixerMix WRITE setMixerMix NOTIFY settingsChanged)
    Q_PROPERTY(qreal mixerFlow READ mixerFlow WRITE setMixerFlow NOTIFY settingsChanged)
    Q_PROPERTY(bool mixerSampleAllLayers READ mixerSampleAllLayers WRITE setMixerSampleAllLayers
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool mixerAutoLoad READ mixerAutoLoad WRITE setMixerAutoLoad NOTIFY settingsChanged)
    Q_PROPERTY(
        bool mixerAutoClean READ mixerAutoClean WRITE setMixerAutoClean NOTIFY settingsChanged)
    Q_PROPERTY(
        bool mixerSolidOnly READ mixerSolidOnly WRITE setMixerSolidOnly NOTIFY settingsChanged)
    Q_PROPERTY(QColor mixerLoadColor READ mixerLoadColor NOTIFY settingsChanged)
    Q_PROPERTY(bool mixerLoaded READ mixerLoaded NOTIFY settingsChanged)
    Q_PROPERTY(int mixerPreset READ mixerPreset NOTIFY settingsChanged)
    Q_PROPERTY(
        bool healingAligned READ healingAligned WRITE setHealingAligned NOTIFY settingsChanged)
    Q_PROPERTY(int healingSampleMode READ healingSampleMode WRITE setHealingSampleMode NOTIFY
                   settingsChanged)
    Q_PROPERTY(
        int healingDiffusion READ healingDiffusion WRITE setHealingDiffusion NOTIFY settingsChanged)
    Q_PROPERTY(int healingMode READ healingMode WRITE setHealingMode NOTIFY settingsChanged)
    Q_PROPERTY(bool healingIgnoreAdjustments READ healingIgnoreAdjustments WRITE
                   setHealingIgnoreAdjustments NOTIFY settingsChanged)
    Q_PROPERTY(
        int contentMoveMode READ contentMoveMode WRITE setContentMoveMode NOTIFY settingsChanged)
    Q_PROPERTY(int contentMoveStructure READ contentMoveStructure WRITE setContentMoveStructure
                   NOTIFY settingsChanged)
    Q_PROPERTY(
        int contentMoveColor READ contentMoveColor WRITE setContentMoveColor NOTIFY settingsChanged)
    Q_PROPERTY(bool contentMoveSampleAllLayers READ contentMoveSampleAllLayers WRITE
                   setContentMoveSampleAllLayers NOTIFY settingsChanged)
    Q_PROPERTY(bool contentMoveBusy READ contentMoveBusy NOTIFY settingsChanged)
    Q_PROPERTY(bool removeSampleAllLayers READ removeSampleAllLayers WRITE setRemoveSampleAllLayers
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool removeAfterEachStroke READ removeAfterEachStroke WRITE setRemoveAfterEachStroke
                   NOTIFY settingsChanged)
    Q_PROPERTY(
        bool removeSubtract READ removeSubtract WRITE setRemoveSubtract NOTIFY settingsChanged)
    Q_PROPERTY(bool removeBusy READ removeBusy NOTIFY settingsChanged)
    Q_PROPERTY(bool removeHasPending READ removeHasPending NOTIFY removeRegionChanged)
    Q_PROPERTY(int patchMode READ patchMode WRITE setPatchMode NOTIFY settingsChanged)
    Q_PROPERTY(
        int patchDiffusion READ patchDiffusion WRITE setPatchDiffusion NOTIFY settingsChanged)
    Q_PROPERTY(bool patchDragging READ patchDragging NOTIFY patchPreviewChanged)
    Q_PROPERTY(QRect patchTargetRect READ patchTargetRect NOTIFY patchPreviewChanged)
    Q_PROPERTY(bool spotHealingSampleAllLayers READ spotHealingSampleAllLayers WRITE
                   setSpotHealingSampleAllLayers NOTIFY settingsChanged)
    Q_PROPERTY(
        int spotHealingMode READ spotHealingMode WRITE setSpotHealingMode NOTIFY settingsChanged)
    Q_PROPERTY(bool healingSourceValid READ healingSourceValid NOTIFY settingsChanged)
    Q_PROPERTY(QPointF healingOffset READ healingOffset NOTIFY settingsChanged)
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
    Q_PROPERTY(bool canReselect READ canReselect NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionEdgesVisible READ selectionEdgesVisible WRITE setSelectionEdgesVisible
                   NOTIFY selectionDisplayChanged)
    Q_PROPERTY(bool selectionBusy READ selectionBusy NOTIFY selectionOperationChanged)
    Q_PROPERTY(bool colorSelectionActive READ colorSelectionActive NOTIFY colorSelectionChanged)
    Q_PROPERTY(bool colorSelectionReady READ colorSelectionReady NOTIFY colorSelectionChanged)
    Q_PROPERTY(int colorSelectionRevision READ colorSelectionRevision NOTIFY colorSelectionChanged)
    Q_PROPERTY(
        int colorSelectionSampleCount READ colorSelectionSampleCount NOTIFY colorSelectionChanged)
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
    Q_PROPERTY(int magicEraserTolerance READ magicEraserTolerance WRITE setMagicEraserTolerance
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool magicEraserContiguous READ magicEraserContiguous WRITE setMagicEraserContiguous
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool magicEraserAntialias READ magicEraserAntialias WRITE setMagicEraserAntialias
                   NOTIFY settingsChanged)
    Q_PROPERTY(bool magicEraserSampleAllLayers READ magicEraserSampleAllLayers WRITE
                   setMagicEraserSampleAllLayers NOTIFY settingsChanged)
    Q_PROPERTY(bool magicEraserBusy READ magicEraserBusy NOTIFY settingsChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
    Q_PROPERTY(int historyBrushSourceIndex READ historyBrushSourceIndex NOTIFY historyChanged)
    Q_PROPERTY(QString historyBrushSourceLabel READ historyBrushSourceLabel NOTIFY
                   historyBrushSourceChanged)
    Q_PROPERTY(bool historyBrushSourceAvailable READ historyBrushSourceAvailable NOTIFY
                   historyBrushSourceChanged)
    Q_PROPERTY(
        int artHistoryStyle READ artHistoryStyle WRITE setArtHistoryStyle NOTIFY settingsChanged)
    Q_PROPERTY(
        int artHistoryArea READ artHistoryArea WRITE setArtHistoryArea NOTIFY settingsChanged)
    Q_PROPERTY(int artHistoryTolerance READ artHistoryTolerance WRITE setArtHistoryTolerance NOTIFY
                   settingsChanged)
    Q_PROPERTY(bool editingMask READ editingMask NOTIFY layersChanged)
public:
    explicit Document(QObject *_parent = nullptr);
    ~Document() override;
    QVariantList penPaths() const;
    int          activePenPath() const
    {
        return m_activePenPath;
    }
    bool penDrawing() const
    {
        return m_penDrawing;
    }
    QPointF penHover() const
    {
        return m_penHover;
    }
    bool penRubberBand() const
    {
        return m_penRubberBand;
    }
    bool penAutoAddDelete() const
    {
        return m_penAutoAddDelete;
    }
    void             setPenRubberBand(bool _value);
    void             setPenAutoAddDelete(bool _value);
    Q_INVOKABLE void setActivePenPath(int _index);
    Q_INVOKABLE void newPenPath();
    Q_INVOKABLE void renamePenPath(const QString &_name);
    Q_INVOKABLE void deletePenPath();
    Q_INVOKABLE void deletePenPoints();
    Q_INVOKABLE void nudgePenPoints(int _dx, int _dy);
    Q_INVOKABLE void finishPenPath();
    Q_INVOKABLE void closePenPath();
    Q_INVOKABLE void beginPenGesture(qreal _x, qreal _y, qreal _tolerance = 6, bool _direct = false,
                                     bool _alternate = false, bool _shift = false);
    Q_INVOKABLE void updatePenGesture(qreal _x, qreal _y, bool _shift = false,
                                      bool _alternate = false, bool _space = false);
    Q_INVOKABLE void endPenGesture(bool _cancel = false);
    Q_INVOKABLE void updatePenHover(qreal _x, qreal _y);
    Q_INVOKABLE QString penCursorAt(qreal _x, qreal _y, qreal _tolerance, bool _direct,
                                    bool _alternate) const;
    Q_INVOKABLE void    penPathToSelection(int _mode = 0);
    Q_INVOKABLE void    fillPenPath();
    Q_INVOKABLE void    strokePenPath();
    Q_PROPERTY(int channelBits READ channelBits NOTIFY documentChanged)
    int channelBits() const
    {
        return m_channelBits;
    }
    Q_INVOKABLE bool setChannelBits(int _bits);
    Q_INVOKABLE bool toolSupportsDepth(const QString &_tool) const;
    QImage::Format   workingFormat() const
    {
        return m_channelBits == 16 ? QImage::Format_RGBA64_Premultiplied
                                   : QImage::Format_ARGB32_Premultiplied;
    }

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
        return m_contentId != m_savedId;
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
    bool canReselect() const
    {
        return !(m_lastSelectionRegion & QRegion(QRect(QPoint{}, m_size))).isEmpty();
    }
    bool selectionEdgesVisible() const
    {
        return m_selectionEdgesVisible;
    }
    bool selectionBusy() const
    {
        return bool(m_selectionCancel);
    }
    void             setSelectionEdgesVisible(bool _visible);
    Q_INVOKABLE void modifySelection(const QString &_operation, int _amount);
    Q_INVOKABLE void cancelSelectionOperation();
    bool             colorSelectionActive() const
    {
        return m_colorSelectionActive;
    }
    bool colorSelectionReady() const
    {
        return m_colorSelectionResult.valid && !selectionBusy();
    }
    int colorSelectionRevision() const
    {
        return m_colorSelectionRevision;
    }
    int colorSelectionSampleCount() const
    {
        return int(m_colorSamples.size());
    }
    Q_INVOKABLE void beginColorSelection();
    Q_INVOKABLE void previewColorSelection(const QVariantMap &_options);
    Q_INVOKABLE void sampleSelectionColor(int _x, int _y, int _mode,
                                          const QVariantMap &_options = {});
    Q_INVOKABLE void acceptColorSelection();
    Q_INVOKABLE void cancelColorSelection();
    QImage           colorSelectionPreview(bool _mask, const QRect &_region = {},
                                           const QSize &_requested = {}) const;
    int              selectionShape() const
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
    const QRegion &selectionOutline() const
    {
        return m_selectionMask.isNull() ? m_selectionRegion : m_selectionOutline;
    }
    QImage         selectionMask() const;
    int            selectionCoverage(int _x, int _y) const;
    const QRegion &selectionRegion() const
    {
        return m_selectionRegion;
    }
    int magicEraserTolerance() const
    {
        return m_magicEraserTolerance;
    }
    bool magicEraserContiguous() const
    {
        return m_magicEraserContiguous;
    }
    bool magicEraserAntialias() const
    {
        return m_magicEraserAntialias;
    }
    bool magicEraserSampleAllLayers() const
    {
        return m_magicEraserSampleAllLayers;
    }
    bool magicEraserBusy() const
    {
        return m_magicEraserBusy;
    }
    void             setMagicEraserTolerance(int _value);
    void             setMagicEraserContiguous(bool _value);
    void             setMagicEraserAntialias(bool _value);
    void             setMagicEraserSampleAllLayers(bool _value);
    Q_INVOKABLE void cancelMagicErase();

    quint64 documentSession() const
    {
        return m_documentSession;
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
    int         historyBrushSourceIndex() const;
    QString     historyBrushSourceLabel() const
    {
        return m_historyBrushSourceLabel;
    }
    bool             historyBrushSourceAvailable() const;
    Q_INVOKABLE void setHistoryBrushSource(int _index);
    int              artHistoryStyle() const
    {
        return m_artHistoryStyle;
    }
    int artHistoryArea() const
    {
        return m_artHistoryArea;
    }
    int artHistoryTolerance() const
    {
        return m_artHistoryTolerance;
    }
    void setArtHistoryStyle(int _value);
    void setArtHistoryArea(int _value);
    void setArtHistoryTolerance(int _value);
    int  historyIndex() const
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
    qreal redEyePupilSize() const
    {
        return m_redEyePupilSize;
    }
    qreal redEyeDarken() const
    {
        return m_redEyeDarken;
    }
    QRectF redEyeRegion() const
    {
        return m_redEyeRegion;
    }
    void setRedEyePupilSize(qreal _value);
    void setRedEyeDarken(qreal _value);
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
    void  setReplacementMode(int _value);
    void  setReplacementSampling(int _value);
    void  setReplacementLimits(int _value);
    void  setReplacementTolerance(int _value);
    void  setReplacementAntialias(bool _value);
    qreal mixerWet() const
    {
        return m_mixerWet;
    }
    void  setMixerWet(qreal _value);
    qreal mixerLoad() const
    {
        return m_mixerLoad;
    }
    void  setMixerLoad(qreal _value);
    qreal mixerMix() const
    {
        return m_mixerMix;
    }
    void  setMixerMix(qreal _value);
    qreal mixerFlow() const
    {
        return m_mixerFlow;
    }
    void setMixerFlow(qreal _value);
    bool mixerSampleAllLayers() const
    {
        return m_mixerSampleAllLayers;
    }
    void setMixerSampleAllLayers(bool _value);
    bool mixerAutoLoad() const
    {
        return m_mixerAutoLoad;
    }
    void setMixerAutoLoad(bool _value);
    bool mixerAutoClean() const
    {
        return m_mixerAutoClean;
    }
    void setMixerAutoClean(bool _value);
    bool mixerSolidOnly() const
    {
        return m_mixerSolidOnly;
    }
    void   setMixerSolidOnly(bool _value);
    QColor mixerLoadColor() const
    {
        return m_mixerColor;
    }
    bool mixerLoaded() const
    {
        return !m_mixerPaint.isNull() && m_mixerRemaining > 0;
    }
    int              mixerPreset() const;
    Q_INVOKABLE void applyMixerPreset(int _preset);
    Q_INVOKABLE void loadMixerBrush();
    Q_INVOKABLE void cleanMixerBrush();
    Q_INVOKABLE void sampleMixerBrush(qreal _x, qreal _y);
    bool             spotHealingSampleAllLayers() const
    {
        return m_spotHealingSampleAllLayers;
    }
    int spotHealingMode() const
    {
        return m_spotHealingMode;
    }
    bool removeSampleAllLayers() const
    {
        return m_removeSampleAllLayers;
    }
    bool removeAfterEachStroke() const
    {
        return m_removeAfterEachStroke;
    }
    bool removeSubtract() const
    {
        return m_removeSubtract;
    }
    bool removeBusy() const
    {
        return m_removeBusy;
    }
    bool removeHasPending() const
    {
        return !m_removeRegion.isEmpty();
    }
    QRegion removeRegion() const
    {
        return m_removeRegion;
    }
    void             setRemoveSampleAllLayers(bool _value);
    void             setRemoveAfterEachStroke(bool _value);
    void             setRemoveSubtract(bool _value);
    Q_INVOKABLE void applyRemoval();
    Q_INVOKABLE void cancelRemoval();
    int              contentMoveMode() const
    {
        return m_contentMoveMode;
    }
    int contentMoveStructure() const
    {
        return m_contentMoveStructure;
    }
    int contentMoveColor() const
    {
        return m_contentMoveColor;
    }
    bool contentMoveSampleAllLayers() const
    {
        return m_contentMoveSampleAllLayers;
    }
    bool contentMoveBusy() const
    {
        return m_contentMoveBusy;
    }
    void setContentMoveMode(int _value);
    void setContentMoveStructure(int _value);
    void setContentMoveColor(int _value);
    void setContentMoveSampleAllLayers(bool _value);
    int  patchMode() const
    {
        return m_patchMode;
    }
    int patchDiffusion() const
    {
        return m_patchDiffusion;
    }
    bool patchDragging() const
    {
        return m_patchDragging;
    }
    QImage patchPreviewImage() const
    {
        return m_patchPreview;
    }
    QRect patchTargetRect() const
    {
        return m_patchTarget.boundingRect();
    }
    QRegion patchTargetRegion() const
    {
        return m_patchTarget;
    }
    Q_INVOKABLE bool selectionContains(qreal _x, qreal _y) const;
    void             setPatchMode(int _value);
    void             setPatchDiffusion(int _value);
    void             setSpotHealingSampleAllLayers(bool _value);
    void             setSpotHealingMode(int _value);
    bool             healingAligned() const
    {
        return m_healingAligned;
    }
    int healingSampleMode() const
    {
        return m_healingSampleMode;
    }
    int healingDiffusion() const
    {
        return m_healingDiffusion;
    }
    int healingMode() const
    {
        return m_healingMode;
    }
    bool healingIgnoreAdjustments() const
    {
        return m_healingIgnoreAdjustments;
    }
    bool healingSourceValid() const
    {
        return m_healingSourceValid;
    }
    QPointF healingOffset() const
    {
        return m_healingOffset;
    }
    void             setHealingAligned(bool _value);
    void             setHealingSampleMode(int _value);
    void             setHealingDiffusion(int _value);
    void             setHealingMode(int _value);
    void             setHealingIgnoreAdjustments(bool _value);
    Q_INVOKABLE void setHealingSource(qreal _x, qreal _y);
    bool             stampAligned() const
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
    Q_INVOKABLE QVariantMap memoryUsage() const;
    Q_INVOKABLE void        configureScratchCache(const QString &_directory, qint64 _maxBytes);
    Q_INVOKABLE QVariantMap scratchCacheUsage() const;
    Q_INVOKABLE void        clearScratchCache();

    void             setStampAligned(bool _value);
    void             setCloneSampleMode(int _value);
    void             setStampPattern(int _value);
    Q_INVOKABLE void setCloneSource(qreal _x, qreal _y);
    Q_INVOKABLE bool defineStampPattern();
    const QImage    &composite() const;
    QImage           displayComposite() const;
    QImage           layerImage(int _index, const QSize &_requested = {}) const;
    QImage           maskImage(int _index, const QSize &_requested = {}) const;
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

    Q_INVOKABLE bool newDocument(int _width, int _height, bool _transparent = false, int _bits = 8);
    Q_INVOKABLE void setPreviewSize(int _width, int _height);
    Q_INVOKABLE bool openFile(const QUrl &_url);
    Q_INVOKABLE bool importLayer(const QUrl &_url);
    Q_INVOKABLE bool isRawFile(const QUrl &_url) const;
    Q_INVOKABLE QString rawFilePatterns() const;
    Q_INVOKABLE bool    openRawFile(const QUrl &_url, bool _asLayer = false);
    Q_INVOKABLE void    cancelRawLoad();
    bool                rawLoading() const
    {
        return m_rawLoading;
    }
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
    Q_INVOKABLE bool prepareFrequencyRetouch();
    Q_INVOKABLE bool restoreDynamicFrequency();
    Q_INVOKABLE void cancelFrequencyRetouch();
    bool             frequencyRetouchBusy() const
    {
        return m_frequencyRetouchBusy;
    }
    Q_INVOKABLE void beginLayerPreview();
    Q_INVOKABLE void previewLayerParameter(const QString &_key, const QVariant &_value);
    Q_INVOKABLE void
    finishLayerPreview(const QString &_label = QStringLiteral("Modifier les propriétés du calque"));
    Q_INVOKABLE void   cancelLayerPreview();
    Q_INVOKABLE void   beginLayerPropertiesSession();
    Q_INVOKABLE void   acceptLayerPropertiesSession();
    Q_INVOKABLE void   cancelLayerPropertiesSession();
    Q_INVOKABLE QColor sampleCurvePoint(const QString &_mode, int _x, int _y);
    Q_INVOKABLE QColor sampleCompositeColor(int _x, int _y) const;
    int                eyedropperSize() const
    {
        return m_eyedropperSize;
    }
    int eyedropperSource() const
    {
        return m_eyedropperSource;
    }
    bool eyedropperRing() const
    {
        return m_eyedropperRing;
    }
    void                     setEyedropperSize(int _value);
    void                     setEyedropperSource(int _value);
    void                     setEyedropperRing(bool _value);
    Q_INVOKABLE QColor       sampleToolColor(int _x, int _y) const;
    Q_INVOKABLE void         pickToolColor(int _x, int _y, bool _background = false);
    Q_INVOKABLE QVariantList autoBlackWhiteValues() const;
    Q_INVOKABLE void         resetAdjustmentLayer();
    Q_INVOKABLE void         resetCurveAdjustment();
    Q_INVOKABLE void         beginStroke(qreal _x, qreal _y, bool _alternate = false,
                                         bool _straight = false);
    Q_INVOKABLE void         setToolModifiers(bool _shift, bool _alternate, bool _space);
    Q_INVOKABLE void         nudgeTool(int _dx, int _dy, bool _copy = false);
    Q_INVOKABLE void         continueStroke(qreal _x, qreal _y);
    Q_INVOKABLE void         endStroke();
    Q_INVOKABLE void         cancelStroke();
    Q_INVOKABLE void         addPolygonLassoPoint(qreal _x, qreal _y);
    Q_INVOKABLE void         continuePolygonLasso(qreal _x, qreal _y);
    Q_INVOKABLE void         finishPolygonLasso();
    Q_INVOKABLE void         removeLastPolygonLassoPoint();
    Q_INVOKABLE void         cancelPolygonLasso();
    Q_INVOKABLE void         resetColors();
    Q_INVOKABLE void         swapColors();
    Q_INVOKABLE void         undo();
    Q_INVOKABLE void         redo();
    Q_INVOKABLE void         selectAll();
    Q_INVOKABLE void         deselect();
    Q_INVOKABLE void         reselect();
    Q_INVOKABLE void         invertSelection();
    Q_INVOKABLE void         invertSelectedPixels();
    Q_INVOKABLE void         selectAllLayers();
    Q_INVOKABLE void         selectSimilarLayers();
    Q_INVOKABLE void         clearSelected();
    Q_INVOKABLE void         fillWithColor(bool _foreground, bool _preserveTransparency = false);
    Q_INVOKABLE void         fillSelectedWithColor(const QColor &_color);
    Q_INVOKABLE void         fillMaskWithColor(const QColor &_color);
    Q_INVOKABLE void         copy();
    Q_INVOKABLE void         paste();
    Q_INVOKABLE void         cropToSelection();
    QVariantList             slices() const;
    int                      activeSlice() const
    {
        return m_activeSlice;
    }
    QRectF sliceDraft() const
    {
        return m_sliceDraft;
    }
    bool sliceBusy() const
    {
        return m_sliceBusy;
    }
    bool sliceShowAuto() const
    {
        return m_sliceShowAuto;
    }
    void             setSliceShowAuto(bool _value);
    Q_INVOKABLE void beginSliceGesture(qreal _x, qreal _y, qreal _tolerance = 8,
                                       bool _select = false);
    Q_INVOKABLE void updateSliceGesture(qreal _x, qreal _y);
    Q_INVOKABLE void endSliceGesture(bool _cancel = false);
    Q_INVOKABLE void deleteActiveSlice();
    Q_INVOKABLE void divideActiveSlice(int _columns, int _rows);
    Q_INVOKABLE void setActiveSliceRect(int _x, int _y, int _width, int _height);
    Q_INVOKABLE void exportSlices(const QUrl &_folder);
    QVariantList     cropPoints() const;
    bool             hasCrop() const;
    bool             cropBusy() const
    {
        return m_cropBusy;
    }
    qreal cropRatio() const
    {
        return m_cropRatio;
    }
    QSize cropOutputSize() const
    {
        return m_cropOutputSize;
    }
    void             setCropRatio(qreal _ratio);
    void             setCropOutputSize(const QSize &_size);
    Q_INVOKABLE void beginCropGesture(qreal _x, qreal _y, qreal _tolerance = 8);
    Q_INVOKABLE void updateCropGesture(qreal _x, qreal _y);
    Q_INVOKABLE void endCropGesture();
    Q_INVOKABLE void setCropCorner(int _index, qreal _x, qreal _y);
    Q_INVOKABLE void resetCrop();
    Q_INVOKABLE void cancelCrop();
    Q_INVOKABLE void applyCrop();
    Q_INVOKABLE void resizeImage(int _width, int _height);
    Q_INVOKABLE void transformImage(const QString &_operation);
    Q_INVOKABLE void applyFilter(const QString &_filter, qreal _amount = 0);
    Q_INVOKABLE void createDemo();
signals:
    void penPathsChanged();
    void penHoverChanged();
    void penOptionsChanged();
    void rawLoadingChanged();
    void frequencyRetouchBusyChanged();
    void rawFileOpened(const QUrl &_url);
    void slicesChanged();
    void cropChanged();
    void canvasSizeChanged();
    void documentChanged();
    void layersChanged();
    void imageChanged();
    void thumbnailsChanged();
    void displayChannelChanged();
    void settingsChanged();
    void selectionChanged();
    void selectionDisplayChanged();
    void selectionOperationChanged();
    void colorSelectionChanged();
    void redEyeRegionChanged();
    void patchPreviewChanged();
    void removeRegionChanged();
    void historyBrushSourceChanged();
    void historyChanged();
    void errorOccurred(const QString &_message);
    void statusMessage(const QString &_message);

private:
    struct Snapshot {
        QVector<Layer>   layers;
        QSize            size;
        int              active;
        QRect            selection;
        QRegion          selectionRegion;
        int              selectionShape;
        QPolygonF        selectionPath;
        QSet<int>        selectedLayers;
        int              layerSelectionAnchor;
        bool             editingMask;
        quint64          id;
        QString          label;
        QVector<QRect>   slices;
        int              activeSlice = -1;
        int              channelBits = 8;
        QVector<PenPath> penPaths;
        int              activePenPath = -1;
        QSet<int>        selectedPenPoints;
        bool             penDrawing    = false;
        bool             penOnly       = false;
        bool             selectionOnly = false;
        quint64          contentId     = 0;
        QRegion          lastSelectionRegion;
        QImage           selectionMask, lastSelectionMask;
        QRegion          selectionOutline, lastSelectionOutline;
    };
    Snapshot m_beforeColorSelection;
    bool     m_colorSelectionActive   = false;
    int      m_colorSelectionRevision = 0;
    QImage   m_colorSelectionSource, m_colorSelectionSourcePreview, m_colorSelectionMaskPreview;
    QVector<SelectionCoverage::Sample> m_colorSamples;
    QVariantMap                        m_colorSelectionOptions;
    SelectionCoverage::Result          m_colorSelectionResult;
    Snapshot                           m_beforeSelectionGesture;
    bool m_selectionGestureRecorded = false, m_selectionEdgesVisible = true;
    std::shared_ptr<std::atomic_bool> m_selectionCancel;
    quint64                           m_selectionTaskStateId = 0;
    void             commitSelectionEdit(const Snapshot &_before, const QString &_label);
    void             finishSelectionGesture();
    QVector<PenPath> m_penPaths;
    int              m_activePenPath = -1, m_penGesture = 0, m_penPoint = -1;
    bool             m_penDrawing = false, m_penWasDrawing = false, m_penBroken = false;
    bool             m_penRubberBand = false, m_penAutoAddDelete = true;
    QPointF          m_penHover, m_penOrigin, m_penLastPointer;
    QSet<int>        m_selectedPenPoints, m_penSelectionBefore;
    Snapshot         m_beforePenGesture;
    void             commitPenEdit(const Snapshot &_before, const QString &_label);
    QPair<int, int>  hitPenPoint(const QPointF &_point, qreal _tolerance) const;
    QVector<QRect>   m_slices;
    int              m_activeSlice = -1, m_sliceHandle = -2;
    bool             m_sliceBusy = false, m_sliceDragging = false, m_sliceCreating = false,
                     m_sliceShowAuto = false;
    QRectF           m_sliceDraft, m_sliceBefore;
    QPointF          m_sliceAnchor;
    void             commitSlices(const Snapshot &_before, const QString &_label);
    int              m_channelBits = 8;
    QSize            m_size{1280, 800};
    QVector<Layer>   m_layers;
    int              m_active = 0;
    QSet<int>        m_selectedLayers{0};
    int              m_layerSelectionAnchor = 0;
    QVector<bool>    m_visibilityBeforeIsolation;
    int              m_isolatedLayer = -1;
    QString          m_name          = QStringLiteral("Sans titre");
    QUrl             m_projectUrl;
    QString          m_tool = QStringLiteral("brush");
    QColor           m_color{Qt::black}, m_secondary{Qt::white};
    qreal            m_brushSize = 24, m_brushOpacity = 1, m_brushHardness = 1, m_brushFlow = 1;
    qreal   m_brushSpacing = .25, m_brushAngle = 0, m_brushRoundness = 1, m_brushSmoothing = .1;
    int     m_brushBlend      = 0;
    bool    m_pencilAutoErase = false;
    qreal   m_redEyePupilSize = .5, m_redEyeDarken = .5;
    QRectF  m_redEyeRegion;
    bool    m_healingAligned = false, m_healingIgnoreAdjustments = false;
    int     m_healingSampleMode = 0, m_healingDiffusion = 5, m_healingMode = 0;
    bool    m_healingSourceValid = false, m_healingOffsetValid = false;
    bool    m_healingOffsetBeforeStrokeValid = false;
    QPointF m_healingSource, m_healingOffset, m_healingOffsetBeforeStroke;
    QImage  m_healingSample, m_healingTip;
    bool m_removeSampleAllLayers = false, m_removeAfterEachStroke = true, m_removeSubtract = false;
    bool m_removeBusy = false, m_removeStrokeSubtract = false;
    quint64                           m_removeGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_removeCancel;
    QRegion                           m_removeRegion;
    QPolygonF                         m_removePath;
    void                              beginRemove(const QPointF &_point, bool _alternate);
    void                              continueRemove(const QPointF &_point);
    void                              finishRemove();
    void                              stopRemovalJob();
    int     m_contentMoveMode = 0, m_contentMoveStructure = 4, m_contentMoveColor = 0;
    bool    m_contentMoveSampleAllLayers = false, m_contentMoveBusy = false;
    bool    m_contentMoveAwaitingImage = false;
    quint64 m_contentMoveGeneration    = 0;
    std::shared_ptr<std::atomic_bool> m_contentMoveCancel;
    int                               m_patchMode = 0, m_patchDiffusion = 5;
    bool                              m_patchDragging = false;
    QPoint                            m_patchOffset;
    QRegion                           m_patchTarget;
    QImage                            m_patchSource, m_patchPreview, m_regionPreviewSource;
    bool                              m_spotHealingSampleAllLayers = false;
    int                               m_spotHealingMode            = 0;
    QImage                            m_spotHealingSample, m_spotHealingTip;
    QRegion                           m_spotHealingExcluded;
    bool                              m_stampAligned     = true;
    int                               m_cloneSampleMode  = 0;
    bool                              m_cloneSourceValid = false, m_cloneOffsetValid = false;
    QPointF                           m_cloneSource, m_cloneOffset, m_patternOrigin;
    bool                              m_patternOriginValid = false;
    int                               m_stampPattern       = 0;
    QImage                            m_customStampPattern, m_stampSource;

    bool   m_smudgeSampleAllLayers = false, m_smudgeFingerPainting = false;
    QImage m_smudgePickup, m_smudgeMask, m_smudgeSample;

    int    m_replacementMode = 0, m_replacementSampling = 0, m_replacementLimits = 0;
    int    m_replacementTolerance = 30;
    bool   m_replacementAntialias = true;
    QColor m_replacementSample;

    qreal   m_mixerWet = .5, m_mixerLoad = .5, m_mixerMix = .5, m_mixerFlow = 1.;
    bool    m_mixerSampleAllLayers = false, m_mixerAutoLoad = true, m_mixerAutoClean = true;
    bool    m_mixerSolidOnly = false;
    QColor  m_mixerColor{Qt::black};
    QImage  m_mixerPaint, m_mixerPickup, m_mixerTip, m_mixerSample, m_mixerReservoirTip;
    QImage  m_mixerPickupBeforeStroke;
    qreal   m_mixerRemaining = 1., m_mixerRemainingBeforeStroke = 1.;
    QPointF m_mixerLastEnd;
    bool    m_mixerLastEndValid = false;

    quint64                           m_documentSession = 0;
    QFutureWatcher<RawImageResult>    m_rawWatcher;
    std::shared_ptr<std::atomic_bool> m_rawCancel;
    bool                              m_rawLoading = false;
    quint64                           m_rawSession = 0, m_rawState = 0;
    QUrl                              m_rawUrl;
    bool                              m_rawAsLayer = false;
    void                              finishRawLoad();
    bool adoptDecodedImage(const QImage &_image, const QUrl &_url, bool _asLayer);
    int  m_magicEraserTolerance  = 32;
    bool m_magicEraserContiguous = true, m_magicEraserAntialias = true;
    bool m_magicEraserSampleAllLayers = false, m_magicEraserBusy = false;
    std::shared_ptr<std::atomic_bool> m_magicEraserCancel;

    int  m_eyedropperSize = 1, m_eyedropperSource = 0;
    bool m_eyedropperRing = true;

    qreal     m_toolStrength   = .5;
    bool      m_spongeSaturate = false;
    int       m_retouchRange   = 1;
    bool      m_protectTones   = true;
    QString   m_text           = QStringLiteral("Votre texte");
    int       m_textSize       = 64;
    bool      m_filledShapes   = true;
    QColor    m_shapeFillColor;
    QColor    m_shapeStrokeColor;
    bool      m_shapeStrokeEnabled = true;
    qreal     m_shapeStrokeWidth   = 1;
    int       m_shapeStrokeStyle   = 0;
    qreal     m_shapeCornerRadius  = 0;
    bool      m_shapeAlignPixels   = true;
    int       m_shapeSides         = 5;
    qreal     m_shapeStarRatio     = .5;
    QPolygonF m_cropPoints, m_cropGesturePoints;
    QPointF   m_cropAnchor;
    int       m_cropHandle   = -2;
    bool      m_cropDragging = false, m_cropBusy = false;
    qreal     m_cropRatio = 0;
    QSize     m_cropOutputSize{0, 0};
    void      setCropRectangle(const QRectF &_rect);
    QRect     m_selection;
    QRegion   m_selectionRegion;
    QImage    m_selectionMask, m_lastSelectionMask;
    QRegion   m_selectionOutline, m_lastSelectionOutline, m_selectionMaskRegion;
    void      applySelectionCoverage(const SelectionCoverage::Result &_result);
    void      blendSelectionEdit(QImage &_image, const QImage &_before);
    void      translateSelectionCoverage(const Snapshot &_before, const QPoint &_offset);
    void      combineSoftSelection(const QRegion &_candidate, int _mode, const Snapshot &_before);
    QRegion   m_lastSelectionRegion;
    int       m_selectionShape = 0;
    QPolygonF m_selectionPath;
    bool      m_polygonLassoActive       = false;
    int       m_selectionMode            = 0;
    int       m_selectionTolerance       = 32;
    bool      m_selectionContiguous      = true;
    bool      m_selectionSampleAllLayers = false;
    bool      m_selectionEnhanceEdge     = true;
    QRegion   m_selectionRegionBeforeStroke;
    QRect     m_selectionBoundsBeforeStroke;
    int       m_selectionShapeBeforeStroke = 0;
    QPolygonF m_selectionPathBeforeStroke;
    bool      m_quickSelectionFirstDab = false;
    QVector<Snapshot> m_undo, m_redo;
    Snapshot          m_historyBrushSource;
    QString           m_historyBrushSourceLabel;
    QImage            m_historyStrokeSource;
    bool              prepareHistoryBrushStroke();
    void              renderHistoryBrushStroke();
    void              artHistoryDab(const QPointF &_point);
    void              renderArtHistoryStroke();
    int               m_artHistoryStyle = 0, m_artHistoryArea = 50, m_artHistoryTolerance = 0;
    quint32           m_artHistoryDabIndex = 0;
    quint64           m_stateId = 1, m_contentId = 1, m_savedId = 1, m_nextId = 2;
    int               m_revision          = 0;
    int               m_thumbnailRevision = 0;
    int               m_displayChannel    = 0;
    bool              m_maskOnlyView      = false;
    mutable QImage    m_channelDisplayCache;
    mutable qint64    m_channelDisplaySourceKey  = 0;
    mutable int       m_channelDisplayCacheIndex = 0;
    mutable QImage    m_composite;
    mutable bool      m_cacheDirty         = true;
    mutable bool      m_fullCompositeDirty = true;
    mutable QRect     m_dirtyRegion;
    bool              m_drawing = false, m_strokeChanged = false;
    bool              m_toolShift = false, m_toolAlternate = false, m_toolSpace = false;
    bool              m_selectionInitialAlternate = false, m_selectionMoving = false;
    bool              m_strokeConnect = false, m_moveCopy = false, m_moveDuplicatedLayer = false;
    QPointF           m_geometryPointer, m_paintLastEnd;
    QString           m_paintLastTool;
    int               m_paintLastLayer = -1;
    bool              m_paintLastMask  = false;
    quint64           m_paintLastState = 0, m_paintLastSession = 0;
    bool              m_selectionCombined = false;
    void              rememberSelection();
    void              restoreGestureSelection();
    void              combineGestureSelection();
    QPointF           constrainedToolPoint(const QPointF &_point, bool _diagonal) const;
    QRectF  toolRectangle(const QPointF &_anchor, const QPointF &_point, bool _center) const;
    bool    m_editingMask           = false;
    bool    m_previewingLayer       = false;
    bool    m_layerPreviewChanged   = false;
    bool    m_frequencyPreviewDraft = false;
    QPointF m_anchor, m_last;
    QPointF m_smoothedLast;
    qreal   m_distanceToNextDab = 0;
    QImage  m_strokeBase, m_strokeCoverage;
    QColor  m_strokeInkColor;
    QRectF  m_strokeBounds;
    mutable QCache<QString, QImage>    m_frequencyCache{128 * 1024};
    std::shared_ptr<ScratchImageCache> m_scratchCache;
    QFutureWatcher<QImage>             m_previewRenderWatcher;
    QVector<Layer>                     m_pendingPreviewLayers;
    QSize                              m_pendingPreviewSize;
    QSize                              m_pendingPreviewTargetSize;
    QSize                              m_previewTargetSize;
    QImage                             m_previewComposite;
    bool                               m_pendingPreviewDraft  = false;
    bool                               m_pendingPreviewRefine = false;
    bool                               m_runningPreviewDraft  = false;
    bool                               m_runningPreviewRefine = false;
    QSize                              m_runningPreviewTargetSize;
    quint64                            m_previewRequestGeneration = 0;
    quint64                            m_runningPreviewGeneration = 0;
    bool                               m_previewRenderPending     = false;
    std::atomic_bool                   m_previewRenderActive{false};
    mutable QMutex                     m_previewImageMutex;
    Snapshot                           m_beforeContentMove;
    Snapshot                           m_beforeStroke;
    Snapshot                           m_beforeLayerPreview;
    QImage                             m_beforeLayerPreviewComposite;
    bool                               m_layerPropertiesSession = false;
    Snapshot                           m_beforeLayerProperties;
    QVector<Snapshot>                  m_undoBeforeLayerProperties;
    QVector<Snapshot>                  m_redoBeforeLayerProperties;
    Snapshot                           snapshot(const QString &_label) const;
    void                               restore(const Snapshot &_state);

    bool                              m_frequencyRetouchBusy = false;
    std::shared_ptr<std::atomic_bool> m_frequencyRetouchCancel;

    void    commit(const Snapshot &_before, const QString &_label, bool _invalidateFull = true,
                   bool _notifyImage = true);
    void    trimHistory();
    void    beginContentMove(const QPointF &_point);
    void    finishContentMove(bool _cancel);
    void    invalidateContentMove(bool _restoreSelection = false);
    void    continueRegionDrag(const QPointF &_point, bool _sourceMode);
    void    beginPatch(const QPointF &_point);
    void    continuePatch(const QPointF &_point);
    void    finishPatch(bool _cancel);
    void    clearPatch();
    void    applyPatch();
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
    bool    validStorageBytes(qint64 _bytes);
    bool    validLayerStorage(int _width, int _height, int _extraColors = 0, int _extraMasks = 0,
                              int _bits = 0);
    bool    editable(bool _materialize = true);
    bool    fail(const QString &_message);
    void    edit(const QString &_label, const std::function<void()> &_operation);
    void    paintSegment(const QPointF &_from, const QPointF &_to);
    void    stampBrush(const QPointF &_point);
    void    renderBrushStroke();
    void    renderRetouchStroke();
    void    prepareSmudgeStroke(bool _alternate);
    void    correctRedEye();
    bool    prepareHealingStroke();
    void    healingDab(const QPointF &_point);
    QImage  sampleHealingLayers(int _sampleMode, bool _ignoreAdjustments);
    QImage  makeHealingTip() const;
    void    applyHealingDab(const QPointF &_point, const QImage &_sample, const QImage &_tip,
                            const QPoint &_sourceOffset, int _mode, int _diffusion,
                            bool _selectionBoundary = true);
    void    prepareSpotHealingStroke();
    void    spotHealingDab(const QPointF &_point);
    void    finishSpotHealingStroke();
    void    finishHealingStroke(bool _cancel);
    void    smudgeDab(const QPointF &_point);
    void    colorReplacementDab(const QPointF &_point);
    void    prepareMixerStroke();
    void    mixerDab(const QPointF &_point);
    void    finishMixerStroke(bool _cancel);
    void    reloadMixerPaint();
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
    void          applyMagicEraser(const QPoint &_seed);
    void          startPendingPreviewRender();
    static QImage renderPreviewSnapshot(QVector<Layer> _layers, const QSize &_size,
                                        const QSize &_targetSize, bool _draft,
                                        std::shared_ptr<ScratchImageCache> _scratch = {});
};
