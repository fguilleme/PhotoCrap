#pragma once
#include "Document.h"
#include "DocumentManager.h"
#include "RawTestImage.h"
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFile>
#include <QMimeData>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

inline bool runDocumentDropSmoke(QQuickWindow *window, DocumentManager &manager)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Document drop check failed: %s", message);
        return ok;
    };
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    using DragPoint = QPointF;
#else
    using DragPoint = QPoint;
#endif
    auto *area        = window->findChild<QQuickItem *>("documentImageDropArea");
    auto *welcomeArea = window->findChild<QQuickItem *>("welcomeImageDropArea");
    auto *welcome     = window->findChild<QObject *>("welcomeDialog");
    auto *guard       = window->findChild<QObject *>("unsavedDialog");
    auto *cancel      = window->findChild<QQuickItem *>("unsavedCancelButton");
    auto *discard     = window->findChild<QQuickItem *>("unsavedDiscardButton");
    if (!require(area && welcomeArea && welcome && guard && cancel && discard,
                 "drop targets and save guard exist"))
        return false;
    QTemporaryDir dir;
    const auto    png    = QUrl::fromLocalFile(dir.filePath("Image.png"));
    const auto    jpeg   = QUrl::fromLocalFile(dir.filePath("Photo.jpg"));
    const auto    native = QUrl::fromLocalFile(dir.filePath("Native.png"));
    const auto    raw    = QUrl::fromLocalFile(dir.filePath("Camera.dng"));
    QImage        image(37, 23, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(23, 135, 77, 160));
    QImage photo(49, 31, QImage::Format_RGB32);
    photo.fill(QColor(45, 76, 121));
    QImage precision(43, 29, QImage::Format_RGBA64_Premultiplied);
    precision.fill(QColor::fromRgba64(12345, 23456, 34567, 45678));
    if (!require(image.save(png.toLocalFile()) && photo.save(jpeg.toLocalFile()) &&
                     precision.save(native.toLocalFile()),
                 "create PNG/JPEG/16-bit fixtures"))
        return false;
    QFile camera(raw.toLocalFile());
    if (!require(camera.open(QIODevice::WriteOnly), "create RAW fixture"))
        return false;
    camera.write(RawTestImage::dng());
    camera.close();
    const auto drop = [&](QQuickItem *target, const QList<QUrl> &files, bool expected) {
        const DragPoint position =
            target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
        QMimeData mime;
        mime.setUrls(files);
        QDragEnterEvent enter(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        if (!require(enter.isAccepted() == expected &&
                         target->property("validDrop").toBool() == expected,
                     "image drag validates files and shows its feedback"))
            return false;
        if (!expected) {
            QDragLeaveEvent leave;
            QCoreApplication::sendEvent(window, &leave);
            return true;
        }
        QDropEvent event(position, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
        return require(event.isAccepted() && !target->property("validDrop").toBool(),
                       "drop is accepted as a copy and clears feedback");
    };
    auto *original = manager.activeDocument();
    original->newDocument(64, 48, true);
    original->setBackgroundEditThreshold(-1);
    original->setColor(Qt::red);
    original->fillWithColor(true);
    const auto before  = original->composite();
    const int  history = original->historyIndex();
    QTest::qWait(60);
    if (!drop(area, {QUrl("https://example.invalid/image.png")}, false) ||
        !drop(area, {QUrl::fromLocalFile(dir.path())}, false) || !drop(area, {png}, true))
        return false;
    auto *opened = manager.activeDocument();
    if (!require(opened != original &&
                     QTest::qWaitFor([&] { return !opened->operationBusy(); }, 5000) &&
                     opened->width() == 37 && opened->height() == 23 && manager.count() == 2 &&
                     !guard->property("visible").toBool() && original->composite() == before &&
                     original->historyIndex() == history && original->modified(),
                 "canvas drop opens a separate document and preserves unsaved source edits"))
        return false;
    if (!require(opened->layerImage(0).pixelColor(10, 10) ==
                     QImage(png.toLocalFile()).convertToFormat(image.format()).pixelColor(10, 10),
                 "opened PNG preserves its decoded pixels"))
        return false;
    QMetaObject::invokeMethod(window, "openNativeDocument", Q_ARG(QVariant, QVariant(jpeg)));
    auto *photoDoc = manager.activeDocument();
    if (!require(photoDoc != opened &&
                     QTest::qWaitFor([&] { return !photoDoc->operationBusy(); }, 5000) &&
                     photoDoc->width() == 49 && manager.count() == 3,
                 "native Open With opens JPEG on another tab"))
        return false;
    if (!drop(area, {native}, true))
        return false;
    auto *nativeDoc = manager.activeDocument();
    if (!require(QTest::qWaitFor([&] { return !nativeDoc->operationBusy(); }, 5000) &&
                     nativeDoc->width() == 43 && nativeDoc->channelBits() == 16,
                 "dropped 16-bit PNG retains native precision"))
        return false;
    QMetaObject::invokeMethod(welcome, "open");
    QTest::qWait(60);
    if (!drop(welcomeArea, {jpeg}, true) ||
        !require(!welcome->property("visible").toBool() && manager.activeDocument() == photoDoc &&
                     manager.count() == 4,
                 "Welcome drop selects an already open image without duplication"))
        return false;
    const auto extraA = QUrl::fromLocalFile(dir.filePath("ExtraA.png"));
    const auto extraB = QUrl::fromLocalFile(dir.filePath("ExtraB.bmp"));
    if (!require(image.save(extraA.toLocalFile()) && image.save(extraB.toLocalFile()),
                 "create batch fixtures"))
        return false;
    if (!drop(area, {extraA, extraB}, true))
        return false;
    if (!require(
            QTest::qWaitFor(
                [&] { return manager.count() == 6 && !manager.activeDocument()->operationBusy(); },
                5000),
            "a batch drop creates one document per image"))
        return false;
    if (!drop(area, {raw}, true))
        return false;
    auto *rawDoc = manager.activeDocument();
    if (!require(QTest::qWaitFor([&] { return !rawDoc->rawLoading(); }, 10000) &&
                     rawDoc->channelBits() == 16 && rawDoc->width() == 128 && manager.count() == 7,
                 "RAW drop develops into its own 16-bit document"))
        return false;
    return require(original->composite() == before && original->historyIndex() == history,
                   "all open routes preserve original pixels and history");
}
