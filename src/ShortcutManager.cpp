#include "ShortcutManager.h"
#include "ShortcutCatalog.h"
#include <algorithm>

#include <QGuiApplication>
#include <QHash>
#include <QKeySequence>
#include <QSet>
#include <QSettings>

namespace
{
const char *translatedShortcutText(const QString &_source)
{
    static const QHash<QString, const char *> texts{
        {"Fichier", QT_TRANSLATE_NOOP("ShortcutManager", "Fichier")},
        {"Édition", QT_TRANSLATE_NOOP("ShortcutManager", "Édition")},
        {"Sélection", QT_TRANSLATE_NOOP("ShortcutManager", "Sélection")},
        {"Calques", QT_TRANSLATE_NOOP("ShortcutManager", "Calques")},
        {"Affichage", QT_TRANSLATE_NOOP("ShortcutManager", "Affichage")},
        {"Outils", QT_TRANSLATE_NOOP("ShortcutManager", "Outils")},
        {"Forme d’historique", QT_TRANSLATE_NOOP("ShortcutManager", "Forme d’historique")},
        {"Tampons", QT_TRANSLATE_NOOP("ShortcutManager", "Tampons")},
        {"Correction", QT_TRANSLATE_NOOP("ShortcutManager", "Correction")},
        {"Nouveau document", QT_TRANSLATE_NOOP("ShortcutManager", "Nouveau document")},
        {"Ouvrir", QT_TRANSLATE_NOOP("ShortcutManager", "Ouvrir")},
        {"Importer comme calque", QT_TRANSLATE_NOOP("ShortcutManager", "Importer comme calque")},
        {"Enregistrer", QT_TRANSLATE_NOOP("ShortcutManager", "Enregistrer")},
        {"Enregistrer sous", QT_TRANSLATE_NOOP("ShortcutManager", "Enregistrer sous")},
        {"Exporter l’image", QT_TRANSLATE_NOOP("ShortcutManager", "Exporter l’image")},
        {"Annuler", QT_TRANSLATE_NOOP("ShortcutManager", "Annuler")},
        {"Rétablir", QT_TRANSLATE_NOOP("ShortcutManager", "Rétablir")},
        {"Copier l’image visible", QT_TRANSLATE_NOOP("ShortcutManager", "Copier l’image visible")},
        {"Coller comme calque", QT_TRANSLATE_NOOP("ShortcutManager", "Coller comme calque")},
        {"Ouvrir les réglages", QT_TRANSLATE_NOOP("ShortcutManager", "Ouvrir les réglages")},
        {"Tout sélectionner", QT_TRANSLATE_NOOP("ShortcutManager", "Tout sélectionner")},
        {"Désélectionner", QT_TRANSLATE_NOOP("ShortcutManager", "Désélectionner")},
        {"Effacer les pixels", QT_TRANSLATE_NOOP("ShortcutManager", "Effacer les pixels")},
        {"Nouveau calque", QT_TRANSLATE_NOOP("ShortcutManager", "Nouveau calque")},
        {"Dupliquer le calque", QT_TRANSLATE_NOOP("ShortcutManager", "Dupliquer le calque")},
        {"Adapter à la fenêtre", QT_TRANSLATE_NOOP("ShortcutManager", "Adapter à la fenêtre")},
        {"Taille réelle", QT_TRANSLATE_NOOP("ShortcutManager", "Taille réelle")},
        {"Réduire la taille du pinceau",
         QT_TRANSLATE_NOOP("ShortcutManager", "Réduire la taille du pinceau")},
        {"Augmenter la taille du pinceau",
         QT_TRANSLATE_NOOP("ShortcutManager", "Augmenter la taille du pinceau")},
        {"Réduire la dureté du pinceau",
         QT_TRANSLATE_NOOP("ShortcutManager", "Réduire la dureté du pinceau")},
        {"Augmenter la dureté du pinceau",
         QT_TRANSLATE_NOOP("ShortcutManager", "Augmenter la dureté du pinceau")},
        {"Couleurs par défaut", QT_TRANSLATE_NOOP("ShortcutManager", "Couleurs par défaut")},
        {"Échanger les couleurs", QT_TRANSLATE_NOOP("ShortcutManager", "Échanger les couleurs")},
        {"Déplacement", QT_TRANSLATE_NOOP("ShortcutManager", "Déplacement")},
        {"Sélection rectangulaire",
         QT_TRANSLATE_NOOP("ShortcutManager", "Sélection rectangulaire")},
        {"Lasso", QT_TRANSLATE_NOOP("ShortcutManager", "Lasso")},
        {"Sélections intelligentes",
         QT_TRANSLATE_NOOP("ShortcutManager", "Sélections intelligentes")},
        {"Recadrage", QT_TRANSLATE_NOOP("ShortcutManager", "Recadrage")},
        {"Pinceau", QT_TRANSLATE_NOOP("ShortcutManager", "Pinceau")},
        {"Gomme", QT_TRANSLATE_NOOP("ShortcutManager", "Gomme")},
        {"Remplissage contigu", QT_TRANSLATE_NOOP("ShortcutManager", "Remplissage contigu")},
        {"Dégradé", QT_TRANSLATE_NOOP("ShortcutManager", "Dégradé")},
        {"Pipette", QT_TRANSLATE_NOOP("ShortcutManager", "Pipette")},
        {"Texte rasterisé", QT_TRANSLATE_NOOP("ShortcutManager", "Texte rasterisé")},
        {"Rectangle", QT_TRANSLATE_NOOP("ShortcutManager", "Rectangle")},
        {"Ellipse", QT_TRANSLATE_NOOP("ShortcutManager", "Ellipse")},
        {"Ligne", QT_TRANSLATE_NOOP("ShortcutManager", "Ligne")},
        {"Navigation", QT_TRANSLATE_NOOP("ShortcutManager", "Navigation")},
        {"Rotation de l’affichage",
         QT_TRANSLATE_NOOP("ShortcutManager", "Rotation de l’affichage")},
        {"Zoom", QT_TRANSLATE_NOOP("ShortcutManager", "Zoom")},
        {"Densité", QT_TRANSLATE_NOOP("ShortcutManager", "Densité")}};
    return texts.value(_source, nullptr);
}

QString translated(const QString &_source)
{
    const char *text = translatedShortcutText(_source);
    return QCoreApplication::translate("ShortcutManager",
                                       text ? text : _source.toUtf8().constData());
}

QString standard(QKeySequence::StandardKey _key)
{
    const auto bindings = QKeySequence::keyBindings(_key);
    return bindings.isEmpty() ? QString{} : bindings.first().toString(QKeySequence::PortableText);
}

QString primary(const QString &_key, bool _shift = false)
{
    // QKeySequence maps Ctrl to the platform's primary command modifier
    // (Command on macOS, Control on Windows and Linux).
    return QStringLiteral("Ctrl+") + (_shift ? QStringLiteral("Shift+") : QString{}) + _key;
}
} // namespace

ShortcutManager::ShortcutManager(QObject *_parent, bool _persist)
    : QObject(_parent), m_persist(_persist)
{
    m_entries = {
        {"new", "Fichier", "Nouveau document", standard(QKeySequence::New), {}},
        {"open", "Fichier", "Ouvrir", standard(QKeySequence::Open), {}},
        {"import", "Fichier", "Importer comme calque", primary("O", true), {}},
        {"save", "Fichier", "Enregistrer", standard(QKeySequence::Save), {}},
        {"saveAs", "Fichier", "Enregistrer sous", standard(QKeySequence::SaveAs), {}},
        {"export", "Fichier", "Exporter l’image", "Ctrl+Alt+Shift+W", {}},
        {"undo", "Édition", "Annuler", standard(QKeySequence::Undo), {}},
        {"redo", "Édition", "Rétablir", primary("Z", true), {}},
        {"copy", "Édition", "Copier l’image visible", standard(QKeySequence::Copy), {}},
        {"paste", "Édition", "Coller comme calque", standard(QKeySequence::Paste), {}},
        {"preferences", "Édition", "Ouvrir les réglages", standard(QKeySequence::Preferences), {}},
        {"selectAll", "Sélection", "Tout sélectionner", standard(QKeySequence::SelectAll), {}},
        {"deselect", "Sélection", "Désélectionner", primary("D"), {}},
        {"clear",
         "Sélection",
         "Effacer les pixels",
         QKeySequence(Qt::Key_Delete).toString(QKeySequence::PortableText),
         {}},
        {"newLayer", "Calques", "Nouveau calque", primary("N", true), {}},
        {"duplicateLayer", "Calques", "Dupliquer le calque", primary("J"), {}},
        {"zoomFit", "Affichage", "Adapter à la fenêtre", primary("0"), {}},
        {"zoomActual", "Affichage", "Taille réelle", primary("1"), {}},
        {"brushSmaller", "Outils", "Réduire la taille du pinceau", "[", {}},
        {"brushLarger", "Outils", "Augmenter la taille du pinceau", "]", {}},
        {"brushSofter", "Outils", "Réduire la dureté du pinceau", "Shift+[", {}},
        {"brushHarder", "Outils", "Augmenter la dureté du pinceau", "Shift+]", {}},
        {"defaultColors", "Outils", "Couleurs par défaut", "D", {}},
        {"swapColors", "Outils", "Échanger les couleurs", "X", {}},
        {"toolMove", "Outils", "Déplacement", "V", {}},
        {"toolSelect", "Outils", "Sélection rectangulaire", "M", {}},
        {"toolLasso", "Outils", "Lasso", "L", {}},
        {"toolSmartSelection", "Outils", "Sélections intelligentes", "W", {}},
        {"toolHealing", "Outils", "Correction", "J", {}},
        {"toolStamp", "Outils", "Tampons", "S", {}},
        {"toolCrop", "Outils", "Recadrage", "C", {}},
        {"toolBrush", "Outils", "Pinceau", "B", {}},
        {"toolHistoryBrush", "Outils", "Forme d’historique", "Y", {}},
        {"toolEraser", "Outils", "Gomme", "E", {}},
        {"toolFill", "Outils", "Dégradé et remplissage", "G", {}},
        {"toolPicker", "Outils", "Pipette", "I", {}},
        {"toolText", "Outils", "Texte rasterisé", "T", {}},
        {"toolRectangle", "Outils", "Formes", "U", {}},
        {"toolHand", "Outils", "Navigation", "H", {}},
        {"toolRotate", "Outils", "Rotation de l’affichage", "R", {}},
        {"toolZoom", "Outils", "Zoom", "Z", {}},
        {"toolDodge", "Outils", "Densité", "O", {}}};
    for (const auto &definition : adobeShortcuts) {
        m_entries.append({definition.id,
                          definition.category,
                          definition.label,
                          definition.sequence,
                          {},
                          definition.context,
                          definition.implemented,
                          definition.editable,
                          definition.gesture});
    }
    for (auto &entry : m_entries) {
        entry.sequence = normalized(entry.defaultSequence);
        if (entry.id == "duplicateLayer")
            entry.context = QString::fromUtf8(
                QT_TRANSLATE_NOOP("ShortcutManager", "PhotoCrap : duplication du calque complet"));
        if (entry.id == "copy")
            entry.context = QString::fromUtf8(
                QT_TRANSLATE_NOOP("ShortcutManager", "PhotoCrap : copie de l’image visible"));
        if (entry.id == "newLayer")
            entry.context = QString::fromUtf8(QT_TRANSLATE_NOOP(
                "ShortcutManager", "PhotoCrap : création directe, sans dialogue"));
        if (entry.id == "fillDialog" || entry.id == "fillDialogF5")
            entry.context = QString::fromUtf8(QT_TRANSLATE_NOOP(
                "ShortcutManager", "PhotoCrap : sélecteur de couleur, pixels ou masque"));
    }
#ifdef Q_OS_MACOS
    for (auto &entry : m_entries)
        if (entry.id == "actionsPanel")
            entry.sequence = entry.defaultSequence = QStringLiteral("Alt+F9");
#endif
    load();
}

int ShortcutManager::indexOf(const QString &_id) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].id == _id)
            return i;
    return -1;
}

QString ShortcutManager::normalized(const QString &_value)
{
    const QString trimmed = _value.trimmed();
    if (trimmed.isEmpty())
        return {};
    return QKeySequence::fromString(trimmed, QKeySequence::PortableText)
        .toString(QKeySequence::PortableText);
}

void ShortcutManager::load()
{
    if (!m_persist)
        return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    for (auto &entry : m_entries) {
        if (!entry.editable || !entry.implemented || !settings.contains(entry.id))
            continue;
        const QString candidate = normalized(settings.value(entry.id).toString());
        if (!candidate.isEmpty() || settings.value(entry.id).toString().trimmed().isEmpty())
            entry.sequence = candidate;
    }
    // New Adobe defaults must not make a previously saved customization ambiguous.
    QVector<int> order;
    for (int i = 0; i < m_entries.size(); ++i)
        order.append(i);
    std::stable_sort(order.begin(), order.end(), [&](int _a, int _b) {
        return settings.contains(m_entries[_a].id) && !settings.contains(m_entries[_b].id);
    });
    QSet<QString> occupied;
    for (const int index : order) {
        auto &entry = m_entries[index];
        if (!entry.implemented || !entry.editable || entry.sequence.isEmpty())
            continue;
        QStringList keys{entry.sequence};
        const auto  key = QKeySequence::fromString(entry.sequence, QKeySequence::PortableText);
        if (entry.id.startsWith("tool") && key.count() == 1 &&
            !key[0].keyboardModifiers().testFlag(Qt::ShiftModifier)) {
            keys.append(QKeySequence(QKeyCombination(key[0].keyboardModifiers() | Qt::ShiftModifier,
                                                     key[0].key()))
                            .toString(QKeySequence::PortableText));
        }
        bool conflict = false;
        for (const auto &candidate : keys)
            conflict |= occupied.contains(candidate);
        if (conflict) {
            entry.sequence.clear();
            continue;
        }
        for (const auto &candidate : keys)
            occupied.insert(candidate);
    }
}

void ShortcutManager::save(const Entry &_entry)
{
    if (!m_persist)
        return;
    QSettings settings;
    settings.setValue(QStringLiteral("shortcuts/") + _entry.id, _entry.sequence);
}

void ShortcutManager::notifyChanged()
{
    ++m_revision;
    emit shortcutsChanged();
}

QString ShortcutManager::sequence(const QString &_id) const
{
    const int index = indexOf(_id);
    return index < 0 || !m_entries[index].implemented || !m_entries[index].editable
               ? QString{}
               : m_entries[index].sequence;
}

QString ShortcutManager::displaySequence(const QString &_id) const
{
    const int index = indexOf(_id);
    if (index < 0)
        return {};
    if (!m_entries[index].gesture.isEmpty())
        return translated(m_entries[index].gesture);
    if (m_entries[index].sequence.isEmpty())
        return {};
    return QKeySequence::fromString(m_entries[index].sequence, QKeySequence::PortableText)
        .toString(QKeySequence::NativeText);
}

QVariantList ShortcutManager::entries() const
{
    QVariantList result;
    result.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        result.append(QVariantMap{
            {"id", entry.id},
            {"category", translated(entry.category)},
            {"label", translated(entry.label)},
            {"sequence", entry.sequence},
            {"displaySequence", displaySequence(entry.id)},
            {"defaultSequence", entry.defaultSequence},
            {"context", translated(entry.context)},
            {"source", entry.context.startsWith("PhotoCrap") || entry.id == "import" ||
                               entry.id == "preferences"
                           ? "PhotoCrap"
                           : "Adobe"},
            {"implemented", entry.implemented},
            {"editable", entry.editable && entry.implemented},
            {"status", entry.implemented ? (entry.editable ? "available" : "gesture") : "nyi"}});
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant &_a, const QVariant &_b) {
        return _a.toMap()
                   .value("category")
                   .toString()
                   .localeAwareCompare(_b.toMap().value("category").toString()) < 0;
    });
    return result;
}

QVariantList ShortcutManager::filteredEntries(const QString &_query, int _filter) const
{
    QVariantList  result;
    const QString query = _query.trimmed();
    for (const auto &value : entries()) {
        const auto    entry  = value.toMap();
        const QString status = entry.value("status").toString();
        if ((_filter == 1 && status == "nyi") || (_filter == 2 && status != "nyi") ||
            (_filter == 3 && status != "gesture"))
            continue;
        const QString searchable =
            entry.value("label").toString() + " " + entry.value("category").toString() + " " +
            entry.value("context").toString() + " " + entry.value("sequence").toString() + " " +
            entry.value("displaySequence").toString();
        if (query.isEmpty() || searchable.contains(query, Qt::CaseInsensitive))
            result.append(value);
    }
    return result;
}

bool ShortcutManager::setSequence(const QString &_id, const QString &_value)
{
    const int index = indexOf(_id);
    if (index < 0) {
        emit errorOccurred(tr("Commande de raccourci inconnue."));
        return false;
    }
    if (!m_entries[index].editable || !m_entries[index].implemented) {
        emit errorOccurred(tr("Ce raccourci est contextuel ou pas encore implémenté (NYI)."));
        return false;
    }
    const QString candidate = normalized(_value);
    if (!_value.trimmed().isEmpty() && candidate.isEmpty()) {
        emit errorOccurred(tr("Raccourci invalide. Exemple : Ctrl+Shift+E."));
        return false;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i != index && m_entries[i].editable && m_entries[i].implemented &&
            !candidate.isEmpty() && m_entries[i].sequence == candidate) {
            emit errorOccurred(
                tr("« %1 » utilise déjà ce raccourci.").arg(translated(m_entries[i].label)));
            return false;
        }
    }
    const auto key = QKeySequence::fromString(candidate, QKeySequence::PortableText);
    if (!key.isEmpty() && key.count() == 1) {
        for (const auto &entry : m_entries) {
            if (entry.id == _id || !entry.id.startsWith("tool") || !entry.implemented)
                continue;
            const auto toolKey =
                QKeySequence::fromString(entry.sequence, QKeySequence::PortableText);
            if (toolKey.isEmpty() || toolKey.count() != 1)
                continue;
            const auto         combination = toolKey[0];
            const QKeySequence cycle(QKeyCombination(
                combination.keyboardModifiers() | Qt::ShiftModifier, combination.key()));
            if (key == cycle) {
                emit errorOccurred(tr("Ce raccourci est réservé au changement d’outil avec Maj."));
                return false;
            }
        }
        if (_id.startsWith("tool")) {
            const auto         combination = key[0];
            const QKeySequence cycle(QKeyCombination(
                combination.keyboardModifiers() | Qt::ShiftModifier, combination.key()));
            for (const auto &entry : m_entries) {
                if (entry.id != _id && entry.implemented && entry.editable &&
                    cycle == QKeySequence::fromString(entry.sequence, QKeySequence::PortableText)) {
                    emit errorOccurred(
                        tr("Le changement d’outil avec Maj entrerait en conflit avec « %1 ».")
                            .arg(translated(entry.label)));
                    return false;
                }
            }
        }
    }
    if (m_entries[index].sequence == candidate)
        return true;
    m_entries[index].sequence = candidate;
    save(m_entries[index]);
    notifyChanged();
    return true;
}

void ShortcutManager::resetDefaults()
{
    if (m_persist) {
        QSettings settings;
        settings.remove(QStringLiteral("shortcuts"));
    }
    for (auto &entry : m_entries)
        entry.sequence = entry.defaultSequence;
    notifyChanged();
}
