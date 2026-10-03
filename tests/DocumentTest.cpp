#include "Document.h"
#include "RecentFilesManager.h"
#include "ShortcutManager.h"
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class DocumentTest : public QObject
{
    Q_OBJECT
private slots:
    void recentFilesAreOrderedDeduplicatedLimitedAndPruned()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        RecentFilesManager recent(nullptr, false);
        QList<QUrl>        urls;
        for (int i = 0; i < 12; ++i) {
            const QString path = dir.filePath(QString("image-%1.png").arg(i));
            QFile         file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("x");
            file.close();
            urls.append(QUrl::fromLocalFile(path));
            recent.add(urls.last());
        }
        QCOMPARE(recent.entries().size(), 10);
        QCOMPARE(recent.entries().first().toMap().value("url").toUrl().toLocalFile(),
                 QFileInfo(urls.last().toLocalFile()).canonicalFilePath());
        recent.add(urls[5]);
        QCOMPARE(recent.entries().size(), 10);
        QCOMPARE(recent.entries().first().toMap().value("url").toUrl().toLocalFile(),
                 QFileInfo(urls[5].toLocalFile()).canonicalFilePath());
        QFile::remove(urls[5].toLocalFile());
        recent.refresh();
        QCOMPARE(recent.entries().size(), 9);
        recent.remove(urls[10]);
        QCOMPARE(recent.entries().size(), 8);
        recent.clear();
        QVERIFY(recent.entries().isEmpty());
        recent.add(QUrl::fromLocalFile(dir.filePath("missing.png")));
        QVERIFY(recent.entries().isEmpty());
    }
    void recentFilesPersistAcrossSessions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QCoreApplication::setOrganizationName("PhotoCrapTests");
        QCoreApplication::setApplicationName("RecentFilesTest");
        const QString path = dir.filePath("saved.pcr");
        QFile         file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("project");
        file.close();
        {
            RecentFilesManager recent(nullptr, true);
            recent.clear();
            recent.add(QUrl::fromLocalFile(path));
            QCOMPARE(recent.entries().size(), 1);
        }
        {
            RecentFilesManager restored(nullptr, true);
            QCOMPARE(restored.entries().size(), 1);
            QCOMPARE(restored.entries().first().toMap().value("name").toString(),
                     QString("saved.pcr"));
            restored.clear();
        }
    }
    void shortcutsAreValidatedAndResettable()
    {
        ShortcutManager shortcuts(nullptr, false);
        QCOMPARE(shortcuts.entries().size(), 37);
        QCOMPARE(shortcuts.sequence("toolBrush"), QString("B"));
        QCOMPARE(shortcuts.sequence("defaultColors"), QString("D"));
        QCOMPARE(shortcuts.sequence("swapColors"), QString("X"));
        QCOMPARE(shortcuts.sequence("brushSofter"), QString("Shift+["));
        QCOMPARE(shortcuts.sequence("brushHarder"), QString("Shift+]"));
        QCOMPARE(shortcuts.sequence("toolGradient"), QString("Shift+G"));
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
    void foregroundAndBackgroundColorsCanResetAndSwap()
    {
        Document d;
        QCOMPARE(d.color(), QColor(Qt::black));
        QCOMPARE(d.secondaryColor(), QColor(Qt::white));
        d.setColor(Qt::red);
        d.setSecondaryColor(Qt::blue);
        d.resetColors();
        QCOMPARE(d.color(), QColor(Qt::black));
        QCOMPARE(d.secondaryColor(), QColor(Qt::white));
        d.swapColors();
        QCOMPARE(d.color(), QColor(Qt::white));
        QCOMPARE(d.secondaryColor(), QColor(Qt::black));
    }
    void brushUndoRedoAndCleanState()
    {
        Document d;
        QVERIFY(d.newDocument(64, 64, true));
        QVERIFY(!d.modified());
        d.setColor(Qt::red);
        d.setBrushSize(8);
        d.beginStroke(16, 16);
        d.continueStroke(40, 16);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(25, 16), QColor(Qt::red));
        QVERIFY(d.modified());
        d.undo();
        QCOMPARE(d.composite().pixelColor(25, 16).alpha(), 0);
        QVERIFY(!d.modified());
        d.redo();
        QCOMPARE(d.composite().pixelColor(25, 16), QColor(Qt::red));
        d.undo();
        d.setColor(Qt::blue);
        d.beginStroke(10, 10);
        d.endStroke();
        QVERIFY(!d.canRedo());
    }
    void brushHardnessFlowOpacityAndSpacingFollowDabs()
    {
        Document soft;
        QVERIFY(soft.newDocument(64, 64, true));
        soft.setColor(Qt::black);
        soft.setBrushSize(20);
        soft.setBrushHardness(0);
        soft.beginStroke(32, 32);
        soft.endStroke();
        QVERIFY(soft.composite().pixelColor(32, 32).alpha() > 225);
        const int softEdge = soft.composite().pixelColor(40, 32).alpha();
        QVERIFY(softEdge > 0 && softEdge < 180);

        Document flow;
        QVERIFY(flow.newDocument(64, 64, true));
        flow.setColor(Qt::black);
        flow.setBrushSize(20);
        flow.setBrushFlow(.25);
        flow.beginStroke(32, 32);
        flow.endStroke();
        const int oneDab = flow.composite().pixelColor(32, 32).alpha();
        QVERIFY(oneDab >= 60 && oneDab <= 70);
        flow.undo();
        flow.setBrushSpacing(.01);
        flow.setBrushSmoothing(0);
        flow.beginStroke(30, 32);
        flow.continueStroke(34, 32);
        flow.endStroke();
        QVERIFY(flow.composite().pixelColor(32, 32).alpha() > oneDab);

        Document capped;
        QVERIFY(capped.newDocument(64, 64, true));
        capped.setColor(Qt::black);
        capped.setBrushSize(20);
        capped.setBrushOpacity(.5);
        capped.setBrushFlow(1);
        capped.setBrushSpacing(.01);
        capped.setBrushSmoothing(0);
        capped.beginStroke(30, 32);
        capped.continueStroke(34, 32);
        capped.endStroke();
        const int cappedAlpha = capped.composite().pixelColor(32, 32).alpha();
        QVERIFY(cappedAlpha >= 125 && cappedAlpha <= 130);

        Document spaced;
        QVERIFY(spaced.newDocument(64, 32, true));
        spaced.setColor(Qt::black);
        spaced.setBrushSize(10);
        spaced.setBrushSpacing(2);
        spaced.setBrushSmoothing(0);
        spaced.beginStroke(10, 16);
        spaced.continueStroke(50, 16);
        spaced.endStroke();
        QCOMPARE(spaced.composite().pixelColor(20, 16).alpha(), 0);
        QCOMPARE(spaced.composite().pixelColor(30, 16).alpha(), 255);

        Document mode;
        QVERIFY(mode.newDocument(32, 32));
        mode.setTool("fill");
        mode.setColor(Qt::green);
        mode.beginStroke(0, 0);
        mode.setTool("brush");
        mode.setColor(Qt::red);
        mode.setBrushSize(12);
        mode.setBrushBlend(3);
        mode.beginStroke(16, 16);
        mode.endStroke();
        const QColor multiplied = mode.composite().pixelColor(16, 16);
        QVERIFY(multiplied.red() < 5 && multiplied.green() < 5 && multiplied.blue() < 5);
    }
    void layerOpacityVisibilityAndOrder()
    {
        Document d;
        d.newDocument(16, 16);
        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::red));
        d.setLayerOpacity(1, .5);
        const auto c = d.composite().pixelColor(8, 8);
        QCOMPARE(c.red(), 255);
        QVERIFY(qAbs(c.green() - 128) <= 1);
        d.setLayerVisible(1, false);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
        d.setLayerVisible(1, true);
        d.moveLayer(-1);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.activeLayer(), 1);
    }
    void clippingMasksRestrictRenderingAndPersist()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32, true));
        d.setColor(Qt::black);
        d.setBrushSize(12);
        d.beginStroke(8, 16);
        d.endStroke();
        QVERIFY(d.composite().pixelColor(8, 16).alpha() > 0);
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);

        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        QCOMPARE(d.composite().pixelColor(28, 16), QColor(Qt::red));

        d.setClippingMask(true);
        QVERIFY(d.layers().first().toMap().value("clippingMask").toBool());
        QCOMPARE(d.composite().pixelColor(8, 16), QColor(Qt::red));
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite().pixelColor(28, 16), QColor(Qt::red));
        d.redo();
        QCOMPARE(d.composite().pixelColor(28, 16).alpha(), 0);

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QUrl url = QUrl::fromLocalFile(dir.filePath("clipping-mask.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QVERIFY(loaded.layers().first().toMap().value("clippingMask").toBool());
        QCOMPARE(loaded.composite(), d.composite());
    }
    void multipleLayerSelectionCreatesPersistentPassThroughGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(24, 24));
        d.addLayer();
        d.addLayer();
        d.selectLayer(0);
        d.selectLayer(2, true);
        QCOMPARE(d.selectedLayerCount(), 2);
        QVERIFY(d.canGroupSelected());
        const QImage before = d.composite();

        d.groupSelectedLayers();
        QCOMPARE(d.layers().size(), 4);
        QCOMPARE(d.layers().first().toMap().value("kind").toString(), QString("group"));
        QVERIFY(d.canUngroupActive());
        QCOMPARE(d.composite(), before);
        const QString groupId = d.layers().first().toMap().value("groupId").toString();
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(d.layers()[1].toMap().value("groupId").toString(), groupId);
        QCOMPARE(d.layers()[2].toMap().value("groupId").toString(), groupId);

        d.setLayerVisible(d.activeLayer(), false);
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite(), before);

        d.toggleGroupCollapsed(d.activeLayer());
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layers().first().toMap().value("kind").toString(), QString("group"));
        const auto url = QUrl::fromLocalFile(dir.filePath("grouped.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layers().first().toMap().value("kind").toString(), QString("group"));
        QCOMPARE(loaded.layers().size(), 2);
        QCOMPARE(loaded.composite(), before);
        loaded.toggleGroupCollapsed(loaded.activeLayer());
        QCOMPARE(loaded.layers().size(), 4);
        loaded.ungroupActiveLayers();
        QCOMPARE(loaded.layers().size(), 3);
        QVERIFY(!loaded.canUngroupActive());
        QCOMPARE(loaded.composite(), before);
    }
    void lockPreventsPixelChanges()
    {
        Document d;
        d.newDocument(32, 32);
        d.setLayerLocked(0, true);
        const QImage before  = d.composite();
        const int    history = d.historyIndex();
        d.beginStroke(10, 10);
        d.endStroke();
        d.applyFilter("invert");
        d.clearSelected();
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.historyIndex(), history);
    }
    void selectionClipsPaintingAndCropping()
    {
        Document d;
        d.newDocument(32, 32, true);
        d.setTool("select");
        d.beginStroke(8, 8);
        d.continueStroke(15, 15);
        d.endStroke();
        QCOMPARE(d.selection(), QRect(8, 8, 8, 8));
        d.setTool("brush");
        d.setColor(Qt::green);
        d.setBrushSize(50);
        d.beginStroke(12, 12);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(0, 0).alpha(), 0);
        QCOMPARE(d.composite().pixelColor(12, 12), QColor(Qt::green));
        d.cropToSelection();
        QCOMPARE(d.width(), 8);
        QCOMPARE(d.height(), 8);
        QVERIFY(!d.hasSelection());
        d.undo();
        QCOMPARE(d.width(), 32);
        QCOMPARE(d.selection(), QRect(8, 8, 8, 8));
    }
    void projectRoundTripAndExport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        d.newDocument(24, 18, true);
        d.addLayer();
        d.setTool("fill");
        d.setColor(QColor(60, 140, 220, 180));
        d.beginStroke(0, 0);
        d.renameLayer(1, "Bleu");
        d.setLayerOpacity(1, .7);
        d.setLayerBlend(1, 2);
        d.setLayerLocked(1, true);
        auto url = QUrl::fromLocalFile(dir.filePath("test.pcr"));
        QVERIFY(d.saveProject(url));
        QVERIFY(!d.modified());
        const QImage expected = d.composite();
        Document     loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(), expected);
        QCOMPARE(loaded.layers(), d.layers());
        QCOMPARE(loaded.activeLayer(), 1);
        QVERIFY(!loaded.modified());
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.png"))));
        QCOMPARE(
            QImage(dir.filePath("export.png")).convertToFormat(QImage::Format_ARGB32_Premultiplied),
            expected);
        QVERIFY(loaded.exportImage(QUrl::fromLocalFile(dir.filePath("export.jpg"))));
        QVERIFY(!QImage(dir.filePath("export.jpg")).hasAlphaChannel());
        d.setLayerLocked(1, false);
        QVERIFY(d.modified());
        d.undo();
        QVERIFY(!d.modified());
    }
    void malformedProjectPreservesDocument()
    {
        QTemporaryDir dir;
        QFile         file(dir.filePath("broken.pcr"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"format\":\"PhotoCrap\",\"version\":1,\"width\":32,\"height\":32,\"layers\":["
                   "{\"png\":\"nonsense\"}]}");
        file.close();
        Document d;
        d.newDocument(8, 8);
        const QImage before = d.composite();
        QVERIFY(!d.openFile(QUrl::fromLocalFile(file.fileName())));
        QCOMPARE(d.composite(), before);
        QCOMPARE(d.width(), 8);
        QVERIFY(!d.newDocument(20000, 100));
        QCOMPARE(d.width(), 8);
        QVERIFY(!d.newDocument(-1, 100));
    }
    void shapePreviewIsSingleUndoOperation()
    {
        Document d;
        d.newDocument(64, 64, true);
        d.setTool("rectangle");
        d.setColor(Qt::blue);
        d.setBrushSize(2);
        d.beginStroke(5, 5);
        d.continueStroke(25, 25);
        d.continueStroke(45, 45);
        d.endStroke();
        QCOMPARE(d.historyIndex(), 1);
        QCOMPARE(d.composite().pixelColor(40, 40), QColor(Qt::blue));
        d.undo();
        QCOMPARE(d.composite().pixelColor(40, 40).alpha(), 0);
        d.redo();
        d.setTool("eraser");
        d.setBrushSize(10);
        d.beginStroke(20, 20);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(20, 20).alpha(), 0);
    }
    void fillIsContiguousAndHandlesPartialAlpha()
    {
        Document d;
        d.newDocument(24, 24);
        d.setColor(Qt::black);
        d.setBrushSize(2);
        d.beginStroke(12, 0);
        d.continueStroke(12, 24);
        d.endStroke();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.setBrushOpacity(.5);
        d.beginStroke(0, 0);
        QVERIFY(d.composite().pixelColor(3, 3).green() < 200);
        QCOMPARE(d.composite().pixelColor(20, 3), QColor(Qt::white));
    }
    void transformsFiltersAndMerge()
    {
        Document d;
        d.newDocument(12, 8);
        d.addLayer();
        d.setTool("fill");
        d.setColor(Qt::red);
        d.beginStroke(0, 0);
        d.applyFilter("invert");
        QCOMPARE(d.composite().pixelColor(0, 0), QColor(Qt::cyan));
        d.applyFilter("grayscale");
        auto c = d.composite().pixelColor(0, 0);
        QCOMPARE(c.red(), c.green());
        d.applyFilter("blur", 2);
        d.applyFilter("sharpen");
        d.transformImage("rotateRight");
        QCOMPARE(d.width(), 8);
        QCOMPARE(d.height(), 12);
        d.resizeImage(20, 30);
        QCOMPARE(d.width(), 20);
        QCOMPARE(d.height(), 30);
        QImage before = d.composite();
        d.mergeDown();
        QCOMPARE(d.layers().size(), 1);
        QCOMPARE(d.composite(), before);
        d.undo();
        QCOMPARE(d.layers().size(), 2);
    }
    void layerMasksCanBePaintedAndToggled()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        d.addLayerMask(false);
        QVERIFY(d.editingMask());
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.invertLayerMask();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.setTool("brush");
        d.setBrushSize(8);
        d.beginStroke(16, 16);
        d.endStroke();
        QCOMPARE(d.maskImage(0).pixelColor(16, 16).red(), 0);
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.beginLayerPreview();
        d.previewLayerParameter("_maskDensity", 0);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.setMaskEnabled(false);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.undo();
        QCOMPARE(d.composite().pixelColor(16, 16).alpha(), 0);
        d.undo();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
    }
    void adjustmentLayersRenderAndPreviewAsOneEdit()
    {
        Document d;
        QVERIFY(d.newDocument(12, 12));
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.layers().size(), 2);
        QVERIFY(!d.maskImage(1).isNull());
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::black));
        d.undo();
        QCOMPARE(d.layers().size(), 1);

        d.addAdjustmentLayer("brightnessContrast");
        const int before = d.historyIndex();
        d.beginLayerPreview();
        d.previewLayerParameter("brightness", -20);
        d.previewLayerParameter("brightness", -40);
        QCOMPARE(d.historyIndex(), before);
        QVERIFY(d.composite().pixelColor(4, 4).red() < 200);
        d.finishLayerPreview("Luminosité");
        QCOMPARE(d.historyIndex(), before + 1);
        d.undo();
        QCOMPARE(d.composite().pixelColor(4, 4), QColor(Qt::white));
    }
    void everyAdjustmentTypeProducesAComposite()
    {
        const QStringList types = {
            "brightnessContrast", "levels",       "curves",     "exposure",    "vibrance",
            "hueSaturation",      "colorBalance", "blackWhite", "photoFilter", "channelMixer",
            "colorLookup",        "invert",       "posterize",  "threshold",   "gradientMap",
            "selectiveColor",     "gaussianBlur", "motionBlur", "boxBlur",     "unsharpMask",
            "highPass",           "mosaic",       "noise"};
        Document d;
        QVERIFY(d.newDocument(4, 4));
        for (const auto &type : types) {
            const int count = d.layers().size();
            d.addAdjustmentLayer(type);
            QCOMPARE(d.layers().size(), count + 1);
            QVERIFY(!d.composite().isNull());
            d.undo();
        }
    }
    void curvesAndLevelsUseDedicatedParametersAndHistogram()
    {
        Document d;
        QVERIFY(d.newDocument(32, 32));
        const QVariantList histogram = d.histogram();
        QCOMPARE(histogram.size(), 256);
        QCOMPARE(histogram.at(255).toDouble(), 1.);
        QCOMPARE(histogram.at(0).toDouble(), 0.);

        d.addAdjustmentLayer("curves");
        d.beginLayerPreview();
        d.previewLayerParameter("points", QVariantList{QVariantMap{{"x", 0}, {"y", 0}},
                                                       QVariantMap{{"x", 255}, {"y", 0}}});
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::black));
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
        d.undo();

        d.addAdjustmentLayer("levels");
        d.beginLayerPreview();
        d.previewLayerParameter("outputWhite", 96);
        const QColor limited = d.composite().pixelColor(16, 16);
        QVERIFY(limited.red() >= 95 && limited.red() <= 97);
        d.cancelLayerPreview();
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::white));
    }
    void pointAdjustmentPreviewsRemainInteractiveAtUhd()
    {
        Document d;
        QVERIFY(d.newDocument(3840, 2160));
        d.addAdjustmentLayer("blackWhite");
        d.beginLayerPreview();
        QElapsedTimer timer;
        timer.start();
        d.previewLayerParameter("reds", 65);
        const QImage first               = d.composite();
        const qint64 elapsedMilliseconds = timer.elapsed();
        QVERIFY(!first.isNull());
        QVERIFY2(
            elapsedMilliseconds < 2000,
            qPrintable(QString("UHD black-and-white preview took %1 ms").arg(elapsedMilliseconds)));
        d.cancelLayerPreview();
        d.undo();

        d.addAdjustmentLayer("brightnessContrast");
        d.beginLayerPreview();
        timer.restart();
        d.previewLayerParameter("brightness", 25);
        const QImage second                        = d.composite();
        const qint64 brightnessElapsedMilliseconds = timer.elapsed();
        QVERIFY(!second.isNull());
        QVERIFY2(brightnessElapsedMilliseconds < 2000,
                 qPrintable(QString("UHD brightness/contrast preview took %1 ms")
                                .arg(brightnessElapsedMilliseconds)));
        d.cancelLayerPreview();
    }
    void asynchronousPreviewKeepsOnlyTheLatestRequestedValue()
    {
        Document d;
        QVERIFY(d.newDocument(1920, 1080));
        d.addAdjustmentLayer("brightnessContrast");
        d.beginLayerPreview();
        QSignalSpy    rendered(&d, &Document::imageChanged);
        QElapsedTimer scheduling;
        scheduling.start();
        d.previewLayerParameter("brightness", -10);
        d.previewLayerParameter("brightness", -40);
        d.previewLayerParameter("brightness", -20);
        QVERIFY2(scheduling.elapsed() < 100, "preview requests must not render on the UI thread");
        QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty(), 5000);
        const QImage latestPreview = d.displayComposite();
        const QColor latest =
            latestPreview.pixelColor(latestPreview.width() / 2, latestPreview.height() / 2);
        QVERIFY(latest.red() >= 203 && latest.red() <= 205);
        QCOMPARE(d.historyIndex(), 1);
        d.cancelLayerPreview();
    }
    void addingAnAdjustmentShowsAScreenProxyBeforeTheFullRender()
    {
        Document d;
        QVERIFY(d.newDocument(1920, 1080));
        d.setPreviewSize(800, 450);
        (void)d.composite();
        QSignalSpy     rendered(&d, &Document::imageChanged);
        QVector<QSize> renderedSizes;
        connect(&d, &Document::imageChanged, &d,
                [&] { renderedSizes.append(d.displayComposite().size()); });
        QElapsedTimer scheduling;
        scheduling.start();
        d.addAdjustmentLayer("gaussianBlur");
        QVERIFY2(scheduling.elapsed() < 100,
                 "adding an adjustment must only schedule rendering on the UI thread");
        QTRY_VERIFY_WITH_TIMEOUT(!rendered.isEmpty() && renderedSizes.contains(QSize(800, 450)),
                                 20000);
        QTRY_VERIFY_WITH_TIMEOUT(rendered.count() >= 3, 10000);
        QCOMPARE(d.composite().size(), QSize(1920, 1080));
        QCOMPARE(d.displayComposite().size(), QSize(800, 450));
    }
    void filterLayersAreNonDestructiveAndEditable()
    {
        Document d;
        QVERIFY(d.newDocument(64, 64));
        d.setColor(Qt::black);
        d.setBrushSize(9);
        d.beginStroke(32, 32);
        d.endStroke();
        const QImage      original = d.composite();
        const QStringList filters  = {"gaussianBlur", "motionBlur", "boxBlur", "unsharpMask",
                                      "highPass",     "mosaic",     "noise"};
        for (const auto &filter : filters) {
            d.addAdjustmentLayer(filter);
            QCOMPARE(d.layers().size(), 2);
            const QImage filtered = d.composite();
            QVERIFY2(filtered != original, qPrintable(filter + " should alter the image"));
            const int historyBefore = d.historyIndex();
            d.beginLayerPreview();
            const QString parameter = filter == "motionBlur" ? QStringLiteral("angle")
                                      : filter == "mosaic"   ? QStringLiteral("cellSize")
                                      : filter == "noise"    ? QStringLiteral("amount")
                                                             : QStringLiteral("radius");
            d.previewLayerParameter(parameter, filter == "motionBlur" ? 45 : 16);
            d.finishLayerPreview(QStringLiteral("Modifier le filtre"));
            QCOMPARE(d.historyIndex(), historyBefore + 1);
            d.undo();
            d.undo();
            QCOMPARE(d.composite(), original);
        }
    }
    void frequencySeparationHasRealtimePreview()
    {
        Document d;
        QVERIFY(d.newDocument(24, 24));
        d.setColor(Qt::black);
        d.setBrushSize(3);
        d.beginStroke(12, 12);
        d.endStroke();
        const QImage original = d.composite();
        d.addFrequencySeparation(4);
        QCOMPARE(d.layers().size(), 3);
        QVERIFY(qAbs(d.composite().pixelColor(12, 12).red() - original.pixelColor(12, 12).red()) <=
                1);
        const QImage initialLow      = d.layerImage(1);
        const int    before          = d.historyIndex();
        const int    thumbnailBefore = d.thumbnailRevision();
        d.beginLayerPreview();
        d.previewLayerParameter("radius", 9);
        QCOMPARE(d.thumbnailRevision(), thumbnailBefore);
        QVERIFY(d.layerImage(1) != initialLow);
        QCOMPARE(d.historyIndex(), before);
        d.finishLayerPreview("Rayon des fréquences");
        QVERIFY(d.thumbnailRevision() > thumbnailBefore);
        QCOMPARE(d.historyIndex(), before + 1);
        d.undo();
        QVERIFY(qAbs(d.composite().pixelColor(12, 12).red() - original.pixelColor(12, 12).red()) <=
                1);
    }
    void highFrequencyMaskUsesRegionalCompositeWithoutChangingResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(128, 128));
        d.setColor(Qt::black);
        d.setBrushSize(12);
        d.beginStroke(64, 64);
        d.endStroke();
        d.addFrequencySeparation(6);
        d.addLayerMask(true);
        d.setTool("brush");
        d.setBrushSize(18);
        (void)d.composite();
        d.beginStroke(48, 64);
        d.continueStroke(80, 64);
        (void)d.composite();
        d.endStroke();
        const QImage regional = d.composite();
        const auto   url      = QUrl::fromLocalFile(dir.filePath("regional.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        const QImage rebuilt   = loaded.composite();
        int          different = 0, maxDelta = 0;
        QRect        differenceBounds;
        for (int y = 0; y < regional.height(); ++y)
            for (int x = 0; x < regional.width(); ++x) {
                const QColor a = regional.pixelColor(x, y), b = rebuilt.pixelColor(x, y);
                const int    delta = qMax(qAbs(a.red() - b.red()), qMax(qAbs(a.green() - b.green()),
                                                                        qAbs(a.blue() - b.blue())));
                if (delta) {
                    ++different;
                    maxDelta         = qMax(maxDelta, delta);
                    differenceBounds = differenceBounds.united(QRect(x, y, 1, 1));
                }
            }
        QVERIFY2(maxDelta <= 1, qPrintable(QString("%1 pixels differ, max %2, bounds %3,%4 %5x%6")
                                               .arg(different)
                                               .arg(maxDelta)
                                               .arg(differenceBounds.x())
                                               .arg(differenceBounds.y())
                                               .arg(differenceBounds.width())
                                               .arg(differenceBounds.height())));
    }
    void blurAdjustmentUsesRegionalCompositeWithoutChangingResult()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(256, 192, true));
        d.setColor(Qt::black);
        d.setBrushSize(24);
        d.beginStroke(64, 96);
        d.endStroke();
        d.addAdjustmentLayer("gaussianBlur");
        (void)d.composite();
        d.setActiveLayer(0);
        d.setColor(Qt::red);
        d.setBrushSize(18);
        d.beginStroke(176, 96);
        d.endStroke();
        const QImage regional = d.composite();

        const auto url = QUrl::fromLocalFile(dir.filePath("regional-blur.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.composite(), regional);
    }
    void selectionDefinesAdjustmentMaskLikePhotoshop()
    {
        Document d;
        QVERIFY(d.newDocument(20, 20));
        d.setTool("select");
        d.beginStroke(5, 5);
        d.continueStroke(14, 14);
        d.endStroke();
        d.addAdjustmentLayer("invert");
        QCOMPARE(d.maskImage(1).pixelColor(8, 8).red(), 255);
        QCOMPARE(d.maskImage(1).pixelColor(1, 1).red(), 0);
        QCOMPARE(d.composite().pixelColor(8, 8), QColor(Qt::black));
        QCOMPARE(d.composite().pixelColor(1, 1), QColor(Qt::white));
    }
    void allPhotoshopBlendModesAreSelectable()
    {
        Document d;
        QVERIFY(d.newDocument(4, 4));
        d.addLayer();
        for (int mode = 0; mode <= 26; ++mode) {
            d.setLayerBlend(1, mode);
            const auto layers = d.layers();
            QCOMPARE(layers.first().toMap().value("blend").toInt(), mode);
            QVERIFY(!d.composite().isNull());
        }
    }
    void advancedLayersSurviveProjectRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Document d;
        QVERIFY(d.newDocument(20, 16));
        d.addLayerMask(true);
        d.setEditingMask(false);
        d.addAdjustmentLayer("posterize");
        d.beginLayerPreview();
        d.previewLayerParameter("levels", 6);
        d.finishLayerPreview();
        d.addFrequencySeparation(5);
        d.setLayerBlend(d.activeLayer(), 8);
        const QImage expected = d.composite();
        const auto   url      = QUrl::fromLocalFile(dir.filePath("advanced.pcr"));
        QVERIFY(d.saveProject(url));
        Document loaded;
        QVERIFY(loaded.openFile(url));
        QCOMPARE(loaded.layers(), d.layers());
        QCOMPARE(loaded.composite(), expected);
        QVERIFY(!loaded.maskImage(0).isNull());
    }
    void moveSelectionAndCancel()
    {
        Document d;
        d.newDocument(32, 32, true);
        d.setColor(Qt::red);
        d.setBrushSize(4);
        d.beginStroke(6, 6);
        d.endStroke();
        d.setTool("select");
        d.beginStroke(2, 2);
        d.continueStroke(10, 10);
        d.endStroke();
        d.setTool("move");
        d.beginStroke(6, 6);
        d.continueStroke(16, 16);
        d.endStroke();
        QCOMPARE(d.composite().pixelColor(6, 6).alpha(), 0);
        QCOMPARE(d.composite().pixelColor(16, 16), QColor(Qt::red));
        QCOMPARE(d.selection().topLeft(), QPoint(12, 12));
        d.deselect();
        const QImage before = d.composite();
        d.setTool("brush");
        d.beginStroke(25, 25);
        d.cancelStroke();
        QCOMPARE(d.composite(), before);
    }
    void failedSaveDoesNotMarkClean()
    {
        Document d;
        d.newDocument(8, 8);
        d.addLayer();
        QVERIFY(d.modified());
        QVERIFY(!d.saveProject(QUrl::fromLocalFile("/missing-directory-photocrap/project.pcr")));
        QVERIFY(d.modified());
    }
};
QTEST_MAIN(DocumentTest)
#include "DocumentTest.moc"
