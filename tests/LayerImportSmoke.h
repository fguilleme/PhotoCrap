#pragma once
#include "Document.h"
#include "Model3DTestImage.h"
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>

inline bool runLayerImportSmoke(QQuickWindow *window, Document &document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Layer import UI check failed: %s", message);
        return ok;
    };
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    using DragPoint = QPointF;
#else
    using DragPoint = QPoint;
#endif
    document.setBackgroundEditThreshold(-1);
    document.newDocument(32, 24, true, 16);
    document.addLayer();
    auto *menu   = window->findChild<QObject *>("importLayerMenuItem");
    auto *dialog = window->findChild<QObject *>("importLayerDialog");
    auto *area   = window->findChild<QQuickItem *>("layerImageDropArea");
    auto *list   = window->findChild<QQuickItem *>("layerListView");
    auto *line   = window->findChild<QQuickItem *>("layerImageDropIndicator");
    if (!require(menu && dialog && area && list && line, "menu, picker and drop target exist"))
        return false;
    QTemporaryDir dir;
    QList<QUrl>   files;
    for (const auto name : {"First", "Second"}) {
        const auto file = QUrl::fromLocalFile(dir.filePath(QString(name) + ".png"));
        QImage     image(12, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        if (!require(image.save(file.toLocalFile()), "create image fixture"))
            return false;
        files.append(file);
    }
    QTest::qWait(80);
    if (!require(area->isVisible() && area->isEnabled(), "stack accepts file drops"))
        return false;
    const auto sendDrop = [&](const QList<QUrl> &urls, int row, bool above, int expectedInsertion) {
        QQuickItem *item = nullptr;
        if (!QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, item),
                                       Q_ARG(int, row)) ||
            !item)
            return require(false, "target layer row is instantiated");
        const DragPoint position =
            item->mapToScene(QPointF(item->width() / 2, above ? 4 : item->height() - 4)).toPoint();
        QMimeData mime;
        mime.setUrls(urls);
        QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QDragMoveEvent move(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &move);
        if (!require(enter.isAccepted() && move.isAccepted() && line->isVisible() &&
                         area->property("insertion").toInt() == expectedInsertion,
                     "external file drag shows the correct insertion line"))
            return false;
        QDropEvent drop(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &drop);
        return require(drop.isAccepted() && !line->isVisible(),
                       "drop imports and clears indicator");
    };
    // The first and last positions must work with the reversed display order.
    if (!sendDrop(files, 0, true, 2) ||
        !require(QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.layers().size() == 4 && document.activeLayer() == 3,
                 "multi-file drop inserts at the top in one transaction"))
        return false;
    document.undo();
    QTest::qWait(50);
    if (!sendDrop({files[1]}, 1, false, 0) ||
        !require(QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.activeLayer() == 0,
                 "drop below the last row inserts at the bottom"))
        return false;
    document.undo();
    QTest::qWait(50);
    // Internal mask drags use a different key; this overlay must not accept them.
    QMimeData mask;
    mask.setData("photocrap-layer-mask", "mask");
    const DragPoint pos = area->mapToScene(QPointF(50, 20)).toPoint();
    QDragEnterEvent maskEnter(pos, Qt::CopyAction, &mask, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &maskEnter);
    if (!require(!area->property("validDrop").toBool(), "image drop ignores mask drags"))
        return false;
    QDragLeaveEvent leave;
    QCoreApplication::sendEvent(window, &leave);
    dialog->setProperty("currentFolder", QUrl::fromLocalFile(dir.path()));
    dialog->setProperty("selectedFile", files[0]);
    if (!require(QMetaObject::invokeMethod(menu, "triggered") &&
                     QTest::qWaitFor([&] { return dialog->property("visible").toBool(); }, 2000),
                 "the add-layer menu opens the image picker"))
        return false;
    const auto findRow = [](auto &&self, QQuickItem *item) -> QQuickItem * {
        if (!item->isVisible())
            return nullptr;
        if (item->objectName() == "fileDialogDelegate0")
            return item;
        for (auto *child : item->childItems())
            if (auto *found = self(self, child))
                return found;
        return nullptr;
    };
    QQuickItem *fileRow = nullptr;
    if (!require(QTest::qWaitFor(
                     [&] {
                         for (auto *surface : QGuiApplication::allWindows())
                             if (auto *quick = qobject_cast<QQuickWindow *>(surface);
                                 quick && quick->isVisible())
                                 if ((fileRow = findRow(findRow, quick->contentItem())))
                                     return true;
                         return false;
                     },
                     3000),
                 "file picker has loaded its image rows"))
        return false;
    QTest::mouseClick(
        fileRow->window(), Qt::LeftButton, Qt::NoModifier,
        fileRow->mapToScene(QPointF(fileRow->width() / 2, fileRow->height() / 2)).toPoint());
    const int history = document.historyIndex();
    if (!require(QMetaObject::invokeMethod(dialog, "accept") &&
                     QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.layers().size() == 3 && document.historyIndex() == history + 1,
                 "picker imports the selected image with one undo step"))
        return false;
    auto *modelMenu        = window->findChild<QObject *>("importModel3DMenuItem");
    auto *modelPicker      = window->findChild<QObject *>("importModel3DDialog");
    auto *modelProperties  = window->findChild<QQuickItem *>("model3dPanel");
    auto *propertiesButton = window->findChild<QObject *>("layerPropertiesButton");
    auto *closeModel       = window->findChild<QObject *>("model3dCloseButton");
    auto *yaw              = window->findChild<QQuickItem *>("model3dYaw");
    auto *lightTab         = window->findChild<QQuickItem *>("model3dLightingTab");
    auto *intensity        = window->findChild<QQuickItem *>("model3dIntensity");
    auto *canvasViewport   = window->findChild<QQuickItem *>("viewport");
    auto *drawingArea      = window->findChild<QQuickItem *>("drawingArea");
    if (!require(modelMenu && modelPicker && modelProperties && propertiesButton && closeModel &&
                     yaw && lightTab && intensity && canvasViewport && drawingArea,
                 "model picker and properties exist"))
        return false;
    const auto modelUrl = QUrl::fromLocalFile(dir.filePath("Model.glb"));
    QFile      modelFile(modelUrl.toLocalFile());
    const auto model = Model3DTest::modelFixture();
    if (!require(!model.isEmpty() && modelFile.open(QIODevice::WriteOnly) &&
                     modelFile.write(model) == model.size(),
                 "create GLB fixture"))
        return false;
    modelFile.close();
    if (!require(
            QMetaObject::invokeMethod(modelMenu, "triggered") &&
                QTest::qWaitFor([&] { return modelPicker->property("visible").toBool(); }, 2000),
            "the add-layer menu opens the GLB picker"))
        return false;
    QMetaObject::invokeMethod(modelPicker, "reject");
    if (!require(document.importModel3D(modelUrl) &&
                     QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.layers().first().toMap()["kind"].toString() == "model3d",
                 "a GLB appears as an editable 3D layer"))
        return false;
    if (!require(propertiesButton->property("enabled").toBool() &&
                     QMetaObject::invokeMethod(propertiesButton, "clicked") &&
                     QTest::qWaitFor([&] { return modelProperties->isVisible(); }, 2000),
                 "layer properties open the 3D controls"))
        return false;
    const auto sceneRect = [](QQuickItem *item) {
        return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
    };
    if (!require(QTest::qWaitFor(
                     [&] {
                         return modelProperties->width() > 0 && modelProperties->height() > 0 &&
                                !sceneRect(modelProperties).intersects(sceneRect(canvasViewport)) &&
                                !window->property("modalOpen").toBool();
                     },
                     2000),
                 "3D controls are beside the canvas and leave it interactive"))
        return false;
    const auto yawValue = [&] {
        return document.layers()
            .first()
            .toMap()["parameters"]
            .toMap()["pose3d"]
            .toMap()["yaw"]
            .toInt();
    };
    const auto sliderPoint = [&](QQuickItem *slider, double fraction) {
        return slider->mapToScene({slider->width() * fraction, slider->height() / 2.}).toPoint();
    };
    const auto original   = document.displayComposite();
    const auto beforePose = document.historyIndex();
    // Let the file picker's closing transition release its pointer grab.
    QTest::qWait(180);
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, sliderPoint(yaw, .5));
    QTest::mouseMove(window, sliderPoint(yaw, .74));
    if (!require(QTest::qWaitFor([&] { return document.displayComposite() != original; }, 5000) &&
                     yawValue() == 0 && document.historyIndex() == beforePose,
                 "dragging a rotation slider previews before committing"))
        return false;
    const auto firstPreview = document.displayComposite();
    QTest::mouseMove(window, sliderPoint(yaw, .36));
    if (!require(
            QTest::qWaitFor([&] { return document.displayComposite() != firstPreview; }, 5000) &&
                document.historyIndex() == beforePose,
            "the preview follows subsequent movement during the same drag"))
        return false;
    const int rotation = yaw->property("value").toInt();
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, sliderPoint(yaw, .36));
    if (!require(QTest::qWaitFor(
                     [&] { return !document.operationBusy() && yawValue() == rotation; }, 5000) &&
                     rotation != 0 && document.historyIndex() == beforePose + 1 &&
                     modelProperties->isVisible(),
                 "releasing commits the pose once and keeps the controls open"))
        return false;
    const int modelIndex = document.activeLayer();
    document.setActiveLayer(modelIndex - 1);
    if (!require(!modelProperties->isVisible(), "3D controls follow the active layer"))
        return false;
    document.setActiveLayer(modelIndex);
    if (!require(modelProperties->isVisible() && yaw->property("value").toInt() == rotation,
                 "returning to the model shows the applied pose"))
        return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      sceneRect(lightTab).center().toPoint());
    if (!require(intensity->isVisible(), "the lighting tab shows the common controls"))
        return false;
    const auto lit         = document.displayComposite();
    const auto beforeLight = document.historyIndex();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, sliderPoint(intensity, .4));
    QTest::mouseMove(window, sliderPoint(intensity, .82));
    if (!require(QTest::qWaitFor([&] { return document.displayComposite() != lit; }, 5000) &&
                     document.historyIndex() == beforeLight,
                 "lighting responds while dragging"))
        return false;
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, sliderPoint(intensity, .82));
    if (!require(QTest::qWaitFor(
                     [&] {
                         return !document.operationBusy() &&
                                document.historyIndex() == beforeLight + 1;
                     },
                     5000) &&
                     modelProperties->isVisible(),
                 "releasing commits lighting once"))
        return false;
    if (!require(QMetaObject::invokeMethod(closeModel, "clicked") &&
                     !modelProperties->isVisible() && yawValue() == rotation,
                 "closing the sidebar preserves applied changes"))
        return false;
    const auto pose = [&] {
        return document.layers().first().toMap()["parameters"].toMap()["pose3d"].toMap();
    };
    QTest::keyClick(window, Qt::Key_V);
    if (!require(document.tool() == "move", "V selects the move tool"))
        return false;
    const auto start         = sceneRect(drawingArea).center().toPoint();
    const auto finish        = start + QPoint(10, 3);
    const auto initialPose   = pose();
    const auto beforeRotate  = document.displayComposite();
    const int  rotateHistory = document.historyIndex();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(window, finish);
    if (!require(
            QTest::qWaitFor([&] { return document.displayComposite() != beforeRotate; }, 5000) &&
                pose() == initialPose && document.historyIndex() == rotateHistory,
            "left drag previews rotation before committing"))
        return false;
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, finish);
    if (!require(QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.historyIndex() == rotateHistory + 1 &&
                     pose()["x"] == initialPose["x"] && pose()["yaw"] != initialPose["yaw"] &&
                     pose()["pitch"] != initialPose["pitch"],
                 "left drag rotates the model in one undo step"))
        return false;

    const auto rotatedPose = pose();
    const auto zoom        = window->property("zoom").toReal();
    QTest::mousePress(window, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(window, finish);
    QTest::mouseRelease(window, Qt::RightButton, Qt::NoModifier, finish);
    if (!require(QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000) &&
                     document.historyIndex() == rotateHistory + 2 &&
                     qAbs(pose()["x"].toReal() - rotatedPose["x"].toReal() - 10. / zoom) < .01 &&
                     qAbs(pose()["y"].toReal() - rotatedPose["y"].toReal() - 3. / zoom) < .01 &&
                     pose()["yaw"] == rotatedPose["yaw"],
                 "right drag pans the model in document coordinates"))
        return false;

    for (int step = 0; step < 2; ++step) {
        QWheelEvent wheel(start, window->mapToGlobal(start), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(window, &wheel);
    }
    if (!require(QTest::qWaitFor(
                     [&] {
                         return document.historyIndex() == rotateHistory + 3 &&
                                !document.operationBusy();
                     },
                     5000) &&
                     qAbs(pose()["scale"].toReal() - rotatedPose["scale"].toReal() * 1.15 * 1.15) <
                         .01 &&
                     window->property("zoom").toReal() == zoom,
                 "wheel scales the layer in one undo step and preserves canvas zoom"))
        return false;

    window->setProperty("zoom", 1.75);
    const int beforeGenerated = document.layerCount();
    if (!require(document.importModel3DData(model, "Generated model", QRectF(6, 4, 22, 18)) &&
                     QTest::qWaitFor([&] { return !document.operationBusy(); }, 5000),
                 "a fitted model imports in the open document"))
        return false;
    QTest::qWait(60);
    return require(document.layerCount() == beforeGenerated + 1 &&
                       window->property("zoom").toReal() == 1.75,
                   "adding a fitted model preserves the current canvas zoom");
}
