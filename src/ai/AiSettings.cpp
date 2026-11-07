#include "AiSettings.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

namespace
{
QString id()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
void store(QVariantList &_items, const QVariantMap &_item)
{
    for (auto &entry : _items)
        if (entry.toMap().value("id") == _item.value("id")) {
            entry = _item;
            return;
        }
    _items.append(_item);
}
} // namespace
AiSettings::AiSettings(QObject *_parent, bool _persist,
                       std::shared_ptr<AiCredentialStore> _credentials)
    : QObject(_parent), m_persist(_persist), m_credentials(_credentials ? std::move(_credentials)
                                                           : _persist   ? nativeAiCredentialStore()
                                                                        : memoryAiCredentialStore())
{
    if (_persist) {
        m_providers = QSettings().value("ai/providers").toList();
        m_prompts   = QSettings().value("ai/prompts").toList();
    }
    if (!_persist || !QSettings().contains("ai/providers")) {
        for (const QString kind : {"openai", "fal", "comfy"}) {
            auto entry  = newProvider(kind);
            entry["id"] = id();
            m_providers.append(entry);
        }
    }
    if (!_persist || !QSettings().contains("ai/prompts")) {
        const auto add = [&](const QString &_name, const QString &_text) {
            m_prompts.append(QVariantMap{{"id", id()}, {"name", _name}, {"text", _text}});
        };
        add(tr("Retouche naturelle"),
            tr("Améliore subtilement cette photo : détails, lumière et couleurs naturelles. "
               "Préserve la composition, les visages et leur identité."));
        add(tr("Retirer un élément"),
            tr("Retire l’élément indésirable de la zone envoyée et reconstruis le décor de façon "
               "réaliste, en respectant la lumière et les textures."));
        add(tr("Changer d’ambiance"),
            tr("Donne à cette image une ambiance cinématographique douce. Conserve la composition, "
               "les détails du sujet et des couleurs crédibles."));
        add(tr("Restaurer une photo"),
            tr("Restaure cette photo : corrige les rayures, les taches et le bruit sans inventer "
               "de nouveaux détails ni modifier les traits des personnes."));
    }
    if (_persist)
        for (const auto &entry : m_providers)
            loadKey(entry.toMap().value("id").toString());
}
void AiSettings::notifyChanged()
{
    ++m_revision;
    emit changed();
}
bool AiSettings::validEndpoint(const QUrl &_url)
{
    return _url.isValid() && (_url.scheme() == "https" || _url.scheme() == "http") &&
           !_url.host().isEmpty() && _url.userInfo().isEmpty() && !_url.hasQuery() &&
           !_url.hasFragment();
}
QVariantMap AiSettings::newProvider(const QString &_kind) const
{
    QVariantMap p{{"kind", _kind},   {"name", tr("Nouveau fournisseur")}, {"maxEdge", 1536},
                  {"options", "{}"}, {"imageField", "image_urls"},        {"imageArray", true}};
    if (_kind == "openai") {
        p["name"]   = "OpenAI · GPT Image";
        p["url"]    = "https://api.openai.com/v1";
        p["model"]  = "gpt-image-2.5-sunburst";
        p["keyEnv"] = "OPENAI_API_KEY";
    }
    if (_kind == "fal") {
        p["name"]   = "fal.ai · FLUX.2";
        p["url"]    = "https://queue.fal.run";
        p["model"]  = "fal-ai/flux-2/edit";
        p["keyEnv"] = "FAL_KEY";
    }
    if (_kind == "comfy") {
        p["name"]     = "ComfyUI local";
        p["url"]      = "http://127.0.0.1:8188";
        p["model"]    = "model.safetensors";
        p["workflow"] = defaultWorkflow();
        p["maxEdge"]  = 1024;
    }
    if (_kind == "compatible") {
        p["name"]  = tr("Serveur compatible OpenAI");
        p["url"]   = "http://127.0.0.1:8000/v1";
        p["model"] = "";
    }
    return p;
}
QVariantMap AiSettings::provider(const QString &_id) const
{
    for (const auto &value : m_providers)
        if (value.toMap().value("id").toString() == _id)
            return value.toMap();
    return {};
}
bool AiSettings::reject(const QString &_error)
{
    m_error = _error;
    notifyChanged();
    return false;
}
void AiSettings::persist()
{
    m_error.clear();
    if (m_persist) {
        QSettings settings;
        settings.setValue("ai/providers", m_providers);
        settings.setValue("ai/prompts", m_prompts);
    }
    notifyChanged();
}
QString AiSettings::saveProvider(QVariantMap _provider)
{
    const auto kind = _provider.value("kind").toString();
    if (!QStringList{"openai", "fal", "comfy", "compatible"}.contains(kind) ||
        _provider.value("name").toString().trimmed().isEmpty() ||
        !validEndpoint(QUrl(_provider.value("url").toString()))) {
        reject(tr("Indiquez un nom, un type et une adresse HTTP(S) valides."));
        return {};
    }
    const auto options =
        QJsonDocument::fromJson(_provider.value("options", "{}").toString().toUtf8());
    if (!options.isObject()) {
        reject(tr("Les options avancées doivent être un objet JSON."));
        return {};
    }
    if (kind == "fal" && !QRegularExpression("^[a-zA-Z0-9_.-]+(/[a-zA-Z0-9_.-]+)+$")
                              .match(_provider.value("model").toString())
                              .hasMatch()) {
        reject(tr("Indiquez le chemin du modèle fal.ai, par exemple fal-ai/flux-2/edit."));
        return {};
    }
    if (kind == "comfy") {
        const auto workflow =
            QJsonDocument::fromJson(_provider.value("workflow").toString().toUtf8());
        if (!workflow.isObject() || workflow.object().isEmpty() ||
            workflow.object().contains("nodes")) {
            reject(tr("Chargez un workflow ComfyUI exporté au format API, et non le format de "
                      "l’éditeur."));
            return {};
        }
    }
    QVariantMap clean;
    for (const auto &field : {"id", "name", "kind", "url", "model", "keyEnv", "options", "workflow",
                              "imageField", "imageArray"})
        clean[field] = _provider.value(field);
    clean["maxEdge"] = qBound(256, _provider.value("maxEdge", 1536).toInt(), 4096);
    if (clean["id"].toString().isEmpty())
        clean["id"] = id();
    clean["name"]   = clean["name"].toString().trimmed().left(100);
    clean["keyEnv"] = clean["keyEnv"].toString().trimmed();
    if (!clean["keyEnv"].toString().isEmpty() && !QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$")
                                                      .match(clean["keyEnv"].toString())
                                                      .hasMatch()) {
        reject(tr("Le nom de variable d’environnement n’est pas valide."));
        return {};
    }
    const auto previous = provider(clean["id"].toString());
    if (!previous.isEmpty() && (previous.value("url") != clean.value("url") ||
                                previous.value("kind") != clean.value("kind"))) {
        eraseCredential(credentialAccount(clean["id"].toString()));
        m_keys.remove(clean["id"].toString());
    }
    store(m_providers, clean);
    persist();
    return clean["id"].toString();
}
bool AiSettings::removeProvider(const QString &_id)
{
    for (int i = 0; i < m_providers.size(); ++i)
        if (m_providers[i].toMap().value("id") == _id) {
            eraseCredential(credentialAccount(_id));
            m_providers.removeAt(i);
            m_keys.remove(_id);
            persist();
            return true;
        }
    return false;
}
QString AiSettings::savePrompt(QVariantMap _prompt)
{
    const QString name = _prompt.value("name").toString().trimmed(),
                  text = _prompt.value("text").toString().trimmed();
    if (name.isEmpty() || text.isEmpty() || text.size() > 32000) {
        reject(tr("Indiquez un nom et un prompt de 1 à 32 000 caractères."));
        return {};
    }
    if (_prompt.value("id").toString().isEmpty())
        _prompt["id"] = id();
    QVariantMap clean{{"id", _prompt["id"]}, {"name", name.left(100)}, {"text", text}};
    store(m_prompts, clean);
    persist();
    return clean["id"].toString();
}
bool AiSettings::removePrompt(const QString &_id)
{
    for (int i = 0; i < m_prompts.size(); ++i)
        if (m_prompts[i].toMap().value("id") == _id) {
            m_prompts.removeAt(i);
            persist();
            return true;
        }
    return false;
}
QString AiSettings::readWorkflow(const QUrl &_url)
{
    QFile file(_url.toLocalFile());
    if (!_url.isLocalFile() || !file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) {
        reject(tr("Workflow illisible ou supérieur à 2 Mio."));
        return {};
    }
    return QString::fromUtf8(file.readAll());
}
void AiSettings::setSessionKey(const QString &_id, const QString &_key)
{
    if (_key.trimmed().isEmpty())
        m_keys.remove(_id);
    else
        m_keys[_id] = _key.trimmed();
    notifyChanged();
}
QString AiSettings::credentialAccount(const QString &_id) const
{
    const auto entry = provider(_id);
    const auto scope = entry.value("kind").toString() + "|" + entry.value("url").toString();
    return _id + ":" +
           QString::fromLatin1(
               QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha256).toHex());
}
void AiSettings::loadKey(const QString &_id)
{
    const auto account = credentialAccount(_id);
    ++m_credentialJobs;
    m_credentials->read(
        account, this, [this, _id, account](const QString &value, const QString &error) {
            --m_credentialJobs;
            if (!provider(_id).isEmpty() && account == credentialAccount(_id)) {
                if (error.isEmpty() && !value.isEmpty())
                    m_keys[_id] = value;
                if (!error.isEmpty())
                    m_error = tr("Impossible d’accéder au trousseau : %1").arg(error);
            }
            notifyChanged();
        });
}
void AiSettings::saveKey(const QString &_id, const QString &_key)
{
    if (provider(_id).isEmpty() || _key.trimmed().isEmpty()) {
        reject(tr("Choisissez un fournisseur et saisissez une clé API."));
        return;
    }
    const auto account = credentialAccount(_id), secret = _key.trimmed();
    ++m_credentialJobs;
    m_error.clear();
    notifyChanged();
    m_credentials->write(
        account, secret, this, [this, _id, account, secret](const QString &, const QString &error) {
            --m_credentialJobs;
            const bool current = !provider(_id).isEmpty() && account == credentialAccount(_id);
            if (current && error.isEmpty())
                m_keys[_id] = secret;
            if (!error.isEmpty())
                m_error = tr("La clé n’a pas été enregistrée dans le trousseau : %1").arg(error);
            notifyChanged();
            emit keySaved(_id, current && error.isEmpty());
        });
}
void AiSettings::eraseCredential(const QString &_account)
{
    ++m_credentialJobs;
    m_credentials->remove(_account, this, [this](const QString &, const QString &error) {
        --m_credentialJobs;
        if (!error.isEmpty())
            m_error = tr("Impossible de supprimer la clé du trousseau : %1").arg(error);
        notifyChanged();
    });
}
void AiSettings::removeKey(const QString &_id)
{
    if (provider(_id).isEmpty() || credentialsBusy())
        return;
    m_keys.remove(_id);
    eraseCredential(credentialAccount(_id));
    notifyChanged();
}
QString AiSettings::keySource(const QString &_id) const
{
    if (m_keys.contains(_id))
        return "stored";
    return hasKey(_id) ? "environment" : QString{};
}
QString AiSettings::key(const QString &_id) const
{
    if (m_keys.contains(_id))
        return m_keys.value(_id);
    const auto variable = provider(_id).value("keyEnv").toString().toUtf8();
    return variable.isEmpty() ? QString{} : qEnvironmentVariable(variable.constData()).trimmed();
}
bool AiSettings::hasKey(const QString &_id) const
{
    return !key(_id).isEmpty();
}
QString AiSettings::defaultWorkflow()
{
    return QString::fromUtf8(R"JSON({
 "1":{"class_type":"LoadImage","inputs":{"image":"{{image}}"}},
 "2":{"class_type":"CheckpointLoaderSimple","inputs":{"ckpt_name":"{{model}}"}},
 "3":{"class_type":"CLIPTextEncode","inputs":{"text":"{{prompt}}","clip":["2",1]}},
 "4":{"class_type":"CLIPTextEncode","inputs":{"text":"","clip":["2",1]}},
 "5":{"class_type":"VAEEncode","inputs":{"pixels":["1",0],"vae":["2",2]}},
 "6":{"class_type":"KSampler","inputs":{"seed":"{{seed}}","steps":20,"cfg":7,"sampler_name":"euler","scheduler":"normal","denoise":0.55,"model":["2",0],"positive":["3",0],"negative":["4",0],"latent_image":["10",0]}},
 "7":{"class_type":"VAEDecode","inputs":{"samples":["6",0],"vae":["2",2]}},
 "8":{"class_type":"SaveImage","inputs":{"images":["7",0],"filename_prefix":"PhotoCrap"}},
 "9":{"class_type":"LoadImage","inputs":{"image":"{{mask}}"}},
 "10":{"class_type":"SetLatentNoiseMask","inputs":{"samples":["5",0],"mask":["9",1]}}
})JSON");
}
