#include "Document.h"
#include "filters/CurvesFilter.h"
#include "filters/LevelsFilter.h"
#include "filters/LightFilter.h"
#include "filters/ToneMappingTable.h"
#include <QSignalSpy>
#include <QtTest>

class GpuTonePreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void lightPreview_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void lightPreview()
    {
        QFETCH(int, bits);
        Document doc;
        QVERIFY(doc.newDocument(512, 384, false, bits));
        doc.setColor(QColor(40, 110, 210));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("light");
        const auto before  = doc.composite();
        const int  history = doc.historyIndex();
        doc.previewLayerParameter("exposure", .8);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QVERIFY(doc.gpuTonePreviewUsesLuminance());
        const auto        base = doc.gpuTonePreviewBase();
        QVariantMap       parameters{{"exposure", .8}};
        const QVariantMap changes{{"contrast", -40},
                                  {"highlights", -60},
                                  {"shadows", 35},
                                  {"whites", 25},
                                  {"blacks", -20}};
        for (auto it = changes.cbegin(); it != changes.cend(); ++it) {
            parameters.insert(it.key(), it.value());
            doc.previewLayerParameter(it.key(), it.value());
            QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base.cacheKey());
            QCOMPARE(doc.gpuTonePreviewTable(),
                     PhotoCrap::Filters::toneMappingTable("light", parameters));
        }
        const auto expected = PhotoCrap::Filters::applyLight(base, parameters);
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.finishLayerPreview();
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.composite(), PhotoCrap::Filters::applyLight(before, parameters));
        QCOMPARE(doc.channelBits(), bits);
        doc.previewLayerParameter("exposure", -1.);
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base.cacheKey());
        doc.cancelLayerPreview();
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.composite(), PhotoCrap::Filters::applyLight(before, parameters));
        doc.undo();
        QCOMPARE(doc.composite(), before);
    }
    void cachedBaseAndFinalRender()
    {
        Document doc;
        QVERIFY(doc.newDocument(512, 384, false));
        doc.setColor(QColor(50, 100, 150));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        const int history = doc.historyIndex();
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        const auto key = doc.gpuTonePreviewBase().cacheKey();
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(256, 192));
        QCOMPARE(doc.gpuTonePreviewBase().pixelColor(100, 100), QColor(50, 100, 150));
        QSignalSpy changed(&doc, &Document::gpuTonePreviewChanged);
        for (const double gamma : {.8, 1.2, 2.4}) {
            doc.previewLayerParameter("gamma", gamma);
            QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), key);
            QCOMPARE(doc.gpuTonePreviewTable(),
                     PhotoCrap::Filters::toneMappingTable("levels", {{"gamma", gamma}}));
        }
        QCOMPARE(changed.count(), 3);
        const auto expected =
            PhotoCrap::Filters::applyLevels(doc.gpuTonePreviewBase(), {{"gamma", 2.4}});
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.finishLayerPreview();
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.composite().pixelColor(100, 100), expected.pixelColor(100, 100));
        // Reuse the same lower stack for a second gesture; cancelling restores the first.
        doc.previewLayerParameter("gamma", .7);
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), key);
        doc.cancelLayerPreview();
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.historyIndex(), history + 1);
        QTRY_COMPARE_WITH_TIMEOUT(doc.displayComposite().pixelColor(100, 100),
                                  expected.pixelColor(100, 100), 5000);
        doc.undo();
        QCOMPARE(doc.composite().pixelColor(100, 100), QColor(50, 100, 150));
    }
    void cancelledBaseCannotReturn()
    {
        Document doc;
        QVERIFY(doc.newDocument(2048, 1024, false));
        doc.setGpuTonePreviewEnabled(true);
        doc.setPreviewSize(512, 256);
        doc.addAdjustmentLayer("curves");
        const auto before = doc.composite();
        doc.previewLayerParameter("points", QVariantList{QVariantMap{{"x", 0}, {"y", 30}},
                                                         QVariantMap{{"x", 255}, {"y", 200}}});
        doc.cancelLayerPreview();
        QTest::qWait(200);
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.composite(), before);
    }
    void unsupportedCasesStayOnCpu()
    {
        for (const bool transparent : {false, true}) {
            Document doc;
            QVERIFY(doc.newDocument(256, 128, transparent));
            doc.setPreviewSize(256, 128);
            doc.setGpuTonePreviewEnabled(true);
            doc.addAdjustmentLayer("levels");
            if (!transparent)
                doc.setLayerOpacity(doc.activeLayer(), .5);
            const int revision = doc.revision();
            doc.previewLayerParameter("gamma", 2.);
            QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
            QVERIFY(!doc.gpuTonePreviewActive());
            doc.finishLayerPreview();
        }
        Document masked;
        QVERIFY(masked.newDocument(256, 128, false));
        masked.setGpuTonePreviewEnabled(true);
        masked.addAdjustmentLayer("curves");
        masked.addLayerMask(true);
        masked.setEditingMask(true);
        masked.setColor(Qt::black);
        masked.beginStroke(40, 40);
        masked.endStroke();
        masked.setEditingMask(false);
        const int revision = masked.revision();
        masked.previewLayerParameter("blackSample", QVariantList{20, 30, 40});
        QTRY_VERIFY_WITH_TIMEOUT(masked.revision() > revision, 5000);
        QVERIFY(!masked.gpuTonePreviewActive());
        masked.cancelLayerPreview();
    }
    void sixteenBitDocumentsKeepNativeFinal()
    {
        Document doc;
        QVERIFY(doc.newDocument(1024, 768, false, 16));
        doc.setColor(QColor(50, 100, 150));
        doc.fillWithColor(true);
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().format(), QImage::Format_ARGB32_Premultiplied);
        doc.finishLayerPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.channelBits(), 16);
        QCOMPARE(doc.composite().format(), QImage::Format_RGBA64_Premultiplied);
        doc.setGpuTonePreviewEnabled(false);
        const int revision = doc.revision();
        doc.previewLayerParameter("gamma", .7);
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        QVERIFY(!doc.gpuTonePreviewActive());
        doc.cancelLayerPreview();
    }
    void previewResolutionChangesDiscardOldBase()
    {
        Document doc;
        QVERIFY(doc.newDocument(1024, 768, false));
        doc.setPreviewSize(256, 192);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        doc.previewLayerParameter("gamma", 2.);
        // Resize before the first asynchronous base has returned.
        doc.setPreviewSize(512, 384);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(512, 384));
        const auto key = doc.gpuTonePreviewBase().cacheKey();
        doc.setPreviewSize(128, 96);
        QTRY_VERIFY_WITH_TIMEOUT(doc.gpuTonePreviewActive(), 5000);
        QCOMPARE(doc.gpuTonePreviewBase().size(), QSize(128, 96));
        QVERIFY(doc.gpuTonePreviewBase().cacheKey() != key);
        doc.cancelLayerPreview();
    }
    void nativeSixteenBitPreviewStaysOnCpu()
    {
        Document doc;
        QVERIFY(doc.newDocument(128, 96, false, 16));
        doc.setPreviewSize(128, 96);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        const int revision = doc.revision();
        doc.previewLayerParameter("gamma", 2.);
        QTRY_VERIFY_WITH_TIMEOUT(doc.revision() > revision, 5000);
        QVERIFY(!doc.gpuTonePreviewActive());
        QCOMPARE(doc.displayComposite().format(), QImage::Format_RGBA64_Premultiplied);
        doc.cancelLayerPreview();
    }
};
QTEST_GUILESS_MAIN(GpuTonePreviewTest)
#include "GpuTonePreviewTest.moc"
