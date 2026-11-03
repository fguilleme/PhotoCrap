#pragma once
#include "Document.h"
#ifdef Q_OS_MACOS
#include "MacToolCursorTest.h"
#endif
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThreadPool>
#include <QtTest>

inline bool runOperationSmoke(QQuickWindow *window, Document &document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Operation check failed: %s", message);
        return ok;
    };
    window->requestActivate();
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
#ifdef Q_OS_MACOS
        activateMacTestWindow(window);
#endif
        window->raise();
        window->requestActivate();
        if (!require(QTest::qWaitForWindowActive(window), "test window is active"))
            return false;
    }
    auto *welcome = window->findChild<QObject *>("welcomeDialog");
    if (welcome)
        QMetaObject::invokeMethod(welcome, "close");
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
    return true;
}
