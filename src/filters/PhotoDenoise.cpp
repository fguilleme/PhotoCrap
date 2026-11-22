#include "PhotoDenoise.h"
#include "ImageUtils.h"

#include <QCache>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QScopeGuard>
#include <QThread>
#include <array>
#include <cmath>
#ifdef PHOTOCRAP_LOCAL_SURFACE
#include <onnxruntime_cxx_api.h>
#endif

namespace PhotoCrap::Denoise
{
bool available()
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    return true;
#else
    return false;
#endif
}

#ifdef PHOTOCRAP_LOCAL_SURFACE
namespace
{
Ort::Session loadSession(const QString &model)
{
    static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "PhotoCrap Denoise");
    const QDir      app(QCoreApplication::applicationDirPath());
    QString         path;
    for (const auto &dir : {"../Resources/models/", "models/", "../models/"}) {
        path = app.filePath(QString::fromLatin1(dir) + model);
        if (QFileInfo::exists(path))
            break;
    }
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(qBound(1, QThread::idealThreadCount() / 2, 4));
    options.SetGraphOptimizationLevel(ORT_ENABLE_EXTENDED);
    options.DisableCpuMemArena();
#ifdef Q_OS_WIN
    return Ort::Session(environment, path.toStdWString().c_str(), options);
#else
    return Ort::Session(environment, QFile::encodeName(path).constData(), options);
#endif
}

Ort::Session &sessionFor(Domain domain)
{
    if (domain == Domain::Bayer) {
        static auto session = loadSession(QStringLiteral("rawdenoise-nind/model_bayer.onnx"));
        return session;
    }
    if (domain == Domain::Linear) {
        static auto session = loadSession(QStringLiteral("rawdenoise-nind/model_linear.onnx"));
        return session;
    }
    static auto session = loadSession(QStringLiteral("denoise-nafnet/model.onnx"));
    return session;
}

int mirror(int x, int n)
{
    if (n <= 1)
        return 0;
    const int period = 2 * (n - 1);
    x %= period;
    if (x < 0)
        x += period;
    return std::min(x, period - x);
}

// A small overlap crossfades neighbouring estimates. Discard the outer halo of
// every prediction, where convolution padding lacks genuine image context.
int ramp(int i, int start, int length, int stride, int overlap)
{
    if (start && i < overlap)
        return i + 1;
    if (start + stride + overlap < length && i >= stride)
        return overlap - (i - stride);
    return overlap + 1;
}

QImage predict(const QImage &input, Domain domain, const Cancel &cancel)
{
    auto      &session = sessionFor(domain);
    const bool bayer   = domain == Domain::Bayer;
    const int  scale = bayer ? 2 : 1, channels = bayer ? 4 : 3;
    const auto info  = session.GetInputTypeInfo(0);
    const auto shape = info.GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != channels || shape[2] != shape[3] ||
        shape[2] < 128 || shape[2] > 1024)
        throw std::runtime_error("Unexpected denoiser input shape");
    const int side = int(shape[2]), plane = side * side, outSide = scale * side;
    const int outPlane = outSide * outSide;
    const int halo = 24, overlap = 32, stride = side - 2 * halo - overlap;
    const int width = input.width(), height = input.height();
    QImage    result(input.size() * scale, QImage::Format_RGBA64);
    if (result.isNull())
        throw std::bad_alloc();
    result.fill(Qt::transparent);
    Ort::AllocatorWithDefaultOptions allocator;
    const auto                       inName  = session.GetInputNameAllocated(0, allocator);
    const auto                       outName = session.GetOutputNameAllocated(0, allocator);
    const char                      *ins[]{inName.get()}, *outs[]{outName.get()};
    const auto         memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<float> samples(channels * plane);
    auto tensor = Ort::Value::CreateTensor<float>(memory, samples.data(), samples.size(),
                                                  shape.data(), shape.size());
    for (int top = 0; top < height; top += stride) {
        for (int left = 0; left < width; left += stride) {
            if (cancel && cancel->load())
                return {};
            for (int y = 0; y < side; ++y) {
                const auto *row = reinterpret_cast<const QRgba64 *>(
                    input.constScanLine(mirror(top + y - halo, height)));
                for (int x = 0; x < side; ++x) {
                    auto      p            = row[mirror(left + x - halo, width)];
                    const int i            = y * side + x;
                    samples[i]             = p.red() / 65535.f;
                    samples[plane + i]     = p.green() / 65535.f;
                    samples[2 * plane + i] = p.blue() / 65535.f;
                    if (bayer)
                        samples[3 * plane + i] = p.alpha() / 65535.f;
                }
            }
            auto       output     = session.Run(Ort::RunOptions{}, ins, &tensor, 1, outs, 1);
            const auto outputInfo = output.front().GetTensorTypeAndShapeInfo();
            if (outputInfo.GetShape() != std::vector<int64_t>{1, 3, outSide, outSide})
                throw std::runtime_error("Unexpected denoiser output shape");
            const float *values = output.front().GetTensorData<float>();
            const int    w      = std::min(width - left, stride + overlap);
            const int    h      = std::min(height - top, stride + overlap);
            double       gain   = 1.;
            if (domain != Domain::DisplayRgb) {
                // RawNIND learns detail up to an arbitrary gain. Match only
                // corresponding measured samples in the unpadded crop.
                double original = 0., predicted = 0.;
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        const int i = (y + halo) * side + x + halo;
                        const int j = (y + halo) * scale * outSide + (x + halo) * scale;
                        for (int c = 0; c < channels; ++c) {
                            original += samples[c * plane + i];
                            constexpr int bayerColors[] = {0, 1, 1, 2};
                            const int     index =
                                bayer ? bayerColors[c] * outPlane + (c / 2) * outSide + c % 2
                                          : c * outPlane;
                            predicted += values[j + index];
                        }
                    }
                gain = std::abs(predicted) > 1.e-12 ? original / predicted : 1.;
            }
            for (int y = 0; y < h * scale; ++y) {
                auto     *row = reinterpret_cast<QRgba64 *>(result.scanLine(top * scale + y));
                const int wy  = ramp(y / scale, top, height, stride, overlap);
                for (int x = 0; x < w * scale; ++x) {
                    const int  weight = wy * ramp(x / scale, left, width, stride, overlap);
                    auto      &p      = row[left * scale + x];
                    const int  total  = p.alpha() + weight;
                    const int  j      = (y + halo * scale) * outSide + x + halo * scale;
                    const auto mix    = [&](int c, int previous) {
                        double v = values[c * outPlane + j] * gain;
                        if (!std::isfinite(v))
                            throw std::runtime_error("Non-finite denoiser output");
                        return qRound(
                            (previous * double(p.alpha()) + qBound(0., v, 1.) * 65535. * weight) /
                            total);
                    };
                    p = QRgba64::fromRgba64(mix(0, p.red()), mix(1, p.green()), mix(2, p.blue()),
                                            total);
                }
            }
            if (left + w == width)
                break;
        }
        if (top + stride + overlap >= height)
            break;
    }
    for (int y = 0; y < result.height(); ++y) {
        auto       *row = reinterpret_cast<QRgba64 *>(result.scanLine(y));
        const auto *src = reinterpret_cast<const QRgba64 *>(input.constScanLine(y / scale));
        for (int x = 0; x < result.width(); ++x)
            row[x].setAlpha(bayer ? 65535 : src[x].alpha());
    }
    return result;
}
} // namespace
#endif

QImage run(const QImage &source, Domain domain, const Cancel &cancel, QString &error)
{
#ifdef PHOTOCRAP_LOCAL_SURFACE
    try {
        const QImage input = source.convertToFormat(QImage::Format_RGBA64);
        if (input.isNull())
            throw std::bad_alloc();
        // Limit peak inference memory across concurrent imports and previews.
        static QMutex inference;
        while (!inference.tryLock(100))
            if (cancel && cancel->load())
                return {};
        const auto unlock = qScopeGuard([&] { inference.unlock(); });
        if (cancel && cancel->load())
            return {};
        QImage result = predict(input, domain, cancel);
        result.setColorSpace(source.colorSpace());
        return result;
    } catch (const std::exception &e) {
        error = QString::fromUtf8(e.what());
    }
#else
    Q_UNUSED(source);
    Q_UNUSED(domain);
    Q_UNUSED(cancel);
    error = QCoreApplication::translate("RawImageReader", "Modèle de débruitage indisponible.");
#endif
    return {};
}

QImage adjust(const QImage &source, double amount)
{
    if (amount <= 0. || source.isNull())
        return source;
    // Sliders, masks and moving layers above this adjustment reuse the prediction.
    // Key by pixels because composition can allocate a fresh QImage on each render.
    static QMutex                     cacheLock;
    static QCache<QByteArray, QImage> cache(256 * 1024); // KiB
    QCryptographicHash                hash(QCryptographicHash::Sha256);
    const std::array<int, 3>          layout{source.width(), source.height(), int(source.format())};
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(layout.data()), sizeof(layout)));
    hash.addData(
        QByteArrayView(reinterpret_cast<const char *>(source.constBits()), source.sizeInBytes()));
    const auto key = hash.result();
    const auto cachedResult = [&]() -> QImage {
        QMutexLocker lock(&cacheLock);
        const auto *cached = cache.object(key);
        return cached ? *cached : QImage{};
    };
    QImage restored = cachedResult();
    if (restored.isNull()) {
        // Two renders may request the same pixels while refinement is running.
        // Hits stay independent of the producer, including screen-sized drafts.
        static QMutex producer;
        QMutexLocker producing(&producer);
        restored = cachedResult();
        if (restored.isNull()) {
            QString error;
            restored = run(source, Domain::DisplayRgb, {}, error);
            if (restored.isNull()) {
                qWarning() << "NAFNet:" << error;
                return source;
            }
            QMutexLocker lock(&cacheLock);
            cache.insert(key, new QImage(restored), (restored.sizeInBytes() + 1023) / 1024);
        }
    }
    const QImage base     = source.convertToFormat(QImage::Format_RGBA64);
    const double strength = qBound(0., amount / 100., 1.);
    for (int y = 0; strength < 1. && y < base.height(); ++y) {
        const auto *src = reinterpret_cast<const QRgba64 *>(base.constScanLine(y));
        auto       *out = reinterpret_cast<QRgba64 *>(restored.scanLine(y));
        for (int x = 0; x < base.width(); ++x) {
            auto blend = [&](int a, int b) { return qRound(a + strength * (b - a)); };
            out[x]     = QRgba64::fromRgba64(blend(src[x].red(), out[x].red()),
                                             blend(src[x].green(), out[x].green()),
                                             blend(src[x].blue(), out[x].blue()), src[x].alpha());
        }
    }
    return restored.convertToFormat(source.format());
}
} // namespace PhotoCrap::Denoise
