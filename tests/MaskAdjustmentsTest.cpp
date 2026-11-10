#include "Document.h"
#include "PreviewRenderWait.h"
#include <QColorSpace>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
QVariantMap layer(const Document &d, int index)
{
    for (const auto &v : d.layers())
        if (v.toMap()["layerIndex"].toInt() == index)
            return v.toMap();
    return {};
}
void path(Document &d)
{
    d.setTool("pen");
    d.setPenMode("path");
    d.newPenPath();
    for (const auto p : {QPoint(20, 20), QPoint(100, 20), QPoint(100, 100), QPoint(20, 100)}) {
        d.beginPenGesture(p.x(), p.y(), 2);
        d.endPenGesture();
    }
    d.closePenPath();
}
void dab(Document &d)
{
    d.setTool("brush");
    d.setBrushSize(35);
    d.setBrushHardness(100);
    d.setColor(Qt::black);
    d.beginStroke(60, 60);
    d.endStroke();
}
} // namespace
class MaskAdjustmentsTest : public QObject
{
    Q_OBJECT
private slots:
    void directMask_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8") << 8;
        QTest::newRow("16") << 16;
    }
    void directMask()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(140, 120, false, bits));
        d.setBackgroundEditThreshold(-1);
        const auto color = d.layerImage(0);
        d.addLayerMask(true);
        dab(d);
        const auto mask = d.maskImage(0);
        QVERIFY(d.beginDirectAdjustment("gaussianBlur"));
        d.previewLayerParameter("radius", 6.);
        QCOMPARE(d.maskImage(0), mask); // transient preview never changes native pixels
        d.finishDirectAdjustment(false);
        QCOMPARE(d.maskImage(0), mask);
        QCOMPARE(d.layerImage(0), color);
        const auto count = d.history().size();
        QVERIFY(d.beginDirectAdjustment("gaussianBlur"));
        d.previewLayerParameter("radius", 6.);
        d.finishDirectAdjustment(true);
        QVERIFY(d.maskImage(0) != mask);
        QCOMPARE(d.layerImage(0), color);
        QCOMPARE(d.history().size(), count + 1);
        QCOMPARE(d.channelBits(), bits);
        d.undo();
        QCOMPARE(d.maskImage(0), mask);
        d.redo();
        QVERIFY(d.maskImage(0) != mask);
    }
    void selectionToMaskBlursBothSides_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("reveal");
        QTest::newRow("reveal-8") << 8 << true;
        QTest::newRow("hide-8") << 8 << false;
        QTest::newRow("reveal-16") << 16 << true;
        QTest::newRow("hide-16") << 16 << false;
    }
    void selectionToMaskBlursBothSides()
    {
        QFETCH(int, bits);
        QFETCH(bool, reveal);
        Document d;
        QVERIFY(d.newDocument(140, 120, false, bits));
        d.setBackgroundEditThreshold(-1);
        const auto pixels = d.layerImage(0);
        d.setTool("select");
        d.beginStroke(20, 20);
        d.continueStroke(100, 100);
        d.endStroke();
        const auto selection    = d.selectionMask();
        const auto historyCount = d.history().size();
        d.addLayerMask(reveal);
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.history().size(), historyCount + 1);
        const auto original = d.maskImage(0);
        d.applyAdjustment("gaussianBlur", {{"radius", 6.}});
        const auto blurred = d.maskImage(0);
        const int  outside = blurred.pixelColor(19, 60).red();
        const int  inside  = blurred.pixelColor(20, 60).red();
        QVERIFY(outside > 0 && outside < 255);
        QVERIFY(inside > 0 && inside < 255);
        QVERIFY(qAbs(outside + inside - 255) <= 4);
        QCOMPARE(d.layerImage(0), pixels);
        d.undo();
        QCOMPARE(d.maskImage(0), original);
        d.undo();
        QVERIFY(!layer(d, 0)["hasMask"].toBool());
        QVERIFY(d.hasSelection());
        QCOMPARE(d.selectionMask(), selection);
        d.redo();
        QVERIFY(!d.hasSelection());
        d.reselect();
        QCOMPARE(d.selectionMask(), selection);
    }
    void explicitSelectionLimitCanBeDisabled()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false, 16));
        d.setTool("select");
        d.beginStroke(20, 20);
        d.continueStroke(100, 100);
        d.endStroke();
        d.addLayerMask(true);
        d.reselect();
        const auto selected = d.selectionMask();
        const auto original = d.maskImage(0);
        d.applyAdjustment("gaussianBlur", {{"radius", 6.}});
        QCOMPARE(d.maskImage(0).pixelColor(19, 60).red(), 0);
        d.undo();
        QCOMPARE(d.maskImage(0), original);
        d.toggleMaskOnlyView(0);
        QVERIFY(d.beginDirectAdjustment("gaussianBlur"));
        d.previewLayerParameter("radius", 6.);
        d.previewLayerParameter("_limitSelection", false);
        QTRY_VERIFY_WITH_TIMEOUT(d.displayComposite().pixelColor(19, 60).red() > 0, 5000);
        QCOMPARE(d.maskImage(0), original);
        d.finishDirectAdjustment(true);
        QVERIFY(d.maskImage(0).pixelColor(19, 60).red() > 0);
        QVERIFY(d.maskImage(0).pixelColor(20, 60).red() < 255);
        QCOMPARE(d.selectionMask(), selected);
    }
    void softSelectionIsConsumedWithoutLosingCoverage()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        d.setTool("select");
        d.beginStroke(20, 20);
        d.continueStroke(100, 100);
        d.endStroke();
        d.modifySelection("feather", 3);
        QTRY_VERIFY_WITH_TIMEOUT(!d.selectionBusy(), 5000);
        const auto selected = d.selectionMask();
        QVERIFY(selected.pixelColor(20, 60).red() > 0);
        QVERIFY(selected.pixelColor(20, 60).red() < 255);
        d.addLayerMask(true);
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.maskImage(0), selected);
        d.reselect();
        QCOMPARE(d.selectionMask(), selected);
    }
    void adjustmentMaskConsumesSelection()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        d.setTool("select");
        d.beginStroke(20, 20);
        d.continueStroke(100, 100);
        d.endStroke();
        const auto selected = d.selectionMask();
        d.addAdjustmentLayer("invert");
        QVERIFY(!d.hasSelection());
        QCOMPARE(d.maskImage(1), selected);
        d.undo();
        QVERIFY(d.hasSelection());
        QCOMPARE(d.layers().size(), 1);
        d.redo();
        QVERIFY(!d.hasSelection());
        d.reselect();
        QCOMPARE(d.selectionMask(), selected);
    }
    void directPixelSelection()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false, 16));
        d.setBackgroundEditThreshold(-1);
        const auto old = d.layerImage(0);
        d.setTool("select");
        d.beginStroke(20, 20);
        d.continueStroke(80, 80);
        d.endStroke();
        d.applyAdjustment("invert", {});
        const auto result = d.layerImage(0);
        QCOMPARE(result.pixelColor(10, 10), old.pixelColor(10, 10));
        QVERIFY(result.pixelColor(40, 40) != old.pixelColor(40, 40));
        QCOMPARE(result.format(), QImage::Format_RGBA64_Premultiplied);
        d.undo();
        QCOMPARE(d.layerImage(0), old);
    }
    void retainedVector_data()
    {
        directMask_data();
    }
    void retainedVector()
    {
        QFETCH(int, bits);
        Document d;
        QVERIFY(d.newDocument(140, 120, false, bits));
        d.setBackgroundEditThreshold(-1);
        const auto old = d.layerImage(0);
        d.addLayerMask(true);
        dab(d);
        const auto raster = d.maskImage(0);
        d.setEditingMask(false);
        path(d);
        d.addVectorMask();
        QVERIFY(layer(d, 0)["hasVectorMask"].toBool());
        QVERIFY(d.editingVectorMask());
        QCOMPARE(d.maskImage(0), raster);
        QCOMPARE(d.layerImage(0), old);
        auto composed = d.composite();
        QCOMPARE(composed.pixelColor(5, 5).alpha(), 0);
        QCOMPARE(composed.pixelColor(30, 30).alpha(), 255);
        QCOMPARE(composed.pixelColor(60, 60).alpha(), 0);
        const auto before = d.vectorMaskImage(0);
        d.beginPenGesture(20, 20, 3, true);
        d.updatePenGesture(8, 8);
        d.endPenGesture();
        QVERIFY(d.vectorMaskImage(0) != before);
        const auto after = d.vectorMaskImage(0);
        d.undo();
        QCOMPARE(d.vectorMaskImage(0), before);
        d.redo();
        QCOMPARE(d.vectorMaskImage(0), after);
        d.beginLayerPreview();
        d.previewLayerParameter("_vectorFeather", 6.);
        d.finishLayerPreview("Feather");
        QVERIFY(waitForCommittedPreview(d));
        const auto feathered = d.composite();
        QVERIFY(feathered.pixelColor(100, 60).alpha() > 0);
        QVERIFY(feathered.pixelColor(100, 60).alpha() < 255);
        QTemporaryDir tmp;
        const auto    url = QUrl::fromLocalFile(tmp.filePath("vector.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(), d.composite());
        QCOMPARE(loaded.maskImage(0), raster);
        loaded.editVectorMask();
        QVERIFY(loaded.editingVectorMask());
        loaded.setVectorMaskEnabled(false);
        QCOMPARE(loaded.composite().pixelColor(5, 5).alpha(), 255);
        loaded.undo();
        QCOMPARE(loaded.composite(), feathered);
        loaded.resizeImage(280, 240);
        QCOMPARE(loaded.vectorMaskImage(0).size(), QSize(280, 240));
        loaded.undo();
        QCOMPARE(loaded.composite(), feathered);
        loaded.removeVectorMask();
        QVERIFY(!layer(loaded, 0)["hasVectorMask"].toBool());
        QCOMPARE(loaded.maskImage(0), raster);
    }
    void background()
    {
        Document d;
        QVERIFY(d.newDocument(180, 160, false, 16));
        d.setBackgroundEditThreshold(1);
        d.addLayerMask(true);
        const auto pixels = d.layerImage(0);
        d.applyAdjustment("invert", {});
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.maskImage(0).pixelColor(20, 20).red(), 0);
        QCOMPARE(d.layerImage(0), pixels);
        d.undo();
        QCOMPARE(d.maskImage(0).pixelColor(20, 20).red(), 255);
    }
    void mosaicSelectionUsesDocumentGrid()
    {
        QImage image(170, 130, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, qRgb((x * 37 + y) % 256, (y * 17 + x) % 256, x ^ y));
        QTemporaryDir directory;
        const auto    filename = directory.filePath("grid.png");
        QVERIFY(image.save(filename));
        Document full, selected;
        QVERIFY(full.openFile(QUrl::fromLocalFile(filename)));
        QVERIFY(selected.openFile(QUrl::fromLocalFile(filename)));
        full.applyAdjustment("mosaic", {{"cellSize", 17}});
        selected.setTool("select");
        selected.beginStroke(51, 39);
        selected.continueStroke(103, 82);
        selected.endStroke();
        const auto area = selected.selection();
        selected.applyAdjustment("mosaic", {{"cellSize", 17}});
        QCOMPARE(selected.layerImage(0).copy(area), full.layerImage(0).copy(area));
    }
    void maskOnlyPreview()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        d.addLayerMask(true);
        dab(d);
        const auto original = d.maskImage(0);
        d.toggleMaskOnlyView(0);
        QVERIFY(d.beginDirectAdjustment("invert"));
        auto expected = original;
        expected.invertPixels();
        expected = expected.convertToFormat(QImage::Format_RGB32);
        expected.setColorSpace(QColorSpace::SRgb);
        QTRY_COMPARE_WITH_TIMEOUT(d.displayComposite(), expected, 5000);
        QCOMPARE(d.maskImage(0), original);
        d.setDirectAdjustmentPreview(false);
        auto unfiltered = original.convertToFormat(QImage::Format_RGB32);
        unfiltered.setColorSpace(QColorSpace::SRgb);
        QTRY_COMPARE_WITH_TIMEOUT(d.displayComposite(), unfiltered, 5000);
        d.finishDirectAdjustment(false);
        QCOMPARE(d.maskImage(0), original);
    }
    void vectorSurvivesShapeRasterization()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        path(d);
        QVERIFY(d.penPathToShape());
        path(d);
        d.addVectorMask();
        const auto mask = d.vectorMaskImage(d.activeLayer());
        d.setActivePenPath(-1);
        d.rasterizeShapeLayer();
        QVERIFY(layer(d, d.activeLayer())["hasVectorMask"].toBool());
        QCOMPARE(d.vectorMaskImage(d.activeLayer()), mask);
    }
    void maskPathCanBecomeIndependentShape()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        path(d);
        d.addVectorMask();
        const auto mask = d.vectorMaskImage(0);
        QVERIFY(d.penPathToShape());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(layer(d, 1)["kind"].toString(), QString("shape"));
        QVERIFY(!d.editingVectorMask());
        d.setActiveLayer(0);
        d.editVectorMask();
        QVERIFY(d.editingVectorMask());
        QCOMPARE(d.vectorMaskImage(0), mask);
    }
    void rasterize()
    {
        Document d;
        QVERIFY(d.newDocument(140, 120, false));
        path(d);
        d.addVectorMask();
        const auto before = d.composite();
        d.rasterizeVectorMask();
        QVERIFY(!layer(d, 0)["hasVectorMask"].toBool());
        QVERIFY(d.editingMask());
        QCOMPARE(d.composite(), before);
        d.undo();
        QVERIFY(layer(d, 0)["hasVectorMask"].toBool());
    }
};
QTEST_MAIN(MaskAdjustmentsTest)
#include "MaskAdjustmentsTest.moc"
