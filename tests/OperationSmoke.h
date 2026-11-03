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
    return true;
}
