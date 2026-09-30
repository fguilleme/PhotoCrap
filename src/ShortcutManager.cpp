#include "ShortcutManager.h"

#include <QGuiApplication>
#include <QKeySequence>
#include <QSettings>

namespace {
QString standard(QKeySequence::StandardKey key) {
    const auto bindings = QKeySequence::keyBindings(key);
    return bindings.isEmpty() ? QString{} : bindings.first().toString(QKeySequence::PortableText);
}

QString primary(const QString &key, bool shift = false) {
    // QKeySequence maps Ctrl to the platform's primary command modifier
    // (Command on macOS, Control on Windows and Linux).
    return QStringLiteral("Ctrl+") + (shift ? QStringLiteral("Shift+") : QString{}) + key;
}
}

ShortcutManager::ShortcutManager(QObject *parent, bool persist)
    : QObject(parent), m_persist(persist) {
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
        {"clear", "Sélection", "Effacer les pixels", QKeySequence(Qt::Key_Delete).toString(QKeySequence::PortableText), {}},
        {"newLayer", "Calques", "Nouveau calque", primary("N", true), {}},
        {"duplicateLayer", "Calques", "Dupliquer le calque", primary("J"), {}},
        {"zoomFit", "Affichage", "Adapter à la fenêtre", primary("0"), {}},
        {"zoomActual", "Affichage", "Taille réelle", primary("1"), {}},
        {"brushSmaller", "Outils", "Réduire la taille du pinceau", "[", {}},
        {"brushLarger", "Outils", "Augmenter la taille du pinceau", "]", {}},
        {"toolMove", "Outils", "Déplacement", "V", {}},
        {"toolSelect", "Outils", "Sélection rectangulaire", "M", {}},
        {"toolCrop", "Outils", "Recadrage", "C", {}},
        {"toolBrush", "Outils", "Pinceau", "B", {}},
        {"toolEraser", "Outils", "Gomme", "E", {}},
        {"toolFill", "Outils", "Remplissage contigu", "G", {}},
        {"toolGradient", "Outils", "Dégradé", "D", {}},
        {"toolPicker", "Outils", "Pipette", "I", {}},
        {"toolText", "Outils", "Texte rasterisé", "T", {}},
        {"toolRectangle", "Outils", "Rectangle", "U", {}},
        {"toolEllipse", "Outils", "Ellipse", "O", {}},
        {"toolLine", "Outils", "Ligne", "L", {}},
        {"toolHand", "Outils", "Navigation", "H", {}}
    };
    for (auto &entry : m_entries)
        entry.sequence = entry.defaultSequence;
    load();
}

int ShortcutManager::indexOf(const QString &id) const {
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].id == id) return i;
    return -1;
}

QString ShortcutManager::normalized(const QString &value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) return {};
    return QKeySequence::fromString(trimmed, QKeySequence::PortableText)
        .toString(QKeySequence::PortableText);
}

void ShortcutManager::load() {
    if (!m_persist) return;
    QSettings settings;
    settings.beginGroup(QStringLiteral("shortcuts"));
    for (auto &entry : m_entries) {
        if (!settings.contains(entry.id)) continue;
        const QString candidate = normalized(settings.value(entry.id).toString());
        if (!candidate.isEmpty() || settings.value(entry.id).toString().trimmed().isEmpty())
            entry.sequence = candidate;
    }
}

void ShortcutManager::save(const Entry &entry) {
    if (!m_persist) return;
    QSettings settings;
    settings.setValue(QStringLiteral("shortcuts/") + entry.id, entry.sequence);
}

void ShortcutManager::notifyChanged() {
    ++m_revision;
    emit shortcutsChanged();
}

QString ShortcutManager::sequence(const QString &id) const {
    const int index = indexOf(id);
    return index < 0 ? QString{} : m_entries[index].sequence;
}

QString ShortcutManager::displaySequence(const QString &id) const {
    const int index = indexOf(id);
    if (index < 0 || m_entries[index].sequence.isEmpty()) return {};
    return QKeySequence::fromString(m_entries[index].sequence, QKeySequence::PortableText)
        .toString(QKeySequence::NativeText);
}

QVariantList ShortcutManager::entries() const {
    QVariantList result;
    result.reserve(m_entries.size());
    for (const auto &entry : m_entries) {
        result.append(QVariantMap{
            {"id", entry.id}, {"category", entry.category}, {"label", entry.label},
            {"sequence", entry.sequence}, {"displaySequence", displaySequence(entry.id)},
            {"defaultSequence", entry.defaultSequence}
        });
    }
    return result;
}

bool ShortcutManager::setSequence(const QString &id, const QString &value) {
    const int index = indexOf(id);
    if (index < 0) {
        emit errorOccurred(QStringLiteral("Commande de raccourci inconnue."));
        return false;
    }
    const QString candidate = normalized(value);
    if (!value.trimmed().isEmpty() && candidate.isEmpty()) {
        emit errorOccurred(QStringLiteral("Raccourci invalide. Exemple : Ctrl+Shift+E."));
        return false;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i != index && !candidate.isEmpty() && m_entries[i].sequence == candidate) {
            emit errorOccurred(QStringLiteral("« %1 » utilise déjà ce raccourci.").arg(m_entries[i].label));
            return false;
        }
    }
    if (m_entries[index].sequence == candidate) return true;
    m_entries[index].sequence = candidate;
    save(m_entries[index]);
    notifyChanged();
    return true;
}

void ShortcutManager::resetDefaults() {
    if (m_persist) {
        QSettings settings;
        settings.remove(QStringLiteral("shortcuts"));
    }
    for (auto &entry : m_entries)
        entry.sequence = entry.defaultSequence;
    notifyChanged();
}
