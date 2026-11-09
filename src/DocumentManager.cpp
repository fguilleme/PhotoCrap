#include "DocumentManager.h"
#include <QFileInfo>
#include <QQmlEngine>
#include <QTimer>

namespace
{
QString fileIdentity(const QUrl &url)
{
    const QFileInfo info(url.toLocalFile());
    return info.canonicalFilePath().isEmpty() ? info.absoluteFilePath() : info.canonicalFilePath();
}
} // namespace
DocumentManager::DocumentManager(AiSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    m_entries.append(makeEntry(true));
}
DocumentManager::Entry DocumentManager::makeEntry(bool placeholder)
{
    Entry entry;
    entry.document = new Document(this);
    entry.ai       = new AiImageController(entry.document, m_settings, entry.document);
    entry.ai->setDocumentManager(this);
    entry.placeholder = placeholder;
    QQmlEngine::setObjectOwnership(entry.document, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(entry.ai, QQmlEngine::CppOwnership);
    const auto refresh = [this] { emit documentsChanged(); };
    connect(entry.document, &Document::documentChanged, this, [this, doc = entry.document] {
        for (auto &entry : m_entries)
            if (entry.document == doc)
                entry.placeholder = false;
        emit documentsChanged();
    });
    connect(entry.document, &Document::historyChanged, this, [this, doc = entry.document] {
        if (doc->modified())
            for (auto &entry : m_entries)
                if (entry.document == doc)
                    entry.placeholder = false;
        emit documentsChanged();
    });
    const auto loadingChanged = [this, doc = entry.document] {
        emit documentsChanged();
        if (!doc->operationBusy() && !doc->rawLoading())
            QTimer::singleShot(0, this, [this, doc = QPointer<Document>(doc)] {
                if (doc)
                    finishOpening(doc);
            });
    };
    connect(entry.document, &Document::operationChanged, this, loadingChanged);
    connect(entry.document, &Document::rawLoadingChanged, this, loadingChanged);
    connect(entry.ai, &AiImageController::changed, this, refresh);
    const auto opened = [this, doc = entry.document](const QUrl &url) {
        for (auto &entry : m_entries)
            if (entry.document == doc) {
                entry.openedUrl   = url;
                entry.placeholder = false;
                entry.opening     = false;
            }
        emit documentsChanged();
    };
    connect(entry.document, &Document::fileOpened, this, opened);
    connect(entry.document, &Document::rawFileOpened, this, opened);
    connect(entry.document, &Document::projectSaved, this, opened);
    connect(entry.document, &Document::errorOccurred, this,
            [this, doc = entry.document](const QString &error) {
                if (doc != activeDocument())
                    emit errorOccurred(error);
            });
    emit documentAdded(entry.document);
    return entry;
}
Document *DocumentManager::activeDocument() const
{
    return m_entries[m_active].document;
}
AiImageController *DocumentManager::activeAi() const
{
    return m_entries[m_active].ai;
}
QString DocumentManager::activeId() const
{
    return activeDocument()->documentId();
}
QVariantMap DocumentManager::activeView() const
{
    return m_entries[m_active].view;
}
int DocumentManager::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].document->documentId() == id)
            return i;
    return -1;
}
Document *DocumentManager::document(const QString &id) const
{
    const auto index = indexOf(id);
    return index < 0 ? nullptr : m_entries[index].document;
}
QVariantList DocumentManager::documents() const
{
    QVariantList list;
    for (const auto &entry : m_entries)
        list.append(
            QVariantMap{{"id", entry.document->documentId()},
                        {"name", entry.document->name()},
                        {"modified", entry.document->modified()},
                        {"busy", entry.document->pixelEditBusy() || entry.document->rawLoading()},
                        {"aiBusy", entry.ai->busy()},
                        {"pendingResult", entry.ai->pendingResult()}});
    return list;
}
bool DocumentManager::activate(const QString &id)
{
    const auto index = indexOf(id);
    if (index < 0)
        return false;
    if (index == m_active)
        return true;
    emit activeAboutToChange();
    activeDocument()->endStroke();
    activeDocument()->finishTextEdit();
    m_active = index;
    emit activeChanged();
    return true;
}
void DocumentManager::saveView(const QVariantMap &view)
{
    m_entries[m_active].view = view;
}
bool DocumentManager::reusablePlaceholder() const
{
    return m_entries[m_active].placeholder && !activeDocument()->modified() &&
           !activeDocument()->pixelEditBusy() && !activeDocument()->rawLoading() &&
           !activeAi()->busy() && !activeAi()->pendingResult();
}
void DocumentManager::append(Entry entry, bool activate)
{
    if (activate) {
        emit activeAboutToChange();
        activeDocument()->endStroke();
        activeDocument()->finishTextEdit();
    }
    m_entries.append(std::move(entry));
    if (activate)
        m_active = m_entries.size() - 1;
    emit documentsChanged();
    if (activate)
        emit activeChanged();
}
bool DocumentManager::newDocument(int width, int height, bool transparent, int bits)
{
    if (reusablePlaceholder()) {
        const auto ok = activeDocument()->newDocument(width, height, transparent, bits);
        if (ok)
            m_entries[m_active].placeholder = false;
        return ok;
    }
    auto entry = makeEntry();
    if (!entry.document->newDocument(width, height, transparent, bits)) {
        entry.document->deleteLater();
        return false;
    }
    append(std::move(entry));
    return true;
}
bool DocumentManager::openFile(const QUrl &url)
{
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) {
        emit errorOccurred(tr("Le fichier demandé est introuvable."));
        return false;
    }
    const auto path = fileIdentity(url);
    for (int i = 0; i < m_entries.size(); ++i)
        if (!m_entries[i].openedUrl.isEmpty() && fileIdentity(m_entries[i].openedUrl) == path)
            return activate(m_entries[i].document->documentId());
    if (!reusablePlaceholder())
        append(makeEntry());
    m_entries[m_active].placeholder = false;
    m_entries[m_active].openedUrl   = url;
    m_entries[m_active].opening     = true;
    activeDocument()->requestOpenFile(url);
    emit documentsChanged();
    return true;
}
void DocumentManager::finishOpening(Document *doc)
{
    const int index = indexOf(doc->documentId());
    if (index < 0 || !m_entries[index].opening || doc->operationBusy() || doc->rawLoading())
        return;
    // Successful loads clear opening before this queued check. Failed or cancelled
    // loads must not leave a blank tab cached under the requested file's name.
    m_entries[index].opening   = false;
    m_entries[index].openedUrl = QUrl{};
    if (!doc->modified())
        closeDocument(doc->documentId(), true);
    emit documentsChanged();
}
bool DocumentManager::canOpenFiles(const QList<QUrl> &urls) const
{
    if (urls.isEmpty())
        return false;
    for (const auto &url : urls) {
        const QFileInfo file(url.toLocalFile());
        if (!url.isLocalFile() || !file.isFile() ||
            (file.suffix().compare("pcr", Qt::CaseInsensitive) != 0 &&
             !activeDocument()->canImportLayerFiles({url})))
            return false;
    }
    return true;
}
bool DocumentManager::closeDocument(const QString &id, bool discard)
{
    const int index = indexOf(id);
    if (index < 0)
        return false;
    const auto entry     = m_entries[index];
    const bool wasActive = index == m_active;
    if (!discard && (entry.document->modified() || entry.ai->busy() || entry.ai->pendingResult()))
        return false;
    // Never destroy a document while an atomic save is in progress.
    if (entry.document->operationBusy())
        return false;
    if (wasActive)
        emit activeAboutToChange();
    if (m_entries.size() == 1)
        m_entries.append(makeEntry(true));
    m_entries.removeAt(index);
    if (index < m_active)
        --m_active;
    else if (index == m_active)
        m_active = qMin(index, int(m_entries.size()) - 1);
    entry.ai->cancel();
    emit documentsChanged();
    if (wasActive)
        emit activeChanged();
    entry.document->deleteLater();
    return true;
}
bool DocumentManager::addAiDocument(const PhotoCrap::Ai::Result &result,
                                    const QVariantMap &metadata, const PhotoCrap::Ai::Input &source,
                                    QString &error, Document *origin)
{
    auto entry = makeEntry();
    if (!entry.document->adoptAiDocument(result, metadata, source, error)) {
        entry.document->deleteLater();
        return false;
    }
    const bool focus = (!origin || origin == activeDocument()) && !m_activationBlocked &&
                       !activeDocument()->aiInsertionBusy();
    append(std::move(entry), focus);
    return true;
}
