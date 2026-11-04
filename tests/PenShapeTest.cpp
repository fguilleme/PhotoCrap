#include "Document.h"
#include "shapes/ShapeLayer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
void click(Document &_document, qreal _x, qreal _y)
{
    _document.beginPenGesture(_x, _y, 2);
    _document.endPenGesture();
}
QVariantMap layer(const Document &_document, int _index)
{
    for (const auto &entry : _document.layers())
        if (entry.toMap()["layerIndex"].toInt() == _index)
            return entry.toMap();
    return {};
}
void square(Document &_document)
{
    _document.setTool("pen");
    _document.setPenMode("shape");
    click(_document, 20, 20);
    click(_document, 100, 20);
    click(_document, 100, 100);
    click(_document, 20, 100);
    _document.closePenPath();
}
} // namespace
class PenShapeTest : public QObject
{
    Q_OBJECT
private slots:
    void retainedShapeAndStyles_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void retainedShapeAndStyles()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(160, 140, true, bits));
        d.setColor(Qt::red);
        square(d);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(layer(d, 1)["kind"].toString(), QString("shape"));
        QCOMPARE(d.penMode(), QString("shape"));
        QVERIFY(d.penPaths().last().toMap()["vectorMask"].toBool());
        QCOMPARE(d.layerImage(1).pixelColor(60, 60), QColor(Qt::red));
        QVERIFY(d.gpuLayerStack().valid);
        const auto format =
            bits == 16 ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied;
        QCOMPARE(d.layerImage(1).format(), format);
        const auto geometry = d.penPaths();
        const auto history  = d.historyIndex();
        d.setPenShapeStyle("fillColor", "#ff0000ff");
        QCOMPARE(d.layerImage(1).pixelColor(60, 60), QColor(Qt::blue));
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.penPaths(), geometry);
        d.undo();
        QCOMPARE(d.layerImage(1).pixelColor(60, 60), QColor(Qt::red));
        d.redo();
        QCOMPARE(d.layerImage(1).pixelColor(60, 60), QColor(Qt::blue));
        d.setPenShapeStyle("strokeEnabled", true);
        d.setPenShapeStyle("strokeColor", "#ff00ff00");
        d.setPenShapeStyle("strokeWidth", 6.);
        d.setPenShapeStyle("fillEnabled", false);
        QCOMPARE(d.layerImage(1).pixelColor(60, 60).alpha(), 0);
        QVERIFY(d.layerImage(1).pixelColor(60, 20).green() > 240);
        const auto outline = d.layerImage(1);
        d.setPenShapeStyle("strokeStyle", 1);
        QVERIFY(d.layerImage(1) != outline);
        d.setPenShapeStyle("strokeWidth", qQNaN());
        QCOMPARE(d.penShapeStyle()["strokeWidth"].toDouble(), 6.);
    }
    void convertEditablePathToShape_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void convertEditablePathToShape()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(240, 180, true, bits));
        d.setTool("pen");
        click(d, 30, 40);
        d.beginPenGesture(150, 40, 2);
        d.updatePenGesture(170, 65);
        d.endPenGesture();
        click(d, 150, 140);
        click(d, 30, 140);
        d.closePenPath();
        d.renamePenPath("Source");
        // Edit the ordinary path before conversion, including a curved handle.
        d.setTool("directSelection");
        d.beginPenGesture(150, 40, 2);
        d.endPenGesture();
        d.beginPenGesture(170, 65, 2);
        d.updatePenGesture(180, 70);
        d.endPenGesture();
        const auto source  = d.penPaths()[0].toMap();
        const auto history = d.historyIndex();
        d.setPenShapeStyle("fillColor", "#ffff9900");
        d.setPenShapeStyle("strokeColor", "#ffff209f");
        d.setPenShapeStyle("strokeWidth", 12.);
        QVERIFY(d.penPathToShape());
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.penPaths().size(), 2);
        auto savedSource      = source;
        auto unselectedPoints = savedSource["points"].toList();
        for (auto &entry : unselectedPoints) {
            auto point        = entry.toMap();
            point["selected"] = false;
            entry             = point;
        }
        savedSource["points"] = unselectedPoints;
        QCOMPARE(d.penPaths()[0].toMap(), savedSource);
        auto path = PhotoCrap::Shape::path(layer(d, 1)["parameters"].toMap());
        QCOMPARE(path.points[1].outgoing, QPointF(180, 70));
        QVERIFY(path.closed);
        QCOMPARE(d.penMode(), QString("shape"));
        QVERIFY(d.penShapeStyle()["strokeEnabled"].toBool());
        QVERIFY(d.layerImage(1).pixelColor(80, 140).blue() > 100);
        const auto converted = d.layerImage(1);
        // Editing the new vector mask must not change the saved source.
        d.beginPenGesture(150, 40, 2);
        d.endPenGesture();
        d.beginPenGesture(180, 70, 2);
        d.updatePenGesture(190, 80);
        d.endPenGesture();
        QVERIFY(d.layerImage(1) != converted);
        QCOMPARE(d.penPaths()[0].toMap(), savedSource);
        d.undo();
        QCOMPARE(d.layerImage(1), converted);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.penPaths()[0].toMap(), source);
        QCOMPARE(d.penMode(), QString("path"));
        d.redo();
        QCOMPARE(d.layerImage(1), converted);
        d.resizePenShape(180, 120);
        QVERIFY(qAbs(d.penShapeBounds().width() - 180) < 1e-6);
        QVERIFY(qAbs(d.penShapeBounds().height() - 120) < 1e-6);
        QCOMPARE(d.penPaths()[0].toMap(), savedSource);
        d.undo();
        QCOMPARE(d.layerImage(1), converted);
        d.setPenShapeStyle("strokeEnabled", false);
        // Selecting even the same colour turns a None stroke back on.
        d.setPenShapeStyle("strokeColor", "#ffff209f");
        QVERIFY(d.penShapeStyle()["strokeEnabled"].toBool());
        const auto painted = d.layerImage(1);
        d.setActivePenPath(-1);
        QCOMPARE(d.layerImage(1), painted);
        QTemporaryDir directory;
        const auto    url = QUrl::fromLocalFile(directory.filePath("converted.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layerImage(1), painted);
        QCOMPARE(loaded.penPaths().size(), 2);
        loaded.editPenShape();
        QVERIFY(loaded.penPaths()[loaded.activePenPath()].toMap()["vectorMask"].toBool());
    }
    void conversionRejectsEmptyPath()
    {
        Document d;
        QVERIFY(d.newDocument(120, 100, true));
        d.createPenPath("Empty");
        const auto history = d.historyIndex();
        QVERIFY(!d.penPathToShape());
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layers().size(), 1);
        d.setTool("pen");
        click(d, 20, 20);
        QVERIFY(!d.penPathToShape());
        QCOMPARE(d.layers().size(), 1);
    }
    void anchorsCancelAndCreationUndo()
    {
        Document d;
        d.newDocument(180, 150, true, 16);
        d.setTool("pen");
        d.setPenMode("shape");
        d.beginPenGesture(20, 20);
        d.updatePenGesture(30, 40);
        d.endPenGesture(true);
        QCOMPARE(d.layers().size(), 1);
        QVERIFY(d.penPaths().isEmpty());
        QCOMPARE(d.historyIndex(), 0);
        square(d);
        const auto original   = d.layerImage(1);
        const auto parameters = layer(d, 1)["parameters"];
        d.beginPenGesture(100, 20, 2, true);
        d.updatePenGesture(140, 40);
        QVERIFY(d.layerImage(1) != original);
        d.endPenGesture(true);
        QCOMPARE(d.layerImage(1), original);
        QCOMPARE(layer(d, 1)["parameters"], parameters);
        d.beginPenGesture(100, 20, 2, true);
        d.updatePenGesture(140, 40);
        d.endPenGesture();
        const auto edited = d.layerImage(1);
        QVERIFY(edited != original);
        d.undo();
        QCOMPARE(d.layerImage(1), original);
        d.redo();
        QCOMPARE(d.layerImage(1), edited);
        d.setActiveLayer(0);
        QCOMPARE(d.activePenPath(), -1);
        d.setActiveLayer(1);
        QVERIFY(d.activePenPath() >= 0);
        d.setTool("directSelection");
        d.beginPenGesture(140, 40, 2);
        d.endPenGesture();
        d.deletePenPoints();
        QVERIFY(d.layerImage(1) != edited);
        d.undo();
        QCOMPARE(d.layerImage(1), edited);
    }
    void projectAndIndependentDuplication()
    {
        Document d;
        d.newDocument(160, 140, true, 16);
        square(d);
        d.setPenShapeStyle("fillColor", "#ff6050e0");
        const auto    pixels = d.layerImage(1);
        QTemporaryDir directory;
        const auto    url = QUrl::fromLocalFile(directory.filePath("shape.pcr"));
        QVERIFY(d.saveProject(url));
        QFile file(url.toLocalFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(root["version"].toInt(), 11);
        QVERIFY(root["penPaths"].toArray().isEmpty());
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(layer(loaded, 1)["kind"].toString(), QString("shape"));
        QCOMPARE(loaded.layerImage(1), pixels);
        QCOMPARE(loaded.penMode(), QString("shape"));
        QVERIFY(loaded.activePenPath() >= 0);
        loaded.duplicateLayer();
        QCOMPARE(loaded.layers().size(), 3);
        loaded.setTool("pen");
        loaded.beginPenGesture(100, 20, 2, true);
        loaded.updatePenGesture(125, 35);
        loaded.endPenGesture();
        QCOMPARE(loaded.layerImage(1), pixels);
        QVERIFY(loaded.layerImage(2) != pixels);
        loaded.removeLayer();
        QCOMPARE(loaded.layers().size(), 2);
        loaded.undo();
        QCOMPARE(loaded.layers().size(), 3);
        loaded.rasterizeShapeLayer();
        QCOMPARE(layer(loaded, 2)["kind"].toString(), QString("pixel"));
        loaded.undo();
        QCOMPARE(layer(loaded, 2)["kind"].toString(), QString("shape"));
        loaded.createPenPath("Indépendant");
        QVERIFY(loaded.saveProject(url));
        Document again;
        QVERIFY(again.openFile(url));
        QVERIFY(again.activePenPath() >= 0);
        QCOMPARE(again.penPaths()[again.activePenPath()].toMap()["name"].toString(),
                 QString("Indépendant"));
        // Malformed geometry is rejected before replacing the current document.
        auto entries          = root["layers"].toArray();
        auto shape            = entries[1].toObject();
        auto params           = shape["parameters"].toObject();
        params["strokeWidth"] = -5;
        shape["parameters"]   = params;
        entries[1]            = shape;
        root["layers"]        = entries;
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(root).toJson());
        file.close();
        const auto before = again.layers();
        QVERIFY(!again.openFile(url));
        QCOMPARE(again.layers(), before);
    }
    void vectorMoveResizeDepthAndLock()
    {
        Document d;
        d.newDocument(160, 140, true);
        square(d);
        d.setTool("move");
        d.beginStroke(40, 40);
        d.continueStroke(50, 55);
        d.endStroke();
        auto params = layer(d, 1)["parameters"].toMap();
        QCOMPARE(PhotoCrap::Shape::path(params).points[0].position, QPointF(30, 35));
        QCOMPARE(d.layerImage(1).pixelColor(25, 25).alpha(), 0);
        d.undo();
        params = layer(d, 1)["parameters"].toMap();
        QCOMPARE(PhotoCrap::Shape::path(params).points[0].position, QPointF(20, 20));
        d.resizeImage(320, 280);
        params = layer(d, 1)["parameters"].toMap();
        QCOMPARE(PhotoCrap::Shape::path(params).points[0].position, QPointF(40, 40));
        QVERIFY(d.setChannelBits(16));
        QCOMPARE(d.layerImage(1), QImage(PhotoCrap::Shape::renderPlane(
                                      params, {320, 280}, QImage::Format_RGBA64_Premultiplied)));
        d.setLayerLocked(1, true);
        const auto before   = d.layerImage(1);
        const auto geometry = d.penPaths();
        d.setTool("pen");
        d.beginPenGesture(40, 40, 2, true);
        d.updatePenGesture(90, 90);
        d.endPenGesture();
        d.setPenShapeStyle("fillColor", "#ff00ff00");
        QCOMPARE(d.layerImage(1), before);
        QCOMPARE(d.penPaths(), geometry);
    }
    void curvesMasksAndPathModeHistory()
    {
        Document d;
        d.newDocument(180, 160, true, 16);
        d.setTool("pen");
        d.setPenMode("shape");
        d.setPenShapeStyle("fillColor", "#ffc03050");
        click(d, 20, 20);
        d.beginPenGesture(120, 20, 2);
        d.updatePenGesture(140, 60);
        d.endPenGesture();
        click(d, 110, 120);
        click(d, 20, 120);
        d.closePenPath();
        const auto geometry = PhotoCrap::Shape::path(layer(d, 1)["parameters"].toMap());
        const auto pixels   = d.layerImage(1);
        QVERIFY(pixels.pixelColor(60, 60).alpha() > 0);
        d.addLayerMask(false);
        d.setEditingMask(true);
        d.setTool("brush");
        d.setBrushSize(20);
        d.setBrushHardness(1.);
        d.setColor(Qt::black);
        d.beginStroke(60, 60);
        d.endStroke();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layerImage(1), pixels);
        QVERIFY(d.composite().pixelColor(60, 60).alpha() < 20);
        d.setActiveLayer(1);
        QVERIFY(PhotoCrap::Shape::path(layer(d, 1)["parameters"].toMap()) == geometry);
        d.duplicatePenPath();
        QCOMPARE(d.penMode(), QString("path"));
        d.undo();
        QCOMPARE(d.penMode(), QString("shape"));
        d.redo();
        QCOMPARE(d.penMode(), QString("path"));
        d.setActiveLayer(1);
        QVERIFY(d.penPaths()[d.activePenPath()].toMap()["vectorMask"].toBool());
    }
    void invalidTransformsProtectInactiveShapeGeometry()
    {
        Document d;
        d.newDocument(160, 140, true);
        d.setTool("pen");
        d.setPenMode("shape");
        click(d, 999990, 20);
        click(d, 999999, 20);
        click(d, 999999, 80);
        d.closePenPath();
        d.newPenPath();
        square(d);
        const auto layers  = d.layers();
        const auto size    = QSize(d.width(), d.height());
        const auto history = d.historyIndex();
        d.resizeImage(320, 280);
        QCOMPARE(QSize(d.width(), d.height()), size);
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.historyIndex(), history);
    }
    void smallShapeUsesOnlyTouchedTiles()
    {
        Document d;
        QVERIFY(d.newDocument(6000, 4000, true, 16));
        d.setTool("pen");
        d.setPenMode("shape");
        click(d, 100, 100);
        click(d, 140, 100);
        click(d, 140, 140);
        click(d, 100, 140);
        d.closePenPath();
        QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() <= 256LL * 256 * 8);
        const auto stack = d.gpuLayerStack();
        QVERIFY(stack.valid);
        QCOMPARE(stack.layers.last().image.region({110, 110, 1, 1}).pixelColor(0, 0),
                 QColor(Qt::black));
        QCOMPARE(stack.layers.last().image.region({5000, 3000, 1, 1}).pixelColor(0, 0).alpha(), 0);
    }
};
QTEST_MAIN(PenShapeTest)
#include "PenShapeTest.moc"
