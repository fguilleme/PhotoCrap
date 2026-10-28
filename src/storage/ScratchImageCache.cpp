#include "ScratchImageCache.h"

#include <QColorSpace>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <utility>

namespace
{
constexpr char OwnerMarker[] = "PhotoCrap scratch images / 1\n";

struct Session {
    QTemporaryDir       directory;
    QLockFile           lock;
    std::atomic<qint64> bytes{0}, mappedBytes{0};

    explicit Session(const QString &_root)
        : directory(_root + QStringLiteral("/session-XXXXXX")),
          lock(directory.path() + QStringLiteral("/session.lock"))
    {
        lock.setStaleLockTime(0);
    }
    ~Session()
    {
        lock.unlock();
    }
};

struct Block {
    std::shared_ptr<Session> session;
    QString                  path;
    QByteArray               header, digest;
    QSize                    size;
    QImage::Format           format;
    qsizetype                stride;
    qint64                   payloadBytes, fileBytes;
    QColorSpace              colorSpace;
    qreal                    pixelRatio;
    int                      dotsX, dotsY;
    QPoint                   offset;
    Block(std::shared_ptr<Session> _session, QString _path, QByteArray _header, QByteArray _digest,
          const QImage &_image, qint64 _fileBytes)
        : session(std::move(_session)), path(std::move(_path)), header(std::move(_header)),
          digest(std::move(_digest)), size(_image.size()), format(_image.format()),
          stride(_image.bytesPerLine()), payloadBytes(_image.sizeInBytes()), fileBytes(_fileBytes),
          colorSpace(_image.colorSpace()), pixelRatio(_image.devicePixelRatio()),
          dotsX(_image.dotsPerMeterX()), dotsY(_image.dotsPerMeterY()), offset(_image.offset())
    {
    }
    Block(const Block &)            = delete;
    Block &operator=(const Block &) = delete;
    ~Block()
    {
        if (QFile::remove(path) || !QFile::exists(path))
            session->bytes.fetch_sub(fileBytes);
    }
};

struct Mapping {
    std::shared_ptr<Block> block;
    QFile                  file;
    uchar                 *pixels = nullptr;
    explicit Mapping(std::shared_ptr<Block> _block) : block(std::move(_block)), file(block->path)
    {
    }
    ~Mapping()
    {
        if (pixels) {
            file.unmap(pixels);
            block->session->mappedBytes.fetch_sub(block->payloadBytes);
        }
        // Windows cannot remove a file until its mapping and handle are closed.
        file.close();
    }
};

void removeAbandonedSessions(const QString &_root)
{
    const auto directories =
        QDir(_root).entryInfoList({QStringLiteral("session-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &directory : directories) {
        if (directory.isSymLink())
            continue;
        QFile marker(directory.filePath() + QStringLiteral("/owner"));
        if (!marker.open(QIODevice::ReadOnly) || marker.read(128) != OwnerMarker)
            continue;
        marker.close();
        QLockFile lock(directory.filePath() + QStringLiteral("/session.lock"));
        lock.setStaleLockTime(0);
        if (lock.tryLock(0)) {
            lock.unlock();
            QDir(directory.filePath()).removeRecursively();
        }
    }
}

bool supported(QImage::Format _format)
{
    return _format == QImage::Format_ARGB32 || _format == QImage::Format_RGB32 ||
           _format == QImage::Format_ARGB32_Premultiplied || _format == QImage::Format_RGBA64 ||
           _format == QImage::Format_RGBA64_Premultiplied || _format == QImage::Format_Grayscale8 ||
           _format == QImage::Format_Grayscale16;
}
} // namespace

struct ScratchImageCache::State {
    struct Entry {
        std::shared_ptr<Block> block;
        quint64                lastUse;
    };
    Options                  options;
    mutable QMutex           mutex;
    QMutex                   writerMutex;
    QHash<QString, Entry>    entries;
    std::shared_ptr<Session> session;
    quint64                  clock = 0, generation = 0, hits = 0, misses = 0;
    QString                  error;
};

ScratchImageCache::ScratchImageCache(Options _options) : m_state(std::make_unique<State>())
{
    if (_options.directory.isEmpty()) {
        const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!root.isEmpty())
            _options.directory = root + QStringLiteral("/scratch");
    }
    _options.maxBytes         = qMax(qint64(0), _options.maxBytes);
    _options.minimumFreeBytes = qMax(qint64(0), _options.minimumFreeBytes);
    m_state->options          = std::move(_options);
}

ScratchImageCache::~ScratchImageCache() = default;

ScratchImageCache::Options ScratchImageCache::options() const
{
    return m_state->options;
}

QImage ScratchImageCache::find(const QString &_key)
{
    std::shared_ptr<Block> block;
    {
        QMutexLocker locker(&m_state->mutex);
        const auto   it = m_state->entries.find(_key);
        if (it == m_state->entries.end()) {
            ++m_state->misses;
            return {};
        }
        it->lastUse = ++m_state->clock;
        block       = it->block;
    }
    auto mapping = std::make_unique<Mapping>(block);
    bool valid   = mapping->file.open(QIODevice::ReadOnly) &&
                   mapping->file.size() == block->fileBytes &&
                   mapping->file.read(block->header.size()) == block->header;
    if (valid) {
        mapping->pixels = mapping->file.map(block->header.size(), block->payloadBytes);
        if (mapping->pixels) {
            block->session->mappedBytes.fetch_add(block->payloadBytes);
            const auto data = QByteArrayView(reinterpret_cast<const char *>(mapping->pixels),
                                             qsizetype(block->payloadBytes));
            valid = QCryptographicHash::hash(data, QCryptographicHash::Sha256) == block->digest &&
                    mapping->file.seek(block->header.size() + block->payloadBytes) &&
                    mapping->file.read(32) == block->digest;
        } else
            valid = false;
    }
    if (!valid) {
        QMutexLocker locker(&m_state->mutex);
        const auto   it = m_state->entries.find(_key);
        if (it != m_state->entries.end() && it->block == block)
            m_state->entries.erase(it);
        ++m_state->misses;
        m_state->error = tr("Bloc de cache illisible ou endommagé ; rendu recalculé.");
        return {};
    }
    // Closing keeps the mapping valid. The image may outlive its render thread,
    // so leave no open descriptor or QObject affinity tied to that worker.
    mapping->file.close();
    mapping->file.moveToThread(nullptr);
    QImage image(
        static_cast<const uchar *>(mapping->pixels), block->size.width(), block->size.height(),
        block->stride, block->format,
        [](void *_mapping) { delete static_cast<Mapping *>(_mapping); }, mapping.get());
    if (image.isNull())
        return {};
    mapping.release();
    image.setColorSpace(block->colorSpace);
    image.setDevicePixelRatio(block->pixelRatio);
    image.setDotsPerMeterX(block->dotsX);
    image.setDotsPerMeterY(block->dotsY);
    image.setOffset(block->offset);
    {
        QMutexLocker locker(&m_state->mutex);
        ++m_state->hits;
    }
    return image;
}

bool ScratchImageCache::insert(const QString &_key, const QImage &_image)
{
    if (_key.isEmpty() || _image.isNull() || !supported(_image.format()) ||
        m_state->options.maxBytes == 0)
        return false;
    QByteArray  header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_5);
    stream << quint32(0x50435343) << quint32(1) << _image.size() << qint32(_image.format())
           << qint64(_image.bytesPerLine()) << qint64(_image.sizeInBytes())
           << _image.devicePixelRatio() << _image.dotsPerMeterX() << _image.dotsPerMeterY()
           << _image.offset() << _image.colorSpace().iccProfile();
    // Keep mapped pixels naturally aligned, including 64-bit channel formats.
    header.append(QByteArray((64 - header.size() % 64) % 64, '\0'));
    const qint64 fileBytes = header.size() + _image.sizeInBytes() + 32;
    // Serialize writers, but keep the metadata lock out of large writes/hashes.
    QMutexLocker             writer(&m_state->writerMutex);
    std::shared_ptr<Session> session;
    quint64                  generation;
    {
        QMutexLocker locker(&m_state->mutex);
        if (m_state->entries.contains(_key))
            return true;
        if (fileBytes > m_state->options.maxBytes) {
            m_state->error = tr("Ce bloc dépasse le budget du cache disque.");
            return false;
        }
        if (!m_state->session) {
            const QString root = m_state->options.directory;
            if (!QDir::isAbsolutePath(root) || !QDir().mkpath(root)) {
                m_state->error = tr("Le dossier du cache disque est inaccessible.");
                return false;
            }
            removeAbandonedSessions(root);
            auto created = std::make_shared<Session>(root);
            if (!created->directory.isValid() || !created->lock.tryLock(0)) {
                m_state->error = tr("Impossible de créer la session de cache.");
                return false;
            }
            QFile marker(created->directory.filePath(QStringLiteral("owner")));
            if (!marker.open(QIODevice::WriteOnly) ||
                marker.write(OwnerMarker) != sizeof(OwnerMarker) - 1) {
                m_state->error = tr("Impossible d’initialiser la session de cache.");
                return false;
            }
            m_state->session = std::move(created);
        }
        session = m_state->session;
        while (session->bytes.load() > m_state->options.maxBytes - fileBytes &&
               !m_state->entries.isEmpty()) {
            auto oldest = m_state->entries.begin();
            for (auto it = m_state->entries.begin(); it != m_state->entries.end(); ++it)
                if (it->lastUse < oldest->lastUse)
                    oldest = it;
            m_state->entries.erase(oldest);
        }
        if (session->bytes.load() > m_state->options.maxBytes - fileBytes) {
            m_state->error = tr("Le cache disque est encore utilisé par des rendus.");
            return false;
        }
        generation = m_state->generation;
    }
    const auto fail = [&](const QString &_error) {
        QMutexLocker locker(&m_state->mutex);
        m_state->error = _error;
        return false;
    };
    QStorageInfo storage(session->directory.path());
    // On APFS firmlinks Qt can report the read-only system volume for a
    // writable data path. Let opening QSaveFile check actual writability.
    if (!storage.isValid() || storage.bytesAvailable() < fileBytes ||
        storage.bytesAvailable() - fileBytes < m_state->options.minimumFreeBytes)
        return fail(tr("Espace libre insuffisant pour le cache disque."));
    const QString path = session->directory.filePath(
        QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".block"));
    QSaveFile output(path);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly))
        return fail(output.errorString());
    output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (output.write(header) != header.size())
        return fail(output.errorString());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (qsizetype offset = 0; offset < _image.sizeInBytes();) {
        const qsizetype count = qMin(qsizetype(4 * 1024 * 1024), _image.sizeInBytes() - offset);
        const auto      data =
            QByteArrayView(reinterpret_cast<const char *>(_image.constBits()) + offset, count);
        if (output.write(data.data(), data.size()) != data.size())
            return fail(output.errorString());
        hash.addData(data);
        offset += count;
    }
    const QByteArray digest = hash.result();
    if (output.write(digest) != digest.size() || !output.commit())
        return fail(output.errorString());
    session->bytes.fetch_add(fileBytes);
    auto         block = std::make_shared<Block>(session, path, header, digest, _image, fileBytes);
    QMutexLocker locker(&m_state->mutex);
    if (generation != m_state->generation)
        return false;
    m_state->entries.insert(_key, {std::move(block), ++m_state->clock});
    m_state->error.clear();
    return true;
}

void ScratchImageCache::clear()
{
    QHash<QString, State::Entry> retired;
    {
        QMutexLocker locker(&m_state->mutex);
        ++m_state->generation;
        retired.swap(m_state->entries);
        m_state->error.clear();
    }
    // Releasing the last lease can unlink a large file. Keep that I/O out of
    // the metadata lock used by readers and the settings statistics.
}

QVariantMap ScratchImageCache::statistics() const
{
    std::unique_lock<QMutex> locker(m_state->mutex, std::try_to_lock);
    if (!locker.owns_lock())
        return {{"busy", true}};
    return {{"diskBytes", m_state->session ? m_state->session->bytes.load() : 0},
            {"mappedBytes", m_state->session ? m_state->session->mappedBytes.load() : 0},
            {"entries", m_state->entries.size()},
            {"hits", QVariant::fromValue(m_state->hits)},
            {"misses", QVariant::fromValue(m_state->misses)},
            {"budgetBytes", m_state->options.maxBytes},
            {"directory", m_state->options.directory},
            {"error", m_state->error}};
}
