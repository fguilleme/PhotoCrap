#pragma once
#include "Document.h"
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
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
    return true;
}
