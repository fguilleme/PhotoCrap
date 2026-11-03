#include "ProjectWriteDevice.h"

namespace
{
bool writeAll(QIODevice &_target, const char *_data, qint64 _size)
{
    while (_size > 0) {
        const qint64 written = _target.write(_data, _size);
        if (written <= 0)
            return false;
        _data += written;
        _size -= written;
    }
    return true;
}
} // namespace

LimitedWriteDevice::LimitedWriteDevice(QIODevice &_target, qint64 _limit)
    : m_target(_target), m_limit(qMax(qint64(0), _limit))
{
    open(QIODevice::WriteOnly | QIODevice::Unbuffered);
}
qint64 LimitedWriteDevice::writeData(const char *_data, qint64 _size)
{
    if (m_failed)
        return -1;
    if (_size > m_limit - m_written) {
        m_limitExceeded = m_failed = true;
        setErrorString(QStringLiteral("Project size limit exceeded"));
        return -1;
    }
    const qint64 size = _size;
    while (_size > 0) {
        const qint64 written = m_target.write(_data, _size);
        if (written <= 0) {
            setErrorString(m_target.errorString());
            m_failed = true;
            return -1;
        }
        _data += written;
        _size -= written;
        m_written += written;
    }
    return size;
}

Base64WriteDevice::Base64WriteDevice(QIODevice &_target) : m_target(_target)
{
    open(QIODevice::WriteOnly | QIODevice::Unbuffered);
}
bool Base64WriteDevice::flush(const char *_data, qsizetype _size)
{
    const auto encoded = QByteArray::fromRawData(_data, _size).toBase64();
    if (!writeAll(m_target, encoded.constData(), encoded.size())) {
        setErrorString(m_target.errorString());
        m_failed = true;
    }
    return !m_failed;
}
qint64 Base64WriteDevice::writeData(const char *_data, qint64 _size)
{
    if (m_failed)
        return -1;
    if (m_finished) {
        m_failed = true;
        setErrorString(QStringLiteral("Base64 stream already finished"));
        return -1;
    }
    const qint64 size = _size;
    if (!m_tail.isEmpty()) {
        const auto needed = qMin(qint64(3 - m_tail.size()), _size);
        m_tail.append(_data, needed);
        _data += needed;
        _size -= needed;
        if (m_tail.size() == 3) {
            if (!flush(m_tail.constData(), m_tail.size()))
                return -1;
            m_tail.clear();
        }
    }
    while (_size >= 3) {
        // 12 KiB input -> 16 KiB ASCII, with at most two pending bytes.
        const auto count = qMin(qint64(12 * 1024), _size - _size % 3);
        if (!flush(_data, count))
            return -1;
        _data += count;
        _size -= count;
    }
    if (_size > 0)
        m_tail.append(_data, _size);
    return size;
}
bool Base64WriteDevice::finish()
{
    if (m_finished)
        return !m_failed;
    m_finished = true;
    if (!m_failed && !m_tail.isEmpty())
        flush(m_tail.constData(), m_tail.size());
    m_tail.clear();
    return !m_failed;
}
