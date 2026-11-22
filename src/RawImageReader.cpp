#include "RawImageReader.h"
#include "filters/PhotoDenoise.h"
#include <QBuffer>
#include <QColorSpace>
#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QFileInfo>
#include <QImageReader>
#include <QStringList>
#include <cstring>
#include <libraw/libraw.h>

namespace
{
// LibRaw supplies black-corrected, white-balanced samples at its demosaic hook.
// Undo the per-channel balance for RawNIND, then restore it before handing
// camera RGB back to LibRaw for the usual colour transform, gamma and rotation.
class DenoiseDecoder final : public LibRaw
{
public:
    PhotoCrap::Denoise::Cancel cancel;
    QString                    error;
    bool                       enableBayer()
    {
        const auto &id = imgdata.idata;
        if (id.filters <= 1000 || id.colors != 3 || imgdata.sizes.width < 2 ||
            imgdata.sizes.height < 2)
            return false;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 2; ++x)
                if (id.cdesc[COLOR(y, x)] == 'R') {
                    redX = x;
                    redY = y % 2;
                }
        // Some four-colour or non-rectangular sensors also set a Bayer filter
        // mask. Send those through the linear model after LibRaw demosaics.
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 2; ++x) {
                const char expected = "RGGB"[2 * ((y + redY) % 2) + (x + redX) % 2];
                if (id.cdesc[COLOR(y, x)] != expected)
                    return false;
            }
        callbacks.interpolate_bayer_cb = [](void *context) {
            static_cast<DenoiseDecoder *>(static_cast<LibRaw *>(context))->demosaic();
        };
        return true;
    }

private:
    int  redX = 0, redY = 0;
    void demosaic()
    {
        const int w = imgdata.sizes.width, h = imgdata.sizes.height;
        QImage    packed((w + redX + 1) / 2, (h + redY + 1) / 2, QImage::Format_RGBA64);
        if (packed.isNull())
            throw std::bad_alloc();
        const auto   reflect = [](int p, int n) { return p < 0 ? -p : p >= n ? 2 * n - 2 - p : p; };
        const float *wb      = imgdata.color.pre_mul;
        for (int y = 0; y < packed.height(); ++y) {
            auto *row = reinterpret_cast<QRgba64 *>(packed.scanLine(y));
            for (int x = 0; x < packed.width(); ++x) {
                quint16 sample[4];
                for (int c = 0; c < 4; ++c) {
                    const int sx      = reflect(2 * x + c % 2 - redX, w);
                    const int sy      = reflect(2 * y + c / 2 - redY, h);
                    const int channel = COLOR(sy, sx);
                    sample[c]         = quint16(qBound(
                        0, qRound(imgdata.image[sy * w + sx][channel] / qMax(0.0001f, wb[channel])),
                        65535));
                }
                row[x] = QRgba64::fromRgba64(sample[0], sample[1], sample[2], sample[3]);
            }
        }
        const auto rgb =
            PhotoCrap::Denoise::run(packed, PhotoCrap::Denoise::Domain::Bayer, cancel, error);
        if (rgb.isNull())
            throw LIBRAW_EXCEPTION_CANCELLED_BY_CALLBACK;
        for (int y = 0; y < h; ++y) {
            const auto *row = reinterpret_cast<const QRgba64 *>(rgb.constScanLine(y + redY));
            for (int x = 0; x < w; ++x) {
                const auto p    = row[x + redX];
                auto      &dest = imgdata.image[y * w + x];
                dest[0]         = qBound(0, qRound(p.red() * wb[0]), 65535);
                dest[1]         = qBound(0, qRound(p.green() * wb[1]), 65535);
                dest[2]         = qBound(0, qRound(p.blue() * wb[2]), 65535);
                dest[3]         = dest[1];
            }
        }
    }
};

QString message(const char *_text)
{
    return QCoreApplication::translate("RawImageReader", _text);
}
int checkCancelled(void *_data, enum LibRaw_progress, int, int)
{
    return _data && static_cast<std::atomic_bool *>(_data)->load() ? 1 : 0;
}
bool validDimensions(unsigned _width, unsigned _height)
{
    return _width > 0 && _height > 0 && _width <= 16384 && _height <= 16384 &&
           quint64(_width) * _height <= 32ULL * 1024 * 1024;
}
} // namespace
namespace
{
const QStringList extensions = {"3fr", "arw", "cr2", "cr3", "crw", "dcr", "dng", "erf", "fff",
                                "iiq", "kdc", "mef", "mos", "mrw", "nef", "nrw", "orf", "pef",
                                "raf", "raw", "rw2", "rwl", "sr2", "srf", "srw", "x3f"};
}
QImage RawImageReader::embeddedThumbnail(const QString &_path, const QSize &_size)
{
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 8 || file.size() > 1024LL * 1024 * 1024)
        return {};
    auto *data = file.map(0, file.size());
    if (!data)
        return {};
    auto decoder = std::make_unique<LibRaw>();
    if (decoder->open_buffer(data, size_t(file.size())) != LIBRAW_SUCCESS ||
        decoder->unpack_thumb() != LIBRAW_SUCCESS)
        return {};
    int        error   = 0;
    const auto release = [](libraw_processed_image_t *_image) { LibRaw::dcraw_clear_mem(_image); };
    std::unique_ptr<libraw_processed_image_t, decltype(release)> thumb(
        decoder->dcraw_make_mem_thumb(&error), release);
    if (!thumb || error != LIBRAW_SUCCESS || thumb->data_size > 64 * 1024 * 1024)
        return {};
    if (thumb->type == LIBRAW_IMAGE_JPEG) {
        QByteArray bytes(reinterpret_cast<const char *>(thumb->data), int(thumb->data_size));
        QBuffer    buffer(&bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "JPEG");
        reader.setAutoTransform(true);
        if (_size.isValid() && reader.size().isValid())
            reader.setScaledSize(reader.size().scaled(_size, Qt::KeepAspectRatio));
        return reader.read();
    }
    if (thumb->type == LIBRAW_IMAGE_BITMAP && thumb->colors == 3 && thumb->bits == 8 &&
        thumb->width > 0 && thumb->height > 0 &&
        quint64(thumb->width) * thumb->height * 3 <= thumb->data_size)
        return QImage(thumb->data, thumb->width, thumb->height, thumb->width * 3,
                      QImage::Format_RGB888)
            .copy();
    return {};
}
bool RawImageReader::denoiseByDefault()
{
    return QSettings().value("raw/denoise", false).toBool();
}

bool RawImageReader::isRawFile(const QString &_path)
{
    return extensions.contains(QFileInfo(_path).suffix().toLower());
}
QString RawImageReader::filePatterns()
{
    QStringList patterns;
    for (const auto &extension : extensions)
        patterns << "*." + extension << "*." + extension.toUpper();
    return patterns.join(' ');
}
RawImageResult RawImageReader::read(const QString                           &_path,
                                    const std::shared_ptr<std::atomic_bool> &_cancel, bool denoise)
try {
    if (_cancel && _cancel->load())
        return {{}, {}, true};
    QFile file(_path);
    if (!file.open(QIODevice::ReadOnly))
        return {{}, file.errorString()};
    if (file.size() < 8 || file.size() > 1024LL * 1024 * 1024)
        return {{}, message("Fichier RAW vide, incomplet ou trop volumineux (1 Gio maximum).")};
    // QFile handles Unicode filenames on every platform; mapped bytes stay
    // alive until the LibRaw instance and all decoder buffers are destroyed.
    uchar *data = file.map(0, file.size());
    if (!data)
        return {{}, message("Impossible de lire le fichier RAW.")};
    // LibRaw contains large lookup tables; keep them off Qt worker stacks.
    auto decoder    = std::make_unique<DenoiseDecoder>();
    decoder->cancel = _cancel;
    decoder->set_progress_handler(checkCancelled, _cancel.get());
    decoder->imgdata.rawparams.max_raw_memory_mb = 512;
    decoder->imgdata.params.use_camera_wb        = 1;
    decoder->imgdata.params.output_color         = 1; // sRGB, matching the editing engine.
    decoder->imgdata.params.output_bps           = 16;
    decoder->imgdata.params.gamm[0]              = 1. / 2.4;
    decoder->imgdata.params.gamm[1]              = 12.92;
    decoder->imgdata.params.no_auto_bright       = 1;
    decoder->imgdata.params.user_qual            = 3;
    const auto failure                           = [&](int _code) -> RawImageResult {
        if ((_cancel && _cancel->load()) || _code == LIBRAW_CANCELLED_BY_CALLBACK)
            return {{}, {}, true};
        return {{},
                message("Impossible de développer ce RAW : %1")
                    .arg(QString::fromUtf8(libraw_strerror(_code)))};
    };
    int result = decoder->open_buffer(data, size_t(file.size()));
    if (result != LIBRAW_SUCCESS)
        return failure(result);
    const auto &sizes = decoder->imgdata.sizes;
    if (!validDimensions(sizes.width, sizes.height) ||
        quint64(sizes.raw_width) * sizes.raw_height > 40ULL * 1024 * 1024)
        return {{}, message("Ce RAW dépasse la limite de 32 mégapixels du document.")};
    if (_cancel && _cancel->load())
        return {{}, {}, true};
    result = decoder->unpack();
    if (result != LIBRAW_SUCCESS)
        return failure(result);
    const bool linearDenoise = denoise && !decoder->enableBayer();
    if (linearDenoise) {
        decoder->imgdata.params.output_color = 8; // Rec.2020 primaries
        decoder->imgdata.params.gamm[0]      = 1.;
        decoder->imgdata.params.gamm[1]      = 1.;
    }
    result = decoder->dcraw_process();
    if (!decoder->error.isEmpty())
        return {{}, decoder->error};
    if (result != LIBRAW_SUCCESS)
        return failure(result);
    auto      *processed = decoder->dcraw_make_mem_image(&result);
    const auto release = [](libraw_processed_image_t *_image) { LibRaw::dcraw_clear_mem(_image); };
    std::unique_ptr<libraw_processed_image_t, decltype(release)> pixels(processed, release);
    if (!processed)
        return failure(result);
    if (_cancel && _cancel->load())
        return {{}, {}, true};
    if (processed->type != LIBRAW_IMAGE_BITMAP || processed->colors != 3 || processed->bits != 16 ||
        !validDimensions(processed->width, processed->height) ||
        quint64(processed->width) * processed->height * 6 != processed->data_size)
        return {{}, message("Le décodeur RAW a produit une image invalide.")};
    QImage image(processed->width, processed->height, QImage::Format_RGBA64_Premultiplied);
    if (image.isNull())
        return {{}, message("Mémoire insuffisante pour ouvrir ce RAW.")};
    for (int y = 0; y < image.height(); ++y) {
        if (_cancel && _cancel->load())
            return {{}, {}, true};
        auto *row = reinterpret_cast<QRgba64 *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            quint16 channels[3];
            std::memcpy(channels, processed->data + (size_t(y) * image.width() + x) * 6, 6);
            row[x] = QRgba64::fromRgba64(channels[0], channels[1], channels[2], 65535);
        }
    }
    if (linearDenoise) {
        image.setColorSpace(QColorSpace({.3127, .3290}, {.708, .292}, {.170, .797}, {.131, .046},
                                        QColorSpace::TransferFunction::Linear));
        QString error;
        image = PhotoCrap::Denoise::run(image, PhotoCrap::Denoise::Domain::Linear, _cancel, error);
        if (image.isNull())
            return {{}, error, _cancel && _cancel->load()};
        image.convertToColorSpace(QColorSpace::SRgb);
        image.convertTo(QImage::Format_RGBA64_Premultiplied);
    }
    image.setColorSpace(QColorSpace::SRgb);
    return {image, {}};
} catch (const std::bad_alloc &) {
    return {{}, message("Mémoire insuffisante pour ouvrir ce RAW.")};
}
