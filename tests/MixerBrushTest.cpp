#include "Document.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
void setup(Document &_d)
{
    _d.newDocument(180, 64, true);
    _d.setColor(Qt::blue);
    _d.fillWithColor(true);
    _d.setTool("mixerBrush");
    _d.setColor(Qt::red);
    _d.setBrushSize(16);
    _d.setBrushHardness(1);
    _d.setBrushSmoothing(0);
    _d.setBrushSpacing(.25);
    _d.setMixerWet(1);
    _d.setMixerLoad(1);
    _d.setMixerMix(.5);
    _d.setMixerFlow(1);
    _d.setMixerAutoClean(false);
    _d.setMixerAutoLoad(false);
}
} // namespace
class MixerBrushTest : public QObject
{
    Q_OBJECT
private slots:
    void picksUpFreshPaint_data()
    {
        QTest::addColumn<bool>("all");
        QTest::newRow("active layer") << false;
        QTest::newRow("all layers") << true;
    }
    void picksUpFreshPaint()
    {
        QFETCH(bool, all);
        Document d;
        setup(d);
        const auto background = d.layerImage(0);
        if (all)
            d.addLayer();
        d.setMixerSampleAllLayers(all);
        const int target = d.activeLayer(), history = d.historyIndex();
        d.beginStroke(30, 32);
        const auto first = d.layerImage(target).pixelColor(30, 32);
        d.continueStroke(34, 32);
        const auto next = d.layerImage(target).pixelColor(34, 32);
        QVERIFY2(next.red() > first.red(),
                 "Overlapping dabs must pick up the paint they have just deposited");
        d.endStroke();
        QCOMPARE(d.historyIndex(), history + 1);
        const auto after = d.layerImage(target);
        if (all)
            QCOMPARE(d.layerImage(0), background);
        d.undo();
        if (!all)
            QCOMPARE(d.layerImage(target), background);
        else
            QCOMPARE(d.layerImage(target).pixelColor(34, 32).alpha(), 0);
        d.redo();
        QCOMPARE(d.layerImage(target), after);
    }
    void presetFamiliesKeepIndependentFlow()
    {
        Document d;
        setup(d);
        QCOMPARE(d.mixerPresets().size(), 13);
        d.setMixerFlow(.23);
        QSet<int> ids;
        for (auto entry : d.mixerPresets()) {
            const int id = entry.toMap()["id"].toInt();
            if (!id)
                continue;
            ids.insert(id);
            d.applyMixerPreset(id);
            QCOMPARE(d.mixerPreset(), id);
            QCOMPARE(d.mixerFlow(), .23);
        }
        QCOMPARE(ids.size(), 12);
        d.applyMixerPreset(4);
        d.setMixerFlow(.37);
        QCOMPARE(d.mixerPreset(), 4);
        d.setMixerWet(.73);
        QCOMPARE(d.mixerPreset(), 0);
    }
    void pureCanvasMixDoesNotConsumeLoadedPaint()
    {
        Document d;
        setup(d);
        d.setMixerLoad(.01);
        d.setMixerMix(1);
        const auto before = d.layerImage(0);
        d.beginStroke(12, 32);
        d.continueStroke(160, 32);
        d.endStroke();
        QCOMPARE(d.layerImage(0), before);
        QVERIFY(d.mixerLoaded());
    }
    void previewRetainsMulticolourLoadAndMenuState()
    {
        QTemporaryDir directory;
        QImage        image(64, 64, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::blue);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 32; ++x)
                image.setPixelColor(x, y, Qt::red);
        const auto path = directory.filePath("load.png");
        QVERIFY(image.save(path));
        Document d;
        QVERIFY(d.openFile(QUrl::fromLocalFile(path)));
        d.setTool("mixerBrush");
        d.setBrushSize(24);
        d.setMixerSolidOnly(false);
        d.sampleMixerBrush(32, 32);
        const auto url = d.mixerLoadPreview();
        QVERIFY(url.startsWith("data:image/png;base64,"));
        const auto preview =
            QImage::fromData(QByteArray::fromBase64(url.section(',', 1).toLatin1()));
        QVERIFY(!preview.isNull());
        QVERIFY(preview.pixelColor(5, 15).red() > 200);
        QVERIFY(preview.pixelColor(35, 15).blue() > 200);
        d.cleanMixerBrush();
        QVERIFY(d.mixerLoadPreview().isEmpty());
        d.loadMixerBrush();
        QCOMPARE(d.mixerLoadPreview(), url);
        d.setMixerSolidOnly(true);
        d.sampleMixerBrush(40, 32);
        const auto solid = QImage::fromData(
            QByteArray::fromBase64(d.mixerLoadPreview().section(',', 1).toLatin1()));
        QCOMPARE(solid.pixelColor(5, 15), solid.pixelColor(35, 15));
    }
    void airbrushBuildsUpWithoutMovingAndCancels()
    {
        Document d;
        setup(d);
        d.newDocument(100, 80, true);
        d.setMixerWet(0);
        d.setMixerMix(0);
        d.setMixerFlow(.2);
        d.setMixerLoad(1);
        d.loadMixerBrush();
        d.setMixerAirbrush(false);
        const auto before  = d.layerImage(0);
        const auto history = d.historyIndex();
        d.beginStroke(40, 40);
        const int first = d.layerImage(0).pixelColor(40, 40).alpha();
        d.accumulateMixerBrush();
        QCOMPARE(d.layerImage(0).pixelColor(40, 40).alpha(), first);
        d.setMixerAirbrush(true);
        for (int i = 0; i < 6; ++i)
            d.accumulateMixerBrush();
        QVERIFY(d.layerImage(0).pixelColor(40, 40).alpha() > first + 100);
        d.cancelStroke();
        QCOMPARE(d.layerImage(0), before);
        QCOMPARE(d.historyIndex(), history);
        QVERIFY(d.mixerLoaded());
        d.beginStroke(40, 40);
        for (int i = 0; i < 6; ++i)
            d.accumulateMixerBrush();
        d.endStroke();
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layerImage(0), before);
    }
    void wetPickupCrossesCanvasColoursAndCleanRemovesCarry()
    {
        Document d;
        setup(d);
        d.setTool("select");
        d.beginStroke(0, 0);
        d.continueStroke(70, 63);
        d.endStroke();
        d.setColor(Qt::red);
        d.fillWithColor(true);
        d.deselect();
        d.setTool("mixerBrush");
        d.setMixerMix(1);
        d.cleanMixerBrush();
        d.beginStroke(60, 32);
        d.continueStroke(95, 32);
        d.endStroke();
        auto streak = d.layerImage(0).pixelColor(85, 32);
        QVERIFY(streak.red() > 0 && streak.blue() > 0);
        d.cleanMixerBrush();
        QVERIFY(!d.mixerLoaded());
        d.setMixerWet(0);
        const auto before = d.layerImage(0);
        d.beginStroke(120, 32);
        d.continueStroke(140, 32);
        d.endStroke();
        QCOMPARE(d.layerImage(0), before);
    }
    void staysRegionalAndPreservesMasks()
    {
        Document d;
        setup(d);
        d.newDocument(6000, 4000, true);
        d.setMixerWet(0);
        d.setMixerMix(0);
        d.setMixerFlow(1);
        d.loadMixerBrush();
        const auto heap = d.memoryUsage()["tileHeapBytes"].toLongLong();
        d.beginStroke(100, 100);
        d.continueStroke(110, 110);
        d.endStroke();
        QVERIFY(d.memoryUsage()["tileHeapBytes"].toLongLong() - heap <= 256LL * 256 * 4);
        d.addLayerMask();
        d.setEditingMask(true);
        d.setColor(Qt::black);
        const auto image = d.layerImage(0);
        d.beginStroke(100, 100);
        d.endStroke();
        QVERIFY(d.maskImage(0).pixelColor(100, 100).red() < 10);
        QCOMPARE(d.layerImage(0), image);
    }
};
QTEST_MAIN(MixerBrushTest)
#include "MixerBrushTest.moc"
