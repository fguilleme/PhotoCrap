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
    auto *panel = _window->findChild<QQuickItem *>("aiProgressPanel");
    if (!require(panel, "progress panel exists"))
        return false;
    QQmlComponent component(qmlEngine(_window));
    component.setData(R"QML(import QtQml
QtObject {
    property bool busy: true
    property bool pendingResult: false
    property bool surfaceAnalysis: false
    property bool surfacePicking: false
    property string status: "Génération dans le fournisseur, étape complète et lisible"
    property string error: ""
    property string providerName: "UI provider"
    property string modelName: "test-model"
    property int completedVariants: 1
    property int variantCount: 3
    property int elapsedSeconds: 73
    property int cancellations: 0
    property int retries: 0
    signal changed()
    function cancel() { cancellations++; busy=false; pendingResult=false; status="Canceled"; changed() }
    function retryInsertion() { retries++; pendingResult=false; busy=true; changed() }
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
    const auto original = panel->property("controller");
    auto       restore  = qScopeGuard([&] { panel->setProperty("controller", original); });
    panel->setProperty("controller", QVariant::fromValue(controller.get()));
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
    return require(!panel->isVisible(), "idle notification can be dismissed");
}
