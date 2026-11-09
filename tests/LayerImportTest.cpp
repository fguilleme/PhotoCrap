#include "Document.h"
#include "RawTestImage.h"
#include <QFile>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>

class LayerImportTest : public QObject
{
    Q_OBJECT
private slots:
    void insertionAndNativeDepth_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<int>("insertion");
        for (int bits : {8, 16})
            for (int insertion : {0, 1, 2})
                QTest::newRow(qPrintable(QString("%1-bit-at-%2").arg(bits).arg(insertion)))
                    << bits << insertion;
    }
    void insertionAndNativeDepth()
    {
        QFETCH(int, bits);
        QFETCH(int, insertion);
        QTemporaryDir dir;
        QImage        image(8, 6,
                     bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                       : QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor::fromRgba64(12345, 23456, 34567, 45678));
        const auto file = QUrl::fromLocalFile(dir.filePath("Portrait.png"));
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(16, 12, true));
        d.addLayer();
        d.renameLayer(0, "Bottom");
        d.renameLayer(1, "Top");
        d.addLayerMask();
        d.setEditingMask(true);
        const auto before  = d.composite();
        const int  history = d.historyIndex();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({file}, insertion);
        QVERIFY(d.operationBusy());
        QCOMPARE(d.layers().size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.channelBits(), bits);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.activeLayer(), insertion);
        QCOMPARE(d.historyIndex(), history + 1);
        const auto layer = d.layers().at(2 - insertion).toMap();
        QCOMPARE(layer["name"].toString(), "Portrait");
        QVERIFY(!layer["editingMask"].toBool());
        const auto imported = d.layerImage(insertion);
        QCOMPARE(imported.pixelColor(0, 0).alpha(), 0);
        const auto decoded = QImage(file.toLocalFile()).convertToFormat(image.format());
        QCOMPARE(imported.format(), image.format());
        for (int y = 0; y < decoded.height(); ++y)
            for (int x = 0; x < decoded.width(); ++x)
                QCOMPARE(imported.pixelColor(x + 4, y + 3), decoded.pixelColor(x, y));
        d.undo();
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.composite(), before);
        d.redo();
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.layerImage(insertion), imported);
    }
    void batchOrderAndFailureAreAtomic()
    {
        QTemporaryDir dir;
        QList<QUrl>   files;
        for (const auto name : {"First", "Second"}) {
            const auto file = QUrl::fromLocalFile(dir.filePath(QString(name) + ".png"));
            QImage     image(8, 6, QImage::Format_RGB32);
            image.fill(Qt::red);
            QVERIFY(image.save(file.toLocalFile()));
            files.append(file);
        }
        Document d;
        QVERIFY(d.newDocument(16, 12, true, 16));
        const int history = d.historyIndex();
        d.requestImportLayers(files);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.layers()[0].toMap()["name"].toString(), "First");
        QCOMPARE(d.layers()[1].toMap()["name"].toString(), "Second");
        QCOMPARE(d.historyIndex(), history + 1);
        d.undo();
        QCOMPARE(d.layers().size(), 1);
        const auto before = d.composite();
        const auto bad    = QUrl::fromLocalFile(dir.filePath("broken.png"));
        QFile      broken(bad.toLocalFile());
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not an image");
        broken.close();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({files[0], bad});
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.composite(), before);
        QVERIFY(d.canRedo());
    }
    void rawImportAndRejectedFiles()
    {
        QTemporaryDir dir;
        const auto    raw = QUrl::fromLocalFile(dir.filePath("Camera.dng"));
        QFile         file(raw.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        Document d;
        QVERIFY(d.newDocument(128, 96, true));
        QVERIFY(d.canImportLayerFiles({raw}));
        QVERIFY(!d.canImportLayerFiles({}));
        QVERIFY(!d.canImportLayerFiles({QUrl("https://example.com/image.png")}));
        QVERIFY(!d.canImportLayerFiles({QUrl::fromLocalFile(dir.path())}));
        QVERIFY(!d.canImportLayerFiles({QUrl::fromLocalFile(dir.filePath("missing.png"))}));
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.requestImportLayers({raw});
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.layers()[0].toMap()["name"].toString(), "Camera");
    }
    void cancellationLeavesStackUntouched()
    {
        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("Image.png"));
        QImage        image(8, 6, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        QVERIFY(d.newDocument(16, 12, true));
        const int history = d.historyIndex();
        auto *    pool    = QThreadPool::globalInstance();
        pool->waitForDone();
        const int maximum = pool->maxThreadCount();
        pool->setMaxThreadCount(1);
        QSemaphore entered, release;
        pool->start([&] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
        d.requestImportLayers({file});
        const bool busy = d.operationBusy();
        d.cancelOperation();
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
        QVERIFY(busy);
        QTRY_VERIFY(!d.operationBusy());
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.historyIndex(), history);
    }
    void groupBoundaries()
    {
        QTemporaryDir dir;
        const auto    file = QUrl::fromLocalFile(dir.filePath("Image.png"));
        QImage        image(8, 6, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(file.toLocalFile()));
        Document d;
        QVERIFY(d.newDocument(16, 12, true));
        d.addLayer();
        d.selectLayer(0, true);
        d.groupSelectedLayers();
        const auto group = d.layers()[0].toMap()["groupId"].toString();
        QVERIFY(!group.isEmpty());
        d.requestImportLayers({file}, 1);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers()[2].toMap()["groupId"].toString(), group);
        d.requestImportLayers({file}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QVERIFY(d.layers().last().toMap()["groupId"].toString().isEmpty());
    }
};
QTEST_MAIN(LayerImportTest)
#include "LayerImportTest.moc"
