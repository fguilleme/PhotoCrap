#pragma once
#include "Document.h"
#include <QCursor>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest>
#include <functional>
#ifdef Q_OS_MACOS
#include "MacToolCursorTest.h"
#endif

inline bool runMixerUiSmoke(QQuickWindow *window, Document &document)
{
    const auto require = [](bool ok, const char *message) {
        if (!ok)
            qWarning("Mixer UI check failed: %s", message);
        return ok;
    };
#ifdef Q_OS_MACOS
    if (QGuiApplication::platformName() == "cocoa")
        activateMacTestWindow(window);
#endif
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
        if (parent->objectName() == name)
            return parent;
        for (auto child : parent->childItems())
            if (auto found = find(child, name))
                return found;
        return nullptr;
    };
    auto item = [&](const char *name) { return find(window->contentItem(), name); };
    window->resize(1480, 940);
    document.newDocument(240, 120, true);
    document.setTool("mixerBrush");
    document.setColor(Qt::red);
    document.setBrushSize(24);
    document.setBrushHardness(1);
    document.setBrushSmoothing(0);
    window->setProperty("zoom", 1.);
    QTest::qWait(100);
    auto options = item("mixerBrushOptions");
    auto area    = item("drawingArea");
    if (!require(options && options->isVisible() && area, "Mixer toolbar exists"))
        return false;
    const auto click = [&](QQuickItem *control) {
        // Reach controls clipped by the horizontal options bar on small screens.
        for (auto parent = control->parentItem(); parent; parent = parent->parentItem()) {
            if (!parent->property("contentX").isValid())
                continue;
            const auto   x   = control->mapToItem(parent, QPointF(control->width() / 2, 0)).x();
            const double old = parent->property("contentX").toDouble();
            if (x > parent->width() - 20)
                parent->setProperty("contentX", old + x - parent->width() + 40);
            else if (x < 20)
                parent->setProperty("contentX", qMax(0., old + x - 40));
            QCoreApplication::processEvents();
            break;
        }
        QTest::mouseClick(
            window, Qt::LeftButton, Qt::NoModifier,
            control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint());
        QCoreApplication::processEvents();
    };
    for (const auto name :
         {"mixerSize", "mixerBrushSettings", "mixerLoadSwatch", "mixerAutoLoad", "mixerAutoClean",
          "mixerPreset", "mixerWet", "mixerLoad", "mixerMix", "mixerFlow", "mixerAirbrush"})
        if (!require(item(name) && item(name)->isVisible(), "Adobe-style toolbar controls exist"))
            return false;
    const auto type = [&](const char *name, int value) {
        auto input = item(name)->property("contentItem").value<QQuickItem *>();
        if (!input)
            return false;
        click(input);
        QTest::keySequence(window, QKeySequence::SelectAll);
        for (const auto c : QString::number(value))
            QTest::keyClick(window, c.toLatin1());
        QTest::keyClick(window, Qt::Key_Return);
        QCoreApplication::processEvents();
        return !input->hasActiveFocus();
    };
    if (!require(type("mixerWet", 80) && type("mixerLoad", 75) && type("mixerMix", 90) &&
                     type("mixerFlow", 20) && qAbs(document.mixerWet() - .8) < .001 &&
                     qAbs(document.mixerLoad() - .75) < .001 &&
                     qAbs(document.mixerMix() - .9) < .001 &&
                     qAbs(document.mixerFlow() - .2) < .001,
                 "typing Wet/Load/Mix/Flow commits and leaves editing"))
        return false;
    auto combo = item("mixerPreset");
    if (!require(combo->property("count").toInt() == 13, "thirteen preset menu entries"))
        return false;
    combo->setProperty("currentIndex", 1);
    QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, 1));
    if (!require(document.mixerPreset() == 1 && document.mixerFlow() == .2 &&
                     !item("mixerMix")->isEnabled(),
                 "Dry disables Mix and leaves Flow unchanged"))
        return false;
    document.setMixerWet(.8);
    if (!require(item("mixerMix")->isEnabled() && item("mixerWet")->property("value").toInt() == 80,
                 "settings refresh numeric fields"))
        return false;
    for (auto pair : {qMakePair("mixerAutoLoad", document.mixerAutoLoad()),
                      qMakePair("mixerAutoClean", document.mixerAutoClean())}) {
        click(item(pair.first));
        const bool state = QString(pair.first) == "mixerAutoLoad" ? document.mixerAutoLoad()
                                                                  : document.mixerAutoClean();
        if (!require(state != pair.second, "visible automatic load/clean buttons toggle"))
            return false;
    }
    const int history = document.historyIndex();
    click(item("mixerLoadMenuButton"));
    auto menu = window->findChild<QObject *>("mixerLoadMenu");
    if (!require(menu && menu->property("visible").toBool(), "reservoir arrow opens its menu"))
        return false;
    QMetaObject::invokeMethod(window->findChild<QObject *>("mixerCleanAction"), "triggered");
    if (!require(!document.mixerLoaded(), "Clean empties reservoir"))
        return false;
    QMetaObject::invokeMethod(window->findChild<QObject *>("mixerLoadAction"), "triggered");
    QMetaObject::invokeMethod(menu, "close");
    QTest::qWait(100);
    if (!require(document.mixerLoaded() && document.historyIndex() == history,
                 "reservoir actions leave canvas history alone"))
        return false;
    QSignalSpy colorRequest(options, SIGNAL(colorRequested()));
    click(item("mixerLoadSwatch"));
    if (!require(colorRequest.count() == 1 && !menu->property("visible").toBool(),
                 "swatch requests a colour instead of opening the load menu"))
        return false;
    auto colorDialog = window->findChild<QObject *>("colorDialog");
    if (colorDialog)
        QMetaObject::invokeMethod(colorDialog, "reject");
    QTest::qWait(400);
    window->requestActivate();
#ifdef Q_OS_MACOS
    if (QGuiApplication::platformName() == "cocoa")
        activateMacTestWindow(window);
#endif
    if (!require(QTest::qWaitForWindowActive(window), "main window active after colour picker"))
        return false;
    if (!require(!window->property("modalOpen").toBool(), "colour dialog closes before painting"))
        return false;
    document.setMixerWet(0);
    document.setMixerMix(0);
    document.setMixerLoad(1);
    document.setMixerFlow(.2);
    document.setMixerAutoClean(false);
    document.setMixerAutoLoad(false);
    document.setMixerAirbrush(false);
    document.loadMixerBrush();
    click(item("mixerAirbrush"));
    area->forceActiveFocus();
    const QPoint center = area->mapToScene(QPointF(60, 60)).toPoint();
    const auto   button = [&](bool _down) {
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa") {
            sendMacTestMouseButton(window, center, _down);
            return;
        }
#endif
        if (_down)
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, center);
        else
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, center);
    };
#ifdef Q_OS_MACOS
    const auto previousPointer = QCursor::pos();
    const auto restorePointer  = qScopeGuard([&] {
        if (QGuiApplication::platformName() == "cocoa")
            QCursor::setPos(previousPointer);
    });
    if (QGuiApplication::platformName() == "cocoa") {
        QCursor::setPos(window->mapToGlobal(center));
        QTest::qWait(50);
    }
#endif
    button(true);
    auto       timer = window->findChild<QObject *>("mixerAirbrushTimer");
    QSignalSpy ticks(timer, SIGNAL(triggered()));
    const int  first   = document.layerImage(0).pixelColor(60, 60).alpha();
    const bool builtUp = QTest::qWaitFor(
        [&] { return document.layerImage(0).pixelColor(60, 60).alpha() > first + 50; }, 2000);
    const int accumulated = document.layerImage(0).pixelColor(60, 60).alpha();
    button(false);
    if (!require(builtUp && ticks.count() >= 2 && first > 0 && accumulated > first + 50 &&
                     document.historyIndex() == history + 1,
                 "Airbrush timer accumulates while stationary in one undo step"))
        return false;
    document.undo();
    document.setMixerAirbrush(false);
    document.setMixerAutoClean(true);
    document.setMixerAutoLoad(true);
    return true;
}
