#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include <memory>

// The receiver bounds callback lifetime; native jobs finish independently.
// Inject a memory implementation in tests, never the user's system keychain.
class AiCredentialStore
{
public:
    using Callback               = std::function<void(const QString &value, const QString &error)>;
    virtual ~AiCredentialStore() = default;
    virtual void read(const QString &_id, QObject *_receiver, Callback _finished)   = 0;
    virtual void write(const QString &_id, const QString &_secret, QObject *_receiver,
                       Callback _finished)                                          = 0;
    virtual void remove(const QString &_id, QObject *_receiver, Callback _finished) = 0;
};
std::shared_ptr<AiCredentialStore> nativeAiCredentialStore();
std::shared_ptr<AiCredentialStore> memoryAiCredentialStore();
