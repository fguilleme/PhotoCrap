#include "AiCredentialStore.h"
#include <QHash>
#include <QOperatingSystemVersion>
#include <QTimer>
#include <qtkeychain/keychain.h>
#include <utility>

namespace
{
class NativeStore final : public AiCredentialStore
{
    template <class Finished>
    static void run(QKeychain::Job *_job, QObject *_receiver, Finished _finished)
    {
        _job->setInsecureFallback(false);
        QObject::connect(_job, &QKeychain::Job::finished, _receiver, std::move(_finished));
        _job->start();
    }
    static QString errorText(const QKeychain::Job *_job, bool _missingAllowed)
    {
        const auto error = _job->error();
        return error == QKeychain::NoError || (_missingAllowed && error == QKeychain::EntryNotFound)
                   ? QString{}
                   : _job->errorString();
    }
    static void readPassword(const QString &_id, QObject *_receiver, Callback _finished,
                             bool _retry)
    {
        auto *job = new QKeychain::ReadPasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        run(job, _receiver, [job, _receiver, _finished = std::move(_finished), _retry] {
            // QtKeychain groups several macOS errors under AccessDenied.
            // Retry only errSecAuthFailed, which it reports with this suffix.
            if (_retry && QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS &&
                job->error() == QKeychain::AccessDenied &&
                job->errorString().endsWith(QKeychain::Job::tr(
                    "The user name or passphrase you entered is not correct"))) {
                const auto account = job->key();
                QTimer::singleShot(250, _receiver, [account, _receiver, _finished] {
                    readPassword(account, _receiver, _finished, false);
                });
                return;
            }
            _finished(job->error() == QKeychain::NoError ? job->textData() : QString{},
                      errorText(job, true));
        });
    }

public:
    void read(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        readPassword(_id, _receiver, std::move(_finished), true);
    }
    void write(const QString &_id, const QString &_secret, QObject *_receiver,
               Callback _finished) override
    {
        auto *job = new QKeychain::WritePasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        job->setTextData(_secret);
        run(job, _receiver, [job, _finished = std::move(_finished)] {
            _finished({}, errorText(job, false));
        });
    }
    void remove(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        auto *job = new QKeychain::DeletePasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        run(job, _receiver, [job, _finished = std::move(_finished)] {
            _finished({}, errorText(job, true));
        });
    }
};
class MemoryStore final : public AiCredentialStore
{
    QHash<QString, QString> m_values;

public:
    void read(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        const auto value = m_values.value(_id);
        QTimer::singleShot(0, _receiver, [value, _finished] { _finished(value, {}); });
    }
    void write(const QString &_id, const QString &_secret, QObject *_receiver,
               Callback _finished) override
    {
        m_values[_id] = _secret;
        QTimer::singleShot(0, _receiver, [_finished] { _finished({}, {}); });
    }
    void remove(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        m_values.remove(_id);
        QTimer::singleShot(0, _receiver, [_finished] { _finished({}, {}); });
    }
};
} // namespace
std::shared_ptr<AiCredentialStore> nativeAiCredentialStore()
{
    return std::make_shared<NativeStore>();
}
std::shared_ptr<AiCredentialStore> memoryAiCredentialStore()
{
    return std::make_shared<MemoryStore>();
}
