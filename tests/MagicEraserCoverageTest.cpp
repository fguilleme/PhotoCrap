#include "Document.h"
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

namespace
{
QImage fixture(int _bits)
{
    QImage       image(48, 32,
                       _bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                   : QImage::Format_ARGB32_Premultiplied);
    const QColor blue =
        _bits == 16 ? QColor::fromRgba64(7001, 12003, 45007, 57001) : QColor(30, 70, 180, 210);
    const QColor red =
        _bits == 16 ? QColor::fromRgba64(47003, 16001, 8007, 40003) : QColor(180, 60, 30, 170);
    image.fill(blue);
    QPainter painter(&image);
    painter.fillRect(10, 9, 8, 12, red);
    painter.fillRect(30, 9, 8, 12, red);
    return image;
}
bool open(Document &_doc, const QUrl &_path, bool _sampleAll)
{
    if (!_doc.openFile(_path))
        return false;
    if (_sampleAll) {
        _doc.addLayer();
        _doc.setColor(QColor::fromRgba64(13001, 35003, 18007, 57001));
        _doc.fillWithColor(true);
        _doc.setLayerOpacity(_doc.activeLayer(), .45);
    }
    return true;
}
} // namespace

class MagicEraserCoverageTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QThreadPool::globalInstance()->setMaxThreadCount(2);
    }
    void fusedCoverageMatchesSeparateNativeBlend_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("contiguous");
        QTest::addColumn<bool>("antialias");
        QTest::addColumn<double>("opacity");
        QTest::addColumn<bool>("sampleAll");
        for (int bits : {8, 16})
            for (bool contiguous : {false, true})
                for (bool antialias : {false, true})
                    for (double opacity : {.37, 1.})
                        QTest::newRow(qPrintable(QString("%1-%2-%3-%4")
                                                     .arg(bits)
                                                     .arg(contiguous)
                                                     .arg(antialias)
                                                     .arg(opacity)))
                            << bits << contiguous << antialias << opacity << false;
        for (int bits : {8, 16})
            for (bool antialias : {false, true})
                QTest::newRow(qPrintable(QString("all-layers-%1-%2").arg(bits).arg(antialias)))
                    << bits << false << antialias << .37 << true;
    }
    void fusedCoverageMatchesSeparateNativeBlend()
    {
        QFETCH(int, bits);
        QFETCH(bool, contiguous);
        QFETCH(bool, antialias);
        QFETCH(double, opacity);
        QFETCH(bool, sampleAll);
        QTemporaryDir dir;
        const auto    path = QUrl::fromLocalFile(dir.filePath("coverage.png"));
        QVERIFY(fixture(bits).save(path.toLocalFile()));
        Document raw, masked;
        QVERIFY(open(raw, path, sampleAll));
        QVERIFY(open(masked, path, sampleAll));
        const int  active   = masked.activeLayer();
        const auto original = masked.layerImage(active);
        const auto base     = masked.layerImage(0);
        masked.setTool("ellipticalSelect");
        masked.beginStroke(8, 6);
        masked.continueStroke(40, 24);
        masked.endStroke();
        masked.modifySelection("feather", 6);
        QTRY_VERIFY_WITH_TIMEOUT(!masked.selectionBusy(), 5000);
        const auto selection = masked.selectionRegion();
        const auto coverage  = masked.selectionMask();
        QVERIFY(coverage.constScanLine(10)[11] > 0 && coverage.constScanLine(10)[11] < 255);
        const int history = masked.historyIndex();
        for (auto *doc : {&raw, &masked}) {
            doc->setTool("magicEraser");
            doc->setMagicEraserTolerance(0);
            doc->setMagicEraserContiguous(contiguous);
            doc->setMagicEraserAntialias(antialias);
            doc->setMagicEraserSampleAllLayers(sampleAll);
            doc->setBrushOpacity(opacity);
        }
        // Reference pipeline: erase without attenuation, then apply the native
        // coverage blend separately, as the GUI callback did before fusion.
        raw.beginStroke(14, 14);
        QTRY_VERIFY_WITH_TIMEOUT(!raw.magicEraserBusy(), 5000);
        QImage expected = raw.layerImage(active);
        QVERIFY(expected != original);
        SelectionCoverage::blend(expected, original, coverage, expected.rect());
        masked.beginStroke(14, 14);
        QVERIFY(masked.magicEraserBusy());
        QVERIFY(masked.pixelEditBusy());
        QCOMPARE(masked.layerImage(active), original);
        QTRY_VERIFY_WITH_TIMEOUT(!masked.magicEraserBusy(), 5000);
        QVERIFY(!masked.pixelEditBusy());
        QCOMPARE(masked.layerImage(active), expected);
        QCOMPARE(masked.channelBits(), bits);
        QCOMPARE(masked.historyIndex(), history + 1);
        QCOMPARE(masked.selectionRegion(), selection);
        QCOMPARE(masked.layerImage(active).pixelColor(24, 15), original.pixelColor(24, 15));
        if (sampleAll)
            QCOMPARE(masked.layerImage(0), base);
        if (bits == 16) {
            bool fractional = false;
            for (int y = 0; y < expected.height(); ++y) {
                const auto *pixel = reinterpret_cast<const QRgba64 *>(expected.constScanLine(y));
                const auto *old   = reinterpret_cast<const QRgba64 *>(original.constScanLine(y));
                for (int x = 0; x < expected.width(); ++x)
                    fractional |= pixel[x].alpha() != old[x].alpha() && pixel[x].alpha() % 257 != 0;
            }
            QVERIFY(fractional);
        }
        masked.undo();
        QCOMPARE(masked.layerImage(active), original);
        QCOMPARE(masked.selectionMask(), coverage);
        masked.redo();
        QCOMPARE(masked.layerImage(active), expected);
    }
    void pendingChangesCanBeCancelled_data()
    {
        QTest::addColumn<QString>("action");
        for (const auto &action :
             {"undo", "redo", "tolerance", "contiguous", "antialias", "sampling"})
            QTest::newRow(action) << QString::fromLatin1(action);
    }
    void pendingChangesCanBeCancelled()
    {
        QFETCH(QString, action);
        Document doc;
        QVERIFY(doc.newDocument(128, 96, false, 16));
        doc.setTool("magicEraser");
        doc.setMagicEraserTolerance(0);
        const auto before  = doc.layerImage(0);
        const int  history = doc.historyIndex();
        doc.beginStroke(20, 20);
        QVERIFY(doc.magicEraserBusy());
        QVERIFY(doc.pixelEditBusy());
        QVERIFY(doc.modified());
        if (action == "undo")
            doc.undo();
        else if (action == "redo")
            doc.redo();
        else if (action == "tolerance")
            doc.setMagicEraserTolerance(1);
        else if (action == "contiguous")
            doc.setMagicEraserContiguous(false);
        else if (action == "antialias")
            doc.setMagicEraserAntialias(false);
        else
            doc.setMagicEraserSampleAllLayers(true);
        QVERIFY(!doc.magicEraserBusy());
        QVERIFY(!doc.pixelEditBusy());
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(doc.layerImage(0), before);
        QCOMPARE(doc.historyIndex(), history);
        QVERIFY(!doc.modified());
    }
    void pendingWorkProtectsSaveAndNoOpKeepsTheDocumentClean()
    {
        QTemporaryDir dir;
        Document      doc;
        QVERIFY(doc.newDocument(128, 96, false, 16));
        doc.setTool("magicEraser");
        const int history = doc.historyIndex();
        doc.setBrushOpacity(0);
        doc.beginStroke(20, 20);
        QVERIFY(doc.pixelEditBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!doc.pixelEditBusy(), 5000);
        QCOMPARE(doc.historyIndex(), history);
        QVERIFY(!doc.modified());
        doc.setBrushOpacity(1);
        doc.beginStroke(20, 20);
        QVERIFY(doc.pixelEditBusy());
        const auto project  = QUrl::fromLocalFile(dir.filePath("magic.pcr"));
        const auto exported = QUrl::fromLocalFile(dir.filePath("magic.png"));
        QVERIFY(!doc.saveProject(project));
        QVERIFY(!doc.exportImage(exported));
        QVERIFY(!QFile::exists(project.toLocalFile()));
        QVERIFY(!QFile::exists(exported.toLocalFile()));
        QTRY_VERIFY_WITH_TIMEOUT(!doc.pixelEditBusy(), 5000);
        QVERIFY(doc.modified());
        QVERIFY(doc.saveProject(project));
        QVERIFY(!doc.modified());
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.channelBits(), 16);
        QCOMPARE(reopened.layerImage(0), doc.layerImage(0));
    }
};
QTEST_MAIN(MagicEraserCoverageTest)
#include "MagicEraserCoverageTest.moc"
