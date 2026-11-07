#include "AiCredentialStore.h"
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <qtkeychain/keychain.h>
#include <type_traits>

namespace
{
class NativeStore final : public AiCredentialStore
{
    template <class Job>
    static void run(Job *_job, QObject *_receiver, Callback _finished)
    {
        _job->setInsecureFallback(false);
        QObject::connect(
            _job, &QKeychain::Job::finished, _receiver, [_job, _finished = std::move(_finished)] {
                const auto error = _job->error();
                QString    data;
                if constexpr (std::is_same_v<Job, QKeychain::ReadPasswordJob>)
                    if (!error)
                        data = _job->textData();
                _finished(data, error == QKeychain::NoError || error == QKeychain::EntryNotFound
                                    ? QString{}
                                    : _job->errorString());
            });
        _job->start();
    }

public:
    void read(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        auto *job = new QKeychain::ReadPasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        run(job, _receiver, std::move(_finished));
    }
    void write(const QString &_id, const QString &_secret, QObject *_receiver,
               Callback _finished) override
    {
        auto *job = new QKeychain::WritePasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        job->setTextData(_secret);
        run(job, _receiver, std::move(_finished));
    }
    void remove(const QString &_id, QObject *_receiver, Callback _finished) override
    {
        auto *job = new QKeychain::DeletePasswordJob("PhotoCrap.AI");
        job->setKey(_id);
        run(job, _receiver, std::move(_finished));
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
