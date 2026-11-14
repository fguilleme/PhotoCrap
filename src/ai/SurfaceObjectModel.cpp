#include "SurfaceObjectModel.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <thread>

#ifdef PHOTOCRAP_LOCAL_SURFACE
#include <onnxruntime_cxx_api.h>
#endif

namespace PhotoCrap::Ai
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
namespace
{
constexpr int                    imageEdge = 1024;
constexpr std::array<int64_t, 4> embeddingShape{1, 256, 64, 64};
constexpr qsizetype              embeddingSize = 256 * 64 * 64;
constexpr char                   repository[]  = "https://huggingface.co/Acly/MobileSAM/resolve/"
                                                 "0d3b403339b4674a82493d5e97964dd78089ddc8/";

struct ModelFile {
    const char *remote, *local, *sha256;
    qint64      bytes;
};
constexpr ModelFile encoder{"mobile_sam_image_encoder.onnx", "mobilesam-encoder-580f5fb6.onnx",
                            "580f5fb648ea1062c0aabc26217aed56921985f03f0cbbd852bba81d760cc749",
                            28157093};
constexpr ModelFile decoder{"sam_mask_decoder_multi.onnx", "mobilesam-decoder-8976b90a.onnx",
                            "8976b90a87ba50a6a72217a5ff994f7d25ce16f2229fcc1ed259e1294c622ffe",
                            16496559};

QString modelPath(const ModelFile &model)
{
    const auto root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.isEmpty())
        throw std::runtime_error("Missing application data directory");
    return QDir(root).filePath(QStringLiteral("models/") + QString::fromLatin1(model.local));
}

bool modelMatches(QFileDevice &file, const ModelFile &model)
{
    if (file.size() != model.bytes)
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) && hash.result().toHex() == model.sha256;
}

bool ensureModel(const ModelFile &model, const Cancel &cancel, bool allowDownload)
{
    if (cancel->load())
        return false;
    const auto path = modelPath(model);
    QFile      existing(path);
    if (existing.open(QIODevice::ReadOnly) && modelMatches(existing, model))
        return true;
    if (!allowDownload || cancel->load())
        return false;
    existing.close();
    if (!QDir().mkpath(QFileInfo(path).path()))
        throw std::runtime_error("Cannot create model directory");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("Cannot write model file");
    QNetworkAccessManager network;
    QNetworkRequest       request(
        QUrl(QString::fromLatin1(repository) + QString::fromLatin1(model.remote)));
    request.setTransferTimeout(60000);
    QNetworkReply     *reply = network.get(request);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64             total = 0;
    const auto         read  = [&] {
        const auto bytes = reply->readAll();
        total += bytes.size();
        if (total > model.bytes || file.write(bytes) != bytes.size())
            reply->abort();
        else
            hash.addData(bytes);
    };
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::readyRead, &loop, read);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (cancel->load())
            reply->abort();
    });
    poll.start(50);
    if (!reply->isFinished())
        loop.exec();
    if (cancel->load())
        return false;
    if (reply->error() != QNetworkReply::NoError)
        throw std::runtime_error(reply->errorString().toStdString());
    read();
    if (total != model.bytes || hash.result().toHex() != model.sha256 || !file.commit())
        throw std::runtime_error("Invalid MobileSAM model file");
    return true;
}

Ort::Session loadModel(const ModelFile &model)
{
    static Ort::Env     environment(ORT_LOGGING_LEVEL_WARNING, "PhotoCrap/MobileSAM");
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(qBound(1, QThread::idealThreadCount() / 2, 4));
    options.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);
    const auto path = modelPath(model);
#ifdef Q_OS_WIN
    const auto native = QDir::toNativeSeparators(path).toStdWString();
#else
    const auto native = QFile::encodeName(path);
#endif
    return Ort::Session(environment, native.data(), options);
}

std::vector<Ort::Value> run(Ort::Session &model, const char *const *inputs,
                            const Ort::Value *values, size_t inputCount, const char *const *outputs,
                            size_t outputCount, const Cancel &cancel)
{
    Ort::RunOptions  options;
    std::atomic_bool finished{false};
    std::thread      poll([&] {
        while (!finished.load() && !cancel->load())
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (cancel->load()) {
            try {
                options.SetTerminate();
            } catch (const Ort::Exception &) {
            }
        }
    });
    const auto       join = qScopeGuard([&] {
        finished.store(true);
        poll.join();
    });
    return model.Run(options, inputs, values, inputCount, outputs, outputCount);
}
} // namespace
#endif

bool SurfaceObjectModel::available()
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    return true;
#else
    return false;
#endif
}

std::shared_ptr<const SurfaceObjectEmbedding>
SurfaceObjectModel::encode(const QImage &source, const Cancel &cancel, bool allowDownload)
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    if (source.isNull() || !ensureModel(encoder, cancel, allowDownload) ||
        !ensureModel(decoder, cancel, allowDownload) || cancel->load())
        return {};
    const qreal scale = qreal(imageEdge) / qMax(source.width(), source.height());
    const QSize size(qMax(1, qRound(source.width() * scale)),
                     qMax(1, qRound(source.height() * scale)));
    const auto  image = source.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_RGB888);
    if (image.isNull())
        throw std::bad_alloc();
    std::vector<float> rgb(3 * size.width() * size.height());
    for (int y = 0; y < size.height(); ++y) {
        if (cancel->load())
            return {};
        const auto *from = image.constScanLine(y);
        auto       *to   = rgb.data() + 3 * size.width() * y;
        std::copy_n(from, 3 * size.width(), to);
    }
    const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    const std::array<int64_t, 3> shape{size.height(), size.width(), 3};
    auto                         input =
        Ort::Value::CreateTensor<float>(memory, rgb.data(), rgb.size(), shape.data(), shape.size());
    const char *names[]   = {"input_image"};
    const char *outputs[] = {"image_embeddings"};
    auto        model     = loadModel(encoder);
    const auto  result    = run(model, names, &input, 1, outputs, 1, cancel);
    if (result.front().GetTensorTypeAndShapeInfo().GetElementCount() != embeddingSize)
        throw std::runtime_error("Unexpected MobileSAM embedding size");
    const auto *values = result.front().GetTensorData<float>();
    return std::make_shared<SurfaceObjectEmbedding>(
        SurfaceObjectEmbedding{size, {values, values + embeddingSize}});
#else
    Q_UNUSED(source);
    Q_UNUSED(cancel);
    Q_UNUSED(allowDownload);
    return {};
#endif
}

QImage SurfaceObjectModel::mask(const SurfaceObjectEmbedding &embedding, const QPointF &point,
                                const QSize &size, const Cancel &cancel, bool preferWhole)
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    if (size.isEmpty() || embedding.values.size() != embeddingSize || cancel->load())
        return {};
    const auto           memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::array<float, 4> coordinates{float(point.x() * embedding.imageSize.width()),
                                     float(point.y() * embedding.imageSize.height()), 0.f, 0.f};
    std::array<float, 2> labels{1.f, -1.f};
    std::array<float, 2> originalSize{float(embedding.imageSize.height()),
                                      float(embedding.imageSize.width())};
    std::vector<float>   emptyMask(256 * 256, 0.f);
    float                hasMask = 0.f;
    constexpr std::array<int64_t, 1> one{1}, two{2};
    constexpr std::array<int64_t, 2> labelShape{1, 2};
    constexpr std::array<int64_t, 3> pointShape{1, 2, 2};
    constexpr std::array<int64_t, 4> maskShape{1, 1, 256, 256};
    auto                             makeInput = [&](auto &values, const auto &shape) {
        return Ort::Value::CreateTensor<float>(memory, values.data(), values.size(), shape.data(),
                                               shape.size());
    };
    const Ort::Value inputs[] = {
        // ONNX Runtime accepts a mutable pointer for externally owned inputs.
        Ort::Value::CreateTensor<float>(memory, const_cast<float *>(embedding.values.data()),
                                        embedding.values.size(), embeddingShape.data(),
                                        embeddingShape.size()),
        makeInput(coordinates, pointShape),
        makeInput(labels, labelShape),
        makeInput(emptyMask, maskShape),
        Ort::Value::CreateTensor<float>(memory, &hasMask, 1, one.data(), one.size()),
        makeInput(originalSize, two)};
    const char *names[]   = {"image_embeddings", "point_coords",   "point_labels",
                             "mask_input",       "has_mask_input", "orig_im_size"};
    const char *outputs[] = {"masks", "iou_predictions"};
    static auto model     = loadModel(decoder);
    const auto  result    = run(model, names, inputs, 6, outputs, 2, cancel);
    const auto  shape     = result[0].GetTensorTypeAndShapeInfo().GetShape();
    const auto  scores    = result[1].GetTensorTypeAndShapeInfo().GetElementCount();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != int64_t(scores) ||
        shape[2] != embedding.imageSize.height() || shape[3] != embedding.imageSize.width())
        throw std::runtime_error("Unexpected MobileSAM mask dimensions");
    const auto  *values    = result[0].GetTensorData<float>();
    const auto  *quality   = result[1].GetTensorData<float>();
    const auto   width     = embedding.imageSize.width();
    const auto   height    = embedding.imageSize.height();
    const auto   x         = qBound(0, int(point.x() * width), width - 1);
    const auto   y         = qBound(0, int(point.y() * height), height - 1);
    const auto   pixels    = width * height;
    const float *best      = nullptr;
    float        bestScore = 0.f;
    // With one prompt, candidates 1–3 represent different object scales.
    for (size_t index = 1; index < scores; ++index) {
        const auto *candidate = values + index * pixels;
        if (candidate[y * width + x] > 0.f && quality[index] > bestScore) {
            best      = candidate;
            bestScore = quality[index];
        }
    }
    if (!best)
        return {};
    if (preferWhole) {
        // A click inside a fold can yield several confident masks of the same
        // object at different scales. Pick the broadest comparable candidate.
        const auto covered = [pixels](const float *data) {
            return std::count_if(data, data + pixels, [](float value) { return value > 0.f; });
        };
        auto largest = covered(best);
        for (size_t index = 1; index < scores; ++index) {
            const auto *candidate = values + index * pixels;
            if (candidate == best || quality[index] < bestScore - .04f ||
                candidate[y * width + x] <= 0.f)
                continue;
            const auto area = covered(candidate);
            if (area > largest) {
                best    = candidate;
                largest = area;
            }
        }
    }
    QImage selected(embedding.imageSize, QImage::Format_Grayscale8);
    if (selected.isNull())
        throw std::bad_alloc();
    for (int row = 0; row < height; ++row) {
        if (cancel->load())
            return {};
        auto *out = selected.scanLine(row);
        for (int col = 0; col < width; ++col)
            out[col] = best[row * width + col] > 0.f ? 255 : 0;
    }
    return selected.scaled(size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
#else
    Q_UNUSED(embedding);
    Q_UNUSED(point);
    Q_UNUSED(size);
    Q_UNUSED(cancel);
    Q_UNUSED(preferWhole);
    return {};
#endif
}
} // namespace PhotoCrap::Ai
