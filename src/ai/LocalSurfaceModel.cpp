#include "LocalSurfaceModel.h"
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QNetworkReply>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QThread>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#ifdef PHOTOCRAP_LOCAL_SURFACE
#include <onnxruntime_cxx_api.h>
#endif

namespace PhotoCrap::Ai
{
namespace
{
constexpr qint64 modelBytes = 140852051;
const auto       modelHash =
    QByteArray::fromHex("24eacb5dc7a2c54c7bc98f7de085ffbed79ad006ea5b664c2c2cdc02ff3a52f0");

#ifdef PHOTOCRAP_LOCAL_SURFACE
struct ModelSession {
    QMutex                        mutex;
    Ort::Env                      environment{ORT_LOGGING_LEVEL_WARNING, "PhotoCrap/MoGe-2"};
    std::unique_ptr<Ort::Session> session;
    QString                       path;
};

bool checkModel(const QString &path, const Cancel &cancel)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != modelBytes)
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd() && !cancel->load()) {
        const QByteArray block = file.read(1024 * 1024);
        if (block.isEmpty())
            return false;
        hash.addData(block);
    }
    return !cancel->load() && hash.result() == modelHash;
}

bool validPoint(const float *point, float mask)
{
    return mask > 0.5f && std::isfinite(point[0]) && std::isfinite(point[1]) &&
           std::isfinite(point[2]);
}

double depthShift(const float *points, const float *mask, const QSize &size)
{
    // The affine point map has an unknown offset on Z. Fit
    // focal * (X,Y) - shift * (u,v) = Z * (u,v) in normalized camera coordinates.
    double       xx = 0., uu = 0., xu = 0., xuz = 0., uuz = 0.;
    const double diagonal = std::hypot(size.width(), size.height());
    const int    columns = qMin(64, size.width()), rows = qMin(64, size.height());
    for (int j = 0; j < rows; ++j) {
        const int    y = (2 * j + 1) * size.height() / (2 * rows);
        const double v = (2. * y + 1. - size.height()) / diagonal;
        for (int i = 0; i < columns; ++i) {
            const int    x     = (2 * i + 1) * size.width() / (2 * columns);
            const double u     = (2. * x + 1. - size.width()) / diagonal;
            const int    index = y * size.width() + x;
            const float *p     = points + 3 * index;
            if (!validPoint(p, mask[index]))
                continue;
            const double uv2 = u * u + v * v;
            const double dot = p[0] * u + p[1] * v;
            xx += double(p[0]) * p[0] + double(p[1]) * p[1];
            uu += uv2;
            xu += dot;
            xuz += dot * p[2];
            uuz += uv2 * p[2];
        }
    }
    const double determinant = xx * uu - xu * xu;
    if (determinant <= xx * uu * 1e-8)
        return 0.;
    const double focal = (uu * xuz - xu * uuz) / determinant;
    const double shift = (xu * xuz - xx * uuz) / determinant;
    return std::isfinite(shift) && std::isfinite(focal) && focal > 0. ? shift : 0.;
}

SurfaceGeometry geometryFromTensors(const QImage &image, const float *points, const float *normals,
                                    const float *mask, const Cancel &cancel)
{
    const auto          size  = image.size();
    const double        shift = depthShift(points, mask, size);
    std::vector<double> range;
    range.reserve(size.width() * size.height());
    SurfaceGeometry result{QImage(size, QImage::Format_Grayscale8),
                           QImage(size, QImage::Format_RGB888),
                           QImage(size, QImage::Format_Grayscale8)};
    if (!result.isValid())
        return {};
    for (int y = 0; y < size.height() && !cancel->load(); ++y) {
        const auto *rgba     = image.constScanLine(y);
        auto       *validity = result.validity.scanLine(y);
        for (int x = 0; x < size.width(); ++x) {
            const int    i     = y * size.width() + x;
            const auto  *point = points + 3 * i;
            const auto  *n     = normals + 3 * i;
            const double z     = point[2] + shift;
            const bool   valid = rgba[4 * x + 3] && z > 0. && validPoint(point, mask[i]) &&
                               std::isfinite(n[0]) && std::isfinite(n[1]) && std::isfinite(n[2]);
            validity[x] = valid ? 255 : 0;
            if (valid)
                range.push_back(1. / qMax(1e-6, z));
        }
    }
    if (range.empty() || cancel->load())
        return {};
    const auto percentile = [&](double f) {
        auto position = range.begin() + size_t(f * (range.size() - 1));
        std::nth_element(range.begin(), position, range.end());
        return *position;
    };
    const double low = percentile(0.001), high = percentile(0.999);
    const double span         = qMax(1e-6, high - low);
    const auto   encodeNormal = [](float n) {
        return uchar(qRound(127.5f * (1.f + qBound(-1.f, n, 1.f))));
    };
    for (int y = 0; y < size.height() && !cancel->load(); ++y) {
        const auto *validity = result.validity.constScanLine(y);
        auto       *depth    = result.disparity.scanLine(y);
        auto       *normal   = result.normals.scanLine(y);
        for (int x = 0; x < size.width(); ++x) {
            const int    i     = y * size.width() + x;
            const auto  *n     = normals + 3 * i;
            const double value = validity[x] ? 1. / qMax(1e-6, points[3 * i + 2] + shift) : low;
            depth[x]           = uchar(qRound(255. * qBound(0., (value - low) / span, 1.)));
            // MoGe uses X right, Y down, Z away from the camera.
            normal[3 * x]     = validity[x] ? encodeNormal(n[0]) : 128;
            normal[3 * x + 1] = validity[x] ? encodeNormal(-n[1]) : 128;
            normal[3 * x + 2] = validity[x] ? encodeNormal(-n[2]) : 128;
        }
    }
    return cancel->load() ? SurfaceGeometry{} : result;
}
#endif
} // namespace

LocalSurfaceModel::LocalSurfaceModel(QObject *parent) : QObject(parent)
{
}

bool LocalSurfaceModel::available()
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    return true;
#else
    return false;
#endif
}

QString LocalSurfaceModel::filePath()
{
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (directory.isEmpty())
        return {};
    return QDir(directory).filePath(QStringLiteral("models/moge-2-vits-24eacb5d.onnx"));
}

void LocalSurfaceModel::prepare()
{
    cancel();
    m_path = filePath();
    if (m_path.isEmpty() || !QDir().mkpath(QFileInfo(m_path).path())) {
        emit ready({}, tr("Impossible de créer le dossier des modèles locaux."));
        return;
    }
    if (QFileInfo(m_path).size() == modelBytes) {
        emit ready(m_path, {});
        return;
    }
    m_download = std::make_unique<QSaveFile>(m_path);
    if (!m_download->open(QIODevice::WriteOnly)) {
        finishDownload(m_download->errorString());
        return;
    }
    m_hash.reset();
    m_downloaded = 0;
    QNetworkRequest request(
        QUrl(QStringLiteral("https://huggingface.co/Ruicheng/moge-2-vits-normal-onnx/resolve/"
                            "e50ffda41565591092adea54c6ac83d6212e1e23/model.onnx")));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &LocalSurfaceModel::readDownload);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (m_reply->error() != QNetworkReply::NoError) {
            finishDownload(m_reply->errorString());
            return;
        }
        readDownload();
        if (!m_reply)
            return;
        if (m_downloaded != modelBytes || m_hash.result() != modelHash) {
            finishDownload(tr("Le fichier MoGe‑2 téléchargé est incomplet ou corrompu."));
            return;
        }
        const bool saved = m_download->commit();
        finishDownload(saved ? QString{} : m_download->errorString());
    });
    emit status(tr("Téléchargement de MoGe‑2 (141 Mo)…"));
}

void LocalSurfaceModel::readDownload()
{
    const auto bytes = m_reply->readAll();
    if (bytes.isEmpty())
        return;
    if (m_downloaded + bytes.size() > modelBytes || m_download->write(bytes) != bytes.size()) {
        finishDownload(tr("Impossible d’enregistrer le modèle MoGe‑2."));
        return;
    }
    const int before = int(100 * m_downloaded / modelBytes);
    m_hash.addData(bytes);
    m_downloaded += bytes.size();
    const int percent = int(100 * m_downloaded / modelBytes);
    if (percent != before)
        emit status(tr("Téléchargement de MoGe‑2 — %1 %").arg(percent));
}

void LocalSurfaceModel::finishDownload(const QString &error)
{
    const auto path = m_path;
    cancel();
    emit ready(error.isEmpty() ? path : QString{},
               error.isEmpty() ? QString{}
                               : tr("Téléchargement du modèle impossible. %1").arg(error));
}

void LocalSurfaceModel::cancel()
{
    if (auto *reply = m_reply.data()) {
        m_reply.clear();
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    m_download.reset();
}

LocalSurfaceResult LocalSurfaceModel::analyze(const QImage &source, const QString &path,
                                              const Cancel &cancel)
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    if (source.isNull() || cancel->load())
        return {};
    try {
        static ModelSession model;
        while (!model.mutex.tryLock(100))
            if (cancel->load())
                return {};
        const auto unlock = qScopeGuard([&] { model.mutex.unlock(); });
        if (cancel->load())
            return {};
        const auto sizeOnDisk = QFileInfo(path).size();
        if (!model.session || model.path != path || sizeOnDisk != modelBytes) {
            if (!checkModel(path, cancel)) {
                if (cancel->load())
                    return {};
                return {{},
                        tr("Le modèle MoGe‑2 est corrompu. Supprimez le fichier et relancez "
                           "l’analyse. %1")
                            .arg(path)};
            }
            Ort::SessionOptions options;
            options.SetIntraOpNumThreads(qBound(1, QThread::idealThreadCount() / 2, 4));
            options.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);
#ifdef Q_OS_WIN
            const auto nativePath = QDir::toNativeSeparators(path).toStdWString();
#else
            const auto nativePath = QFile::encodeName(path);
#endif
            model.session =
                std::make_unique<Ort::Session>(model.environment, nativePath.data(), options);
            model.path = path;
        }
        if (cancel->load())
            return {};
        const auto image = source.convertToFormat(QImage::Format_RGBA8888);
        if (image.isNull())
            return {};
        const auto         size   = image.size();
        const int          pixels = size.width() * size.height();
        std::vector<float> rgb(3 * pixels);
        for (int y = 0; y < size.height(); ++y) {
            if (cancel->load())
                return {};
            const auto *row = image.constScanLine(y);
            for (int x = 0; x < size.width(); ++x)
                for (int c = 0; c < 3; ++c)
                    rgb[c * pixels + y * size.width() + x] = row[4 * x + c] / 255.f;
        }
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 4> shape{1, 3, size.height(), size.width()};
        int64_t                      tokens   = 1200;
        const Ort::Value             inputs[] = {
            Ort::Value::CreateTensor<float>(memory, rgb.data(), rgb.size(), shape.data(),
                                                        shape.size()),
            Ort::Value::CreateTensor<int64_t>(memory, &tokens, 1, nullptr, 0)};
        const char      *inputNames[]  = {"image", "num_tokens"};
        const char      *outputNames[] = {"points", "normal", "mask"};
        Ort::RunOptions  options;
        std::atomic_bool finished{false};
        std::thread      cancelRun([&] {
            while (!finished.load() && !cancel->load())
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (cancel->load()) {
                try {
                    options.SetTerminate();
                } catch (const Ort::Exception &) {
                }
            }
        });
        const auto       endRun = qScopeGuard([&] {
            finished.store(true);
            cancelRun.join();
        });
        const auto output = model.session->Run(options, inputNames, inputs, 2, outputNames, 3);
        const std::vector<int64_t> vectorShape{1, size.height(), size.width(), 3};
        const std::vector<int64_t> maskShape{1, size.height(), size.width()};
        if (output[0].GetTensorTypeAndShapeInfo().GetShape() != vectorShape ||
            output[1].GetTensorTypeAndShapeInfo().GetShape() != vectorShape ||
            output[2].GetTensorTypeAndShapeInfo().GetShape() != maskShape)
            return {{}, tr("Les cartes du modèle MoGe‑2 ont des dimensions inattendues.")};
        return {geometryFromTensors(image, output[0].GetTensorData<float>(),
                                    output[1].GetTensorData<float>(),
                                    output[2].GetTensorData<float>(), cancel),
                {}};
    } catch (const Ort::Exception &error) {
        if (cancel->load())
            return {};
        return {{},
                tr("Analyse locale MoGe‑2 impossible. %1").arg(QString::fromUtf8(error.what()))};
    }
#else
    Q_UNUSED(source);
    Q_UNUSED(path);
    Q_UNUSED(cancel);
    return {{}, tr("L’analyse locale n’est pas disponible pour cette version.")};
#endif
}
} // namespace PhotoCrap::Ai
