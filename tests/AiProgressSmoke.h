#pragma once
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QtTest>
#include <memory>

inline bool runAiProgressSmoke(QQuickWindow *_window)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("AI progress UI check failed: %s", _message);
        return _ok;
    };
    auto *panel    = _window->findChild<QQuickItem *>("aiProgressPanel");
    auto *surfaces = _window->findChild<QQuickItem *>("surfacePaintBar");
    auto *viewport = _window->findChild<QQuickItem *>("viewport");
    if (!require(panel && surfaces && viewport, "progress panel and surface toolbar exist"))
        return false;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(import QtQml
QtObject {
    property bool busy: true
    property bool pendingResult: false
    property bool surfaceAnalysis: false
    property bool surfacePicking: false
    property bool hasSurfaceSelection: false
    property bool surfaceProtectionAvailable: true
    property bool surfacePickingHint: true
    property string status: "Génération dans le fournisseur, étape complète et lisible"
    property string error: ""
    property string providerName: "UI provider"
    property string modelName: "test-model"
    property int completedVariants: 1
    property int variantCount: 3
    property int elapsedSeconds: 73
    property int cancellations: 0
    property int retries: 0
    property int surfaceApplies: 0
    signal changed()
    function cancel() { cancellations++; busy=false; pendingResult=false; surfacePicking=false; error=""; status="Canceled"; changed() }
    function retryInsertion() { retries++; pendingResult=false; busy=true; changed() }
    function applySurfaceSelection() { surfaceApplies++ }
})QML",
                      QUrl("qrc:/photocrap-test/AiProgressFixture.qml"));
    if (!QTest::qWaitFor([&] { return component.isReady() || component.isError(); }, 3000) ||
        component.isError()) {
        qWarning().noquote() << component.errorString();
        return require(false, "progress fixture compiles");
    }
    std::unique_ptr<QObject> controller(component.create());
    if (!require(controller != nullptr, "progress fixture loads"))
        return false;
    const auto originalPanel    = panel->property("controller");
    const auto originalSurfaces = surfaces->property("controller");
    auto       restore          = qScopeGuard([&] {
        panel->setProperty("controller", originalPanel);
        surfaces->setProperty("controller", originalSurfaces);
    });
    const auto fixture          = QVariant::fromValue(controller.get());
    panel->setProperty("controller", fixture);
    surfaces->setProperty("controller", fixture);
    QMetaObject::invokeMethod(controller.get(), "changed");
    QTest::qWait(30);
    auto *stage    = panel->findChild<QObject *>("aiProgressStage");
    auto *provider = panel->findChild<QObject *>("aiProgressProvider");
    auto *elapsed  = panel->findChild<QObject *>("aiProgressElapsed");
    auto *add      = panel->findChild<QQuickItem *>("aiProgressAddResult");
    auto *cancel   = panel->findChild<QObject *>("aiProgressCancel");
    if (!require(panel->isVisible() && panel->width() >= 400 && panel->height() >= 130 && stage &&
                     provider && elapsed && add && cancel,
                 "a prominent progress card is visible over the canvas"))
        return false;
    if (!require(stage->property("text").toString() == controller->property("status").toString() &&
                     stage->property("elide").toInt() == Qt::ElideNone &&
                     provider->property("text").toString().contains("UI provider") &&
                     elapsed->property("text").toString().contains("01:13"),
                 "stage is untruncated, with provider and elapsed time"))
        return false;
    controller->setProperty("busy", false);
    controller->setProperty("pendingResult", true);
    controller->setProperty("error", "Result retained, free up a layer");
    QMetaObject::invokeMethod(controller.get(), "changed");
    QTest::qWait(20);
    if (!require(panel->isVisible() && add->isVisible(),
                 "pending results keep a visible insertion action"))
        return false;
    QMetaObject::invokeMethod(add, "clicked");
    if (!require(controller->property("retries").toInt() == 1,
                 "retry action reaches the controller"))
        return false;
    QMetaObject::invokeMethod(cancel, "clicked");
    QTest::qWait(20);
    if (!require(controller->property("cancellations").toInt() == 1 && panel->isVisible(),
                 "cancel stops the job and leaves a readable outcome"))
        return false;
    QMetaObject::invokeMethod(cancel, "clicked");
    QTest::qWait(20);
    if (!require(!panel->isVisible(), "idle notification can be dismissed"))
        return false;

    auto *relief        = surfaces->findChild<QQuickItem *>("surfaceFollowRelief");
    auto *paint         = surfaces->findChild<QQuickItem *>("surfacePaintApply");
    auto *surfaceCancel = surfaces->findChild<QQuickItem *>("surfacePaintCancel");
    auto *protection    = surfaces->findChild<QQuickItem *>("surfaceProtectObject");
    controller->setProperty("surfaceAnalysis", true);
    controller->setProperty("busy", true);
    QMetaObject::invokeMethod(controller.get(), "changed");
    if (!require(surfaces->isVisible() && !panel->isVisible() && relief && !relief->isVisible(),
                 "surface analysis shows progress outside the canvas"))
        return false;

    controller->setProperty("busy", false);
    controller->setProperty("surfacePicking", true);
    QMetaObject::invokeMethod(controller.get(), "changed");
    QTest::qWait(30);
    if (!require(!panel->isVisible() && surfaces->isVisible() &&
                     surfaces->mapToScene({0, surfaces->height()}).y() <=
                         viewport->mapToScene({0, 0}).y(),
                 "surface selection leaves the whole canvas exposed") ||
        !require(relief->isVisible() && relief->isEnabled() && paint && paint->isVisible() &&
                     !paint->isEnabled() && surfaceCancel && surfaceCancel->isVisible(),
                 "surface controls are shown and painting requires a selected region"))
        return false;

    const auto click = [&](QQuickItem *item) {
        const auto point = item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
        QTest::mouseClick(_window, Qt::LeftButton, Qt::NoModifier, point);
    };
    click(relief);
    if (!require(relief->property("checked").toBool(), "the relief checkbox responds to a click"))
        return false;
    if (!require(protection && protection->isVisible(), "object protection is available"))
        return false;
    click(protection);
    if (!require(protection->property("checked").toBool(), "object protection responds to a click"))
        return false;
    controller->setProperty("hasSurfaceSelection", true);
    QMetaObject::invokeMethod(controller.get(), "changed");
    if (!require(paint->isEnabled(), "selection enables Paint"))
        return false;
    click(paint);
    if (!require(controller->property("surfaceApplies").toInt() == 1,
                 "clicking Paint applies the surface selection"))
        return false;
    QTest::qWait(30);
    const int previousCancellations = controller->property("cancellations").toInt();
    click(surfaceCancel);
    return require(QTest::qWaitFor(
                           [&] {
                               return controller->property("cancellations").toInt() ==
                                          previousCancellations + 1 &&
                                      !surfaces->isVisible() && !panel->isVisible();
                           },
                           1000),
                   "Cancel dismisses surface selection without leaving a card on the canvas");
}
