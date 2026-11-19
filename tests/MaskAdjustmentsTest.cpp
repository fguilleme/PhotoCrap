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
    void importXmpAsOneEditableStack()
    {
        QTemporaryDir tmp;
        const auto    url = QUrl::fromLocalFile(tmp.filePath("Studio.xmp"));
        QFile         xmp(url.toLocalFile());
        QVERIFY(xmp.open(QIODevice::WriteOnly));
        xmp.write("<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\""
                  " xmlns:raw=\"http://ns.adobe.com/camera-raw-settings/1.0/\">"
                  "<rdf:Description raw:Exposure2012=\"+0.75\" raw:Exposure=\"2\" "
                  "raw:Temperature=\"5200\">"
                  "<raw:Vibrance>23</raw:Vibrance>"
                  "<raw:ToneCurveRed><rdf:Seq><rdf:li>0, 0</rdf:li><rdf:li>255, 110</rdf:li>"
                  "</rdf:Seq></raw:ToneCurveRed>"
                  "<raw:ToneCurveBlue><rdf:Seq><rdf:li>0, 0</rdf:li><rdf:li>255, 225</rdf:li>"
                  "</rdf:Seq></raw:ToneCurveBlue>"
                  "<raw:ToneCurvePV2012Red><rdf:Seq>"
                  "<rdf:li>0, 0</rdf:li><rdf:li>128, 90</rdf:li><rdf:li>255, 200</rdf:li>"
                  "</rdf:Seq></raw:ToneCurvePV2012Red>"
                  "<raw:MaskGroupBasedCorrections><rdf:Seq><rdf:li raw:Exposure2012=\"-3\"/>"
                  "</rdf:Seq></raw:MaskGroupBasedCorrections>"
                  "</rdf:Description></rdf:RDF>");
        xmp.close();
        Document doc;
        QVERIFY(doc.newDocument(32, 32, false, 16));
        const auto original = doc.composite();
        const auto count    = doc.history().size();
        QSignalSpy skipped(&doc, &Document::xmpSettingsSkipped);
        doc.importAdobeXmp(url);
        QCOMPARE(doc.history().size(), count + 1);
        QCOMPARE(doc.layers().size(), 4);
        QCOMPARE(skipped.count(), 1);
        QCOMPARE(skipped.first()[0].toInt(), 3);
        QCOMPARE(skipped.first()[1].toStringList(),
                 QStringList({"MaskGroupBasedCorrections", "Temperature"}));
        QCOMPARE(layer(doc, 1)["parameters"].toMap()["exposure"].toDouble(), .75);
        QCOMPARE(layer(doc, 2)["parameters"].toMap()["vibrance"].toInt(), 23);
        QCOMPARE(layer(doc, 3)["parameters"].toMap()["redPoints"].toList().size(), 3);
        QCOMPARE(doc.activeLayer(), 3);
        const auto adjusted = doc.composite();
        QCOMPARE(adjusted.format(), QImage::Format_RGBA64_Premultiplied);
        QCOMPARE(adjusted.pixelColor(8, 8).red(), 200);
        QCOMPARE(adjusted.pixelColor(8, 8).green(), 255);
        QCOMPARE(adjusted.pixelColor(8, 8).blue(), 225);
        const auto project = QUrl::fromLocalFile(tmp.filePath("Studio.pcr"));
        QVERIFY(doc.saveProject(project));
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.composite(), adjusted);
        doc.undo();
        QCOMPARE(doc.layers().size(), 1);
        QCOMPARE(doc.composite(), original);
        doc.redo();
        QCOMPARE(doc.composite(), adjusted);
        doc.resetCurveAdjustment();
        QCOMPARE(doc.composite().pixelColor(8, 8).red(), 255);
    }

    void xmpToningAndTexture_data()
    {
        QTest::addColumn<bool>("wide");
        QTest::newRow("8-bit") << false;
        QTest::newRow("16-bit") << true;
    }

    void xmpToningAndTexture()
    {
        QFETCH(bool, wide);
        QTemporaryDir tmp;
        QImage        input(160, 48, wide ? QImage::Format_RGBA64 : QImage::Format_ARGB32);
        for (int x = 0; x < input.width(); ++x) {
            const int level = x < 32 ? 55 : x < 64 ? 128 : x < 96 ? 200 : (x / 5 % 2 ? 145 : 115);
            for (int y = 0; y < input.height(); ++y)
                input.setPixelColor(x, y, QColor(level, level, level, y < 8 ? 128 : 255));
        }
        const auto photo = QUrl::fromLocalFile(tmp.filePath("tones.png"));
        QVERIFY(input.save(photo.toLocalFile()));
        const auto preset = QUrl::fromLocalFile(tmp.filePath("tones.xmp"));
        QFile      xmp(preset.toLocalFile());
        QVERIFY(xmp.open(QIODevice::WriteOnly));
        xmp.write("<r:RDF xmlns:r=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" "
                  "xmlns:c=\"http://ns.adobe.com/camera-raw-settings/1.0/\"> "
                  "<r:Description c:SplitToningBalance=\"+45\" c:SplitToningShadowHue=\"235\" "
                  "c:SplitToningShadowSaturation=\"80\" c:SplitToningHighlightHue=\"360\" "
                  "c:SplitToningHighlightSaturation=\"90\"><c:Texture>-85</c:Texture> "
                  "</r:Description></r:RDF>");
        xmp.close();
        Document doc;
        QVERIFY(doc.openFile(photo));
        const auto original = doc.composite();
        QSignalSpy skipped(&doc, &Document::xmpSettingsSkipped);
        doc.importAdobeXmp(preset);
        QCOMPARE(skipped.size(), 0);
        QCOMPARE(doc.layers().size(), 3);
        QCOMPARE(layer(doc, 1)["adjustmentType"].toString(), QStringLiteral("texture"));
        QCOMPARE(layer(doc, 1)["parameters"].toMap()["amount"].toInt(), -85);
        QCOMPARE(layer(doc, 2)["adjustmentType"].toString(), QStringLiteral("splitToning"));
        const auto parameters = layer(doc, 2)["parameters"].toMap();
        QCOMPARE(parameters["shadowHue"].toInt(), 235);
        QCOMPARE(parameters["shadowSaturation"].toInt(), 80);
        QCOMPARE(parameters["highlightHue"].toInt(), 360);
        QCOMPARE(parameters["highlightSaturation"].toInt(), 90);
        QCOMPARE(parameters["balance"].toInt(), 45);
        const auto graded = doc.composite();
        QCOMPARE(graded.format(),
                 wide ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied);
        auto warmth = [](const QImage &img, int x) {
            const auto c = img.pixelColor(x, 20);
            return c.red() - c.blue();
        };
        QVERIFY(warmth(graded, 16) < 0);
        QVERIFY(warmth(graded, 48) > 0);
        QVERIFY(warmth(graded, 80) > 0);
        QCOMPARE(graded.pixelColor(16, 2).alpha(), original.pixelColor(16, 2).alpha());
        const auto project = QUrl::fromLocalFile(tmp.filePath("tones.pcr"));
        QVERIFY(doc.saveProject(project));
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.composite(), graded);
        QCOMPARE(layer(reopened, 2)["parameters"], layer(doc, 2)["parameters"]);
        doc.undo();
        QCOMPARE(doc.composite(), original);
        doc.redo();
        QCOMPARE(doc.composite(), graded);
        doc.setLayerOpacity(2, .5);
        const auto half = doc.composite();
        QCOMPARE(half.pixelColor(48, 2).alpha(), original.pixelColor(48, 2).alpha());
        QVERIFY(warmth(half, 48) > 0 && warmth(half, 48) < warmth(graded, 48));
        doc.setLayerOpacity(2, 1.);
        doc.beginLayerPreview();
        doc.previewLayerParameter("balance", -45);
        doc.finishLayerPreview();
        QVERIFY(warmth(doc.composite(), 48) < 0);

        doc.resetAdjustmentLayer();
        const auto softened = doc.composite();
        auto       contrast = [](const QImage &img) {
            return img.pixelColor(127, 20).red() - img.pixelColor(122, 20).red();
        };
        QVERIFY(contrast(softened) < contrast(original));
        QCOMPARE(softened.pixelColor(48, 20), original.pixelColor(48, 20));
        doc.setActiveLayer(1);
        doc.beginLayerPreview();
        doc.previewLayerParameter("amount", 85);
        doc.finishLayerPreview();
        QVERIFY(contrast(doc.composite()) > contrast(original));
        const QRect tile(114, 10, 15, 16);
        QCOMPARE(doc.compositeRegion(tile), doc.composite().copy(tile));
        doc.resetAdjustmentLayer();
        QCOMPARE(doc.composite(), original);
    }

    void xmpNeutralTextureAndUnsaturatedTints()
    {
        QTemporaryDir tmp;
        QFile         xmp(tmp.filePath("neutral.xmp"));
        QVERIFY(xmp.open(QIODevice::WriteOnly));
        xmp.write(
            "<r:RDF xmlns:r=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" "
            "xmlns:c=\"http://ns.adobe.com/camera-raw-settings/1.0/\"> "
            "<r:Description c:SplitToningShadowHue=\"40\" c:SplitToningShadowSaturation=\"0\" "
            "c:SplitToningHighlightHue=\"280\" c:SplitToningBalance=\"30\" c:Texture=\"0\"/> "
            "</r:RDF>");
        xmp.close();
        Document doc;
        QVERIFY(doc.newDocument(32, 32, false));
        const auto before = doc.history();
        QSignalSpy skipped(&doc, &Document::xmpSettingsSkipped);
        doc.importAdobeXmp(QUrl::fromLocalFile(xmp.fileName()));
        QCOMPARE(doc.layers().size(), 1);
        QCOMPARE(doc.history(), before);
        QCOMPARE(skipped.size(), 0);
    }

    void invalidXmpIsAtomic()
    {
        QTemporaryDir tmp;
        const auto    url = QUrl::fromLocalFile(tmp.filePath("Broken.xmp"));
        QFile         xmp(url.toLocalFile());
        QVERIFY(xmp.open(QIODevice::WriteOnly));
        xmp.write("<r:RDF xmlns:r=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\""
                  " xmlns:c=\"http://ns.adobe.com/camera-raw-settings/1.0/\">"
                  "<r:Description c:Exposure2012=\"1\"><c:ToneCurvePV2012><r:Seq>"
                  "<r:li>255, 100</r:li><r:li>0, "
                  "0</r:li></r:Seq></c:ToneCurvePV2012></r:Description></r:RDF>");
        xmp.close();
        Document doc;
        QVERIFY(doc.newDocument(32, 32, false));
        const auto history = doc.history();
        QSignalSpy errors(&doc, &Document::errorOccurred);
        doc.importAdobeXmp(url);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(doc.history(), history);
        QCOMPARE(doc.layers().size(), 1);
    }

    void referenceLookMatchesVisiblePixels()
    {
        QTemporaryDir tmp;
        QImage source(128, 64, QImage::Format_ARGB32), reference(source.size(), source.format());
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                int v = x + 25;
                source.setPixel(x, y, qRgb(v, v, v));
                reference.setPixel(x, y,
                                   y < 32 ? qRgb(v + 55, v + 30, v + 10) : qRgba(250, 0, 10, 0));
            }
        }
        const auto photo  = QUrl::fromLocalFile(tmp.filePath("photo.png"));
        const auto target = QUrl::fromLocalFile(tmp.filePath("reference.png"));
        QVERIFY(source.save(photo.toLocalFile()));
        QVERIFY(reference.save(target.toLocalFile()));
        Document doc;
        QVERIFY(doc.openFile(photo));
        QSignalSpy errors(&doc, &Document::errorOccurred);
        QSignalSpy status(&doc, &Document::statusMessage);
        const auto baseline = doc.history();
        doc.matchReferenceImage(photo);
        QTRY_VERIFY(!doc.operationBusy());
        QCOMPARE(status.size(), 1);
        QCOMPARE(doc.history(), baseline);
        doc.matchReferenceImage(target);
        QTRY_VERIFY(!doc.operationBusy());
        QCOMPARE(errors.size(), 0);
        QCOMPARE(doc.history().size(), baseline.size() + 1);
        QVERIFY(doc.layers().size() > 1);
        for (int i = 1; i < doc.layers().size(); ++i)
            QCOMPARE(layer(doc, i)["kind"].toString(), "adjustment");
        const auto result   = doc.composite();
        const auto expected = reference.pixelColor(64, 12);
        const auto actual   = result.pixelColor(64, 12);
        QVERIFY(qAbs(expected.red() - actual.red()) < 4);
        QVERIFY(qAbs(expected.green() - actual.green()) < 4);
        QVERIFY(qAbs(expected.blue() - actual.blue()) < 4);
        QCOMPARE(doc.layerImage(0).pixelColor(64, 12), source.pixelColor(64, 12));
        const auto matched = QUrl::fromLocalFile(tmp.filePath("visible.png"));
        QVERIFY(result.save(matched.toLocalFile()));
        const auto count = doc.history().size();
        doc.matchReferenceImage(matched);
        QTRY_VERIFY(!doc.operationBusy());
        QCOMPARE(doc.history().size(), count);
        QCOMPARE(status.size(), 2);
        doc.undo();
        QCOMPARE(doc.layers().size(), 1);
        doc.redo();
        QCOMPARE(doc.composite(), result);
        reference.fill(Qt::transparent);
        QVERIFY(reference.save(target.toLocalFile()));
        doc.matchReferenceImage(target);
        QTRY_VERIFY(!doc.operationBusy());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(doc.history().size(), count);
    }

    void referencePreservesHighFrequencyContrast()
    {
        QTemporaryDir tmp;
        QImage image(1024, 768, QImage::Format_RGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, (x + y) % 2 ? qRgb(42, 19, 80) : qRgb(211, 144, 162));
        const auto photo = QUrl::fromLocalFile(tmp.filePath("texture.png"));
        QVERIFY(image.save(photo.toLocalFile()));
        Document doc;
        QVERIFY(doc.openFile(photo));
        QSignalSpy status(&doc, &Document::statusMessage);
        doc.matchReferenceImage(photo);
        QTRY_VERIFY(!doc.operationBusy());
        QCOMPARE(status.size(), 1);
        QCOMPARE(doc.layerCount(), 1);
    }

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
