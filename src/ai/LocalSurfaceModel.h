#pragma once
#include "SurfaceGeometry.h"
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>

class QNetworkReply;

namespace PhotoCrap::Ai
{
struct LocalSurfaceResult {
    SurfaceGeometry geometry;
    QString         error;
};

class LocalSurfaceModel final : public QObject
{
    Q_OBJECT
public:
    explicit LocalSurfaceModel(QObject *parent = nullptr);
    static bool               available();
    static QString            filePath();
    static LocalSurfaceResult analyze(const QImage &image, const QString &path,
                                      const Cancel &cancel);
    void                      prepare();
    void                      cancel();

signals:
    void status(const QString &message);
    void ready(const QString &path, const QString &error);

private:
    void readDownload();
    void finishDownload(const QString &error);

    QNetworkAccessManager      m_network;
    QPointer<QNetworkReply>    m_reply;
    std::unique_ptr<QSaveFile> m_download;
    QCryptographicHash         m_hash{QCryptographicHash::Sha256};
    qint64                     m_downloaded = 0;
    QString                    m_path;
};
} // namespace PhotoCrap::Ai
