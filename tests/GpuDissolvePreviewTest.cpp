#include "Document.h"
#include "filters/BlendMath.h"
#include "filters/ToneMappingTable.h"
#include <QtTest>

class GpuDissolvePreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void cachedDissolve_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<int>("bits");
        for (const auto &type : PhotoCrap::Filters::gpuTonePreviewTypes())
            for (int bits : {8, 16})
                QTest::newRow(qPrintable(type + QString::number(bits))) << type << bits;
    }
    void cachedDissolve()
    {
        QFETCH(QString, type);
        QFETCH(int, bits);
        Document doc, cpu;
        for (auto *target : {&doc, &cpu}) {
            QVERIFY(target->newDocument(128, 64, false, bits));
            target->setColor(QColor(40, 110, 210));
            target->fillWithColor(true);
            target->setPreviewSize(64, 32);
            target->addAdjustmentLayer(type);
            target->setLayerOpacity(target->activeLayer(), .37);
        }
        doc.setGpuTonePreviewEnabled(true);
        const auto before  = doc.composite();
        const int  history = doc.historyIndex();
        for (auto *target : {&doc, &cpu})
            target->previewLayerBlend(target->activeLayer(), 1);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        QCOMPARE(doc.gpuTonePreviewSettings().maskCoverage, float(quint32(.37 * 65535.)));
        const auto base  = doc.gpuTonePreviewBase().cacheKey();
        const auto table = doc.gpuTonePreviewTable().cacheKey();
        for (auto *target : {&doc, &cpu})
            target->previewLayerBlend(target->activeLayer(), 3);
        QVERIFY(doc.gpuTonePreviewActive());
        for (auto *target : {&doc, &cpu})
            target->previewLayerBlend(target->activeLayer(), 1);
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), table);
        for (auto *target : {&doc, &cpu})
            target->finishLayerPreview();
        QVERIFY(QTest::qWaitFor([&] { return !doc.gpuTonePreviewActive(); }, 5000));
        QCOMPARE(doc.composite(), cpu.composite());
        QCOMPARE(doc.historyIndex(), history + 1);
        QCOMPARE(doc.channelBits(), bits);
        doc.previewLayerBlend(doc.activeLayer(), 3);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        doc.cancelLayerPreview();
        QCOMPARE(doc.composite(), cpu.composite());
        doc.undo();
        QCOMPARE(doc.composite(), before);
    }
    void maskAndViewFallback_data()
    {
        QTest::addColumn<int>("blend");
        QTest::newRow("dissolve") << 1;
        QTest::newRow("hardMix") << 18;
    }
    void maskAndViewFallback()
    {
        QFETCH(int, blend);
        Document doc;
        QVERIFY(doc.newDocument(128, 64, false));
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("invert");
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        int revision = doc.revision();
        doc.setGpuToneNearestSampling(false);
        QVERIFY(!doc.gpuTonePreviewActive());
        QVERIFY(QTest::qWaitFor([&] { return doc.revision() > revision; }, 5000));
        doc.setGpuToneNearestSampling(true);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        doc.cancelLayerPreview();
        doc.setEditingMask(true);
        doc.fillMaskWithColor(QColor(128, 128, 128));
        doc.setEditingMask(false);
        revision = doc.revision();
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.revision() > revision; }, 5000));
        QVERIFY(!doc.gpuTonePreviewActive());
        doc.previewLayerParameter("_maskDensity", 0.);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        doc.cancelLayerPreview();
    }
    void hardMixLookupPreservesNativeSeam()
    {
        const auto table = PhotoCrap::Filters::toneMappingTable("levels", {});
        QCOMPARE(table.size(), QSize(256, 2));
        int high = 0, low = 0;
        for (int source = 0; source < 256; ++source) {
            const int expected =
                PhotoCrap::Filters::blendChannel((255 - source) / 255., source / 255., 18) >= .5
                    ? 255
                    : 0;
            QCOMPARE(qRed(table.pixel(source, 1)), expected);
            high += expected == 255;
            low += expected == 0;
        }
        QVERIFY(high > 0);
        QVERIFY(low > 0);
        QCOMPARE(qRed(table.pixel(100, 0)), 100);
    }
};
QTEST_GUILESS_MAIN(GpuDissolvePreviewTest)
#include "GpuDissolvePreviewTest.moc"
