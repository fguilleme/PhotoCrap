#include "RawImageReader.h"
#include <QColorSpace>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <cstring>
#include <libraw/libraw.h>

namespace
{
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
                                    const std::shared_ptr<std::atomic_bool> &_cancel)
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
    auto decoder = std::make_unique<LibRaw>();
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
    result = decoder->dcraw_process();
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
    image.setColorSpace(QColorSpace::SRgb);
    return {image, {}};
} catch (const std::bad_alloc &) {
    return {{}, message("Mémoire insuffisante pour ouvrir ce RAW.")};
}
