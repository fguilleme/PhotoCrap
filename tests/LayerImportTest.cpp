#include "Document.h"
#include "Model3DTestImage.h"
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
    void skinnedModelUsesDefaultJointPose()
    {
        QTemporaryDir dir;
        Document      document;
        QVERIFY(document.newDocument(240, 160, true, 16));
        using Model3DTest::Pose;
        int layer = 0;
        for (auto pose : {Pose::Skinned, Pose::Deformed, Pose::Original}) {
            QFile glb(dir.filePath(QString::number(int(pose)) + ".glb"));
            QVERIFY(glb.open(QIODevice::WriteOnly));
            const auto fixture = Model3DTest::modelFixture(pose);
            QVERIFY(!fixture.isEmpty());
            QCOMPARE(glb.write(fixture), fixture.size());
            glb.close();
            QVERIFY(document.importModel3D(QUrl::fromLocalFile(glb.fileName())));
            QTRY_VERIFY_WITH_TIMEOUT(!document.operationBusy(), 5000);
            QCOMPARE(document.activeLayer(), ++layer);
        }
        QCOMPARE(document.layerImage(1), document.layerImage(2));
        QVERIFY(document.layerImage(1) != document.layerImage(3));
    }

    void editable3DModelsShareLightingAndSurviveProjectReload()
    {
        QTemporaryDir dir;
        const auto    path = dir.filePath("Objet.glb");
        QFile         glb(path);
        QVERIFY(glb.open(QIODevice::WriteOnly));
        const auto fixture = Model3DTest::modelFixture();
        QVERIFY(!fixture.isEmpty());
        QCOMPARE(glb.write(fixture), fixture.size());
        glb.close();

        Document d;
        QVERIFY(d.newDocument(240, 160, true, 16));
        const auto url = QUrl::fromLocalFile(path);
        QVERIFY(d.importModel3D(url));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 2);
        QCOMPARE(d.layers().first().toMap()["kind"].toString(), QString("model3d"));
        const auto first = d.layerImage(1);
        QCOMPARE(first.format(), QImage::Format_RGBA64_Premultiplied);
        const auto sample = first.pixelColor(108, 88);
        QCOMPARE(sample.alpha(), 255);
        QVERIFY(sample.red() > sample.green() && sample.green() > sample.blue());
        QVERIFY(sample.rgba64().red() % 257 || sample.rgba64().green() % 257 ||
                sample.rgba64().blue() % 257);
        QCOMPARE(first.pixelColor(0, 0).alpha(), 0);

        QVERIFY(d.importModel3D(url));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.layers().size(), 3);
        const auto info = d.layers().first().toMap()["parameters"].toMap();
        auto       pose = info["pose3d"].toMap(), light = info["light3d"].toMap();
        pose["x"]   = 177.;
        pose["yaw"] = 42.;
        QVERIFY(d.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        const auto second = d.layerImage(2);
        QVERIFY(second != first);
        QCOMPARE(d.layerImage(1), first);
        d.addLayerMask(true);
        QVERIFY(!d.maskImage(2).isNull());
        d.setEditingMask(false);
        light["color"]         = "#4080ff";
        light["ambient"]       = 12.;
        const auto history     = d.historyIndex();
        const auto beforeLight = d.displayComposite();
        d.previewModel3D(2, pose, light);
        QTRY_VERIFY_WITH_TIMEOUT(d.displayComposite() != beforeLight, 5000);
        QCOMPARE(d.historyIndex(), history);
        QCOMPARE(d.layerImage(1), first);
        QCOMPARE(d.layerImage(2), second);
        d.cancelModel3DPreview();
        QTRY_COMPARE_WITH_TIMEOUT(d.displayComposite(), beforeLight, 5000);
        QVERIFY(d.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!d.operationBusy(), 5000);
        QCOMPARE(d.historyIndex(), history + 1);
        QVERIFY(d.layerImage(1) != first);
        QVERIFY(d.layerImage(2) != second);
        QVERIFY(!d.maskImage(2).isNull());
        d.undo();
        QCOMPARE(d.layerImage(1), first);
        QCOMPARE(d.layerImage(2), second);
        d.redo();

        const auto project = QUrl::fromLocalFile(dir.filePath("Scene.pcr"));
        QVERIFY(d.saveProject(project));
        QVERIFY(glb.remove());
        Document reopened;
        QVERIFY(reopened.openFile(project));
        QCOMPARE(reopened.layers().size(), 3);
        QCOMPARE(reopened.layerImage(2), d.layerImage(2));
        QCOMPARE(reopened.layerImage(1), d.layerImage(1));
        pose["yaw"] = -42.;
        QVERIFY(reopened.editModel3D(2, pose, light));
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QVERIFY(reopened.layerImage(2) != d.layerImage(2));
        QCOMPARE(reopened.layerImage(1), d.layerImage(1));

        // A model that has left the canvas must still be movable from its mesh.
        reopened.setEditingMask(false);
        reopened.setTool("move");
        const auto visibleModel = reopened.layerImage(2);
        const int  beforeMove   = reopened.historyIndex();
        reopened.beginStroke(120, 80);
        reopened.continueStroke(520, 80);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.historyIndex(), beforeMove + 1);
        QVERIFY(reopened.layerImage(2) != visibleModel);
        reopened.beginStroke(120, 80);
        reopened.continueStroke(-280, 80);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.layerImage(2), visibleModel);
        reopened.undo();
        QVERIFY(reopened.layerImage(2) != visibleModel);
        reopened.redo();
        QCOMPARE(reopened.layerImage(2), visibleModel);
        const int beforeCopy = reopened.historyIndex();
        reopened.beginStroke(120, 80, true);
        reopened.continueStroke(135, 85);
        reopened.cancelStroke();
        QCOMPARE(reopened.historyIndex(), beforeCopy);
        QCOMPARE(reopened.layers().size(), 3);
        reopened.beginStroke(120, 80, true);
        reopened.continueStroke(135, 85);
        reopened.endStroke();
        QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 5000);
        QCOMPARE(reopened.historyIndex(), beforeCopy + 1);
        QCOMPARE(reopened.activeLayer(), 3);
        QCOMPARE(reopened.layers().size(), 4);
        QCOMPARE(reopened.layerImage(2), visibleModel);
        QVERIFY(reopened.layerImage(3) != visibleModel);
    }

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
