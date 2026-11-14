#pragma once
#include "AiProgressSmoke.h"
#include "Document.h"
#include "ai/AiImageController.h"
#include <QBuffer>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

inline bool runAiImageSmoke(QQuickWindow *_window, Document &_document, AiSettings &_settings,
                            AiImageController &_controller)
{
    const auto require = [](bool _ok, const char *_message) {
        if (!_ok)
            qWarning("AI UI check failed: %s", _message);
        return _ok;
    };
    _document.newDocument(160, 120, true, 16);
    _document.setTool("ellipticalSelect");
    _document.beginStroke(20, 20);
    _document.continueStroke(100, 90);
    _document.endStroke();
    auto *meshyAction = _window->findChild<QObject *>("meshyAction");
    auto *meshyDialog = _window->findChild<QObject *>("meshyDialog");
    auto *meshySource = _window->findChild<QObject *>("meshySource");
    auto *meshyKey    = _window->findChild<QQuickItem *>("meshyApiKey");
    auto *meshyPrompt = _window->findChild<QQuickItem *>("meshyPrompt");
    auto *meshySubmit = _window->findChild<QQuickItem *>("meshyGenerate");
    auto *meshyTask   = _window->findChild<QQuickItem *>("meshyTaskId");
    auto *meshyTabs   = _window->findChild<QQuickItem *>("meshyTabs");
    auto *meshyLimit  = _window->findChild<QQuickItem *>("meshyLimitTriangles");
    auto *meshyTarget = _window->findChild<QQuickItem *>("meshyTriangleTarget");
    if (!require(
            meshyAction && meshyDialog && meshySource && meshyKey && meshyPrompt && meshySubmit &&
                QMetaObject::invokeMethod(meshyAction, "triggered") &&
                QTest::qWaitFor([&] { return meshyDialog->property("opened").toBool(); }, 2000) &&
                meshySource->property("currentIndex").toInt() == 2 &&
                meshyKey->property("echoMode").toInt() != 0,
            "Meshy offers the current selection and masks the API key"))
        return false;
    meshySource->setProperty("currentIndex", 0);
    meshyKey->setProperty("text", "ui-only-meshy-key");
    meshyPrompt->setProperty("text", "First line");
    meshyPrompt->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(_window, Qt::Key_Return);
    if (!require(meshyDialog->property("opened").toBool() && meshySubmit->isEnabled() &&
                     meshyPrompt->property("text").toString().contains('\n'),
                 "Return in a Meshy description adds a line"))
        return false;
    if (!require(meshyLimit && meshyTarget && !meshyTarget->isVisible(),
                 "Meshy defaults to the original mesh"))
        return false;
    meshyLimit->setProperty("checked", true);
    if (!require(meshyTarget->isVisible() && meshyTarget->property("value").toInt() == 1000000 &&
                     meshyTarget->property("to").toInt() == 8000000,
                 "enabling the triangle target shows its editable value"))
        return false;
    if (!require(meshyTabs && meshyTabs->property("count").toInt() == 2,
                 "Meshy has separate generation and recovery tabs"))
        return false;
    meshyTabs->setProperty("currentIndex", 1);
    if (!require(meshyTask && meshyTask->isVisible() && meshyTarget->isVisible() &&
                     !meshySubmit->isEnabled(),
                 "recovering a Meshy task requires an ID"))
        return false;
    meshyTask->setProperty("text", "existing-task");
    if (!require(meshySubmit->isEnabled() && !meshyPrompt->isVisible(),
                 "a valid task ID enables recovery"))
        return false;
    QMetaObject::invokeMethod(meshyDialog, "reject");
    if (!require(
            QTest::qWaitFor([&] { return meshyKey->property("text").toString().isEmpty(); }, 2000),
            "closing Meshy clears the typed key"))
        return false;
    auto      *surfaceAction = _window->findChild<QObject *>("surfacePaintAction");
    auto      *surfaceDialog = _window->findChild<QObject *>("surfacePaintDialog");
    auto      *surfaceServer = _window->findChild<QObject *>("surfaceServer");
    const auto singleServer  = _settings.surfaceProviderId();
    if (!require(surfaceAction && surfaceDialog && surfaceServer && !singleServer.isEmpty() &&
                     QMetaObject::invokeMethod(surfaceAction, "triggered") && _controller.busy() &&
                     !surfaceDialog->property("visible").toBool(),
                 "surface painting starts directly with a single ComfyUI server"))
        return false;
    _controller.cancel();
    const auto extraServer         = _settings.saveProvider(_settings.newProvider("comfy"));
    auto      *surfaceServerAction = _window->findChild<QObject *>("surfaceServerAction");
    if (!require(
            surfaceServerAction && !extraServer.isEmpty() &&
                QMetaObject::invokeMethod(surfaceServerAction, "triggered") &&
                QTest::qWaitFor([&] { return surfaceDialog->property("opened").toBool(); }, 2000) &&
                surfaceServer->property("count").toInt() == 2 &&
                surfaceServer->property("currentValue").toString() == singleServer,
            "the explicit server chooser lists only ComfyUI and selects the previous server"))
        return false;
    surfaceServer->setProperty("currentIndex", 1);
    if (!require(QMetaObject::invokeMethod(surfaceDialog, "accept") && _controller.busy() &&
                     _settings.surfaceProviderId() == extraServer,
                 "choosing another server starts analysis and remembers the new server"))
        return false;
    _controller.cancel();
    if (!require(
            QTest::qWaitFor([&] { return !surfaceDialog->property("visible").toBool(); }, 2000) &&
                _settings.removeProvider(extraServer),
            "the extra test server can be removed"))
        return false;
    auto *action   = _window->findChild<QObject *>("aiImageAction");
    auto *dialog   = _window->findChild<QObject *>("aiImageDialog");
    auto *provider = _window->findChild<QObject *>("aiGenerationProvider");
    auto *variants = _window->findChild<QObject *>("aiVariantCount");
    auto *source   = _window->findChild<QObject *>("aiGenerationSource");
    auto *prompts  = _window->findChild<QObject *>("aiGenerationPrompt");
    auto *prompt   = _window->findChild<QQuickItem *>("aiGenerationText");
    if (!require(action && dialog && provider && source && prompts && prompt && variants,
                 "AI dialog and action are present"))
        return false;
    QMetaObject::invokeMethod(action, "triggered");
    QTest::qWait(80);
    if (!require(dialog->property("visible").toBool() && provider->property("count").toInt() == 3 &&
                     source->property("currentIndex").toInt() == 1,
                 "dialog defaults to the active selection and lists all providers"))
        return false;
    if (!require(variants->property("from").toInt() == 1 && variants->property("to").toInt() == 8 &&
                     variants->property("value").toInt() == 1,
                 "variant count defaults to one and supports up to eight"))
        return false;
    variants->setProperty("value", 3);
    const int count = prompts->property("count").toInt();
    _settings.savePrompt({{"name", "UI prompt"}, {"text", "Preserve the subject"}});
    QTest::qWait(20);
    if (!require(prompts->property("count").toInt() == count + 1,
                 "saved prompts update the generation menu"))
        return false;
    prompt->setProperty("text", "First line");
    prompt->forceActiveFocus();
    QTest::keyClick(_window, Qt::Key_End, Qt::ControlModifier);
    QTest::keyClick(_window, Qt::Key_Return);
    QTest::qWait(30);
    if (!require(prompt->property("text").toString().contains('\n') && !_controller.busy(),
                 "Return adds a prompt paragraph and never submits an image"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    QMetaObject::invokeMethod(dialog, "settingsRequested");
    QTest::qWait(60);
    auto *preferences = _window->findChild<QObject *>("preferencesDialog");
    auto *panel       = _window->findChild<QObject *>("aiPreferencesPanel");
    auto *key         = _window->findChild<QObject *>("aiApiKey");
    auto *name        = _window->findChild<QObject *>("aiProviderName");
    auto *address     = _window->findChild<QObject *>("aiProviderAddress");
    auto *save        = _window->findChild<QQuickItem *>("aiSaveProvider");
    auto *list        = _window->findChild<QObject *>("aiProviderList");
    auto *feedback    = _window->findChild<QQuickItem *>("aiProviderFeedback");
    if (!require(preferences && preferences->property("visible").toBool() &&
                     _window->property("preferencesPage").toInt() == 4 && key && name && save &&
                     panel && list && address && feedback,
                 "AI settings expose a provider list, form and pinned save button"))
        return false;
    if (!require(address->property("text").toString() == "https://api.openai.com/v1" &&
                     key->property("echoMode").toInt() != 0 &&
                     key->property("text").toString().isEmpty(),
                 "the server address is populated and API keys are never displayed"))
        return false;
    QMetaObject::invokeMethod(panel, "createProvider", Q_ARG(QVariant, QVariant("openai")));
    QTest::qWait(30);
    if (!require(_settings.providers().size() == 4 && list->property("count").toInt() == 4 &&
                     panel->property("providerId").toString() ==
                         _settings.providers().last().toMap().value("id").toString(),
                 "adding a provider creates and selects a real saved profile immediately"))
        return false;
    address->setProperty("text", "");
    QMetaObject::invokeMethod(save, "clicked");
    QTest::qWait(20);
    if (!require(!_settings.error().isEmpty() && feedback->isVisible() &&
                     _window->height() > feedback->mapToScene(QPointF()).y() + feedback->height(),
                 "validation errors stay visible above the pinned save action"))
        return false;
    address->setProperty("text", "https://api.openai.com/v1");
    name->setProperty("text", "Configured provider");
    key->setProperty("text", "ui-only-dummy-key");
    QMetaObject::invokeMethod(save, "clicked");
    QTest::qWait(40);
    const auto id = panel->property("providerId").toString();
    if (!require(_settings.provider(id).value("name").toString() == "Configured provider" &&
                     _settings.error().isEmpty() && _settings.hasKey(id) &&
                     key->property("text").toString().isEmpty(),
                 "saving a provider also stores its API key and clears the input"))
        return false;
    QMetaObject::invokeMethod(preferences, "close");
    QTest::qWait(30);
    if (!require(dialog->property("visible").toBool() &&
                     provider->property("currentIndex").toInt() == 3 && !_controller.busy(),
                 "closing settings returns to generation with the saved provider selected"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    QVariant          description;
    const QVariantMap ai{{"provider", "Test provider"},
                         {"model", "image-test"},
                         {"variant", 2},
                         {"variantCount", 3},
                         {"date", "2026-11-08T12:00:00Z"},
                         {"prompt", "Preserve <subject>"}};
    if (!require(
            QMetaObject::invokeMethod(_window, "aiLayerDescription",
                                      Q_RETURN_ARG(QVariant, description),
                                      Q_ARG(QVariant, QVariant(ai))) &&
                description.toString().contains("Test provider") &&
                description.toString().contains("image-test") &&
                description.toString().contains("2/3") &&
                description.toString().contains("Preserve <subject>") &&
                description.toString().contains("2026"),
            "layer information includes provider, model, variant count, date and literal prompt"))
        return false;
    auto topaz         = _settings.newProvider("fal");
    topaz["name"]      = "upscale";
    topaz["model"]     = "topaz/upscale/image/precision";
    topaz["maxEdge"]   = 4096;
    const auto topazId = _settings.saveProvider(topaz);
    if (!require(!topazId.isEmpty(), "Topaz preset can be added"))
        return false;
    QMetaObject::invokeMethod(panel, "selectProvider", Q_ARG(QVariant, QVariant(topazId)));
    auto *mode        = _window->findChild<QObject *>("aiProviderPromptMode");
    auto *nativeInput = _window->findChild<QObject *>("aiProviderNativeInput");
    if (!require(mode && nativeInput && mode->property("currentIndex").toInt() == 2 &&
                     nativeInput->property("checked").toBool(),
                 "legacy Topaz profile loads without prompt and without reducing input"))
        return false;
    QImage small(32, 24, QImage::Format_RGB32);
    small.fill(Qt::green);
    QByteArray bytes;
    QBuffer    buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!require(small.save(&buffer, "PNG"), "create small AI source"))
        return false;
    PhotoCrap::Ai::Input input;
    input.canvas  = QSize(_document.width(), _document.height());
    input.bounds  = QRect(QPoint{}, input.canvas);
    input.session = _document.documentSession();
    const auto result =
        PhotoCrap::Ai::prepareResult(input, bytes, 16, std::make_shared<std::atomic_bool>(false));
    QString error;
    if (!require(_document.insertAiResult(input, result, {{"provider", "AI source"}}, error),
                 "create an AI layer for upscaling"))
        return false;
    prompt->setProperty("text", "");
    QMetaObject::invokeMethod(dialog, "chooseProvider", Q_ARG(QVariant, QVariant(topazId)));
    QMetaObject::invokeMethod(dialog, "start");
    QTest::qWait(80);
    auto *generate   = _window->findChild<QQuickItem *>("aiGenerateButton");
    auto *topazModel = _window->findChild<QQuickItem *>("aiTopazModel");
    auto *factor     = _window->findChild<QObject *>("aiUpscaleFactor");
    if (!require(generate && generate->isEnabled() && !prompt->isVisible() && topazModel &&
                     topazModel->isVisible() && factor &&
                     factor->property("value").toInt() == 200 &&
                     source->property("currentText").toString().contains("32"),
                 "promptless upscale enables Run, shows model/factor and defaults to original AI "
                 "pixels"))
        return false;
    QMetaObject::invokeMethod(dialog, "chooseProvider",
                              Q_ARG(QVariant, _settings.providers()[0].toMap().value("id")));
    QTest::qWait(40);
    if (!require(prompt->isVisible() && !generate->isEnabled(),
                 "text editing providers still require a prompt"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    auto optional          = _settings.newProvider("fal");
    optional["promptMode"] = "optional";
    const auto optionalId  = _settings.saveProvider(optional);
    QMetaObject::invokeMethod(dialog, "chooseProvider", Q_ARG(QVariant, QVariant(optionalId)));
    QMetaObject::invokeMethod(dialog, "start");
    QTest::qWait(40);
    if (!require(prompt->isVisible() && generate->isEnabled(),
                 "optional prompt profiles accept an empty prompt"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    const auto esrganId = _settings.saveProvider(_settings.newProvider("falEsrgan"));
    if (!require(!esrganId.isEmpty(), "Real-ESRGAN preset can be saved"))
        return false;
    QMetaObject::invokeMethod(dialog, "chooseProvider", Q_ARG(QVariant, QVariant(esrganId)));
    QMetaObject::invokeMethod(dialog, "start");
    QTest::qWait(80);
    if (!require(!prompt->isVisible() && generate->isEnabled() && topazModel->isVisible() &&
                     topazModel->property("currentText").toString() == "RealESRGAN_x2plus" &&
                     factor->property("value").toInt() == 200 &&
                     source->property("currentText").toString().contains("32"),
                 "Real-ESRGAN defaults to x2 photo model and native AI source without prompt"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    auto sharedEdit      = _settings.newProvider("fal");
    sharedEdit["name"]   = "Saved fal test profile";
    sharedEdit["keyEnv"] = "";
    const auto sharedId  = _settings.saveProvider(sharedEdit);
    _settings.saveKey(sharedId, "shared-ui-dummy");
    QTest::qWait(30);
    auto sharedUpscale      = _settings.provider(esrganId);
    sharedUpscale["keyEnv"] = "";
    _settings.saveProvider(sharedUpscale);
    QMetaObject::invokeMethod(dialog, "settingsRequested");
    QMetaObject::invokeMethod(panel, "selectProvider", Q_ARG(QVariant, QVariant(esrganId)));
    QTest::qWait(30);
    auto *keyStatus = _window->findChild<QObject *>("aiApiKeyStatus");
    if (!require(_settings.keySource(esrganId) == "shared" && keyStatus &&
                     keyStatus->property("text").toString().contains("Saved fal test profile") &&
                     key->property("text").toString().isEmpty(),
                 "upscale displays the saved key source without exposing its secret"))
        return false;
    key->setProperty("text", "unsaved-ui-dummy");
    if (!require(keyStatus->property("text").toString().contains("Enregistrer") ||
                     keyStatus->property("text").toString().contains("Save"),
                 "a typed key is marked as awaiting explicit saving"))
        return false;
    key->setProperty("text", "");
    QMetaObject::invokeMethod(preferences, "close");
    QMetaObject::invokeMethod(dialog, "reject");
    QMetaObject::invokeMethod(dialog, "settingsRequested");
    QMetaObject::invokeMethod(panel, "createProvider", Q_ARG(QVariant, QVariant("gradio")));
    QTest::qWait(40);
    const auto qwenId   = panel->property("providerId").toString();
    auto *     qwen     = panel->findChild<QObject *>("aiQwenProviderParameters");
    auto *     username = panel->findChild<QObject *>("aiGradioUsername");
    auto *     qwenLora = qwen ? qwen->findChild<QObject *>("qwenLora") : nullptr;
    if (!require(qwen && username && qwenLora && qwenLora->property("count").toInt() == 20,
                 "Qwen preferences expose HTTP Basic, nineteen LoRA adapters and no LoRA"))
        return false;
    auto adapters = qwen->property("adapters");
    if (adapters.metaType().id() == qMetaTypeId<QJSValue>())
        adapters = adapters.value<QJSValue>().toVariant();
    const QStringList expectedAdapters{"Multiple-Angles",
                                       "Photo-to-Anime",
                                       "Anime-V2",
                                       "Light-Migration",
                                       "Upscaler",
                                       "Style-Transfer",
                                       "Manga-Tone",
                                       "Anything2Real",
                                       "Fal-Multiple-Angles",
                                       "Polaroid-Photo",
                                       "Unblur-Anything",
                                       "Midnight-Noir-Eyes-Spotlight",
                                       "Hyper-Realistic-Portrait",
                                       "Ultra-Realistic-Portrait",
                                       "Pixar-Inspired-3D",
                                       "Noir-Comic-Book",
                                       "Any-light",
                                       "Studio-DeLight",
                                       "Cinematic-FlatLog"};
    if (!require(adapters.toStringList() == expectedAdapters,
                 "LoRA names and capitalization match the supplied server list"))
        return false;
    address->setProperty("text", "http://127.0.0.1:7861");
    username->setProperty("text", "qwen-ui-user");
    key->setProperty("text", "qwen-ui-only-password");
    qwenLora->setProperty("currentIndex", 5);
    qwen->findChild<QObject *>("qwenRandomizeSeed")->setProperty("checked", false);
    qwen->findChild<QObject *>("qwenSeed")->setProperty("text", "123");
    qwen->findChild<QObject *>("qwenGuidance")->setProperty("value", 75);
    qwen->findChild<QObject *>("qwenSteps")->setProperty("value", 8);
    QMetaObject::invokeMethod(save, "clicked");
    QTest::qWait(40);
    const auto qwenProfile = _settings.provider(qwenId);
    const auto qwenOptions =
        QJsonDocument::fromJson(qwenProfile.value("options").toString().toUtf8()).object();
    if (!require(qwenProfile.value("username") == "qwen-ui-user" &&
                     qwenOptions.value("lora_adapter") == "Upscaler" &&
                     qwenOptions.value("seed").toInt() == 123 &&
                     qwenOptions.value("steps").toInt() == 8 &&
                     qAbs(qwenOptions.value("guidance_scale").toDouble() - .75) < .001 &&
                     _settings.hasKey(qwenId) && key->property("text").toString().isEmpty(),
                 "Qwen saves LoRA and parameters separately from its masked secure password"))
        return false;
    QMetaObject::invokeMethod(preferences, "close");
    QMetaObject::invokeMethod(dialog, "chooseProvider", Q_ARG(QVariant, QVariant(qwenId)));
    QMetaObject::invokeMethod(dialog, "start");
    prompt->setProperty("text", "Test Qwen parameters without a request");
    QTest::qWait(60);
    auto *generationQwen = dialog->findChild<QQuickItem *>("aiQwenParameters");
    auto *generationLora =
        generationQwen ? generationQwen->findChild<QObject *>("qwenLora") : nullptr;
    if (!require(generationQwen && generationQwen->isVisible() && generationLora &&
                     generationLora->property("editText") == "Upscaler" &&
                     generationQwen->findChild<QObject *>("qwenSteps")->property("value").toInt() ==
                         8 &&
                     generate->isEnabled(),
                 "generation restores the saved Qwen adapter and exposes editable parameters"))
        return false;
    generationLora->setProperty("currentIndex", 0);
    QMetaObject::invokeMethod(generationLora, "activated", Q_ARG(int, 0));
    auto *generationSteps = generationQwen->findChild<QObject *>("qwenSteps");
    auto *generationGuidance = generationQwen->findChild<QObject *>("qwenGuidance");
    if (!require(generationLora->property("editText") == generationQwen->property("noAdapterLabel") &&
                     generate->isEnabled() && generationSteps->property("value").toInt() == 40 &&
                     generationGuidance->property("value").toInt() == 400,
                 "no LoRA starts from the base model inference settings"))
        return false;
    generationLora->setProperty("currentIndex", 5);
    QMetaObject::invokeMethod(generationLora, "activated", Q_ARG(int, 5));
    if (!require(generationSteps->property("value").toInt() == 8 &&
                     generationGuidance->property("value").toInt() == 75,
                 "returning to a LoRA restores the previous inference settings"))
        return false;
    QMetaObject::invokeMethod(dialog, "reject");
    return runAiProgressSmoke(_window);
}
