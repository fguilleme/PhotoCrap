#pragma once
#include "Document.h"
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

inline bool runDocumentDropSmoke(QQuickWindow *window, Document &document)
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
    QSignalSpy opened(&document, &Document::fileOpened);
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
    const auto click = [&](QQuickItem *button) {
        QTest::qWait(40);
        QTest::mouseClick(
            button->window(), Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
    };
    document.newDocument(64, 48, true);
    QTest::qWait(60);
    if (!drop(area, {QUrl("https://example.invalid/image.png")}, false) ||
        !drop(area, {QUrl::fromLocalFile(dir.path())}, false) || !drop(area, {png, jpeg}, false) ||
        !drop(area, {png}, true))
        return false;
    if (!require(QTest::qWaitFor([&] { return opened.size() == 1 && !document.operationBusy(); },
                                 5000) &&
                     document.width() == 37 && document.height() == 23 &&
                     document.layers().size() == 1,
                 "canvas drop opens PNG as a document rather than importing a layer"))
        return false;
    if (!require(document.layerImage(0).pixelColor(10, 10) ==
                     QImage(png.toLocalFile()).convertToFormat(image.format()).pixelColor(10, 10),
                 "opened PNG preserves its decoded pixels"))
        return false;

    document.newDocument(64, 48, true);
    if (!require(QMetaObject::invokeMethod(window, "openNativeDocument",
                                           Q_ARG(QVariant, QVariant(jpeg))) &&
                     QTest::qWaitFor(
                         [&] { return opened.size() == 2 && !document.operationBusy(); }, 5000) &&
                     document.width() == 49 && document.height() == 31,
                 "the native Open With route decodes JPEG"))
        return false;

    document.setBackgroundEditThreshold(-1);
    document.setColor(Qt::red);
    document.fillWithColor(true);
    const auto before  = document.composite();
    const int  history = document.historyIndex();
    if (!drop(area, {native}, true) ||
        !require(guard->property("visible").toBool() && !document.operationBusy() &&
                     document.composite() == before,
                 "unsaved changes are protected before opening"))
        return false;
    click(cancel);
    if (!require(QTest::qWaitFor([&] { return !guard->property("visible").toBool(); }, 2000) &&
                     document.composite() == before && document.historyIndex() == history,
                 "cancel preserves pixels and history"))
        return false;
    if (!drop(area, {native}, true))
        return false;
    click(discard);
    if (!require(QTest::qWaitFor([&] { return opened.size() == 3 && !document.operationBusy(); },
                                 5000) &&
                     document.width() == 43 && document.height() == 29 &&
                     document.channelBits() == 16,
                 "discard and open loads native 16-bit PNG"))
        return false;

    document.newDocument(64, 48, true);
    QMetaObject::invokeMethod(welcome, "open");
    QTest::qWait(60);
    if (!drop(welcomeArea, {jpeg}, true) ||
        !require(QTest::qWaitFor([&] { return opened.size() == 4 && !document.operationBusy(); },
                                 5000) &&
                     !welcome->property("visible").toBool() && document.width() == 49,
                 "dropping onto Welcome closes it and opens the image"))
        return false;

    document.newDocument(64, 48, true);
    QSignalSpy rawOpened(&document, &Document::rawFileOpened);
    if (!drop(area, {raw}, true) ||
        !require(QTest::qWaitFor([&] { return rawOpened.size() == 1 && !document.rawLoading(); },
                                 10000) &&
                     document.channelBits() == 16 && document.width() == 128 &&
                     document.height() == 96,
                 "canvas drop develops RAW in the background"))
        return false;
    return true;
}
