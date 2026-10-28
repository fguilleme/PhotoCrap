#pragma once
#include "Document.h"
#include <QDir>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

inline bool runFileDialogSmoke(QQuickWindow *_window, Document &_document)
{
    const auto require = [](bool _condition, const char *_message) {
        if (!_condition)
            qWarning("File dialog check failed: %s", _message);
        return _condition;
    };
    const auto findVisual = [](auto &&_self, QQuickItem *_item, const char *_name) -> QQuickItem * {
        if (!_item->isVisible())
            return nullptr;
        if (_item->objectName() == QLatin1String(_name) || _item->inherits(_name))
            return _item;
        for (auto *child : _item->childItems())
            if (auto *found = _self(_self, child, _name))
                return found;
        return nullptr;
    };
    const auto findAcrossWindows = [&](const char *_name) -> QQuickItem * {
        for (auto *window : QGuiApplication::allWindows())
            if (auto quick = qobject_cast<QQuickWindow *>(window); quick && quick->isVisible())
                if (auto *found = findVisual(findVisual, quick->contentItem(), _name))
                    return found;
        return nullptr;
    };
    QTemporaryDir directory;
    QDir(directory.path()).mkdir("pictures");
    QImage image(43, 29, QImage::Format_RGB32);
    image.fill(Qt::red);
    const auto file = QUrl::fromLocalFile(directory.filePath("image.png"));
    if (!require(image.save(file.toLocalFile()), "create file fixture"))
        return false;
    const auto checkDialog = [&](QObject *_dialog) -> bool {
        if (!require(_dialog && QMetaObject::invokeMethod(_dialog, "open"), "open the file picker"))
            return false;
        QQuickItem *list = nullptr, *breadcrumb = nullptr;
        if (!require(QTest::qWaitFor(
                         [&] {
                             list = findAcrossWindows("fileDialogListView");
                             if (!list)
                                 list = findAcrossWindows("folderDialogListView");
                             breadcrumb = findAcrossWindows("QQuickFolderBreadcrumbBar");
                             return list && breadcrumb;
                         },
                         5000),
                     "Qt Quick fallback content appears"))
            return false;
        QTest::qWait(60);
        const auto frame = breadcrumb->window()->grabWindow();
        const auto bounds =
            breadcrumb->mapRectToScene(QRectF(0, 0, breadcrumb->width(), breadcrumb->height()));
        const qreal scale = qreal(frame.width()) / breadcrumb->window()->width();
        const auto  bar =
            frame.copy(QRect(qRound(bounds.x() * scale), qRound(bounds.y() * scale),
                             qRound(bounds.width() * scale), qRound(bounds.height() * scale)));
        int bright = 0;
        for (int y = 0; y < bar.height(); ++y)
            for (int x = 0; x < bar.width(); ++x) {
                const auto color = bar.pixelColor(x, y);
                bright += color.red() > 190 && color.green() > 190 && color.blue() > 190;
            }
        frame.save("/tmp/photocrap-file-dialog.png");
        if (!require(!bar.isNull() && bright > 0 && bright < bar.width() * bar.height() / 3,
                     "breadcrumb text and icons remain visible against a dark background"))
            return false;
        // Newer Qt versions add a places sidebar. Its text must also inherit
        // the foreground color, rather than defaulting to black on dark gray.
        if (auto sidebar = findAcrossWindows("QQuickSideBar")) {
            const auto rect =
                sidebar->mapRectToScene(QRectF(0, 0, sidebar->width(), sidebar->height()));
            const auto places =
                frame.copy(QRect(qRound(rect.x() * scale), qRound(rect.y() * scale),
                                 qRound(rect.width() * scale), qRound(rect.height() * scale)));
            int lightText = 0;
            for (int y = 0; y < places.height(); ++y)
                for (int x = 0; x < places.width(); ++x) {
                    const auto color = places.pixelColor(x, y);
                    lightText += color.red() > 190 && color.green() > 190 && color.blue() > 190;
                }
            if (!require(lightText > 0, "places sidebar labels remain readable"))
                return false;
        }
        auto row = findAcrossWindows("fileDialogDelegate0");
        if (row) {
            auto rowBackground = row->property("background").value<QQuickItem *>();
            if (!require(rowBackground &&
                             rowBackground->property("color").value<QColor>().lightnessF() < .6,
                         "highlighted rows remain readable"))
                return false;
        }
        if (auto window = breadcrumb->window())
            window->grabWindow().save("/tmp/photocrap-file-dialog.png");
        QTest::keyClick(breadcrumb->window(), Qt::Key_Escape);
        return require(
            QTest::qWaitFor([&] { return !_dialog->property("visible").toBool(); }, 3000),
            "Escape closes the fallback dialog");
    };
    for (const char *name : {"openDialog", "saveDialog", "exportDialog", "sliceFolderDialog"}) {
        auto dialog = _window->findChild<QObject *>(name);
        if (!require(dialog, "file dialog exists"))
            return false;
        dialog->setProperty("currentFolder", QUrl::fromLocalFile(directory.path()));
        if (!checkDialog(dialog))
            return false;
    }
    const auto pixels = _document.composite();
    auto       dialog = _window->findChild<QObject *>("openDialog");
    dialog->setProperty("selectedFile", file);
    if (!require(QMetaObject::invokeMethod(dialog, "open") &&
                     QTest::qWaitFor(
                         [&] { return findAcrossWindows("fileDialogListView") != nullptr; }, 5000),
                 "reopen the file picker"))
        return false;
    if (!require(QMetaObject::invokeMethod(dialog, "accept"), "accept selected image"))
        return false;
    if (!require(QTest::qWaitFor(
                     [&] { return _document.width() == 43 && _document.height() == 29; }, 3000),
                 "Open still loads the selected image"))
        return false;
    return require(_document.composite() != pixels, "selected file replaces document pixels");
}
