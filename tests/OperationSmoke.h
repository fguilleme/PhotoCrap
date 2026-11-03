#pragma once
#include "Document.h"
#ifdef Q_OS_MACOS
#include "MacToolCursorTest.h"
#endif
#include <QFile>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest>
#ifdef PHOTOCRAP_RHI_STACK
#include <qpa/qwindowsysteminterface.h>
#endif

inline bool runOperationSmoke(QQuickWindow *window, Document &document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Operation check failed: %s", message);
        return ok;
    };
    auto *welcome = window->findChild<QObject *>("welcomeDialog");
    if (welcome)
        QMetaObject::invokeMethod(welcome, "close");
    QTest::qWait(100);
    window->requestActivate();
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
#ifdef Q_OS_MACOS
        activateMacTestWindow(window);
#endif
        window->raise();
        window->requestActivate();
#ifdef PHOTOCRAP_RHI_STACK
        if (!QTest::qWaitForWindowActive(window, 1000)) {
            // CLI launches may be denied foreground activation by the desktop.
            // QTest sends Qt key events, so establish their Qt focus context
            // without injecting system-wide keys or requiring user interaction.
            qInfo("Desktop kept foreground focus; testing Qt key routing on the native window.");
            QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationActive);
            QWindowSystemInterface::handleFocusWindowChanged(window);
        }
#endif
        if (!require(QTest::qWaitForWindowActive(window), "test window is active"))
            return false;
    }
    auto *pool = QThreadPool::globalInstance();
    pool->waitForDone();
    const int maximum = pool->maxThreadCount();
    pool->setMaxThreadCount(1);
    QSemaphore entered, release;
    pool->start([&] {
        entered.release();
        release.acquire();
    });
    entered.acquire();
    const auto cleanup = qScopeGuard([&] {
        release.release();
        pool->waitForDone();
        pool->setMaxThreadCount(maximum);
    });
    document.newDocument(80, 60, false);
    document.setBackgroundEditThreshold(0);
    const auto before = document.composite();
    document.resizeImage(40, 30);
    QTest::qWait(100);
    const auto find = [](auto &&self, QQuickItem *item, const QString &name) -> QQuickItem * {
        if (item->objectName() == name)
            return item;
        for (auto *child : item->childItems())
            if (auto *found = self(self, child, name))
                return found;
        return nullptr;
    };
    auto *progress = find(find, window->contentItem(), "operationProgress");
    auto *cancel   = find(find, window->contentItem(), "operationCancel");
    if (!require(progress && progress->isVisible() && cancel, "progress and cancel are visible"))
        return false;
    QTest::mouseClick(window, Qt::LeftButton, {},
                      cancel->mapToScene({cancel->width() / 2, cancel->height() / 2}).toPoint());
    if (!require(!document.operationBusy() && document.composite() == before,
                 "cancel keeps original pixels"))
        return false;
    document.resizeImage(40, 30);
    QTest::qWait(50);
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    if (!require(!document.operationBusy(), "Escape cancels background operation"))
        return false;
    QTemporaryDir directory;
    const auto    target       = QUrl::fromLocalFile(directory.filePath("export.png"));
    auto         *exportDialog = window->findChild<QObject *>("exportDialog");
    const auto    history      = document.history();
    if (!require(directory.isValid() && exportDialog &&
                     exportDialog->setProperty("selectedFile", target) &&
                     QMetaObject::invokeMethod(exportDialog, "accepted"),
                 "the export file picker dispatches its selected path"))
        return false;
    QTest::qWait(100);
    if (!require(document.operationBusy() && progress->isVisible() &&
                     !QFile::exists(target.toLocalFile()) && document.history() == history,
                 "file picker export queues without writing or changing history"))
        return false;
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    if (!require(!document.operationBusy() && !QFile::exists(target.toLocalFile()) &&
                     document.history() == history,
                 "Escape cancels the queued file export"))
        return false;
    auto      *saveDialog = window->findChild<QObject *>("saveDialog");
    const auto project    = QUrl::fromLocalFile(directory.filePath("project.pcr"));
    window->setProperty("resumeAfterSave", true);
    window->setProperty("pendingAction", QStringLiteral("close"));
    if (!require(saveDialog && saveDialog->setProperty("selectedFile", project) &&
                     QMetaObject::invokeMethod(saveDialog, "accepted"),
                 "the project file picker dispatches its selected path"))
        return false;
    QTest::qWait(50);
    if (!require(document.operationBusy() && window->property("saveInFlight").toBool() &&
                     window->property("resumeAfterSave").toBool() &&
                     !QFile::exists(project.toLocalFile()),
                 "Save and Close waits for publication instead of closing early"))
        return false;
    if (!require(!window->close() && window->isVisible() && document.operationBusy(),
                 "closing during a save waits for that save"))
        return false;
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    if (!require(!document.operationBusy() && !window->property("saveInFlight").toBool() &&
                     !window->property("resumeAfterSave").toBool() && window->isVisible() &&
                     !QFile::exists(project.toLocalFile()),
                 "cancelling Save and Close keeps the window and forgets the pending close"))
        return false;
    Document source;
    source.newDocument(40, 30, false, 16);
    if (!require(source.saveProject(project), "create project for the open picker"))
        return false;
    auto *openDialog = window->findChild<QObject *>("openDialog");
    window->setProperty("importing", false);
    if (!require(openDialog && openDialog->setProperty("selectedFile", project) &&
                     QMetaObject::invokeMethod(openDialog, "accepted"),
                 "the open picker dispatches a project asynchronously"))
        return false;
    QTest::qWait(50);
    if (!require(document.operationBusy() && progress->isVisible() &&
                     document.composite() == before && document.history() == history,
                 "project opening leaves the current document visible"))
        return false;
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    if (!require(!document.operationBusy() && document.composite() == before &&
                     document.history() == history,
                 "Escape cancels the queued project open"))
        return false;
    const auto image = QUrl::fromLocalFile(directory.filePath("image.png"));
    if (!require(source.exportImage(image) && openDialog->setProperty("selectedFile", image) &&
                     QMetaObject::invokeMethod(openDialog, "accepted"),
                 "the open picker dispatches an image asynchronously"))
        return false;
    QTest::qWait(50);
    if (!require(document.operationBusy() && progress->isVisible() &&
                     document.composite() == before,
                 "image decoding leaves the old document visible"))
        return false;
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    if (!require(!document.operationBusy() && document.composite() == before &&
                     document.history() == history,
                 "Escape cancels the queued image open"))
        return false;
    window->setProperty("importing", true);
    if (!require(QMetaObject::invokeMethod(openDialog, "accepted"),
                 "the import picker dispatches decoding asynchronously"))
        return false;
    QTest::qWait(50);
    if (!require(document.operationBusy() && progress->isVisible() &&
                     document.composite() == before && document.history() == history,
                 "layer import leaves pixels and history untouched while queued"))
        return false;
    QTest::keyClick(window, Qt::Key_Escape);
    QTest::qWait(50);
    window->setProperty("importing", false);
    if (!require(!document.operationBusy() && document.composite() == before &&
                     document.history() == history,
                 "Escape cancels the queued layer import"))
        return false;
    return true;
}
