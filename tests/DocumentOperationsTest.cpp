#include "Document.h"
#include "RawTestImage.h"
#include <QBuffer>
#include <QColorSpace>
#include <QElapsedTimer>
#include <QFile>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QtTest>

namespace
{
struct BlockPool {
    QThreadPool *pool    = QThreadPool::globalInstance();
    int          maximum = pool->maxThreadCount();
    QSemaphore   entered, release;
    BlockPool()
    {
        pool->waitForDone();
        pool->setMaxThreadCount(1);
        pool->start([this] {
            entered.release();
            release.acquire();
        });
        entered.acquire();
    }
    ~BlockPool()
    {
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
    }
};
void populate(Document &d, int bits)
{
    d.setBackgroundEditThreshold(-1);
    d.newDocument(80, 60, false, bits);
    d.setColor(QColor(157, 31, 79));
    d.setBrushSize(20);
    d.beginStroke(18, 21);
    d.continueStroke(60, 40);
    d.endStroke();
    d.addLayer();
    d.setColor(QColor(31, 126, 203, 153));
    d.setBrushSize(15);
    d.beginStroke(30, 10);
    d.continueStroke(55, 28);
    d.endStroke();
}
void frequencyFixture(Document &_document, int _bits, const QString &_kind)
{
    populate(_document, _bits);
    if (_kind == "mask") {
        _document.addLayerMask();
        _document.setTool("pencil");
        _document.setColor(Qt::black);
        _document.setBrushSize(17);
        _document.beginStroke(30, 22);
        _document.endStroke();
        _document.setEditingMask(false);
        _document.beginLayerPreview();
        _document.previewLayerParameter("_maskDensity", 63);
        _document.previewLayerParameter("_maskFeather", 4);
        _document.finishLayerPreview("Mask");
    } else if (_kind == "adjustment") {
        _document.addAdjustmentLayer("boxBlur");
        _document.beginLayerPreview();
        _document.previewLayerParameter("radius", 3);
        _document.finishLayerPreview("Blur");
    } else
        _document.addFrequencySeparation(3);
}
void operation(Document &d, const QString &name)
{
    if (name == "resize")
        d.resizeImage(53, 41);
    else if (name == "crop")
        d.cropToSelection();
    else if (name == "depth")
        d.requestChannelBits(d.channelBits() == 8 ? 16 : 8);
    else if (name == "fill")
        d.fillWithColor(true, true);
    else if (name.startsWith("bucket")) {
        d.setTool("fill");
        d.setBrushOpacity(.63);
        d.setColor(QColor::fromRgba64(14321, 32123, 54321, 41234));
        d.beginStroke(10, 15);
    } else if (name == "clear")
        d.clearSelected();
    else if (name == "merge")
        d.mergeDown();
    else if (name == "invertMask")
        d.invertLayerMask();
    else if (name == "filter")
        d.applyFilter("blur", 3);
    else
        d.transformImage(name);
}
} // namespace
class DocumentOperationsTest : public QObject
{
    Q_OBJECT
private slots:
    void frequencySourcePreparationMatchesSynchronousWork_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("warm");
        for (int bits : {8, 16})
            for (const QString kind :
                 {QString("mask"), QString("adjustment"), QString("frequency")})
                for (bool warm : {false, true})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(bits).arg(kind).arg(warm)))
                        << bits << kind << warm;
    }
    void frequencySourcePreparationMatchesSynchronousWork()
    {
        QFETCH(int, bits);
        QFETCH(QString, kind);
        QFETCH(bool, warm);
        Document d, reference;
        frequencyFixture(d, bits, kind);
        frequencyFixture(reference, bits, kind);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(d.saveProject(QUrl::fromLocalFile(directory.filePath("source.pcr"))));
        QVERIFY(!d.modified());
        if (warm)
            QVERIFY(!d.composite().isNull());
        const auto before  = d.layers();
        const auto history = d.history();
        const int  index = d.historyIndex(), count = d.layers().size();
        reference.addFrequencySeparation(7);
        d.setBackgroundEditThreshold(0);
        QSignalSpy geometry(&d, &Document::canvasSizeChanged);
        QSignalSpy errors(&d, &Document::errorOccurred);
        {
            BlockPool blocked;
            d.addFrequencySeparation(7);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.layers(), before);
            QCOMPARE(d.history(), history);
            QVERIFY(d.modified()); // Pending pixel edits protect Save/Close like other workers.
            bool heartbeat = false;
            QTimer::singleShot(0, &d, [&] { heartbeat = true; });
            QCoreApplication::processEvents();
            QVERIFY(heartbeat);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(geometry.size(), 0);
        QCOMPARE(d.width(), 80);
        QCOMPARE(d.height(), 60);
        QCOMPARE(d.layers().size(), count + 2);
        QCOMPARE(d.historyIndex(), index + 1);
        QVERIFY(d.modified());
        for (int i = 0; i < count + 2; ++i) {
            const auto actual = d.layers()[i].toMap(), expected = reference.layers()[i].toMap();
            QCOMPARE(actual["kind"], expected["kind"]);
            QCOMPARE(actual["parameters"], expected["parameters"]);
        }
        const auto low = reference.layerImage(count), high = reference.layerImage(count + 1);
        QCOMPARE(d.layerImage(count), low);
        QCOMPARE(d.layerImage(count + 1), high);
        QCOMPARE(d.composite(), reference.composite());
        if (kind == "frequency") {
            QSet<QString> pairs;
            for (const auto &layer : d.layers()) {
                const auto info = layer.toMap();
                if (info["kind"] == "frequencyHigh")
                    pairs.insert(info["parameters"].toMap()["pairId"].toString());
            }
            QCOMPARE(pairs.size(), 2);
        }
        d.undo();
        QCOMPARE(d.layers(), before);
        QVERIFY(!d.modified());
        d.redo();
        QCOMPARE(d.layerImage(count), low);
        QCOMPARE(d.layerImage(count + 1), high);
        QCOMPARE(geometry.size(), 0);
    }
    void composedFrequencySourceBorrowsItsCompletedNativeImage()
    {
        Document d;
        populate(d, 16);
        d.selectAllLayers();
        QVERIFY(d.canGroupSelected());
        d.groupSelectedLayers();
        QCOMPARE(d.layers().first().toMap().value("kind").toString(), QString("group"));
        const int     count = d.layers().size(), index = d.activeLayer();
        const QImage &completed = d.composite();
        QVERIFY(completed.isDetached());
        const auto key = completed.cacheKey();
        d.setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            d.addFrequencySeparation(7);
            QVERIFY(d.operationBusy());
            QVERIFY(!completed.isDetached());
        }
        // Sealing the source releases the full-frame borrow before the GUI
        // accepts the result. Its original cached pixels remain untouched.
        QVERIFY(completed.isDetached());
        QCOMPARE(completed.cacheKey(), key);
        QCOMPARE(d.layers().size(), count);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(d.layers().size(), count + 2);
        QVERIFY(!d.layerImage(index + 1).isNull());
        QVERIFY(!d.layerImage(index + 2).isNull());
    }

    void frequencySourceUsesPreparationBudgetBeforeGuiPublication_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("available");
        for (int bits : {8, 16})
            for (const QString kind : {QString("mask"), QString("adjustment")})
                for (bool available : {false, true})
                    QTest::newRow(
                        qPrintable(QString("%1-%2-%3").arg(bits).arg(kind).arg(available)))
                        << bits << kind << available;
    }
    void frequencySourceUsesPreparationBudgetBeforeGuiPublication()
    {
        QFETCH(int, bits);
        QFETCH(QString, kind);
        QFETCH(bool, available);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Document d, reference;
        frequencyFixture(d, bits, kind);
        frequencyFixture(reference, bits, kind);
        const int count = d.layers().size();
        reference.addFrequencySeparation(7);
        const auto before  = d.layers();
        const auto history = d.history();
        QString    cache   = directory.filePath("cache");
        if (!available) {
            QFile blocker(cache);
            QVERIFY(blocker.open(QIODevice::WriteOnly));
        }
        d.configureScratchCache(cache, 16 * 1024 * 1024);
        d.configurePlaneRamMiB(0);
        d.setBackgroundEditThreshold(0);
        QSignalSpy errors(&d, &Document::errorOccurred);
        {
            BlockPool blocked;
            d.addFrequencySeparation(7);
            QVERIFY(d.operationBusy());
        }
        // The worker and its source spill are complete. The GUI has not
        // accepted its snapshot yet, and cannot have run its spill timer.
        QVERIFY(d.operationBusy());
        QCOMPARE(d.layers(), before);
        QCOMPARE(d.history(), history);
        if (available)
            QVERIFY(d.scratchCacheUsage()["diskBytes"].toLongLong() > 0);
        else
            QVERIFY(!d.scratchCacheUsage()["error"].toString().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QVERIFY(errors.isEmpty());
        QCOMPARE(d.layers().size(), count + 2);
        d.clearScratchCache();
        QCOMPARE(d.layerImage(count), reference.layerImage(count));
        QCOMPARE(d.layerImage(count + 1), reference.layerImage(count + 1));
        QCOMPARE(d.composite(), reference.composite());
        d.undo();
        QCOMPARE(d.layers(), before);
        d.redo();
        QCOMPARE(d.layerImage(count), reference.layerImage(count));
        QCOMPARE(d.layerImage(count + 1), reference.layerImage(count + 1));
    }

    void frequencyPairIdentityAvoidsLoadedAndPreparedLinks_data()
    {
        QTest::addColumn<bool>("asynchronous");
        QTest::newRow("sync") << false;
        QTest::newRow("async") << true;
    }
    void frequencyPairIdentityAvoidsLoadedAndPreparedLinks()
    {
        QFETCH(bool, asynchronous);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Document original;
        QVERIFY(original.newDocument(37, 31, false));
        original.addFrequencySeparation(3);
        const auto url = QUrl::fromLocalFile(directory.filePath("links.pcr"));
        QVERIFY(original.saveProject(url));
        QFile file(url.toLocalFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto       layers  = root["layers"].toArray();
        const auto setPair = [&](const QString &_id) {
            for (int i : {1, 2}) {
                auto layer           = layers[i].toObject();
                auto parameters      = layer["parameters"].toObject();
                parameters["pairId"] = _id;
                layer["parameters"]  = parameters;
                layers[i]            = layer;
            }
        };
        const auto save = [&] {
            root["layers"] = layers;
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return false;
            const bool ok = file.write(QJsonDocument(root).toJson()) > 0;
            file.close();
            return ok;
        };
        setPair("existing-dynamic");
        QVERIFY(save());
        // Find this freshly opened document's next candidate without relying
        // on a particular constructor/history counter initialization.
        Document control;
        control.setBackgroundEditThreshold(-1);
        QVERIFY(control.openFile(url));
        control.addFrequencySeparation(7);
        QString candidate;
        for (const auto &value : control.layers()) {
            const auto info = value.toMap();
            if (info["layerIndex"].toInt() == control.activeLayer())
                candidate = info["parameters"].toMap()["pairId"].toString();
        }
        QVERIFY(!candidate.isEmpty());
        setPair(candidate);
        const QString prepared = candidate + "-1";
        for (const int i : {1, 2}) {
            auto layer          = layers[i].toObject();
            layer["kind"]       = "pixel";
            layer["visible"]    = false;
            layer["parameters"] = QJsonObject{{"_frequencyPairId", prepared},
                                              {"_frequencyBand", i == 1 ? "low" : "high"},
                                              {"_frequencyRadius", 3}};
            QByteArray bytes;
            QBuffer    buffer(&bytes);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QVERIFY(original.layerImage(i).save(&buffer, "PNG"));
            layer["png"] = QString::fromLatin1(bytes.toBase64());
            layers.append(layer);
        }
        QVERIFY(save());
        Document d;
        QVERIFY(d.openFile(url));
        d.setBackgroundEditThreshold(asynchronous ? 0 : -1);
        d.addFrequencySeparation(7);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QString actual;
        for (const auto &value : d.layers()) {
            const auto info = value.toMap();
            if (info["layerIndex"].toInt() == d.activeLayer())
                actual = info["parameters"].toMap()["pairId"].toString();
        }
        QCOMPARE(actual, candidate + "-2");
        QVERIFY(actual != prepared);
        QCOMPARE(d.layers().size(), 7);
        d.undo();
        QCOMPARE(d.layers().size(), 5);
        d.redo();
        QCOMPARE(d.layers().size(), 7);
    }

    void frequencyPreparationSubmissionProbe()
    {
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_FREQUENCY_CREATE_PROBE"))
            QSKIP("Set PHOTOCRAP_FREQUENCY_CREATE_PROBE=1 to measure 24 MP source preparation.");
        Document d;
        d.setBackgroundEditThreshold(-1);
        QVERIFY(d.newDocument(6000, 4000, false, 16));
        d.setTool("gradient");
        d.setColor(Qt::red);
        d.setSecondaryColor(Qt::blue);
        d.beginStroke(0, 0);
        d.continueStroke(6000, 4000);
        d.endStroke();
        d.addLayerMask();
        d.setColor(QColor(127, 127, 127));
        d.fillWithColor(true);
        d.setEditingMask(false);
        d.setBackgroundEditThreshold(4LL * 1024 * 1024);
        const auto    history = d.historyIndex();
        QElapsedTimer timer;
        timer.start();
        d.addFrequencySeparation(8);
        const auto submitted = timer.nsecsElapsed();
        const bool queued    = d.operationBusy();
        if (queued) {
            QThreadPool::globalInstance()->waitForDone();
            QCoreApplication::processEvents();
            QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        }
        QCOMPARE(d.layers().size(), 3);
        QCOMPARE(d.historyIndex(), history + 1);
        qInfo() << "24 MP / 16-bit masked source, submit ns, total ns and queued:" << submitted
                << timer.nsecsElapsed() << queued;
    }

    void directFrequencySourcesStayImmediate_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<bool>("compact");
        QTest::addColumn<bool>("mask");
        for (int bits : {8, 16})
            for (bool compact : {false, true})
                for (bool mask : {false, true})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(bits).arg(compact).arg(mask)))
                        << bits << compact << mask;
    }
    void directFrequencySourcesStayImmediate()
    {
        QFETCH(int, bits);
        QFETCH(bool, compact);
        QFETCH(bool, mask);
        Document d;
        QVERIFY(d.newDocument(80, 60, false, bits));
        if (compact)
            d.addLayer();
        if (mask) {
            d.addLayerMask();
            d.setEditingMask(false);
        }
        const int count = d.layers().size(), history = d.historyIndex();
        d.setBackgroundEditThreshold(0);
        BlockPool blocked;
        d.addFrequencySeparation(7);
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.layers().size(), count + 2);
        QCOMPARE(d.historyIndex(), history + 1);
    }
    void frequencyPreparationRejectsCancelledAndStaleWork_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("action");
        for (const QString kind : {QString("mask"), QString("adjustment")})
            for (const QString action :
                 {QString("cancel"), QString("undo"), QString("redo"), QString("replace"),
                  QString("selection"), QString("rename"), QString("paint"),
                  QString("newOperation")})
                QTest::newRow(qPrintable(kind + '-' + action)) << kind << action;
    }
    void frequencyPreparationRejectsCancelledAndStaleWork()
    {
        QFETCH(QString, kind);
        QFETCH(QString, action);
        Document d;
        frequencyFixture(d, 16, kind);
        if (action == "redo")
            d.undo();
        d.setBackgroundEditThreshold(0);
        QVariantList expected;
        QStringList  history;
        QRegion      selection;
        QImage       pixels;
        {
            BlockPool blocked;
            d.addFrequencySeparation(7);
            QVERIFY(d.operationBusy());
            if (action == "cancel")
                d.cancelOperation();
            else if (action == "undo")
                d.undo();
            else if (action == "redo")
                d.redo();
            else if (action == "replace")
                QVERIFY(d.newDocument(31, 23, true, 16));
            else if (action == "selection")
                d.selectAll();
            else if (action == "rename")
                d.renameLayer(d.activeLayer(), "New layer name");
            else if (action == "paint") {
                d.setActiveLayer(0);
                d.setEditingMask(false);
                d.setTool("pencil");
                d.setColor(Qt::blue);
                d.setBrushSize(5);
                d.beginStroke(11, 13);
                d.endStroke();
            } else {
                d.cancelOperation();
                d.addFrequencySeparation(15);
                QVERIFY(d.operationBusy());
            }
            expected  = d.layers();
            history   = d.history();
            selection = d.selectionRegion();
            pixels    = d.layerImage(0);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        if (action == "newOperation") {
            QCOMPARE(d.layers().size(), expected.size() + 2);
            QCOMPARE(d.historyIndex(), history.size());
            QCOMPARE(d.layers()[0].toMap()["parameters"].toMap()["radius"].toDouble(), 15.);
            QCOMPARE(d.layers()[1].toMap()["parameters"].toMap()["radius"].toDouble(), 15.);
        } else {
            QCOMPARE(d.layers(), expected);
            QCOMPARE(d.history(), history);
        }
        QCOMPARE(d.selectionRegion(), selection);
        QCOMPARE(d.layerImage(0), pixels);
    }
    void frequencyPreparationCanOutliveItsDocument()
    {
        for (const QString kind : {QString("mask"), QString("adjustment")}) {
            BlockPool blocked;
            auto      d = std::make_unique<Document>();
            frequencyFixture(*d, 16, kind);
            d->setBackgroundEditThreshold(0);
            d->addFrequencySeparation(7);
            QVERIFY(d->operationBusy());
            d.reset();
        }
    }

    void replacementFileLoadsDoNotRetainTheDiscardedDocumentsTiles_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::newRow("image") << QString("image");
        QTest::newRow("project") << QString("project");
        QTest::newRow("raw") << QString("raw");
    }
    void replacementFileLoadsDoNotRetainTheDiscardedDocumentsTiles()
    {
        QFETCH(QString, kind);
        QTemporaryDir directory;
        const auto    input = QUrl::fromLocalFile(directory.filePath("source.png"));
        QImage        pixels(512, 512, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        QVERIFY(pixels.save(input.toLocalFile()));
        Document d;
        QVERIFY(d.openFile(input));
        QImage tile;
        {
            const auto stack = d.gpuLayerStack();
            QVERIFY(stack.valid);
            tile = stack.layers.first().image.tileImage(0, 0);
        }
        QVERIFY(!tile.isDetached());
        QUrl target = input;
        if (kind == "project") {
            target = QUrl::fromLocalFile(directory.filePath("source.pcr"));
            QVERIFY(d.saveProject(target));
        } else if (kind == "raw") {
            target = QUrl::fromLocalFile(directory.filePath("sample.dng"));
            QFile file(target.toLocalFile());
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(RawTestImage::dng());
        }
        d.setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            if (kind == "raw")
                QVERIFY(d.openRawFile(target));
            else
                d.requestOpenFile(target);
            QVERIFY(d.newDocument(31, 23, true));
            // This QImage is now the only owner of its native pixels. The queued
            // file load must not pin the old layer/history snapshot behind it.
            QVERIFY(tile.isDetached());
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy() && !d.rawLoading(), 10000);
        QCOMPARE(d.width(), 31);
        QCOMPARE(d.height(), 23);
        QVERIFY(tile.isDetached());
    }
    void rawPlanesArePreparedBeforeTheGuiCallback_data()
    {
        QTest::addColumn<bool>("import");
        QTest::addColumn<bool>("cacheAvailable");
        for (bool import : {false, true})
            for (bool cache : {false, true})
                QTest::newRow(qPrintable(QString(import ? "import-" : "open-") +
                                         (cache ? "disk" : "unavailable")))
                    << import << cache;
    }
    void rawPlanesArePreparedBeforeTheGuiCallback()
    {
        QFETCH(bool, import);
        QFETCH(bool, cacheAvailable);
        QTemporaryDir directory;
        const auto    target = QUrl::fromLocalFile(directory.filePath("large.dng"));
        QFile         file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng(6, 1024, 768));
        file.close();
        Document   d, reference;
        const auto project = QUrl::fromLocalFile(directory.filePath("original.pcr"));
        for (Document *doc : {&d, &reference}) {
            doc->setBackgroundEditThreshold(-1);
            QVERIFY(doc->newDocument(512, 512, false, 8));
            doc->setTool("brush");
            doc->setColor(Qt::red);
            doc->setBrushSize(12);
            doc->setBrushSmoothing(0);
            doc->beginStroke(100, 100);
            doc->endStroke();
            doc->addLayerMask();
            doc->setColor(Qt::black);
            doc->beginStroke(100, 100);
            doc->endStroke();
            doc->setEditingMask(false);
            QVERIFY(doc->saveProject(project));
        }
        QVERIFY(import ? reference.importLayer(target) : reference.openFile(target));
        const auto expected = reference.composite();
        const auto before   = d.composite();
        const auto history  = d.history();
        const int  index    = d.historyIndex();
        QString    cache    = directory.filePath("cache");
        if (!cacheAvailable) {
            QFile blocker(cache);
            QVERIFY(blocker.open(QIODevice::WriteOnly));
        }
        d.configureScratchCache(cache, 64LL * 1024 * 1024);
        d.configurePlaneRamMiB(0);
        d.setTool("eraser");
        d.setBrushSize(39);
        QSignalSpy opened(&d, &Document::rawFileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        {
            BlockPool blocked;
            QVERIFY(d.openRawFile(target, import));
            QVERIFY(d.rawLoading());
            QCOMPARE(d.composite(), before);
            QCOMPARE(d.history(), history);
        }
        // Decode, tiling, centering, depth promotion and spill have all finished;
        // the GUI still has not accepted their prepared snapshot.
        QVERIFY(d.rawLoading());
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.channelBits(), 8);
        if (cacheAvailable)
            QVERIFY(d.scratchCacheUsage()["diskBytes"].toLongLong() > 0);
        QTRY_VERIFY_WITH_TIMEOUT(!d.rawLoading(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(opened.size(), import ? 0 : 1);
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layers(), reference.layers());
        QCOMPARE(d.historyIndex(), import ? index + 1 : 0);
        QCOMPARE(d.projectUrl(), import ? project : QUrl{});
        QCOMPARE(d.tool(), QString("eraser"));
        QCOMPARE(d.brushSize(), 39.);
        QCOMPARE(d.memoryUsage()["retainedDecoderBytes"].toLongLong(), qint64(0));
        const auto stack = d.gpuLayerStack();
        QVERIFY(stack.valid);
        qint64 heap = 0;
        for (const auto &layer : stack.layers)
            for (auto bytes : layer.image.tiles().heapBlocks())
                heap += bytes;
        if (cacheAvailable) {
            QCOMPARE(heap, qint64(0));
            QVERIFY(d.scratchCacheUsage()["diskBytes"].toLongLong() > 0);
        } else {
            QVERIFY(heap > 0);
            QVERIFY(!d.scratchCacheUsage()["error"].toString().isEmpty());
        }
        d.clearScratchCache();
        QCOMPARE(d.layerImage(d.activeLayer()), reference.layerImage(reference.activeLayer()));
        if (import) {
            d.undo();
            QCOMPARE(d.channelBits(), 8);
            QCOMPARE(d.composite(), before);
            QVERIFY(!d.modified());
            d.redo();
            QCOMPARE(d.composite(), expected);
        }
    }
    void rawPreparationCanBeDiscardedAtEveryHandoff_data()
    {
        QTest::addColumn<bool>("import");
        QTest::addColumn<QString>("action");
        for (bool import : {false, true})
            for (const auto *name :
                 {"queued", "finished", "new-document", "new-gesture", "destroy"})
                QTest::newRow(qPrintable(QString(import ? "import-" : "open-") + name))
                    << import << QString(name);
    }
    void rawPreparationCanBeDiscardedAtEveryHandoff()
    {
        QFETCH(bool, import);
        QFETCH(QString, action);
        QTemporaryDir directory;
        const auto    target = QUrl::fromLocalFile(directory.filePath("sample.dng"));
        QFile         file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        auto d = std::make_unique<Document>();
        populate(*d, 8);
        const auto before  = d->composite();
        const auto history = d->history();
        QSignalSpy opened(d.get(), &Document::rawFileOpened);
        QSignalSpy errors(d.get(), &Document::errorOccurred);
        {
            BlockPool blocked;
            QVERIFY(d->openRawFile(target, import));
            if (action == "queued")
                d->cancelRawLoad();
            else if (action == "new-document")
                QVERIFY(d->newDocument(31, 23, true));
            else if (action == "new-gesture") {
                d->setTool("brush");
                d->beginStroke(40, 20);
                d->endStroke();
            } else if (action == "destroy")
                d.reset();
        }
        if (d) {
            if (action == "finished")
                d->cancelRawLoad();
            const auto current        = d->composite();
            const auto currentHistory = d->history();
            QTRY_VERIFY_WITH_TIMEOUT(!d->rawLoading(), 10000);
            QCOMPARE(d->composite(), current);
            QCOMPARE(d->history(), currentHistory);
            if (action == "queued" || action == "finished") {
                QCOMPARE(d->composite(), before);
                QCOMPARE(d->history(), history);
            }
            QCOMPARE(opened.size(), 0);
            QCOMPARE(errors.size(), 0);
        }
    }
    void rawImportDepthLimitFailureDoesNotChangeTheLiveStack()
    {
        QTemporaryDir directory;
        const auto    target = QUrl::fromLocalFile(directory.filePath("sample.dng"));
        QFile         file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(RawTestImage::dng());
        file.close();
        Document d;
        QVERIFY(d.newDocument(6000, 4000, false, 8));
        for (int i = 0; i < 11; ++i)
            d.duplicateLayer();
        QCOMPARE(d.layers().size(), 12);
        const auto layers  = d.layers();
        const auto history = d.history();
        QSignalSpy errors(&d, &Document::errorOccurred);
        QVERIFY(d.openRawFile(target, true));
        QTRY_VERIFY_WITH_TIMEOUT(!d.rawLoading(), 10000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.history(), history);
    }
    void queuedOffloadGroupsStayImmutableWhileTheDocumentIsPainted()
    {
        QTemporaryDir directory;
        const auto    input = QUrl::fromLocalFile(directory.filePath("source.png"));
        QImage        pixels(512, 512, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        QVERIFY(pixels.save(input.toLocalFile()));
        Document d;
        QVERIFY(d.openFile(input));
        const auto                    original = d.gpuLayerStack();
        const auto                    before   = d.composite();
        PhotoCrap::Render::LayerStack painted;
        {
            BlockPool blocked;
            d.configureScratchCache(directory.filePath("cache"), 64LL * 1024 * 1024);
            d.configurePlaneRamMiB(0);
            QTest::qWait(650);
            d.setTool("brush");
            d.setColor(Qt::red);
            d.setBrushSize(12);
            d.setBrushHardness(1);
            d.setBrushSmoothing(0);
            d.beginStroke(100, 100);
            d.endStroke();
            painted = d.gpuLayerStack();
            QVERIFY(painted.layers.first().image.cacheKey() !=
                    original.layers.first().image.cacheKey());
        }
        // The worker walks its captured groups, not the now-painted live layers.
        QCOMPARE(original.layers.first().image.tiles().heapBytes(), qint64(0));
        QCOMPARE(painted.layers.first().image.tiles().heapBytes(), 256LL * 256 * 4);
        const auto after = painted.layers.first().image.region({0, 0, 512, 512});
        QVERIFY(after != before);
        d.undo();
        QCOMPARE(d.composite(), before);
        d.redo();
        QCOMPARE(d.composite(), after);
    }
    void staleQueuedOffloadsStopWhenTheirContextChanges_data()
    {
        QTest::addColumn<QString>("action");
        for (const auto *name : {"new-document", "directory", "budget", "destroy"})
            QTest::newRow(name) << QString(name);
    }
    void staleQueuedOffloadsStopWhenTheirContextChanges()
    {
        QFETCH(QString, action);
        QTemporaryDir directory;
        const auto    input = QUrl::fromLocalFile(directory.filePath("source.png"));
        QImage        pixels(512, 512, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        QVERIFY(pixels.save(input.toLocalFile()));
        auto d = std::make_unique<Document>();
        QVERIFY(d->openFile(input));
        const auto original = d->gpuLayerStack();
        QVERIFY(original.valid);
        const auto heap = original.layers.first().image.tiles().heapBytes();
        QVERIFY(heap > 0);
        const auto expected = original.layers.first().image.region({0, 0, 512, 512});
        {
            BlockPool blocked;
            d->configureScratchCache(directory.filePath("cache"), 64LL * 1024 * 1024);
            d->configurePlaneRamMiB(0);
            // The timer fires, but its offload is queued behind the blocked pool.
            QTest::qWait(650);
            QCOMPARE(original.layers.first().image.tiles().heapBytes(), heap);
            if (action == "new-document")
                QVERIFY(d->newDocument(31, 23, true));
            else if (action == "directory")
                d->configureScratchCache(directory.filePath("replacement"), 64LL * 1024 * 1024);
            else if (action == "budget")
                d->configurePlaneRamMiB(8);
            else
                d.reset();
        }
        // No GUI event is needed to stop the stale worker once it gets its turn.
        QCOMPARE(original.layers.first().image.tiles().heapBytes(), heap);
        QCOMPARE(original.layers.first().image.region({0, 0, 512, 512}), expected);
    }
    void decodedPlanesUseTheRamBudgetAndKeepTheirCacheAfterOpen_data()
    {
        QTest::addColumn<bool>("project");
        QTest::addColumn<bool>("cacheAvailable");
        for (bool project : {false, true})
            for (bool cache : {false, true})
                QTest::newRow(qPrintable(QString(project ? "project-" : "image-") +
                                         (cache ? "disk" : "unavailable")))
                    << project << cache;
    }
    void decodedPlanesUseTheRamBudgetAndKeepTheirCacheAfterOpen()
    {
        QFETCH(bool, project);
        QFETCH(bool, cacheAvailable);
        QTemporaryDir directory;
        const auto    image = QUrl::fromLocalFile(directory.filePath("source.png"));
        const auto    file  = QUrl::fromLocalFile(directory.filePath("source.pcr"));
        QImage        pixels(512, 512, QImage::Format_RGBA64_Premultiplied);
        pixels.setColorSpace(QColorSpace::SRgb);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y,
                                     QColor::fromRgba64(ushort(x * 97), ushort(y * 131),
                                                        ushort((x + y) * 53), 65535));
        QVERIFY(pixels.save(image.toLocalFile()));
        Document source;
        QVERIFY(source.openFile(image));
        source.duplicateLayer();
        source.addLayerMask();
        source.setColor(Qt::black);
        source.setBrushSize(12);
        source.beginStroke(100, 100);
        source.endStroke();
        QVERIFY(source.saveProject(file));
        const auto target = project ? file : image;
        Document   reference;
        QVERIFY(reference.openFile(target));
        const auto expected      = reference.composite();
        const auto expectedLayer = reference.layerImage(0);
        Document   d;
        populate(d, 8);
        const auto before = d.composite();
        QString    cache  = directory.filePath("cache");
        if (!cacheAvailable) {
            QFile blocker(cache);
            QVERIFY(blocker.open(QIODevice::WriteOnly));
        }
        d.configureScratchCache(cache, 64LL * 1024 * 1024);
        d.configurePlaneRamMiB(1);
        d.setBackgroundEditThreshold(0);
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        {
            BlockPool blocked;
            d.requestOpenFile(target);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.composite(), before);
        }
        QCOMPARE(d.composite(), before);
        if (cacheAvailable)
            QVERIFY(d.scratchCacheUsage()["diskBytes"].toLongLong() >= 2LL * 1024 * 1024);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layers(), reference.layers());
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.modified(), !project);
        QCOMPARE(d.scratchCacheUsage()["directory"].toString(), cache);
        if (cacheAvailable) {
            QVERIFY(d.scratchCacheUsage()["diskBytes"].toLongLong() >= 2LL * 1024 * 1024);
            QVERIFY(d.memoryUsage()["tileHeapBytes"].toLongLong() <= 1024 * 1024);
        } else {
            QVERIFY(!d.scratchCacheUsage()["error"].toString().isEmpty());
            QVERIFY(d.memoryUsage()["tileHeapBytes"].toLongLong() >= 2LL * 1024 * 1024);
        }
        if (project)
            QCOMPARE(d.maskImage(d.activeLayer()), reference.maskImage(reference.activeLayer()));
        d.clearScratchCache();
        QCOMPARE(d.layerImage(0), expectedLayer);
        QCOMPARE(d.composite(), expected);
    }
    void changingCacheSettingsDuringDecodeKeepsTheNewConfiguration()
    {
        QTemporaryDir directory;
        const auto    image = QUrl::fromLocalFile(directory.filePath("source.png"));
        QImage        pixels(512, 512, QImage::Format_RGBA64_Premultiplied);
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                pixels.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        QVERIFY(pixels.save(image.toLocalFile()));
        Document d, reference;
        populate(d, 8);
        QVERIFY(reference.openFile(image));
        d.configureScratchCache(directory.filePath("old-cache"), 64LL * 1024 * 1024);
        d.configurePlaneRamMiB(1);
        d.setBackgroundEditThreshold(0);
        const auto newCache = directory.filePath("new-cache");
        {
            BlockPool blocked;
            d.requestOpenFile(image);
            d.configureScratchCache(newCache, 32LL * 1024 * 1024);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(d.scratchCacheUsage()["directory"].toString(), newCache);
        QCOMPARE(d.composite(), reference.composite());
        d.clearScratchCache();
        QCOMPARE(d.layerImage(0), reference.layerImage(0));
    }
    void preparedPlanesUseTheCapturedRamBudgetBeforePublication_data()
    {
        QTest::addColumn<bool>("import");
        QTest::addColumn<int>("cacheMode");
        for (bool import : {false, true})
            for (int cache : {0, 1, 2})
                QTest::newRow(qPrintable(QString(import ? "import-" : "depth-") +
                                         (cache == 1   ? "disk"
                                          : cache == 2 ? "capacity"
                                                       : "unavailable")))
                    << import << cache;
    }
    void preparedPlanesUseTheCapturedRamBudgetBeforePublication()
    {
        QFETCH(bool, import);
        QFETCH(int, cacheMode);
        const bool    cacheAvailable = cacheMode == 1;
        QTemporaryDir directory;
        const auto    input    = QUrl::fromLocalFile(directory.filePath("source.png"));
        const auto    imported = QUrl::fromLocalFile(directory.filePath("import.png"));
        QImage        source(512, 512, QImage::Format_ARGB32_Premultiplied);
        source.setColorSpace(QColorSpace::SRgb);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256));
        QVERIFY(source.save(input.toLocalFile()));
        QVERIFY(source.convertToFormat(QImage::Format_RGBA64_Premultiplied)
                    .save(imported.toLocalFile()));
        Document d, reference;
        for (Document *doc : {&d, &reference}) {
            doc->setBackgroundEditThreshold(-1);
            QVERIFY(doc->openFile(input));
            doc->duplicateLayer();
        }
        const auto before  = d.composite();
        const auto history = d.history();
        const int  index   = d.historyIndex();
        QVERIFY(import ? reference.importLayer(imported) : reference.setChannelBits(16));
        const auto expected = reference.composite();
        QString    cache    = directory.filePath("cache");
        if (cacheMode == 0) {
            QFile blocker(cache);
            QVERIFY(blocker.open(QIODevice::WriteOnly));
        }
        d.configureScratchCache(cache, (cacheMode == 2 ? 1LL : 64LL) * 1024 * 1024);
        // This must be captured by the worker, rather than using its default 256 MiB.
        d.configurePlaneRamMiB(1);
        d.setBackgroundEditThreshold(0);
        QSignalSpy errors(&d, &Document::errorOccurred);
        {
            BlockPool blocked;
            if (import)
                d.requestImportLayer(imported);
            else
                d.requestChannelBits(16);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.channelBits(), 8);
            QCOMPARE(d.history(), history);
        }
        // The file cache is populated before the GUI applies the prepared planes.
        QCOMPARE(d.channelBits(), 8);
        QVERIFY(d.operationBusy());
        const auto usage = d.scratchCacheUsage();
        if (cacheAvailable) {
            QVERIFY(usage["diskBytes"].toLongLong() >= 4LL * 1024 * 1024);
            QVERIFY(usage["error"].toString().isEmpty());
        } else {
            QVERIFY(!usage["error"].toString().isEmpty());
            if (cacheMode == 2) {
                QVERIFY(usage["diskBytes"].toLongLong() > 0);
                QVERIFY(usage["diskBytes"].toLongLong() <= 1024 * 1024);
            }
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(d.channelBits(), 16);
        QCOMPARE(d.historyIndex(), index + 1);
        QCOMPARE(d.composite(), expected);
        const auto stack = d.gpuLayerStack();
        QVERIFY(stack.valid);
        qint64 heap = 0;
        for (const auto &layer : stack.layers)
            for (auto bytes : layer.image.tiles().heapBlocks())
                heap += bytes;
        if (cacheAvailable)
            QCOMPARE(heap, qint64(0));
        else if (cacheMode == 0)
            QCOMPARE(heap, (import ? 6LL : 4LL) * 1024 * 1024);
        else {
            QVERIFY(heap > 0);
            QVERIFY(heap < (import ? 6LL : 4LL) * 1024 * 1024);
        }
        d.clearScratchCache();
        QCOMPARE(d.composite(), expected);
        d.undo();
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.composite(), before);
        d.redo();
        QCOMPARE(d.composite(), expected);
    }
    void asynchronousLayerImportMatchesCenteredNativePixels_data()
    {
        QTest::addColumn<int>("documentBits");
        QTest::addColumn<int>("imageBits");
        QTest::addColumn<QSize>("size");
        for (int documentBits : {8, 16})
            for (int imageBits : {8, 16})
                for (QSize size : {QSize(33, 27), QSize(80, 60), QSize(139, 97)})
                    QTest::newRow(qPrintable(QString("%1-%2-%3x%4")
                                                 .arg(documentBits)
                                                 .arg(imageBits)
                                                 .arg(size.width())
                                                 .arg(size.height())))
                        << documentBits << imageBits << size;
    }
    void asynchronousLayerImportMatchesCenteredNativePixels()
    {
        QFETCH(int, documentBits);
        QFETCH(int, imageBits);
        QFETCH(QSize, size);
        QTemporaryDir directory;
        const auto    target  = QUrl::fromLocalFile(directory.filePath("import.png"));
        const auto    project = QUrl::fromLocalFile(directory.filePath("original.pcr"));
        QImage        image(size, imageBits == 16 ? QImage::Format_RGBA64_Premultiplied
                                                  : QImage::Format_ARGB32_Premultiplied);
        image.setColorSpace(QColorSpace::DisplayP3);
        for (int y = 0; y < size.height(); ++y)
            for (int x = 0; x < size.width(); ++x)
                image.setPixelColor(x, y,
                                    QColor::fromRgba64(ushort(x * 439), ushort(y * 587),
                                                       ushort(x * 217 + y * 113), 51000));
        QVERIFY(image.save(target.toLocalFile()));
        Document d, reference;
        for (Document *doc : {&d, &reference}) {
            populate(*doc, documentBits);
            doc->addLayerMask();
            doc->setColor(Qt::black);
            doc->beginStroke(20, 20);
            doc->endStroke();
            doc->setEditingMask(false);
            QVERIFY(doc->saveProject(project));
        }
        const auto before  = d.composite();
        const auto layers  = d.layers();
        const auto mask    = d.maskImage(d.activeLayer());
        const int  history = d.historyIndex();
        QVERIFY(reference.importLayer(target));
        const auto expected = reference.composite();
        QSignalSpy errors(&d, &Document::errorOccurred);
        QVERIFY(!d.modified());
        d.setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            d.requestImportLayer(target);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.composite(), before);
            QCOMPARE(d.layers(), layers);
            QCOMPARE(d.historyIndex(), history);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layers(), reference.layers());
        QCOMPARE(d.channelBits(), qMax(documentBits, imageBits));
        QCOMPARE(d.historyIndex(), history + 1);
        QCOMPARE(d.projectUrl(), project);
        QVERIFY(d.modified());
        QCOMPARE(d.maskImage(d.activeLayer() - 1), mask);
        d.undo();
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.channelBits(), documentBits);
        QVERIFY(!d.modified());
        d.redo();
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.channelBits(), qMax(documentBits, imageBits));
    }
    void asynchronousImageOpenMatchesNativeDecoding_data()
    {
        QTest::addColumn<QByteArray>("format");
        QTest::addColumn<int>("bits");
        QTest::newRow("png8") << QByteArray("png") << 8;
        QTest::newRow("png16") << QByteArray("png") << 16;
        QTest::newRow("jpeg") << QByteArray("jpg") << 8;
        QTest::newRow("bmp") << QByteArray("bmp") << 8;
        QTest::newRow("webp") << QByteArray("webp") << 8;
        QTest::newRow("tiff16") << QByteArray("tiff") << 16;
    }
    void asynchronousImageOpenMatchesNativeDecoding()
    {
        QFETCH(QByteArray, format);
        QFETCH(int, bits);
        if (!QImageWriter::supportedImageFormats().contains(format))
            QSKIP("This Qt kit does not include the image writer plugin.");
        QTemporaryDir directory;
        const auto    target =
            QUrl::fromLocalFile(directory.filePath("image." + QString::fromLatin1(format)));
        QImage source(139, 97,
                      bits == 16 ? QImage::Format_RGBA64_Premultiplied
                                 : QImage::Format_ARGB32_Premultiplied);
        source.setColorSpace(QColorSpace::DisplayP3);
        for (int y = 0; y < source.height(); ++y)
            for (int x = 0; x < source.width(); ++x)
                source.setPixelColor(x, y,
                                     QColor::fromRgba64(ushort(x * 439), ushort(y * 587),
                                                        ushort(x * 217 + y * 113),
                                                        ushort(25000 + (x + y) * 151)));
        QVERIFY(source.save(target.toLocalFile(), format.constData()));
        Document reference;
        QVERIFY(reference.openFile(target));
        const auto expected = reference.composite();
        Document   d;
        populate(d, 8);
        d.setTool("eraser");
        d.setBrushSize(39);
        const auto before  = d.composite();
        const auto history = d.history();
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            d.requestOpenFile(target);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.composite(), before);
            QCOMPARE(d.history(), history);
            QCOMPARE(opened.size(), 0);
        }
        QCOMPARE(d.composite(), before);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(opened[0][0].toUrl(), target);
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layers(), reference.layers());
        QCOMPARE(d.channelBits(), reference.channelBits());
        QCOMPARE(d.name(), reference.name());
        QVERIFY(d.projectUrl().isEmpty());
        QVERIFY(d.modified());
        QCOMPARE(d.historyIndex(), 0);
        QCOMPARE(d.tool(), QString("eraser"));
        QCOMPARE(d.brushSize(), 39.);
        d.undo();
        QCOMPARE(d.composite(), expected);
    }
    void invalidImageOpenKeepsTheCurrentDocument_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<bool>("import");
        for (bool import : {false, true})
            for (const auto *name : {"missing", "unreadable", "remote"})
                QTest::newRow(qPrintable(QString(import ? "import-" : "open-") + name))
                    << QString(name) << import;
    }
    void invalidImageOpenKeepsTheCurrentDocument()
    {
        QFETCH(QString, kind);
        QFETCH(bool, import);
        QTemporaryDir directory;
        Document      d;
        populate(d, 16);
        const auto before  = d.composite();
        const auto history = d.history();
        QUrl       target  = QUrl::fromLocalFile(directory.filePath("bad.png"));
        if (kind == "unreadable") {
            QFile file(target.toLocalFile());
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("not an image");
        } else if (kind == "remote")
            target = QUrl("https://example.invalid/image.png");
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        if (import)
            d.requestImportLayer(target);
        else
            d.requestOpenFile(target);
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(opened.size(), 0);
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
        QVERIFY(d.modified());
    }
    void asynchronousProjectOpenAppliesOnlyACompleteValidatedDocument_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void asynchronousProjectOpenAppliesOnlyACompleteValidatedDocument()
    {
        QFETCH(int, bits);
        QTemporaryDir directory;
        const auto    target = QUrl::fromLocalFile(directory.filePath("loaded.pcr"));
        Document      source;
        populate(source, bits);
        source.addLayerMask();
        source.setColor(Qt::black);
        source.beginStroke(35, 25);
        source.endStroke();
        source.addAdjustmentLayer("brightnessContrast");
        source.setBackgroundEditThreshold(-1);
        source.beginLayerPreview();
        source.previewLayerParameter("brightness", 12);
        source.finishLayerPreview();
        source.setTool("text");
        source.beginStroke(5, 5);
        source.endStroke();
        source.setText("Editable after load");
        source.finishTextEdit();
        source.setTool("pen");
        source.beginPenGesture(10, 12);
        source.endPenGesture();
        source.beginPenGesture(65, 45);
        source.endPenGesture();
        source.finishPenPath();
        source.setTool("slice");
        source.beginSliceGesture(4, 5);
        source.updateSliceGesture(50, 40);
        source.endSliceGesture();
        QVERIFY(source.saveProject(target));
        const auto expected = source.composite();
        Document   reference;
        QVERIFY(reference.openFile(target));
        Document d;
        populate(d, 8);
        d.setTool("eraser");
        d.setBrushSize(39);
        const auto before  = d.composite();
        const auto history = d.history();
        const auto url     = d.projectUrl();
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            d.requestOpenFile(target);
            QVERIFY(d.operationBusy());
            QCOMPARE(d.composite(), before);
            QCOMPARE(d.history(), history);
            QCOMPARE(d.projectUrl(), url);
            QCOMPARE(opened.size(), 0);
            QVERIFY(d.modified());
        }
        // Finished decoding does not replace the document before the GUI callback.
        QCOMPARE(d.composite(), before);
        QVERIFY(d.operationBusy());
        QVERIFY(d.operationCancellable());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened[0][0].toUrl(), target);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(d.composite(), expected);
        QCOMPARE(d.layers(), reference.layers());
        QCOMPARE(d.projectUrl(), target);
        QCOMPARE(d.channelBits(), bits);
        QCOMPARE(d.penPaths(), reference.penPaths());
        QCOMPARE(d.activePenPath(), reference.activePenPath());
        QCOMPARE(d.slices(), reference.slices());
        QCOMPARE(d.activeSlice(), reference.activeSlice());
        QCOMPARE(d.historyIndex(), 0);
        QVERIFY(!d.modified());
        QCOMPARE(d.tool(), QString("eraser"));
        QCOMPARE(d.brushSize(), 39.);
        d.undo();
        QCOMPARE(d.composite(), expected);
    }
    void cancelledOrStaleProjectOpenKeepsTheCurrentDocument_data()
    {
        QTest::addColumn<QString>("action");
        QTest::addColumn<bool>("image");
        QTest::addColumn<bool>("import");
        for (int mode : {0, 1, 2})
            for (const auto *name :
                 {"queued", "finished", "new-document", "new-gesture", "destroyed"})
                QTest::newRow(qPrintable(QString(mode == 2   ? "import-"
                                                 : mode == 1 ? "image-"
                                                             : "project-") +
                                         name))
                    << QString(name) << (mode != 0) << (mode == 2);
    }
    void cancelledOrStaleProjectOpenKeepsTheCurrentDocument()
    {
        QFETCH(QString, action);
        QFETCH(bool, image);
        QFETCH(bool, import);
        QTemporaryDir directory;
        const auto    target =
            QUrl::fromLocalFile(directory.filePath(image ? "open.png" : "open.pcr"));
        Document source;
        populate(source, 16);
        QVERIFY(image ? source.exportImage(target) : source.saveProject(target));
        auto d = std::make_unique<Document>();
        populate(*d, 8);
        const auto before  = d->composite();
        const auto history = d->history();
        QSignalSpy opened(d.get(), &Document::fileOpened);
        QSignalSpy errors(d.get(), &Document::errorOccurred);
        d->setBackgroundEditThreshold(0);
        {
            BlockPool blocked;
            if (import)
                d->requestImportLayer(target);
            else
                d->requestOpenFile(target);
            QVERIFY(d->operationBusy());
            if (action == "queued")
                d->cancelOperation();
            else if (action == "new-document")
                QVERIFY(d->newDocument(33, 27, true));
            else if (action == "new-gesture") {
                d->setTool("brush");
                d->beginStroke(40, 20);
                d->endStroke();
            } else if (action == "destroyed")
                d.reset();
        }
        if (action == "finished") {
            QVERIFY(d->operationBusy());
            d->cancelOperation();
        }
        if (d) {
            const auto current        = d->composite();
            const auto currentHistory = d->history();
            QCoreApplication::processEvents();
            QTRY_VERIFY_WITH_TIMEOUT(!d->operationBusy(), 10000);
            QCOMPARE(d->composite(), current);
            QCOMPARE(d->history(), currentHistory);
            if (action == "queued" || action == "finished") {
                QCOMPARE(d->composite(), before);
                QCOMPARE(d->history(), history);
            }
            QCOMPARE(opened.size(), 0);
            QCOMPARE(errors.size(), 0);
        }
    }
    void projectOpenErrorsAndSynchronousFallbackPreserveTheContract()
    {
        QTemporaryDir directory;
        Document      d;
        populate(d, 16);
        const auto before  = d.composite();
        const auto history = d.history();
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        const auto bad = QUrl::fromLocalFile(directory.filePath("bad.pcr"));
        QFile      file(bad.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a project");
        file.close();
        d.requestOpenFile(bad);
        QVERIFY(d.operationBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(opened.size(), 0);
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
        QVERIFY(d.modified());
        const auto target = QUrl::fromLocalFile(directory.filePath("good.pcr"));
        QVERIFY(d.saveProject(target));
        d.setBackgroundEditThreshold(-1);
        d.requestOpenFile(target);
        QVERIFY(!d.operationBusy());
        QCOMPARE(opened.size(), 1);
        QVERIFY(!d.modified());
        const auto image = QUrl::fromLocalFile(directory.filePath("image.png"));
        QVERIFY(before.save(image.toLocalFile()));
        d.requestOpenFile(image);
        QVERIFY(!d.operationBusy());
        QCOMPARE(opened.size(), 2);
        QCOMPARE(d.composite(), before);
        QVERIFY(d.modified());
    }
    void lateProjectValidationFailureKeepsPreviousLayers_data()
    {
        QTest::addColumn<bool>("badPlane");
        QTest::newRow("last-layer-png") << true;
        QTest::newRow("active-path-after-all-planes") << false;
    }
    void lateProjectValidationFailureKeepsPreviousLayers()
    {
        QFETCH(bool, badPlane);
        QTemporaryDir directory;
        const auto    target = QUrl::fromLocalFile(directory.filePath("late-invalid.pcr"));
        Document      source;
        populate(source, 16);
        QVERIFY(source.saveProject(target));
        QFile file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto json = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        if (badPlane) {
            auto layers               = json["layers"].toArray();
            auto last                 = layers.last().toObject();
            last["png"]               = "invalid-base64-png";
            layers[layers.size() - 1] = last;
            json["layers"]            = layers;
        } else
            json["activePenPath"] = 1000;
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
        file.close();
        Document d;
        populate(d, 8);
        const auto before  = d.composite();
        const auto layers  = d.layers();
        const auto history = d.history();
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        d.requestOpenFile(target);
        QVERIFY(d.operationBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(opened.size(), 0);
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.history(), history);
        QVERIFY(d.modified());
    }
    void runningProjectDecodeCanBeCancelledWithoutReplacingTheDocument_data()
    {
        QTest::addColumn<bool>("image");
        QTest::addColumn<bool>("import");
        QTest::newRow("project") << false << false;
        QTest::newRow("image") << true << false;
        QTest::newRow("import") << true << true;
    }
    void runningProjectDecodeCanBeCancelledWithoutReplacingTheDocument()
    {
        QFETCH(bool, image);
        QFETCH(bool, import);
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_OPEN_CANCEL_STRESS"))
            QSKIP("Set PHOTOCRAP_OPEN_CANCEL_STRESS=1 to interrupt real native PNG decoding.");
        QTemporaryDir directory;
        const auto    target =
            QUrl::fromLocalFile(directory.filePath(image ? "large.png" : "large.pcr"));
        {
            Document source;
            source.setBackgroundEditThreshold(-1);
            QVERIFY(source.newDocument(4096, 4096, false, 16));
            source.setColor(Qt::red);
            source.setBrushSize(12);
            source.beginStroke(100, 100);
            source.endStroke();
            QVERIFY(image ? source.exportImage(target) : source.saveProject(target));
        }
        Document d;
        populate(d, 8);
        const auto before  = d.composite();
        const auto history = d.history();
        QSignalSpy opened(&d, &Document::fileOpened);
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.setBackgroundEditThreshold(0);
        if (import)
            d.requestImportLayer(target);
        else
            d.requestOpenFile(target);
        QElapsedTimer timer;
        timer.start();
        bool decoding = false;
        while (timer.elapsed() < 10000) {
            if (d.operationProgress() > 0 && d.operationProgress() < .5) {
                decoding = true;
                break;
            }
            if (d.operationProgress() >= 1)
                break;
            QThread::msleep(1);
        }
        if (!decoding) {
            d.cancelOperation();
            QThreadPool::globalInstance()->waitForDone();
            QSKIP("The decoder finished before its cancellation window could be observed.");
        }
        d.cancelOperation();
        QThreadPool::globalInstance()->waitForDone();
        QCoreApplication::processEvents();
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
        QVERIFY(d.modified());
        QCOMPARE(opened.size(), 0);
        QCOMPARE(errors.size(), 0);
    }
    void asynchronousSavePreservesHistoryUntilTheFileIsPublished_data()
    {
        QTest::addColumn<int>("bits");
        QTest::newRow("8-bit") << 8;
        QTest::newRow("16-bit") << 16;
    }
    void asynchronousSavePreservesHistoryUntilTheFileIsPublished()
    {
        QFETCH(int, bits);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (bool existing : {false, true}) {
            Document reference, d;
            populate(reference, bits);
            populate(d, bits);
            for (auto *doc : {&reference, &d}) {
                doc->addLayerMask();
                doc->setEditingMask(true);
                doc->setColor(Qt::black);
                doc->beginStroke(30, 20);
                doc->endStroke();
                doc->setEditingMask(false);
            }
            if (existing)
                QVERIFY(d.saveProject(QUrl::fromLocalFile(directory.filePath("original.pcr"))));
            for (auto *doc : {&reference, &d})
                doc->renameLayer(doc->activeLayer(), "Changed after previous save");
            const auto target =
                QUrl::fromLocalFile(directory.filePath(existing ? "resaved.pcr" : "new.pcr"));
            const auto expected = QUrl::fromLocalFile(
                directory.filePath(existing ? "reference-resaved.pcr" : "reference-new.pcr"));
            QVERIFY(reference.saveProject(expected));
            const auto pixels  = d.composite();
            const auto history = d.history();
            const auto layers  = d.layers();
            const auto oldUrl  = d.projectUrl();
            QVERIFY(d.modified());
            d.setBackgroundEditThreshold(0);
            QSignalSpy saved(&d, &Document::projectSaved);
            QSignalSpy errors(&d, &Document::errorOccurred);
            QSignalSpy images(&d, &Document::imageChanged);
            {
                BlockPool block;
                d.requestSaveProject(target);
                QVERIFY(d.operationBusy());
                QVERIFY(d.operationCancellable());
                QVERIFY(d.modified());
                QCOMPARE(d.projectUrl(), oldUrl);
                QCOMPARE(saved.size(), 0);
                QCOMPARE(d.history(), history);
                QVERIFY(!QFile::exists(target.toLocalFile()));
                d.setBrushSize(37);
                d.setColor(Qt::green);
                block.release.release();
                block.pool->waitForDone();
                QVERIFY(d.operationBusy());
                QVERIFY(!d.operationCancellable());
                QVERIFY(d.modified());
                QCOMPARE(d.projectUrl(), oldUrl);
                d.cancelOperation(); // Publication has finished; await its GUI acknowledgement.
                QVERIFY(d.operationBusy());
                QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
            }
            QCOMPARE(saved.size(), 1);
            QCOMPARE(saved[0][0].toUrl(), target);
            QCOMPARE(errors.size(), 0);
            QCOMPARE(images.size(), 0);
            QVERIFY(!d.modified());
            QCOMPARE(d.projectUrl(), target);
            QCOMPARE(d.history(), history);
            QCOMPARE(d.layers(), layers);
            QCOMPARE(d.composite(), pixels);
            QCOMPARE(d.brushSize(), 37.);
            QCOMPARE(d.color(), QColor(Qt::green));
            QFile actualFile(target.toLocalFile()), expectedFile(expected.toLocalFile());
            QVERIFY(actualFile.open(QIODevice::ReadOnly));
            QVERIFY(expectedFile.open(QIODevice::ReadOnly));
            QCOMPARE(actualFile.readAll(), expectedFile.readAll());
            Document loaded;
            QVERIFY(loaded.openFile(target));
            QCOMPARE(loaded.composite(), pixels);
        }
    }
    void saveErrorsAndSmallSaveKeepTheirCompletionContract()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Document d;
        populate(d, 16);
        const auto history = d.history();
        const auto pixels  = d.composite();
        const auto url     = d.projectUrl();
        QSignalSpy errors(&d, &Document::errorOccurred);
        QSignalSpy saved(&d, &Document::projectSaved);
        d.setBackgroundEditThreshold(0);
        d.requestSaveProject(QUrl("https://invalid.example/project.pcr"));
        QVERIFY(!d.operationBusy());
        d.requestSaveProject(QUrl::fromLocalFile(directory.filePath("missing/project.pcr")));
        QVERIFY(d.operationBusy());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(errors.size(), 2);
        QCOMPARE(saved.size(), 0);
        QVERIFY(d.modified());
        QCOMPARE(d.projectUrl(), url);
        QCOMPARE(d.history(), history);
        QCOMPARE(d.composite(), pixels);
        d.setBackgroundEditThreshold(-1);
        const auto target = QUrl::fromLocalFile(directory.filePath("small"));
        d.requestSaveProject(target);
        QVERIFY(!d.operationBusy());
        QVERIFY(!d.modified());
        QCOMPARE(saved.size(), 1);
        QCOMPARE(d.projectUrl(), QUrl::fromLocalFile(target.toLocalFile() + ".pcr"));
        QCOMPARE(d.history(), history);
    }
    void savingFinalizesTextAndKeepsSignalsStableWhenTheDocumentChanges()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (bool asynchronous : {false, true}) {
            Document d;
            QVERIFY(d.newDocument(160, 100, true, 16));
            d.setTool("text");
            d.beginStroke(10, 10);
            d.endStroke();
            d.setText("Saved while editing");
            QVERIFY(d.textEditing());
            d.setBackgroundEditThreshold(asynchronous ? 0 : -1);
            const auto target =
                QUrl::fromLocalFile(directory.filePath(asynchronous ? "async.pcr" : "sync.pcr"));
            QObject::connect(&d, &Document::projectSaved, &d,
                             [&d] { d.newDocument(27, 19, true); });
            QSignalSpy saved(&d, &Document::projectSaved);
            d.requestSaveProject(target);
            QTRY_COMPARE_WITH_TIMEOUT(saved.size(), 1, 10000);
            QCOMPARE(saved[0][0].toUrl(), target);
            QCOMPARE(d.width(), 27);
            QCOMPARE(d.height(), 19);
            QVERIFY(!d.textEditing());
            QVERIFY(!d.operationBusy());
            Document loaded;
            QVERIFY(loaded.openFile(target));
            QCOMPARE(loaded.layers().size(), 2);
            loaded.editTextLayer(1);
            QCOMPARE(loaded.text(), QString("Saved while editing"));
        }
    }
    void runningSaveCanBeCancelledBeforeFileCommit()
    {
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_SAVE_CANCEL_STRESS"))
            QSKIP("Opt in with PHOTOCRAP_SAVE_CANCEL_STRESS=1 for the large real project encoder "
                  "test.");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto       target = QUrl::fromLocalFile(directory.filePath("project.pcr"));
        const QByteArray previous("Previous project contents");
        QFile            file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(previous), previous.size());
        file.close();
        Document d;
        QVERIFY(d.newDocument(4096, 4096, false, 16));
        d.setTool("brush");
        d.setColor(Qt::red);
        d.setBrushSize(20);
        d.beginStroke(100, 100);
        d.endStroke();
        d.duplicateLayer();
        d.duplicateLayer(); // Exercise a recording candidate during cancellation.
        QVERIFY(d.modified());
        const auto history = d.history();
        const auto url     = d.projectUrl();
        QSignalSpy saved(&d, &Document::projectSaved);
        d.setBackgroundEditThreshold(0);
        d.requestSaveProject(target);
        QVERIFY(d.operationBusy());
        QElapsedTimer timer;
        timer.start();
        bool encoding = false;
        while (d.operationBusy() && timer.elapsed() < 10000) {
            const auto progress = d.operationProgress();
            if (progress > 0 && progress < .5) {
                encoding = true;
                break;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            QThread::msleep(1);
        }
        d.cancelOperation();
        QVERIFY(!d.operationBusy());
        QThreadPool::globalInstance()->waitForDone();
        if (!encoding)
            QSKIP("The encoder completed before the cancellation window could be observed.");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), previous);
        QCOMPARE(d.history(), history);
        QCOMPARE(d.projectUrl(), url);
        QCOMPARE(saved.size(), 0);
        QVERIFY(d.modified());
        qInfo() << "Cancelled a running 16 MP / 16 bit project before atomic file commit";
    }
    void runningExportCanBeCancelledBeforeFileCommit()
    {
        if (!qEnvironmentVariableIsSet("PHOTOCRAP_EXPORT_CANCEL_STRESS"))
            QSKIP("Opt in with PHOTOCRAP_EXPORT_CANCEL_STRESS=1 for the large real encoder test.");
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto       target = QUrl::fromLocalFile(directory.filePath("export.png"));
        const QByteArray previous("Previous file contents");
        QFile            file(target.toLocalFile());
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(previous), previous.size());
        file.close();
        Document d;
        QVERIFY(d.newDocument(4096, 4096, false, 16));
        d.setBackgroundEditThreshold(0);
        const auto history = d.history();
        d.requestExportImage(target);
        QVERIFY(d.operationBusy());
        QElapsedTimer timer;
        timer.start();
        bool encoding = false;
        while (d.operationBusy() && timer.elapsed() < 10000) {
            if (d.operationProgress() == .5) {
                encoding = true;
                break;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            QThread::msleep(1);
        }
        d.cancelOperation();
        QVERIFY(!d.operationBusy());
        QThreadPool::globalInstance()->waitForDone();
        if (!encoding)
            QSKIP("The encoder completed before the cancellation window could be observed.");
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), previous);
        QCOMPARE(d.history(), history);
        qInfo() << "Cancelled a running 16 MP / 16 bit PNG before atomic file commit";
    }
    void asynchronousExportMatchesSynchronous_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("suffix");
        for (int bits : {8, 16})
            for (const auto suffix : {"png", "jpg", "bmp", "webp"}) {
                if (!QImageWriter::supportedImageFormats().contains(QByteArray(suffix)))
                    continue;
                QTest::newRow(qPrintable(QString::number(bits) + "-" + suffix))
                    << bits << QString(suffix);
            }
    }
    void asynchronousExportMatchesSynchronous()
    {
        QFETCH(int, bits);
        QFETCH(QString, suffix);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (bool saved : {false, true}) {
            Document reference, d;
            populate(reference, bits);
            populate(d, bits);
            const auto tag      = saved ? "saved" : "modified";
            const auto original = QUrl::fromLocalFile(directory.filePath(QString(tag) + ".pcr"));
            if (saved)
                QVERIFY(d.saveProject(original));
            const auto expected =
                QUrl::fromLocalFile(directory.filePath(QString(tag) + "-reference." + suffix));
            const auto target =
                QUrl::fromLocalFile(directory.filePath(QString(tag) + "-async." + suffix));
            QVERIFY(reference.exportImage(expected));
            const auto pixels   = d.composite();
            const auto history  = d.history();
            const auto layers   = d.layers();
            const auto url      = d.projectUrl();
            const bool modified = d.modified();
            d.setBackgroundEditThreshold(0);
            QSignalSpy errors(&d, &Document::errorOccurred);
            QSignalSpy messages(&d, &Document::statusMessage);
            QSignalSpy images(&d, &Document::imageChanged);
            {
                BlockPool block;
                d.requestExportImage(target);
                QVERIFY(d.operationBusy());
                QVERIFY(!QFile::exists(target.toLocalFile()));
                QCOMPARE(d.composite(), pixels);
                QCOMPARE(d.history(), history);
                d.setBrushSize(37);
                d.setColor(Qt::green);
                block.release.release();
                block.pool->waitForDone();
                // The file is already published, but its GUI completion is queued.
                // Escape must not report a successful publication as cancelled.
                QVERIFY(d.operationBusy());
                QVERIFY(!d.operationCancellable());
                d.cancelOperation();
                QVERIFY(d.operationBusy());
                QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
            }
            QCOMPARE(errors.size(), 0);
            QCOMPARE(messages.size(), 1);
            QCOMPARE(images.size(), 0);
            QCOMPARE(d.composite(), pixels);
            QCOMPARE(d.history(), history);
            QCOMPARE(d.layers(), layers);
            QCOMPARE(d.projectUrl(), url);
            QCOMPARE(d.modified(), modified);
            QCOMPARE(d.brushSize(), 37.);
            QCOMPARE(d.color(), QColor(Qt::green));
            QFile actualFile(target.toLocalFile()), expectedFile(expected.toLocalFile());
            QVERIFY(actualFile.open(QIODevice::ReadOnly));
            QVERIFY(expectedFile.open(QIODevice::ReadOnly));
            QCOMPARE(actualFile.readAll(), expectedFile.readAll());
        }
    }
    void cancelledExportKeepsTheExistingFileAndDocumentLifetime_data()
    {
        QTest::addColumn<bool>("save");
        QTest::newRow("export") << false;
        QTest::newRow("save") << true;
    }
    void cancelledExportKeepsTheExistingFileAndDocumentLifetime()
    {
        QFETCH(bool, save);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto target =
            QUrl::fromLocalFile(directory.filePath(save ? "project.pcr" : "export.png"));
        const QByteArray previous("Previous file contents");
        for (const auto action : {"cancel", "newDocument", "newGesture", "destroy"}) {
            QFile file(target.toLocalFile());
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(previous), previous.size());
            file.close();
            auto d = std::make_unique<Document>();
            populate(*d, 16);
            d->setBackgroundEditThreshold(0);
            const auto history = d->history();
            {
                BlockPool block;
                if (save)
                    d->requestSaveProject(target);
                else
                    d->requestExportImage(target);
                QVERIFY(d->operationBusy());
                if (QString(action) == "destroy")
                    d.reset();
                else if (QString(action) == "newDocument")
                    QVERIFY(d->newDocument(27, 19, true));
                else if (QString(action) == "newGesture") {
                    d->setTool("brush");
                    d->beginStroke(20, 20);
                    d->cancelStroke();
                } else
                    d->cancelOperation();
                if (d)
                    QVERIFY(!d->operationBusy());
            }
            QCoreApplication::processEvents();
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), previous);
            if (d && QString(action) != "newDocument")
                QCOMPARE(d->history(), history);
        }
    }
    void exportErrorsLeaveHistoryAndPixelsUnchanged()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        Document d;
        populate(d, 8);
        const auto pixels  = d.composite();
        const auto history = d.history();
        d.setBackgroundEditThreshold(0);
        QSignalSpy errors(&d, &Document::errorOccurred);
        for (const auto target : {"unsupported.bad", "missing/export.png"}) {
            d.requestExportImage(QUrl::fromLocalFile(directory.filePath(target)));
            QVERIFY(d.operationBusy());
            QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
            QCOMPARE(d.composite(), pixels);
            QCOMPARE(d.history(), history);
            QVERIFY(!QFile::exists(directory.filePath(target)));
        }
        QCOMPARE(errors.size(), 2);
        d.setBackgroundEditThreshold(-1);
        const auto target = QUrl::fromLocalFile(directory.filePath("small.png"));
        d.requestExportImage(target);
        QVERIFY(!d.operationBusy());
        QVERIFY(QFile::exists(target.toLocalFile()));
        QCOMPARE(d.history(), history);
    }
    void bucketCanBeCancelledOrSupersededBeforeAFirstDrag()
    {
        Document d;
        populate(d, 8);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.layerImage(d.activeLayer());
        const auto history = d.history();
        {
            BlockPool block;
            for (const auto &tool : {"move", "redEye", "rectangle"}) {
                d.setTool("fill");
                d.setColor(Qt::green);
                d.beginStroke(4, 4);
                QVERIFY(d.operationBusy());
                QCOMPARE(d.layerImage(d.activeLayer()), before);
                QCOMPARE(d.history(), history);
                d.setShapeNewLayer(false);
                d.setTool(tool);
                d.beginStroke(30, 30);
                QVERIFY(!d.operationBusy());
                d.cancelStroke();
                QCOMPARE(d.layerImage(d.activeLayer()), before);
                QCOMPARE(d.history(), history);
            }
            d.setTool("fill");
            d.beginStroke(4, 4);
            QVERIFY(d.operationBusy());
            d.cancelOperation();
            QVERIFY(!d.operationBusy());
            QCOMPARE(d.history(), history);
            d.beginStroke(4, 4);
            QVERIFY(d.operationBusy());
            QVERIFY(d.newDocument(27, 19, true));
            QVERIFY(!d.operationBusy());
        }
        QCoreApplication::processEvents();
        QCOMPARE(d.width(), 27);
        QCOMPARE(d.height(), 19);
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        QCOMPARE(d.historyIndex(), 0);
    }
    void bucketCapturesInkAndOpacityAndKeepsLaterToolSettings()
    {
        Document   reference, asynchronous;
        const auto ink = QColor::fromRgba64(51321, 12345, 23456, 45678);
        for (auto *d : {&reference, &asynchronous}) {
            d->setBackgroundEditThreshold(-1);
            QVERIFY(d->newDocument(40, 30, false, 16));
            d->setTool("fill");
            d->setColor(ink);
            d->setBrushOpacity(.37);
        }
        reference.beginStroke(4, 4);
        {
            BlockPool block;
            asynchronous.setBackgroundEditThreshold(0);
            asynchronous.beginStroke(4, 4);
            QVERIFY(asynchronous.operationBusy());
            asynchronous.setColor(Qt::blue);
            asynchronous.setBrushOpacity(.91);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!asynchronous.operationBusy(), 10000);
        QCOMPARE(asynchronous.composite(), reference.composite());
        QCOMPARE(asynchronous.color(), QColor(Qt::blue));
        QCOMPARE(asynchronous.brushOpacity(), qreal(.91));
    }
    void sameColourBucketDoesNotQueueOrCommit()
    {
        Document d;
        QVERIFY(d.newDocument(40, 30, false, 16));
        d.setTool("fill");
        d.setColor(Qt::white);
        d.setBrushOpacity(1);
        d.setBackgroundEditThreshold(0);
        const auto history = d.history();
        QSignalSpy operations(&d, &Document::operationChanged);
        d.beginStroke(4, 4);
        QVERIFY(!d.operationBusy());
        QCOMPARE(operations.size(), 0);
        QCOMPARE(d.history(), history);
    }
    void exactAndAtomic_data()
    {
        QTest::addColumn<int>("bits");
        QTest::addColumn<QString>("name");
        for (int bits : {8, 16})
            for (const QString name :
                 {"resize", "crop", "depth", "fill", "bucket", "bucketMask", "bucketSoft", "clear",
                  "merge", "invertMask", "filter", "rotateLeft", "rotateRight", "flipH", "flipV"})
                QTest::newRow(qPrintable(QString::number(bits) + name)) << bits << name;
    }
    void exactAndAtomic()
    {
        QFETCH(int, bits);
        QFETCH(QString, name);
        Document reference, asynchronous;
        for (auto *d : {&reference, &asynchronous}) {
            populate(*d, bits);
            if (name == "crop" || name == "clear") {
                d->setTool("select");
                d->beginStroke(7, 9);
                d->continueStroke(65, 48);
                d->endStroke();
                QVERIFY(d->hasSelection());
            }
            if (name == "bucketSoft") {
                d->setTool("ellipticalSelect");
                d->beginStroke(0, 0);
                d->continueStroke(70, 50);
                d->endStroke();
                d->modifySelection("feather", 2);
            }
            if (name == "invertMask" || name == "bucketMask") {
                d->addLayerMask();
                d->setColor(Qt::black);
                d->beginStroke(10, 10);
                d->endStroke();
            }
        }
        const QImage before  = asynchronous.composite();
        const auto   history = asynchronous.history();
        const QSize  size    = QSize(asynchronous.width(), asynchronous.height());
        QSignalSpy   errors(&asynchronous, &Document::errorOccurred);
        QSignalSpy   commits(&asynchronous, &Document::historyChanged);
        asynchronous.setBackgroundEditThreshold(0);
        operation(reference, name);
        operation(asynchronous, name);
        QVERIFY(asynchronous.operationBusy());
        QVERIFY(asynchronous.pixelEditBusy());
        QCOMPARE(asynchronous.composite(), before);
        QCOMPARE(asynchronous.history(), history);
        QTRY_VERIFY_WITH_TIMEOUT(!asynchronous.operationBusy(), 10000);
        QCOMPARE(errors.size(), 0);
        QCOMPARE(commits.size(), 1);
        QCOMPARE(QSize(asynchronous.width(), asynchronous.height()),
                 QSize(reference.width(), reference.height()));
        QCOMPARE(asynchronous.channelBits(), reference.channelBits());
        QCOMPARE(asynchronous.composite(), reference.composite());
        QCOMPARE(asynchronous.layers().size(), reference.layers().size());
        for (int i = 0; i < reference.layers().size(); ++i) {
            QCOMPARE(asynchronous.layerImage(i), reference.layerImage(i));
            QCOMPARE(asynchronous.maskImage(i), reference.maskImage(i));
        }
        const QImage after = asynchronous.composite();
        asynchronous.undo();
        QCOMPARE(QSize(asynchronous.width(), asynchronous.height()), size);
        QCOMPARE(asynchronous.composite(), before);
        asynchronous.redo();
        QCOMPARE(asynchronous.composite(), after);
    }
    void cancelDiscardAndLifetime()
    {
        BlockPool block;
        Document  d;
        populate(d, 16);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.composite();
        const auto history = d.history();
        d.resizeImage(37, 29);
        QVERIFY(d.operationBusy());
        d.cancelOperation();
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
        d.applyFilter("invert");
        QVERIFY(d.operationBusy());
        d.undo();
        QVERIFY(!d.operationBusy());
        QCOMPARE(d.history(), history);
        d.transformImage("rotateRight");
        QVERIFY(d.operationBusy());
        d.setActiveLayer(0);
        QVERIFY(!d.operationBusy());
        d.resizeImage(11, 13);
        QVERIFY(d.operationBusy());
        QVERIFY(d.newDocument(27, 19, true, 8));
        QVERIFY(!d.operationBusy());
        auto transient = std::make_unique<Document>();
        populate(*transient, 8);
        transient->setBackgroundEditThreshold(0);
        transient->resizeImage(31, 23);
        QElapsedTimer timer;
        timer.start();
        transient.reset();
        QVERIFY(timer.elapsed() < 1000); // Destruction must not wait for the blocked worker.
        block.release.release();
        block.pool->waitForDone();
        QTest::qWait(50);
        QCOMPARE(QSize(d.width(), d.height()), QSize(27, 19));
        QCOMPARE(d.channelBits(), 8);
        QCOMPARE(d.history().size(), 1);
    }
    void liveGesturesSupersedeQueuedEdits_data()
    {
        QTest::addColumn<QString>("gesture");
        for (const char *name : {"parameter", "blend", "pen", "slice"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void liveGesturesSupersedeQueuedEdits()
    {
        QFETCH(QString, gesture);
        BlockPool block;
        Document  d;
        populate(d, 16);
        if (gesture == "parameter")
            d.addAdjustmentLayer("brightnessContrast");
        if (gesture == "pen")
            d.setTool("pen");
        if (gesture == "slice")
            d.setTool("slice");
        d.setBackgroundEditThreshold(0);
        d.resizeImage(41, 29);
        QVERIFY(d.operationBusy());
        if (gesture == "parameter")
            d.previewLayerParameter("brightness", 35);
        else if (gesture == "blend")
            d.previewLayerBlend(d.activeLayer(), 3);
        else if (gesture == "pen")
            d.beginPenGesture(12, 15);
        else {
            d.beginSliceGesture(9, 9);
            d.updateSliceGesture(40, 30);
        }
        // The old worker must be invalidated before release/commit changes the ID.
        QVERIFY(!d.operationBusy());
        if (gesture == "pen")
            d.endPenGesture();
        else if (gesture == "slice")
            d.endSliceGesture();
        else
            d.finishLayerPreview();
        const auto layers = d.layers();
        const auto paths = d.penPaths(), slices = d.slices();
        const auto history = d.history();
        block.release.release();
        block.pool->waitForDone();
        QTest::qWait(100);
        QCOMPARE(QSize(d.width(), d.height()), QSize(80, 60));
        QCOMPARE(d.layers(), layers);
        QCOMPARE(d.penPaths(), paths);
        QCOMPARE(d.slices(), slices);
        QCOMPARE(d.history(), history);
    }
    void backgroundGeometryFinalizesAnExistingTextEdit()
    {
        Document d;
        d.newDocument(80, 60, true, 16);
        d.setTool("text");
        d.beginStroke(5, 5);
        d.endStroke();
        d.setText("Keep");
        QVERIFY(d.textEditing());
        d.setBackgroundEditThreshold(0);
        d.resizeImage(40, 30);
        QVERIFY(d.operationBusy());
        QVERIFY(!d.textEditing());
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 10000);
        QCOMPARE(QSize(d.width(), d.height()), QSize(40, 30));
        d.undo();
        QCOMPARE(QSize(d.width(), d.height()), QSize(80, 60));
        QCOMPARE(d.text(), QString("Keep"));
    }

    void invalidOperationKeepsDocument()
    {
        Document d;
        populate(d, 8);
        d.setBackgroundEditThreshold(0);
        const auto before  = d.composite();
        const auto history = d.history();
        QSignalSpy errors(&d, &Document::errorOccurred);
        d.resizeImage(100000, 100000);
        QVERIFY(!d.operationBusy());
        QCOMPARE(errors.size(), 1);
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.history(), history);
    }
};
QTEST_MAIN(DocumentOperationsTest)
#include "DocumentOperationsTest.moc"
