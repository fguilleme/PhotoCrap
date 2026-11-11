#pragma once
#include "Document.h"
#include "ai/AiImageController.h"
#include "ai/MeshyController.h"
#include <QPointer>

// Documents and their asynchronous jobs keep their identity when tabs change.
class DocumentManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Document *activeDocument READ activeDocument NOTIFY activeChanged)
    Q_PROPERTY(AiImageController *activeAi READ activeAi NOTIFY activeChanged)
    Q_PROPERTY(MeshyController *activeMeshy READ activeMeshy NOTIFY activeChanged)
    Q_PROPERTY(QVariantList documents READ documents NOTIFY documentsChanged)
    Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY documentsChanged)
    Q_PROPERTY(QVariantMap activeView READ activeView NOTIFY activeChanged)
    Q_PROPERTY(bool activationBlocked MEMBER m_activationBlocked)
public:
    explicit DocumentManager(AiSettings *settings, QObject *parent = nullptr);
    Document          *activeDocument() const;
    AiImageController *activeAi() const;
    MeshyController   *activeMeshy() const;
    QString            activeId() const;
    int                count() const
    {
        return m_entries.size();
    }
    QVariantList     documents() const;
    QVariantMap      activeView() const;
    Document        *document(const QString &id) const;
    Q_INVOKABLE bool activate(const QString &id);
    Q_INVOKABLE bool newDocument(int width, int height, bool transparent, int bits = 8);
    Q_INVOKABLE bool openFile(const QUrl &url);
    Q_INVOKABLE bool canOpenFiles(const QList<QUrl> &urls) const;
    Q_INVOKABLE bool closeDocument(const QString &id, bool discard = false);
    Q_INVOKABLE void saveView(const QVariantMap &view);
    bool             addAiDocument(const PhotoCrap::Ai::Result &result, const QVariantMap &metadata,
                                   const PhotoCrap::Ai::Input &source, QString &error,
                                   Document *origin = nullptr);
signals:
    void activeAboutToChange();
    void activeChanged();
    void documentsChanged();
    void documentAdded(Document *document);
    void errorOccurred(QString message);

private:
    struct Entry {
        Document          *document = nullptr;
        AiImageController *ai       = nullptr;
        MeshyController   *meshy    = nullptr;
        QUrl               openedUrl;
        QVariantMap        view;
        bool               placeholder = false;
        bool               opening     = false;
    };
    Entry          makeEntry(bool placeholder = false);
    void           append(Entry entry, bool activate = true);
    int            indexOf(const QString &id) const;
    bool           reusablePlaceholder() const;
    void           finishOpening(Document *doc);
    AiSettings    *m_settings;
    QVector<Entry> m_entries;
    int            m_active            = 0;
    bool           m_activationBlocked = false;
};
