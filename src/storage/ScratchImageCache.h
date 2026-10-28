#pragma once

#include <QCoreApplication>
#include <QImage>
#include <QString>
#include <QVariantMap>
#include <memory>

// Lossless, disposable storage for immutable image blocks. A block can be a
// full render today or a tile later. Reading returns a read-only file mapping;
// QImage detaches before a caller modifies its pixels.
class ScratchImageCache final
{
    Q_DECLARE_TR_FUNCTIONS(ScratchImageCache)
public:
    struct Options {
        QString directory;
        qint64  maxBytes         = 4LL * 1024 * 1024 * 1024;
        qint64  minimumFreeBytes = 512LL * 1024 * 1024;
    };

    explicit ScratchImageCache(Options _options);
    ~ScratchImageCache();
    ScratchImageCache(const ScratchImageCache &)            = delete;
    ScratchImageCache &operator=(const ScratchImageCache &) = delete;

    // Disk I/O belongs on render workers, not pointer-event handlers.
    QImage      find(const QString &_key);
    bool        insert(const QString &_key, const QImage &_image);
    void        clear();
    QVariantMap statistics() const;
    Options     options() const;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
