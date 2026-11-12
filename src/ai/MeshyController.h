#pragma once
#include <QNetworkAccessManager>
#include <QPointer>
#include <QRectF>
#include <QTimer>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <memory>

class AiSettings;
class Document;
class QTemporaryFile;

class MeshyController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool canResume READ canResume NOTIFY changed)
    Q_PROPERTY(QVariantMap resultInfo READ resultInfo NOTIFY changed)
public:
    MeshyController(Document *_document, AiSettings *_settings,
                    const QUrl &_endpoint = QUrl(QStringLiteral("https://api.meshy.ai")));
    ~MeshyController() override;

    bool busy() const
    {
        return m_busy;
    }
    int progress() const
    {
        return m_progress;
    }
    QString status() const
    {
        return m_status;
    }
    QString error() const
    {
        return m_error;
    }
    QVariantMap resultInfo() const
    {
        return m_resultInfo;
    }
    Q_INVOKABLE bool generate(int _source, const QString &_prompt, const QString &_key,
                              bool _remember, bool _ultra = false, int _targetTriangles = 0);
    Q_INVOKABLE bool recover(const QString &_task, bool _text, const QString &_key, bool _remember,
                             int _targetTriangles = 0);
    Q_INVOKABLE void resume();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void dismiss();
    bool             canResume() const;

signals:
    void changed();

private:
    enum class Stage { TextPreview, TextRefine, Image };
    enum class RequestKind { Submit, Poll, Download };
    using Response = std::function<void(QByteArray, int _retryDelay)>;
    void start(Stage _stage, const QString &_name, const QString &_status, int _triangleTarget);
    void withKey(const QString &_key, bool _remember, std::function<void()> _ready);
    void followTask();
    void rememberKey();
    void prepare(bool _selection, QJsonObject _options);
    void submit(const QJsonObject &_body);
    void poll();
    void download(const QUrl &_url);
    void insert();
    void completeImport();
    void request(const QUrl &_url, RequestKind _kind, const QByteArray &_body, qsizetype _limit,
                 Response _response, std::shared_ptr<QTemporaryFile> _file = {});
    void finish(const QString &_status, const QString &_error = {}, bool _resumable = false);
    bool currentDocument() const;
    QUrl taskUrl() const;

    Document *                        m_document;
    AiSettings *                      m_settings;
    const QUrl                        m_endpoint;
    QNetworkAccessManager             m_network;
    QPointer<QNetworkReply>           m_reply;
    QTimer                            m_next, m_deadline;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QByteArray                        m_key;
    std::shared_ptr<QTemporaryFile>   m_model;
    QRectF                            m_reference;
    QString                           m_task, m_name, m_status, m_error, m_importError;
    QVariantMap                       m_resultInfo;
    quint64                           m_session  = 0;
    int                               m_progress = 0, m_layerCount = 0, m_triangleTarget = 0;
    Stage                             m_stage = Stage::TextPreview;
    bool                              m_busy = false, m_importing = false, m_remember = false;
};
