#pragma once
#include "DocumentManager.h"
#include "ai/AiSettings.h"
#include <QKeySequence>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>
#include <functional>

inline bool runDocumentsSmoke(QQuickWindow *window, DocumentManager &manager, AiSettings &settings)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Documents UI check failed: %s", message);
        return ok;
    };
    auto *first = manager.activeDocument();
    first->newDocument(200, 140, true, 16);
    first->setBackgroundEditThreshold(-1);
    first->setColor(Qt::red);
    first->fillWithColor(true);
    first->selectAll();
    auto *viewport = window->findChild<QObject *>("viewport");
    if (!require(viewport, "viewport exists"))
        return false;
    QTest::qWait(80);
    window->setProperty("zoom", 2.);
    viewport->setProperty("contentX", 80.);
    viewport->setProperty("contentY", 60.);
    if (!require(manager.newDocument(64, 48, true, 8), "create second tab"))
        return false;
    auto *second = manager.activeDocument();
    second->setBackgroundEditThreshold(-1);
    second->setColor(Qt::blue);
    second->fillWithColor(true);
    QTest::qWait(80);
    const std::function<QQuickItem *(QQuickItem *, const QString &)> findVisual =
        [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name)
            return parent;
        for (auto *child : parent->childItems())
            if (auto *found = findVisual(child, name))
                return found;
        return nullptr;
    };
    auto *firstTab  = findVisual(window->contentItem(), "documentTab_" + first->documentId());
    auto *secondTab = findVisual(window->contentItem(), "documentTab_" + second->documentId());
    if (!require(qvariant_cast<Document *>(window->property("editor")) == second && firstTab &&
                     secondTab && firstTab->isVisible() && secondTab->isVisible(),
                 "tabs and editor target the active independent document"))
        return false;
    const auto canvasColor = [&] {
        auto *     area  = window->findChild<QQuickItem *>("drawingArea");
        const auto frame = window->grabWindow();
        if (!area || frame.isNull())
            return QColor{};
        const auto   point = area->mapToScene(QPointF(area->width() / 2, area->height() / 2));
        const qreal  scale = qreal(frame.width()) / window->width();
        const QPoint pixel(qRound(point.x() * scale), qRound(point.y() * scale));
        return frame.rect().contains(pixel) ? frame.pixelColor(pixel) : QColor{};
    };
    if (!require(QTest::qWaitFor(
                     [&] {
                         const auto c = canvasColor();
                         return c.blue() > 200 && c.red() < 50;
                     },
                     2000),
                 "the canvas displays the second document pixels"))
        return false;
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      firstTab->mapToScene(QPointF(15, 16)).toPoint());
    QTest::qWait(100);
    if (!require(manager.activeDocument() == first && first->hasSelection() &&
                     qAbs(window->property("zoom").toDouble() - 2.) < .001 &&
                     qAbs(viewport->property("contentX").toDouble() - 80.) < 1 &&
                     qAbs(viewport->property("contentY").toDouble() - 60.) < 1,
                 "tab switching restores selection, zoom and scroll"))
        return false;
    if (!require(QTest::qWaitFor(
                     [&] {
                         const auto c = canvasColor();
                         return c.red() > 200 && c.blue() < 50;
                     },
                     2000),
                 "switching tabs renders the first document rather than a stale cached image"))
        return false;
    auto *guard   = window->findChild<QObject *>("unsavedDialog");
    auto *cancel  = window->findChild<QObject *>("unsavedCancelButton");
    auto *discard = window->findChild<QObject *>("unsavedDiscardButton");
    if (!require(guard && cancel && discard, "save guard controls exist"))
        return false;
    QMetaObject::invokeMethod(window, "guard", Q_ARG(QVariant, QVariant("close")));
    QTest::qWait(80);
    if (!require(guard->property("visible").toBool() && manager.count() == 2,
                 "quit asks about the first dirty document"))
        return false;
    QMetaObject::invokeMethod(discard, "clicked");
    QTest::qWait(80);
    if (!require(guard->property("visible").toBool() && manager.activeDocument() == second &&
                     manager.count() == 2,
                 "quit checks the second dirty tab without destroying the first"))
        return false;
    QMetaObject::invokeMethod(cancel, "clicked");
    QTest::qWait(80);
    if (!require(!guard->property("visible").toBool() && manager.count() == 2 &&
                     window->isVisible(),
                 "canceling quit leaves every tab open"))
        return false;
    QMetaObject::invokeMethod(window, "closeDocument",
                              Q_ARG(QVariant, QVariant(second->documentId())));
    QTest::qWait(60);
    if (!require(guard->property("visible").toBool(), "closing a dirty tab asks for confirmation"))
        return false;
    QMetaObject::invokeMethod(discard, "clicked");
    QTest::qWait(80);
    if (!require(manager.count() == 1 && manager.activeDocument() == first,
                 "discarding one tab preserves its neighbor"))
        return false;
    const auto profile = settings.saveProvider(settings.newProvider("falEsrgan"));
    auto *     dialog  = window->findChild<QObject *>("aiImageDialog");
    QMetaObject::invokeMethod(dialog, "chooseProvider", Q_ARG(QVariant, QVariant(profile)));
    QMetaObject::invokeMethod(dialog, "start");
    QTest::qWait(100);
    auto *destination = window->findChild<QObject *>("aiResultDestination");
    if (!require(destination && destination->property("currentIndex").toInt() == 1,
                 "upscales default to a native-size new document"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    QTest::qWait(80);
    for (int i = 0; i < 10; ++i)
        if (!require(manager.newDocument(16, 12, true, 8), "create overflow tab"))
            return false;
    QTest::qWait(100);
    auto *     tabs    = window->findChild<QQuickItem *>("documentTabs");
    auto *     lastTab = findVisual(window->contentItem(), "documentTab_" + manager.activeId());
    const bool revealed =
        tabs && lastTab && tabs->property("contentX").toReal() > 0 &&
        lastTab->mapToItem(tabs, QPointF()).x() >= -1 &&
        lastTab->mapToItem(tabs, QPointF(lastTab->width(), 0)).x() <= tabs->width() + 1;
    if (!revealed && tabs && lastTab)
        qWarning() << "Tab strip" << tabs->width() << tabs->property("contentWidth")
                   << tabs->property("contentX") << lastTab->mapToItem(tabs, QPointF())
                   << lastTab->width();
    if (!require(revealed, "the active tab stays visible when the tab strip overflows"))
        return false;
    window->contentItem()->forceActiveFocus();
    const auto next = QKeySequence::keyBindings(QKeySequence::NextChild);
    if (!require(!next.isEmpty(), "platform defines next-document shortcut"))
        return false;
    QTest::keyClick(window, next.first()[0].key(), next.first()[0].keyboardModifiers());
    QTest::qWait(80);
    return require(manager.activeDocument() == first,
                   "the configurable next-document shortcut wraps from last tab to first");
}
