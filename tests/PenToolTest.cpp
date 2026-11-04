#include "Document.h"
#include "tools/PenPath.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

namespace
{
QVariantMap node(const Document &_document, int _index, int _path = 0)
{
    return _document.penPaths()[_path].toMap()["points"].toList()[_index].toMap();
}
QPointF position(const Document &_document, int _index, int _path = 0)
{
    const auto p = node(_document, _index, _path);
    return {p["x"].toReal(), p["y"].toReal()};
}
void click(Document &_document, qreal _x, qreal _y)
{
    _document.beginPenGesture(_x, _y, 2);
    _document.endPenGesture();
}
QPointF cubic(const PenPoint &_a, const PenPoint &_b, qreal _t)
{
    const qreal u = 1 - _t;
    return _a.position * (u * u * u) + _a.outgoing * (3 * u * u * _t) +
           _b.incoming * (3 * u * _t * _t) + _b.position * (_t * _t * _t);
}
} // namespace
class PenToolTest : public QObject
{
    Q_OBJECT
private slots:
    void panelPathLifecycleAndEmptyPersistence()
    {
        Document d;
        QVERIFY(d.newDocument(200, 150, true, 16));
        const auto pixels  = d.composite().cacheKey();
        const auto history = d.historyIndex();
        d.createPenPath("Contour");
        QCOMPARE(d.penPaths().size(), 1);
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(!d.penDrawing());
        d.setTool("pen");
        click(d, 20, 20);
        click(d, 80, 20);
        click(d, 80, 90);
        d.closePenPath();
        QCOMPARE(d.penPaths().size(), 1);
        QCOMPARE(d.penPaths()[0].toMap()["points"].toList().size(), 3);
        d.duplicatePenPath();
        QCOMPARE(d.activePenPath(), 1);
        QCOMPARE(d.penPaths()[1].toMap()["points"], d.penPaths()[0].toMap()["points"]);
        d.renamePenPath("Copie");
        d.movePenPath(0);
        QCOMPARE(d.activePenPath(), 0);
        QCOMPARE(d.penPaths()[0].toMap()["name"].toString(), QString("Copie"));
        d.undo();
        QCOMPARE(d.activePenPath(), 1);
        d.redo();
        QCOMPARE(d.activePenPath(), 0);
        d.createPenPath("Vide");
        const auto    retained = d.penPaths();
        QTemporaryDir directory;
        const auto    url = QUrl::fromLocalFile(directory.filePath("paths.pcr"));
        QVERIFY(d.saveProject(url));
        QFile saved(url.toLocalFile());
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto json = QJsonDocument::fromJson(saved.readAll()).object();
        QCOMPARE(json["version"].toInt(), 10);
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.penPaths(), retained);
        QCOMPARE(loaded.activePenPath(), d.activePenPath());
        d.setActivePenPath(-1);
        QCOMPARE(d.penPaths().size(), 3);
        QCOMPARE(d.composite().cacheKey(), pixels);
        d.setActivePenPath(2);
        d.deletePenPath();
        d.undo();
        QCOMPARE(d.penPaths().size(), 3);
        QVector<PenPath> paths;
        QVERIFY(PenGeometry::decode(PenGeometry::encode({PenPath{"Empty", {}, false}}), paths));
        QVERIFY(
            !PenGeometry::decode(PenGeometry::encode({PenPath{"Empty", {}, false}}), paths, false));
        QVERIFY(PenGeometry::transform(paths, QTransform::fromScale(2, 2)));
    }
    void selectionArrowsAndSegmentDragging()
    {
        Document d;
        d.newDocument(200, 150, true);
        d.setTool("pen");
        click(d, 20, 20);
        click(d, 80, 20);
        click(d, 80, 90);
        d.closePenPath();
        d.setTool("pathSelection");
        QCOMPARE(d.tool(), QString("pathSelection"));
        const auto original = d.penPaths();
        d.beginPenGesture(50, 20, 2);
        d.updatePenGesture(60, 30);
        d.endPenGesture();
        QCOMPARE(position(d, 0), QPointF(30, 30));
        QCOMPARE(position(d, 1), QPointF(90, 30));
        QCOMPARE(position(d, 2), QPointF(90, 100));
        d.undo();
        QCOMPARE(d.penPaths(), original);
        d.setTool("directSelection");
        d.beginPenGesture(50, 20, 2);
        d.updatePenGesture(60, 30);
        d.endPenGesture();
        QCOMPARE(position(d, 0), QPointF(30, 30));
        QCOMPARE(position(d, 1), QPointF(90, 30));
        QCOMPARE(position(d, 2), QPointF(80, 90));
        d.undo();
        const auto history = d.historyIndex();
        d.beginPenGesture(10, 10, 2);
        d.updatePenGesture(30, 30);
        QCOMPARE(d.penSelectionRect(), QRectF(10, 10, 20, 20));
        QVERIFY(node(d, 0)["selected"].toBool());
        QVERIFY(!node(d, 1)["selected"].toBool());
        d.endPenGesture();
        QVERIFY(d.penSelectionRect().isEmpty());
        QCOMPARE(d.historyIndex(), history);
        d.nudgePenPoints(1, 0);
        QCOMPARE(position(d, 0), QPointF(21, 20));
        QCOMPARE(position(d, 1), QPointF(80, 20));
        d.setActivePenPath(-1);
        d.setTool("pathSelection");
        d.beginPenGesture(80, 60, 2);
        d.endPenGesture();
        QCOMPARE(d.activePenPath(), 0);
        QCOMPARE(d.penPaths().size(), 1);
        d.beginPenGesture(190, 140, 2);
        d.endPenGesture();
        QCOMPARE(d.activePenPath(), -1);
        d.setActivePenPath(0);
        d.beginPenGesture(65, 45, 2);
        d.endPenGesture();
        QCOMPARE(d.activePenPath(), 0);
        d.setTool("directSelection");
        d.beginPenGesture(65, 45, 2, false, true);
        d.endPenGesture();
        QVERIFY(node(d, 0)["selected"].toBool());
        QVERIFY(node(d, 1)["selected"].toBool());
        QVERIFY(node(d, 2)["selected"].toBool());
        d.setTool("pen");
        d.beginPenGesture(190, 140, 2, true);
        d.endPenGesture();
        QCOMPARE(d.activePenPath(), -1);
    }
    void altEndpointPreservesIncomingCurve()
    {
        Document d;
        d.newDocument(200, 150, true);
        d.setTool("pen");
        click(d, 20, 20);
        d.beginPenGesture(80, 40, 2);
        d.updatePenGesture(100, 50);
        d.endPenGesture();
        const auto incoming = QPointF(node(d, 1)["inX"].toReal(), node(d, 1)["inY"].toReal());
        d.beginPenGesture(80, 40, 2, false, true);
        d.endPenGesture();
        QCOMPARE(QPointF(node(d, 1)["inX"].toReal(), node(d, 1)["inY"].toReal()), incoming);
        QCOMPARE(node(d, 1)["outX"].toReal(), 80.);
        d.beginPenGesture(80, 40, 2, false, true);
        d.updatePenGesture(110, 15, false, true);
        d.endPenGesture();
        QCOMPARE(QPointF(node(d, 1)["inX"].toReal(), node(d, 1)["inY"].toReal()), incoming);
        QCOMPARE(node(d, 1)["outX"].toReal(), 110.);
        QVERIFY(!node(d, 1)["smooth"].toBool());
        QVERIFY(d.penDrawing());
    }
    void resumeEndpointDragAndCancel()
    {
        Document d;
        d.newDocument(200, 150, true);
        d.setTool("pen");
        click(d, 20, 20);
        click(d, 80, 20);
        d.finishPenPath();
        const auto original = d.penPaths();
        d.beginPenGesture(80, 20, 2);
        d.updatePenGesture(100, 50);
        d.endPenGesture();
        QCOMPARE(node(d, 1)["outX"].toReal(), 100.);
        QCOMPARE(node(d, 1)["outY"].toReal(), 50.);
        QCOMPARE(node(d, 1)["inX"].toReal(), 80.);
        QVERIFY(d.penDrawing());
        d.undo();
        QCOMPARE(d.penPaths(), original);
        d.finishPenPath();
        d.beginPenGesture(20, 20, 2);
        d.updatePenGesture(5, 40);
        d.endPenGesture(true);
        QCOMPARE(d.penPaths(), original);
        QVERIFY(!d.penDrawing());
    }
    void smallPathFillOnlyAllocatesTouchedTiles()
    {
        Document d;
        QVERIFY(d.newDocument(6000, 4000, true, 16));
        d.setTool("pen");
        click(d, 100, 100);
        click(d, 120, 100);
        click(d, 120, 120);
        click(d, 100, 120);
        d.closePenPath();
        for (bool mask : {false, true}) {
            if (mask)
                d.addLayerMask(false);
            const auto heapBefore = d.memoryUsage()["tileHeapBytes"].toLongLong();
            d.setColor(Qt::red);
            d.fillPenPath();
            QVERIFY(d.memoryUsage()["tileHeapBytes"].toLongLong() - heapBefore <= 256LL * 256 * 8);
            if (!mask) {
                const auto patch =
                    d.gpuLayerStack().layers.first().image.region({100, 100, 20, 20});
                QCOMPARE(patch.pixelColor(10, 10), QColor(Qt::red));
                d.undo();
                QCOMPARE(d.memoryUsage()["layerBytes"].toLongLong(), qint64(0));
                d.redo();
                QCOMPARE(d.gpuLayerStack().layers.first().image.region({100, 100, 20, 20}), patch);
            } else {
                // Inspect the native mask without expanding it to document size.
                const auto bytes = d.memoryUsage()["layerBytes"].toLongLong();
                QVERIFY(bytes <= 256LL * 256 * 9);
                d.undo();
                QVERIFY(d.memoryUsage()["layerBytes"].toLongLong() < bytes);
                d.redo();
                QCOMPARE(d.memoryUsage()["layerBytes"].toLongLong(), bytes);
            }
        }
    }
    void regionalFillKeepsCurvesSelectionAndNativeDepth()
    {
        QCryptographicHash pixels(QCryptographicHash::Sha256);
        for (int bits : {8, 16})
            for (bool mask : {false, true}) {
                Document d;
                QVERIFY(d.newDocument(523, 531, false, bits));
                if (mask)
                    d.addLayerMask(false);
                d.setTool("ellipticalSelect");
                d.beginStroke(249, 247);
                d.continueStroke(273, 273);
                d.endStroke();
                d.modifySelection("feather", 3);
                d.setTool("pen");
                click(d, 248.25, 245.5);
                d.beginPenGesture(274.5, 249.25, 2);
                d.updatePenGesture(279.5, 243.25);
                d.endPenGesture();
                click(d, 267.75, 278.25);
                click(d, 246.5, 269.75);
                d.closePenPath();
                const auto before  = mask ? d.maskImage(0) : d.layerImage(0);
                const auto history = d.historyIndex();
                d.setColor(QColor::fromRgba64(14321, 32123, 54321, 41234));
                d.setBrushOpacity(.63);
                PenPath referencePath;
                referencePath.closed = true;
                for (int i = 0; i < 4; ++i) {
                    const auto p = node(d, i);
                    referencePath.points.append({position(d, i),
                                                 {p["inX"].toReal(), p["inY"].toReal()},
                                                 {p["outX"].toReal(), p["outY"].toReal()},
                                                 p["smooth"].toBool()});
                }
                const auto path      = referencePath.painterPath();
                const auto bounds    = path.boundingRect()
                                           .toAlignedRect()
                                           .adjusted(-1, -1, 1, 1)
                                           .intersected(before.rect())
                                           .intersected(d.selectionRegion().boundingRect());
                QImage     reference = before.copy();
                {
                    QPainter painter(&reference);
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setClipRect(bounds);
                    painter.setClipRegion(d.selectionRegion(), Qt::IntersectClip);
                    painter.setOpacity(d.brushOpacity());
                    painter.fillPath(path, d.color());
                }
                SelectionCoverage::blend(reference, before, d.selectionMask(), bounds);
                d.fillPenPath();
                const auto after = mask ? d.maskImage(0) : d.layerImage(0);
                QVERIFY(after != before);
                QCOMPARE(after.format(), before.format());
                QCOMPARE(d.historyIndex(), history + 1);
                for (const QRect &outside : QRegion(after.rect()) - QRegion(235, 230, 55, 60))
                    QCOMPARE(after.copy(outside), before.copy(outside));
                int maximum = 0, different = 0;
                for (int y = 0; y < after.height(); ++y)
                    for (int x = 0; x < after.width(); ++x) {
                        const auto a = after.pixelColor(x, y).rgba64();
                        const auto b = reference.pixelColor(x, y).rgba64();
                        const int  delta =
                            qMax(qMax(qAbs(a.red() - b.red()), qAbs(a.green() - b.green())),
                                 qMax(qAbs(a.blue() - b.blue()), qAbs(a.alpha() - b.alpha())));
                        maximum = qMax(maximum, delta);
                        different += delta != 0;
                    }
                // Device-local curve clipping may shift one AA coverage unit.
                QVERIFY(maximum <= (mask ? 0 : 257));
                qInfo() << "Pen fill full-painter difference" << bits << mask << different
                        << maximum;
                const QImage hashed =
                    mask ? after.convertToFormat(QImage::Format_ARGB32_Premultiplied) : after;
                pixels.addData(QByteArrayView(reinterpret_cast<const char *>(hashed.constBits()),
                                              hashed.sizeInBytes()));
                d.undo();
                QCOMPARE(mask ? d.maskImage(0) : d.layerImage(0), before);
                d.redo();
                QCOMPARE(mask ? d.maskImage(0) : d.layerImage(0), after);
            }
        qInfo() << "Pen fill pixels, native colour and masks" << pixels.result().toHex();
    }
    void anchorsHandlesAndHistory()
    {
        Document d;
        d.newDocument(100, 100, true);
        d.setTool("pen");
        const auto pixels   = d.composite();
        const auto cacheKey = pixels.cacheKey();
        QSignalSpy images(&d, &Document::imageChanged), thumbs(&d, &Document::thumbnailsChanged);
        click(d, 10, 10);
        d.beginPenGesture(60, 40, 2);
        d.updatePenGesture(80, 40);
        d.endPenGesture();
        QCOMPARE(d.historyIndex(), 2);
        QVERIFY(d.modified());
        QVERIFY(d.penDrawing());
        QCOMPARE(node(d, 1)["inX"].toReal(), 40.);
        QCOMPARE(node(d, 1)["outX"].toReal(), 80.);
        QCOMPARE(d.penCursorAt(10, 10, 2, false, false), QString("penClose"));
        d.beginPenGesture(60, 40, 2, true);
        d.updatePenGesture(65, 50);
        d.endPenGesture();
        QCOMPARE(position(d, 1), QPointF(65, 50));
        QCOMPARE(node(d, 1)["outX"].toReal(), 85.);
        d.undo();
        QCOMPARE(position(d, 1), QPointF(60, 40));
        d.redo();
        QCOMPARE(position(d, 1), QPointF(65, 50));
        QCOMPARE(images.size(), 0);
        QCOMPARE(thumbs.size(), 0);
        QCOMPARE(d.composite().cacheKey(), cacheKey);
        d.finishPenPath();
        d.beginPenGesture(65, 50, 2, false, true);
        d.endPenGesture();
        QCOMPARE(node(d, 1)["inX"].toReal(), 65.);
        QCOMPARE(node(d, 1)["outX"].toReal(), 65.);
        d.beginPenGesture(65, 50, 2, false, true);
        d.updatePenGesture(80, 50, false, true);
        d.endPenGesture();
        QVERIFY(node(d, 1)["smooth"].toBool());
        QCOMPARE(node(d, 1)["inX"].toReal(), 50.);
        d.beginPenGesture(80, 50, 2, false, true);
        d.updatePenGesture(85, 60, false, true);
        d.endPenGesture();
        QCOMPARE(node(d, 1)["inX"].toReal(), 50.);
        QVERIFY(!node(d, 1)["smooth"].toBool());
        const auto before  = d.penPaths();
        const auto history = d.historyIndex();
        d.beginPenGesture(65, 50, 2, true);
        d.updatePenGesture(45, 30);
        d.endPenGesture(true);
        QCOMPARE(d.penPaths(), before);
        QCOMPARE(d.historyIndex(), history);
    }
    void closeResumeConstrainAndDelete()
    {
        Document d;
        d.newDocument(120, 100, true);
        d.setTool("pen");
        click(d, 10, 10);
        d.beginPenGesture(45, 42, 2, false, false, true);
        d.endPenGesture();
        QVERIFY(qAbs(position(d, 1).x() - position(d, 1).y()) < .001);
        d.finishPenPath();
        QVERIFY(!d.penDrawing());
        d.beginPenGesture(position(d, 1).x(), position(d, 1).y(), 2);
        d.endPenGesture();
        QVERIFY(d.penDrawing());
        click(d, 80, 70);
        click(d, 10, 10);
        QVERIFY(d.penPaths()[0].toMap()["closed"].toBool());
        QVERIFY(!d.penDrawing());
        d.undo();
        QVERIFY(!d.penPaths()[0].toMap()["closed"].toBool());
        d.finishPenPath();
        const auto last = position(d, 2);
        d.beginPenGesture(10, 10, 2);
        d.endPenGesture();
        QVERIFY(d.penDrawing());
        QCOMPARE(position(d, 0), last);
        d.deletePenPoints();
        QCOMPARE(d.penPaths()[0].toMap()["points"].toList().size(), 2);
        d.newPenPath();
        click(d, 5, 90);
        QCOMPARE(d.penPaths().size(), 2);
        d.deletePenPath();
        QCOMPARE(d.penPaths().size(), 1);
        d.undo();
        QCOMPARE(d.penPaths().size(), 2);
    }
    void subdivisionPreservesBezier()
    {
        PenPath path;
        path.name   = "Curve";
        path.points = {{{0, 0}, {0, 0}, {20, 80}, true}, {{100, 0}, {80, -60}, {100, 0}, true}};
        const auto original = path;
        QCOMPARE(PenGeometry::splitSegment(path, 0, .37), 1);
        for (int i = 0; i <= 100; ++i) {
            const qreal   t      = qreal(i) / 100;
            const QPointF result = t <= .37
                                       ? cubic(path.points[0], path.points[1], t / .37)
                                       : cubic(path.points[1], path.points[2], (t - .37) / .63);
            QVERIFY(QLineF(result, cubic(original.points[0], original.points[1], t)).length() <
                    1e-8);
        }
        const auto target = cubic(original.points[0], original.points[1], .63);
        const auto hit    = PenGeometry::nearestSegment(original, target, 2);
        QCOMPARE(hit.index, 0);
        QVERIFY(qAbs(hit.t - .63) < .0001);
        Document d;
        d.newDocument(100, 100, true);
        d.setTool("pen");
        click(d, 10, 10);
        click(d, 90, 10);
        d.finishPenPath();
        QCOMPARE(d.penCursorAt(50, 10, 2, false, false), QString("penAdd"));
        click(d, 50, 10);
        QCOMPARE(d.penPaths()[0].toMap()["points"].toList().size(), 3);
        QVERIFY(QLineF(position(d, 1), QPointF(50, 10)).length() < .01);
        click(d, 50, 10);
        QCOMPARE(d.penPaths()[0].toMap()["points"].toList().size(), 2);
        d.setPenAutoAddDelete(false);
        click(d, 50, 10);
        QCOMPARE(d.penPaths().size(), 2);
    }
    void releasedAltKeepsBrokenTangents()
    {
        Document d;
        d.newDocument(100, 100, true);
        d.setTool("pen");
        d.beginPenGesture(30, 30);
        d.updatePenGesture(50, 30);
        d.updatePenGesture(50, 40, false, true);
        const qreal incoming = node(d, 0)["inX"].toReal();
        d.updatePenGesture(60, 50, false, false);
        d.endPenGesture();
        QCOMPARE(node(d, 0)["inX"].toReal(), incoming);
        QVERIFY(!node(d, 0)["smooth"].toBool());
    }
    void repositionAndMultiPointSelection()
    {
        Document d;
        d.newDocument(120, 100, true);
        d.setTool("pen");
        d.beginPenGesture(20, 20);
        d.updatePenGesture(40, 20);
        d.updatePenGesture(40, 20, false, false, true);
        QCOMPARE(position(d, 0), QPointF(20, 20));
        d.updatePenGesture(50, 30, false, false, true);
        d.endPenGesture();
        QCOMPARE(position(d, 0), QPointF(30, 30));
        QCOMPARE(node(d, 0)["outX"].toReal(), 50.);
        click(d, 90, 60);
        d.beginPenGesture(30, 30, 2, true, false, true);
        d.endPenGesture();
        QVERIFY(node(d, 0)["selected"].toBool());
        QVERIFY(node(d, 1)["selected"].toBool());
        d.nudgePenPoints(1, 0);
        QCOMPARE(position(d, 0), QPointF(31, 30));
        QCOMPARE(position(d, 1), QPointF(91, 60));
        d.undo();
        QCOMPARE(position(d, 0), QPointF(30, 30));
        d.beginPenGesture(30, 30, 2, true, false, true);
        d.endPenGesture();
        QVERIFY(!node(d, 0)["selected"].toBool());
        d.newPenPath();
        click(d, 15, 80);
        click(d, 60, 80);
        d.undo();
        QVERIFY(d.penDrawing());
        click(d, 70, 70);
        QCOMPARE(d.penPaths().size(), 2);
        QCOMPARE(position(d, 1, 1), QPointF(70, 70));
    }
    void projectedCurvesAreMappedAsPaths()
    {
        PenPath curve;
        curve.name   = "Curve";
        curve.points = {{{10, 10}, {10, 10}, {20, 80}, true},
                        {{100, 10}, {80, 80}, {100, 10}, true}};
        QVector<PenPath> paths{curve};
        QTransform       perspective(1, 0, .002, 0, 1, .001, 0, 0, 1);
        const auto       expected = perspective.map(curve.painterPath());
        QVERIFY(PenGeometry::transform(paths, perspective));
        QVERIFY(qAbs(paths[0].painterPath().length() - expected.length()) < .5);
        const auto before = paths;
        QVERIFY(!PenGeometry::transform(paths, QTransform::fromScale(1e9, 1e9)));
        QVERIFY(paths == before);
    }
    void projectPersistenceAndValidation()
    {
        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("paths.pcr"));
        Document      d;
        d.newDocument(80, 80, true);
        d.setTool("pen");
        click(d, 8, 9);
        click(d, 60, 20);
        d.renamePenPath("Contour du visage");
        d.newPenPath();
        click(d, 20, 70);
        QVERIFY(d.saveProject(file));
        QVERIFY(!d.modified());
        Document loaded;
        QVERIFY(loaded.openFile(file));
        QCOMPARE(loaded.penPaths().size(), 2);
        QCOMPARE(loaded.penPaths()[0].toMap()["name"].toString(),
                 QString::fromUtf8("Contour du visage"));
        QCOMPARE(loaded.activePenPath(), 1);
        QCOMPARE(position(loaded, 0, 1), QPointF(20, 70));
        QFile io(file.toLocalFile());
        QVERIFY(io.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(io.readAll()).object();
        io.close();
        QCOMPARE(root["version"].toInt(), 9);
        auto paths        = root["penPaths"].toArray();
        auto invalid      = paths[0].toObject();
        invalid["points"] = QJsonArray{QJsonArray{1, 2}};
        paths[0]          = invalid;
        root["penPaths"]  = paths;
        QVERIFY(io.open(QIODevice::WriteOnly));
        io.write(QJsonDocument(root).toJson());
        io.close();
        const auto before = loaded.penPaths();
        QVERIFY(!loaded.openFile(file));
        QCOMPARE(loaded.penPaths(), before);
        root["version"] = 7;
        root.remove("penPaths");
        root.remove("activePenPath");
        QVERIFY(io.open(QIODevice::WriteOnly));
        io.write(QJsonDocument(root).toJson());
        io.close();
        QVERIFY(loaded.openFile(file));
        QVERIFY(loaded.penPaths().isEmpty());
    }
    void selectionFillAndStroke_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void selectionFillAndStroke()
    {
        QFETCH(int, bits);
        Document d;
        d.newDocument(100, 100, true, bits);
        d.setTool("pen");
        click(d, 20, 20);
        click(d, 80, 20);
        click(d, 80, 80);
        click(d, 20, 80);
        d.closePenPath();
        const auto historyBeforeSelection = d.historyIndex();
        d.setBrushOpacity(0);
        d.setColor(Qt::transparent);
        d.penPathToSelection();
        QVERIFY(d.selectionContains(40, 40));
        QVERIFY(!d.selectionContains(5, 5));
        QCOMPARE(d.activePenPath(), -1);
        QCOMPARE(d.penPaths().size(), 1);
        QCOMPARE(position(d, 0), QPointF(20, 20));
        QCOMPARE(d.historyIndex(), historyBeforeSelection + 1);
        d.setActivePenPath(0);
        QCOMPARE(position(d, 0), QPointF(20, 20));
        QVERIFY(d.selectionContains(40, 40));
        d.setBrushOpacity(1);
        d.setColor(QColor::fromRgbF(.21345, .4, .7));
        d.fillPenPath();
        QCOMPARE(d.composite().pixelColor(5, 5).alpha(), 0);
        QCOMPARE(d.composite().pixelColor(40, 40).alpha(), 255);
        QCOMPARE(d.channelBits(), bits);
        if (bits == 16) {
            const auto native = d.layerImage(0);
            QCOMPARE(native.format(), QImage::Format_RGBA64_Premultiplied);
            QVERIFY(reinterpret_cast<const QRgba64 *>(native.constScanLine(40))[40].red() % 257 !=
                    0);
        }
        d.undo();
        QCOMPARE(d.composite().pixelColor(40, 40).alpha(), 0);
        d.deselect();
        d.setBrushSize(4);
        d.setBrushOpacity(1);
        d.strokePenPath();
        QCOMPARE(d.tool(), QString("pen"));
        QVERIFY(d.composite().pixelColor(50, 20).alpha() > 0);
        QCOMPARE(d.composite().pixelColor(50, 50).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite().pixelColor(50, 20).alpha(), 0);
        d.setLayerLocked(0, true);
        const auto history = d.historyIndex();
        d.fillPenPath();
        QCOMPARE(d.historyIndex(), history);
        d.setLayerLocked(0, false);
        d.addLayerMask(false);
        d.setColor(Qt::white);
        d.fillPenPath();
        QCOMPARE(d.maskImage(0).pixelColor(40, 40).red(), 255);
        QCOMPARE(d.maskImage(0).pixelColor(5, 5).red(), 0);
    }
    void transformsAndMalformedCoordinates()
    {
        Document d;
        d.newDocument(100, 80, true);
        d.setTool("pen");
        click(d, 20, 30);
        click(d, 70, 40);
        d.finishPenPath();
        d.resizeImage(200, 160);
        QCOMPARE(position(d, 0), QPointF(40, 60));
        d.undo();
        QCOMPARE(position(d, 0), QPointF(20, 30));
        d.transformImage("rotateRight");
        QCOMPARE(position(d, 0), QPointF(50, 20));
        d.undo();
        const auto paths   = d.penPaths();
        const auto history = d.historyIndex();
        d.beginPenGesture(qQNaN(), 5);
        d.endPenGesture();
        d.beginPenGesture(5, qInf());
        d.endPenGesture();
        QCOMPARE(d.penPaths(), paths);
        QCOMPARE(d.historyIndex(), history);
    }
};
QTEST_MAIN(PenToolTest)
#include "PenToolTest.moc"
