#include "ShortcutManager.h"

#include <QGuiApplication>
#include <QHash>
#include <QKeySequence>
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
    return text ? QCoreApplication::translate("ShortcutManager", text) : _source;
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
        {"export", "Fichier", "Exporter l’image", primary("E", true), {}},
        {"undo", "Édition", "Annuler", standard(QKeySequence::Undo), {}},
        {"redo", "Édition", "Rétablir", standard(QKeySequence::Redo), {}},
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
        {"toolCrop", "Outils", "Recadrage", "C", {}},
        {"toolBrush", "Outils", "Pinceau", "B", {}},
        {"toolEraser", "Outils", "Gomme", "E", {}},
        {"toolFill", "Outils", "Dégradé et remplissage", "G", {}},
        {"toolPicker", "Outils", "Pipette", "I", {}},
        {"toolText", "Outils", "Texte rasterisé", "T", {}},
        {"toolRectangle", "Outils", "Formes", "U", {}},
        {"toolHand", "Outils", "Navigation", "H", {}},
        {"toolRotate", "Outils", "Rotation de l’affichage", "R", {}},
        {"toolZoom", "Outils", "Zoom", "Z", {}},
        {"toolDodge", "Outils", "Densité", "O", {}}};
    for (auto &entry : m_entries)
        entry.sequence = entry.defaultSequence;
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
        if (!settings.contains(entry.id))
            continue;
        const QString candidate = normalized(settings.value(entry.id).toString());
        if (!candidate.isEmpty() || settings.value(entry.id).toString().trimmed().isEmpty())
            entry.sequence = candidate;
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
    return index < 0 ? QString{} : m_entries[index].sequence;
}

QString ShortcutManager::displaySequence(const QString &_id) const
{
    const int index = indexOf(_id);
    if (index < 0 || m_entries[index].sequence.isEmpty())
        return {};
    return QKeySequence::fromString(m_entries[index].sequence, QKeySequence::PortableText)
        .toString(QKeySequence::NativeText);
}

QVariantList ShortcutManager::entries() const
{
    QVariantList result;
    result.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        result.append(QVariantMap{{"id", entry.id},
                                  {"category", translated(entry.category)},
                                  {"label", translated(entry.label)},
                                  {"sequence", entry.sequence},
                                  {"displaySequence", displaySequence(entry.id)},
                                  {"defaultSequence", entry.defaultSequence}});
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
    const QString candidate = normalized(_value);
    if (!_value.trimmed().isEmpty() && candidate.isEmpty()) {
        emit errorOccurred(tr("Raccourci invalide. Exemple : Ctrl+Shift+E."));
        return false;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i != index && !candidate.isEmpty() && m_entries[i].sequence == candidate) {
            emit errorOccurred(
                tr("« %1 » utilise déjà ce raccourci.").arg(translated(m_entries[i].label)));
            return false;
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
