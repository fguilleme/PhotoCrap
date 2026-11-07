#pragma once
#include "AiImageJob.h"
#include "AiSettings.h"
#include <QElapsedTimer>
#include <QPointer>
class Document;

class AiImageController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool pendingResult READ pendingResult NOTIFY changed)
    Q_PROPERTY(QString providerName READ providerName NOTIFY changed)
    Q_PROPERTY(QString modelName READ modelName NOTIFY changed)
    Q_PROPERTY(int variantCount READ variantCount NOTIFY changed)
    Q_PROPERTY(int completedVariants READ completedVariants NOTIFY changed)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY changed)
public:
    AiImageController(Document *_document, AiSettings *_settings, QObject *_parent = nullptr);
    ~AiImageController() override;
    bool busy() const
    {
        return m_busy;
    }
    QString status() const
    {
        return m_status;
    }
    QString error() const
    {
        return m_error;
    }
    QString providerName() const
    {
        return m_runProvider;
    }
    QString modelName() const
    {
        return m_runModel;
    }
    int variantCount() const
    {
        return m_variantCount;
    }
    int completedVariants() const
    {
        return m_completed;
    }
    int elapsedSeconds() const
    {
        return m_elapsedSeconds;
    }
    Q_INVOKABLE bool generate(const QString &_providerId, const QString &_prompt, bool _selection,
                              int _variants = 1);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retryInsertion();
    bool             pendingResult() const
    {
        return !m_resultBytes.isEmpty();
    }
signals:
    void changed();
    void layerAdded();

private:
    QString               variantStatus(const QString &_status) const;
    void                  startVariant();
    void                  tryInsertion();
    void                  stop(const QString &_error = {});
    void                  received(const QByteArray &_bytes, const QString &_error);
    void                  prepareResult();
    QPointer<Document>    m_document;
    AiSettings *          m_settings;
    AiImageJob            m_job;
    PhotoCrap::Ai::Input  m_input;
    PhotoCrap::Ai::Cancel m_cancel;
    QVariantMap           m_provider;
    QString               m_prompt, m_status, m_error, m_key, m_batchId;
    QByteArray            m_resultBytes;
    std::shared_ptr<PhotoCrap::Ai::Result> m_readyResult;
    QTimer                                 m_insertionTimer, m_clock;
    QElapsedTimer                          m_elapsed;
    QString                                m_runProvider, m_runModel;
    int                                    m_elapsedSeconds = 0;
    int                                    m_variantCount = 1, m_completed = 0;
    bool                                   m_busy             = false;
    bool                                   m_awaitingResponse = false;
    quint64                                m_generation       = 0;
};
