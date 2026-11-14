#pragma once
#include "AiCredentialStore.h"
#include <QHash>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class AiSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList providers READ providers NOTIFY changed)
    Q_PROPERTY(QVariantList surfaceProviders READ surfaceProviders NOTIFY changed)
    Q_PROPERTY(QString surfaceProviderId READ surfaceProviderId NOTIFY changed)
    Q_PROPERTY(QVariantList prompts READ prompts NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool credentialsBusy READ credentialsBusy NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    explicit AiSettings(QObject *_parent = nullptr, bool _persist = true,
                        std::shared_ptr<AiCredentialStore> _credentials = {});
    bool credentialsBusy() const
    {
        return m_credentialJobs > 0;
    }
    int revision() const
    {
        return m_revision;
    }
    QVariantList providers() const
    {
        return m_providers;
    }
    QVariantList prompts() const
    {
        return m_prompts;
    }
    QString error() const
    {
        return m_error;
    }
    QVariantMap             provider(const QString &_id) const;
    QVariantList            surfaceProviders() const;
    QString                 surfaceProviderId() const;
    void                    rememberSurfaceProvider(const QString &providerId);
    Q_INVOKABLE QString     saveProvider(QVariantMap _provider);
    Q_INVOKABLE bool        removeProvider(const QString &_id);
    Q_INVOKABLE QString     savePrompt(QVariantMap _prompt);
    Q_INVOKABLE bool        removePrompt(const QString &_id);
    Q_INVOKABLE QVariantMap newProvider(const QString &_kind) const;
    Q_INVOKABLE QString     readWorkflow(const QUrl &_url);
    void                    setSessionKey(const QString &_id, const QString &_key);
    Q_INVOKABLE void        saveKey(const QString &_id, const QString &_key);
    Q_INVOKABLE void        removeKey(const QString &_id);
    Q_INVOKABLE QString     keySource(const QString &_id) const;
    Q_INVOKABLE QString     sharedKeyName(const QString &_id) const;
    Q_INVOKABLE bool        hasKey(const QString &_id) const;
    QString                 key(const QString &_id) const;
    void resolveKey(const QString &_id, QObject *_receiver, AiCredentialStore::Callback _finished);
    static bool             validEndpoint(const QUrl &_url);
    static QVariantMap      normalizedProvider(QVariantMap _provider);
    static QString          gradioOptionsError(const QVariantMap &_options);
    static QString          defaultWorkflow();
signals:
    void changed();
    void keySaved(const QString &_id, bool _success);

private:
    QString                            ownKey(const QString &_id) const;
    QString                            sharedKeyProvider(const QString &_id) const;
    void                               resolveOwnKey(const QString &_id, QObject *_receiver,
                                                     AiCredentialStore::Callback _finished);
    void                               notifyChanged();
    void                               storedKeyHint(const QString &_account, bool _stored);
    void                               eraseCredential(const QString &_account);
    QString                            credentialAccount(const QString &_id) const;
    bool                               reject(const QString &_error);
    void                               persist();
    QVariantList                       m_providers, m_prompts;
    QHash<QString, QString>            m_keys;
    QSet<QString>                      m_resolvedAccounts, m_storedAccounts;
    QHash<QString, quint64>            m_keyVersions;
    QHash<QString, QList<AiCredentialStore::Callback>> m_keyRequests;
    QString                            m_error;
    QString                                            m_surfaceProviderId;
    bool                               m_persist;
    std::shared_ptr<AiCredentialStore> m_credentials;
    int                                m_credentialJobs = 0, m_revision = 0;
};
