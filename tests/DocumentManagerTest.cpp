#include "DocumentManager.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class DocumentManagerTest : public QObject
{
    Q_OBJECT
private slots:
    void independentDocumentsAndViews()
    {
        AiSettings      settings(nullptr, false);
        DocumentManager manager(&settings);
        auto *          first = manager.activeDocument();
        QVERIFY(manager.newDocument(40, 30, true, 16));
        QCOMPARE(manager.activeDocument(), first);
        first->setBackgroundEditThreshold(-1);
        first->setColor(Qt::red);
        first->fillWithColor(true);
        first->selectAll();
        const auto pixels  = first->composite();
        const auto history = first->historyIndex();
        manager.saveView({{"zoom", 1.25}, {"x", 80.}, {"y", 50.}});
        QVERIFY(manager.newDocument(32, 24, true, 8));
        auto *second = manager.activeDocument();
        QVERIFY(first != second);
        QVERIFY(first->documentId() != second->documentId());
        QCOMPARE(manager.count(), 2);
        QVERIFY(!second->hasSelection());
        QVERIFY(!second->canUndo());
        second->setBackgroundEditThreshold(-1);
        second->setColor(Qt::blue);
        second->fillWithColor(true);
        const auto blue = second->composite();
        second->undo();
        QCOMPARE(first->composite(), pixels);
        QCOMPARE(first->historyIndex(), history);
        QVERIFY(manager.activate(first->documentId()));
        QCOMPARE(manager.activeView().value("zoom").toDouble(), 1.25);
        QVERIFY(first->hasSelection());
        QCOMPARE(first->channelBits(), 16);
        QVERIFY(manager.activate(second->documentId()));
        second->redo();
        QCOMPARE(second->composite(), blue);
        QCOMPARE(second->channelBits(), 8);
    }
    void openingPreservesDirtyDocumentsAndReusesExistingFiles()
    {
        AiSettings      settings(nullptr, false);
        DocumentManager manager(&settings);
        auto *          source = manager.activeDocument();
        source->newDocument(20, 15, true);
        source->setBackgroundEditThreshold(-1);
        source->setColor(Qt::red);
        source->fillWithColor(true);
        const auto    original = source->composite();
        const auto    history  = source->historyIndex();
        QTemporaryDir dir;
        QImage        image(37, 23, QImage::Format_RGBA64_Premultiplied);
        image.fill(QColor::fromRgba64(12345, 23456, 34567, 65535));
        const auto url = QUrl::fromLocalFile(dir.filePath("native.png"));
        QVERIFY(image.save(url.toLocalFile()));
        QVERIFY(manager.openFile(url));
        auto *opened = manager.activeDocument();
        QVERIFY(opened != source);
        QTRY_VERIFY_WITH_TIMEOUT(!opened->operationBusy(), 5000);
        QCOMPARE(opened->width(), 37);
        QCOMPARE(opened->channelBits(), 16);
        QCOMPARE(opened->layerImage(0).pixelColor(1, 1), image.pixelColor(1, 1));
        QCOMPARE(source->composite(), original);
        QCOMPARE(source->historyIndex(), history);
        QVERIFY(source->modified());
        QVERIFY(manager.activate(source->documentId()));
        QVERIFY(manager.openFile(url));
        QCOMPARE(manager.activeDocument(), opened);
        QCOMPARE(manager.count(), 2);
        QSignalSpy errors(&manager, &DocumentManager::errorOccurred);
        QVERIFY(!manager.openFile(QUrl::fromLocalFile(dir.filePath("missing.png"))));
        QCOMPARE(manager.count(), 2);
        QCOMPARE(errors.size(), 1);
        const auto badUrl = QUrl::fromLocalFile(dir.filePath("broken.png"));
        QFile      bad(badUrl.toLocalFile());
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("not an image");
        bad.close();
        QVERIFY(manager.openFile(badUrl));
        QPointer<Document> failed(manager.activeDocument());
        QTRY_VERIFY_WITH_TIMEOUT(failed.isNull(), 5000);
        QCOMPARE(manager.count(), 2);
        QCOMPARE(manager.activeDocument(), opened);
        QCOMPARE(source->composite(), original);
    }
    void guardedCloseAndFreshLastDocument()
    {
        AiSettings      settings(nullptr, false);
        DocumentManager manager(&settings);
        auto *          first = manager.activeDocument();
        first->addLayer();
        const auto         id = first->documentId();
        QPointer<Document> retired(first);
        QVERIFY(!manager.closeDocument(id));
        QCOMPARE(manager.count(), 1);
        QVERIFY(manager.closeDocument(id, true));
        QCOMPARE(manager.count(), 1);
        QVERIFY(manager.activeId() != id);
        QVERIFY(!manager.activeDocument()->modified());
        QTRY_VERIFY(retired.isNull());
        QVERIFY(!manager.activate(id));
        QVERIFY(!manager.closeDocument(id, true));
    }
};
QTEST_MAIN(DocumentManagerTest)
#include "DocumentManagerTest.moc"
