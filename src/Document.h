#pragma once
#include <QObject>
#include <QColor>
#include <QImage>
#include <QRect>
#include <QUrl>
#include <QVariantList>
#include <QPainter>
#include <functional>

struct Layer {
    QString name;
    QImage image;
    bool visible = true;
    bool locked = false;
    qreal opacity = 1;
    int blend = 0;
};

class Document : public QObject {
    Q_OBJECT
    Q_PROPERTY(int width READ width NOTIFY documentChanged)
    Q_PROPERTY(int height READ height NOTIFY documentChanged)
    Q_PROPERTY(QString name READ name NOTIFY documentChanged)
    Q_PROPERTY(QUrl projectUrl READ projectUrl NOTIFY documentChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY historyChanged)
    Q_PROPERTY(QVariantList layers READ layers NOTIFY layersChanged)
    Q_PROPERTY(int activeLayer READ activeLayer WRITE setActiveLayer NOTIFY layersChanged)
    Q_PROPERTY(int revision READ revision NOTIFY imageChanged)
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY settingsChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY settingsChanged)
    Q_PROPERTY(qreal brushOpacity READ brushOpacity WRITE setBrushOpacity NOTIFY settingsChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY settingsChanged)
    Q_PROPERTY(int textSize READ textSize WRITE setTextSize NOTIFY settingsChanged)
    Q_PROPERTY(bool filledShapes READ filledShapes WRITE setFilledShapes NOTIFY settingsChanged)
    Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyIndex READ historyIndex NOTIFY historyChanged)
public:
    explicit Document(QObject *parent = nullptr);
    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    QString name() const { return m_name; }
    QUrl projectUrl() const { return m_projectUrl; }
    bool modified() const { return m_stateId != m_savedId; }
    QVariantList layers() const;
    int activeLayer() const { return m_active; }
    int revision() const { return m_revision; }
    QString tool() const { return m_tool; }
    QColor color() const { return m_color; }
    QColor secondaryColor() const { return m_secondary; }
    qreal brushSize() const { return m_brushSize; }
    qreal brushOpacity() const { return m_brushOpacity; }
    QString text() const { return m_text; }
    int textSize() const { return m_textSize; }
    bool filledShapes() const { return m_filledShapes; }
    QRect selection() const { return m_selection; }
    bool hasSelection() const { return !m_selection.isEmpty(); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QStringList history() const;
    int historyIndex() const { return int(m_undo.size()); }
    const QImage &composite() const;
    QImage layerImage(int index) const;
    void setActiveLayer(int index);
    void setTool(const QString &value);
    void setColor(const QColor &value);
    void setSecondaryColor(const QColor &value);
    void setBrushSize(qreal value);
    void setBrushOpacity(qreal value);
    void setText(const QString &value);
    void setTextSize(int value);
    void setFilledShapes(bool value);

    Q_INVOKABLE bool newDocument(int width, int height, bool transparent = false);
    Q_INVOKABLE bool openFile(const QUrl &url);
    Q_INVOKABLE bool importLayer(const QUrl &url);
    Q_INVOKABLE bool saveProject(const QUrl &url);
    Q_INVOKABLE bool exportImage(const QUrl &url);
    Q_INVOKABLE void addLayer();
    Q_INVOKABLE void duplicateLayer();
    Q_INVOKABLE void removeLayer();
    Q_INVOKABLE void moveLayer(int direction);
    Q_INVOKABLE void mergeDown();
    Q_INVOKABLE void renameLayer(int index, const QString &name);
    Q_INVOKABLE void setLayerVisible(int index, bool visible);
    Q_INVOKABLE void setLayerLocked(int index, bool locked);
    Q_INVOKABLE void setLayerOpacity(int index, qreal opacity);
    Q_INVOKABLE void setLayerBlend(int index, int blend);
    Q_INVOKABLE void beginStroke(qreal x, qreal y);
    Q_INVOKABLE void continueStroke(qreal x, qreal y);
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void cancelStroke();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselect();
    Q_INVOKABLE void clearSelected();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void cropToSelection();
    Q_INVOKABLE void resizeImage(int width, int height);
    Q_INVOKABLE void transformImage(const QString &operation);
    Q_INVOKABLE void applyFilter(const QString &filter, qreal amount = 0);
    Q_INVOKABLE void createDemo();
signals:
    void documentChanged();
    void layersChanged();
    void imageChanged();
    void settingsChanged();
    void selectionChanged();
    void historyChanged();
    void errorOccurred(const QString &message);
    void statusMessage(const QString &message);
private:
    struct Snapshot { QVector<Layer> layers; QSize size; int active; QRect selection; quint64 id; QString label; };
    QSize m_size{1280, 800};
    QVector<Layer> m_layers;
    int m_active = 0;
    QString m_name = QStringLiteral("Sans titre");
    QUrl m_projectUrl;
    QString m_tool = QStringLiteral("brush");
    QColor m_color{"#b1f5cb"}, m_secondary{"#293b50"};
    qreal m_brushSize = 24, m_brushOpacity = 1;
    QString m_text = QStringLiteral("Votre texte");
    int m_textSize = 64;
    bool m_filledShapes = true;
    QRect m_selection;
    QVector<Snapshot> m_undo, m_redo;
    quint64 m_stateId = 1, m_savedId = 1, m_nextId = 2;
    int m_revision = 0;
    mutable QImage m_composite;
    mutable bool m_cacheDirty = true;
    bool m_drawing = false, m_strokeChanged = false;
    QPointF m_anchor, m_last;
    Snapshot m_beforeStroke;
    Snapshot snapshot(const QString &label) const;
    void restore(const Snapshot &state);
    void commit(const Snapshot &before, const QString &label);
    void trimHistory();
    void changed(bool metadata = true);
    void resetHistory(bool clean);
    bool validSize(int width, int height, int layerCount = 1);
    bool editable();
    bool fail(const QString &message);
    void edit(const QString &label, const std::function<void()> &operation);
    void paintSegment(const QPointF &from, const QPointF &to);
    void renderShape(const QPointF &to);
    void floodFill(QPoint point);
    QImage readImage(const QString &path);
    bool loadProject(const QString &path);
    static QPainter::CompositionMode blendMode(int mode);
};
