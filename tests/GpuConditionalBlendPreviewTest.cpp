#include "Document.h"
#include "filters/ToneMappingTable.h"
#include <QtTest>

class GpuConditionalBlendPreviewTest : public QObject
{
    Q_OBJECT
private slots:
    void conditionalBlendPreview_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<int>("blend");
        QTest::addColumn<int>("bits");
        for (const auto &type : PhotoCrap::Filters::gpuTonePreviewTypes())
            for (int blend : PhotoCrap::Filters::gpuToneConditionalBlendModes())
                if (PhotoCrap::Filters::gpuToneBlendSupported(type, blend, true))
                    for (int bits : {8, 16})
                        QTest::newRow(qPrintable(type + QString("-%1-%2").arg(blend).arg(bits)))
                            << type << blend << bits;
    }
    void conditionalBlendPreview()
    {
        QFETCH(QString, type);
        QFETCH(int, blend);
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
            target->previewLayerBlend(target->activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        QCOMPARE(doc.gpuTonePreviewSettings().blendMode, blend);
        const auto base  = doc.gpuTonePreviewBase().cacheKey();
        const auto table = doc.gpuTonePreviewTable().cacheKey();
        const int  next  = blend == 23 ? 24 : 23;
        for (auto *target : {&doc, &cpu})
            target->previewLayerBlend(target->activeLayer(), next);
        QVERIFY(doc.gpuTonePreviewActive());
        QCOMPARE(doc.gpuTonePreviewBase().cacheKey(), base);
        QCOMPARE(doc.gpuTonePreviewTable().cacheKey(), table);
        for (auto *target : {&doc, &cpu})
            target->finishLayerPreview();
        QVERIFY(QTest::qWaitFor([&] { return !doc.gpuTonePreviewActive(); }, 5000));
        QCOMPARE(doc.composite(), cpu.composite());
        QCOMPARE(doc.channelBits(), bits);
        QCOMPARE(doc.historyIndex(), history + 1);
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        doc.cancelLayerPreview();
        QCOMPARE(doc.composite(), cpu.composite());
        doc.undo();
        QCOMPARE(doc.composite(), before);
    }
    void maskCoverage_data()
    {
        QTest::addColumn<int>("blend");
        QTest::addColumn<int>("bits");
        for (int blend : PhotoCrap::Filters::gpuToneConditionalBlendModes())
            for (int bits : {8, 16})
                QTest::newRow(qPrintable(QString("%1-%2").arg(blend).arg(bits))) << blend << bits;
    }
    void maskCoverage()
    {
        QFETCH(int, blend);
        QFETCH(int, bits);
        Document doc;
        QVERIFY(doc.newDocument(128, 64, false, bits));
        doc.setPreviewSize(64, 32);
        doc.setGpuTonePreviewEnabled(true);
        doc.addAdjustmentLayer("levels");
        // Gray masks stay CPU until density is zero or the mask is disabled.
        doc.setEditingMask(true);
        doc.fillMaskWithColor(QColor(128, 128, 128));
        doc.setEditingMask(false);
        const int revision = doc.revision();
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.revision() > revision; }, 5000));
        QVERIFY(!doc.gpuTonePreviewActive());
        doc.previewLayerParameter("_maskDensity", 0.);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        QCOMPARE(doc.gpuTonePreviewSettings().maskCoverage, 255.f);
        doc.cancelLayerPreview();
        doc.setMaskEnabled(false);
        doc.previewLayerBlend(doc.activeLayer(), blend);
        QVERIFY(QTest::qWaitFor([&] { return doc.gpuTonePreviewActive(); }, 5000));
        doc.cancelLayerPreview();
    }
    void analyticSourcesStayOnCpu()
    {
        for (const auto &type : PhotoCrap::Filters::gpuTonePreviewTypes()) {
            if (PhotoCrap::Filters::gpuToneBlendSupported(type, 23, true))
                continue;
            for (int blend : PhotoCrap::Filters::gpuToneConditionalBlendModes()) {
                Document doc;
                QVERIFY(doc.newDocument(128, 64, false));
                doc.setGpuTonePreviewEnabled(true);
                doc.addAdjustmentLayer(type);
                const int revision = doc.revision();
                doc.previewLayerBlend(doc.activeLayer(), blend);
                QVERIFY(QTest::qWaitFor([&] { return doc.revision() > revision; }, 5000));
                QVERIFY(!doc.gpuTonePreviewActive());
                doc.cancelLayerPreview();
            }
        }
    }
};
QTEST_GUILESS_MAIN(GpuConditionalBlendPreviewTest)
#include "GpuConditionalBlendPreviewTest.moc"
