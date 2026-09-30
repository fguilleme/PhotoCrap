#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include "Document.h"
#include "ShortcutManager.h"

class DocumentTest : public QObject {
    Q_OBJECT
private slots:
    void shortcutsAreValidatedAndResettable() {
        ShortcutManager shortcuts(nullptr, false);
        QCOMPARE(shortcuts.entries().size(), 33);
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("B"));
        QVERIFY(shortcuts.setSequence("toolBrush", "P"));
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("P"));
        QVERIFY(!shortcuts.setSequence("toolMove", "P"));
        QCOMPARE(shortcuts.sequence("toolMove"), QString("V"));
        QVERIFY(shortcuts.setSequence("toolBrush", ""));
        QVERIFY(shortcuts.sequence("toolBrush").isEmpty());
        shortcuts.resetDefaults();
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("B"));
        QVERIFY(!shortcuts.setSequence("missing", "Q"));
    }
    void brushUndoRedoAndCleanState() {
        Document d; QVERIFY(d.newDocument(64,64,true)); QVERIFY(!d.modified());
        d.setColor(Qt::red); d.setBrushSize(8); d.beginStroke(16,16); d.continueStroke(40,16); d.endStroke();
        QCOMPARE(d.composite().pixelColor(25,16),QColor(Qt::red)); QVERIFY(d.modified());
        d.undo(); QCOMPARE(d.composite().pixelColor(25,16).alpha(),0); QVERIFY(!d.modified());
        d.redo(); QCOMPARE(d.composite().pixelColor(25,16),QColor(Qt::red));
        d.undo(); d.setColor(Qt::blue); d.beginStroke(10,10); d.endStroke(); QVERIFY(!d.canRedo());
    }
    void layerOpacityVisibilityAndOrder() {
        Document d; d.newDocument(16,16); d.addLayer(); d.setTool("fill"); d.setColor(Qt::red);
        d.beginStroke(0,0); QCOMPARE(d.composite().pixelColor(8,8),QColor(Qt::red));
        d.setLayerOpacity(1,.5); const auto c=d.composite().pixelColor(8,8); QCOMPARE(c.red(),255); QVERIFY(qAbs(c.green()-128)<=1);
        d.setLayerVisible(1,false); QCOMPARE(d.composite().pixelColor(8,8),QColor(Qt::white));
        d.setLayerVisible(1,true); d.moveLayer(-1); QCOMPARE(d.composite().pixelColor(8,8),QColor(Qt::white));
        d.undo(); QCOMPARE(d.activeLayer(),1);
    }
    void lockPreventsPixelChanges() {
        Document d; d.newDocument(32,32); d.setLayerLocked(0,true);
        const QImage before=d.composite(); const int history=d.historyIndex();
        d.beginStroke(10,10); d.endStroke(); d.applyFilter("invert"); d.clearSelected();
        QCOMPARE(d.composite(),before); QCOMPARE(d.historyIndex(),history);
    }
    void selectionClipsPaintingAndCropping() {
        Document d; d.newDocument(32,32,true); d.setTool("select"); d.beginStroke(8,8); d.continueStroke(15,15); d.endStroke();
        QCOMPARE(d.selection(),QRect(8,8,8,8));
        d.setTool("brush"); d.setColor(Qt::green); d.setBrushSize(50); d.beginStroke(12,12); d.endStroke();
        QCOMPARE(d.composite().pixelColor(0,0).alpha(),0); QCOMPARE(d.composite().pixelColor(12,12),QColor(Qt::green));
        d.cropToSelection(); QCOMPARE(d.width(),8); QCOMPARE(d.height(),8); QVERIFY(!d.hasSelection());
        d.undo(); QCOMPARE(d.width(),32); QCOMPARE(d.selection(),QRect(8,8,8,8));
    }
    void projectRoundTripAndExport() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        Document d; d.newDocument(24,18,true); d.addLayer(); d.setTool("fill"); d.setColor(QColor(60,140,220,180)); d.beginStroke(0,0);
        d.renameLayer(1,"Bleu"); d.setLayerOpacity(1,.7); d.setLayerBlend(1,2); d.setLayerLocked(1,true);
        auto url=QUrl::fromLocalFile(dir.filePath("test.pcr")); QVERIFY(d.saveProject(url)); QVERIFY(!d.modified());
        const QImage expected=d.composite(); Document loaded; QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(),expected); QCOMPARE(loaded.layers(),d.layers()); QCOMPARE(loaded.activeLayer(),1); QVERIFY(!loaded.modified());
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.png"))));
        QCOMPARE(QImage(dir.filePath("export.png")).convertToFormat(QImage::Format_ARGB32_Premultiplied),expected);
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.jpg"))));
        QVERIFY(!QImage(dir.filePath("export.jpg")).hasAlphaChannel());
        d.setLayerLocked(1,false); QVERIFY(d.modified()); d.undo(); QVERIFY(!d.modified());
    }
    void malformedProjectPreservesDocument() {
        QTemporaryDir dir; QFile file(dir.filePath("broken.pcr")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"format\":\"PhotoCrap\",\"version\":1,\"width\":32,\"height\":32,\"layers\":[{\"png\":\"nonsense\"}]}"); file.close();
        Document d; d.newDocument(8,8); const QImage before=d.composite();
        QVERIFY(!d.openFile(QUrl::fromLocalFile(file.fileName()))); QCOMPARE(d.composite(),before); QCOMPARE(d.width(),8);
        QVERIFY(!d.newDocument(20000,100)); QCOMPARE(d.width(),8);
        QVERIFY(!d.newDocument(-1,100));
    }
    void shapePreviewIsSingleUndoOperation() {
        Document d; d.newDocument(64,64,true); d.setTool("rectangle"); d.setColor(Qt::blue); d.setBrushSize(2);
        d.beginStroke(5,5); d.continueStroke(25,25); d.continueStroke(45,45); d.endStroke();
        QCOMPARE(d.historyIndex(),1); QCOMPARE(d.composite().pixelColor(40,40),QColor(Qt::blue));
        d.undo(); QCOMPARE(d.composite().pixelColor(40,40).alpha(),0);
        d.redo(); d.setTool("eraser"); d.setBrushSize(10); d.beginStroke(20,20); d.endStroke();
        QCOMPARE(d.composite().pixelColor(20,20).alpha(),0);
    }
    void fillIsContiguousAndHandlesPartialAlpha() {
        Document d; d.newDocument(24,24); d.setColor(Qt::black); d.setBrushSize(2);
        d.beginStroke(12,0); d.continueStroke(12,24); d.endStroke();
        d.setTool("fill"); d.setColor(Qt::red); d.setBrushOpacity(.5); d.beginStroke(0,0);
        QVERIFY(d.composite().pixelColor(3,3).green()<200); QCOMPARE(d.composite().pixelColor(20,3),QColor(Qt::white));
    }
    void transformsFiltersAndMerge() {
        Document d; d.newDocument(12,8); d.addLayer(); d.setTool("fill"); d.setColor(Qt::red); d.beginStroke(0,0);
        d.applyFilter("invert"); QCOMPARE(d.composite().pixelColor(0,0),QColor(Qt::cyan));
        d.applyFilter("grayscale"); auto c=d.composite().pixelColor(0,0); QCOMPARE(c.red(),c.green());
        d.applyFilter("blur",2); d.applyFilter("sharpen"); d.transformImage("rotateRight"); QCOMPARE(d.width(),8); QCOMPARE(d.height(),12);
        d.resizeImage(20,30); QCOMPARE(d.width(),20); QCOMPARE(d.height(),30);
        QImage before=d.composite(); d.mergeDown(); QCOMPARE(d.layers().size(),1); QCOMPARE(d.composite(),before);
        d.undo(); QCOMPARE(d.layers().size(),2);
    }
    void moveSelectionAndCancel() {
        Document d; d.newDocument(32,32,true); d.setColor(Qt::red); d.setBrushSize(4); d.beginStroke(6,6); d.endStroke();
        d.setTool("select"); d.beginStroke(2,2); d.continueStroke(10,10); d.endStroke();
        d.setTool("move"); d.beginStroke(6,6); d.continueStroke(16,16); d.endStroke();
        QCOMPARE(d.composite().pixelColor(6,6).alpha(),0); QCOMPARE(d.composite().pixelColor(16,16),QColor(Qt::red));
        QCOMPARE(d.selection().topLeft(),QPoint(12,12));
        d.deselect(); const QImage before=d.composite(); d.setTool("brush"); d.beginStroke(25,25); d.cancelStroke();
        QCOMPARE(d.composite(),before);
    }
    void failedSaveDoesNotMarkClean() {
        Document d; d.newDocument(8,8); d.addLayer(); QVERIFY(d.modified());
        QVERIFY(!d.saveProject(QUrl::fromLocalFile("/missing-directory-photocrap/project.pcr"))); QVERIFY(d.modified());
    }
};
QTEST_MAIN(DocumentTest)
#include "DocumentTest.moc"
