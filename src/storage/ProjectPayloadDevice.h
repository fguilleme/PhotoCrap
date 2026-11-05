#pragma once

#include "ProjectReadIndex.h"
#include <algorithm>
#include <cstring>

// Read the JSON string as a sequential PNG stream. Each consumer starts at
// the same file span, so hashing and decoding never retain a full payload.
class ProjectPayloadDevice final : public QIODevice
{
public:
    ProjectPayloadDevice(QIODevice &_file, ProjectReadIndex::Payload _payload,
                         const std::function<bool()> &_continue)
        : m_file(_file), m_payload(_payload), m_continue(_continue)
    {
        rewind();
    }
    bool rewind()
    {
        close();
        m_remaining = m_payload.size;
        m_raw.clear();
        m_decoded.clear();
        m_cursor = m_consumed = 0;
        m_failed              = !m_file.seek(m_payload.offset);
        return !m_failed && open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    bool isSequential() const override
    {
        return true;
    }
    bool atEnd() const override
    {
        return !m_failed && !hasInput() && m_consumed == m_decoded.size() &&
               QIODevice::bytesAvailable() == 0;
    }
    bool failed() const
    {
        return m_failed;
    }

protected:
    qint64 readData(char *_target, qint64 _size) override
    {
        qint64 written = 0;
        while (written < _size && !m_failed) {
            if (m_consumed == m_decoded.size() && (!hasInput() || !decodeChunk()))
                break;
            const auto count = qMin(_size - written, qint64(m_decoded.size() - m_consumed));
            std::memcpy(_target + written, m_decoded.constData() + m_consumed, size_t(count));
            m_consumed += count;
            written += count;
        }
        return written == 0 && m_failed ? -1 : written;
    }
    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    static constexpr int      ChunkBytes = 16 * 1024;
    QIODevice                &m_file;
    ProjectReadIndex::Payload m_payload;
    std::function<bool()>     m_continue;
    qint64                    m_remaining = 0;
    QByteArray                m_raw, m_decoded;
    qsizetype                 m_cursor = 0, m_consumed = 0;
    bool                      m_failed = false;

    bool hasInput() const
    {
        return m_remaining > 0 || m_cursor < m_raw.size();
    }
    QByteArray rawChunk()
    {
        const auto count = qMin(m_remaining, qint64(ChunkBytes));
        if (count <= 0 || (m_continue && !m_continue())) {
            m_failed = true;
            return {};
        }
        auto chunk = m_file.read(count);
        m_remaining -= count;
        m_failed = chunk.size() != count;
        return chunk;
    }
    int nextByte()
    {
        if (m_cursor == m_raw.size()) {
            m_raw    = rawChunk();
            m_cursor = 0;
            if (m_failed)
                return -1;
        }
        return uchar(m_raw.at(m_cursor++));
    }
    QByteArray escapedChunk()
    {
        QByteArray encoded;
        encoded.reserve(ChunkBytes);
        while (encoded.size() < ChunkBytes && hasInput() && !m_failed) {
            int byte = nextByte();
            if (byte == '\\') {
                byte = nextByte();
                if (byte == 'u') {
                    byte = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int h   = nextByte();
                        const int hex = h >= '0' && h <= '9'   ? h - '0'
                                        : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                        : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                               : -1;
                        if (hex < 0) {
                            m_failed = true;
                            break;
                        }
                        byte = byte * 16 + hex;
                    }
                } else if (byte != '/')
                    m_failed = true; // Other JSON escapes cannot produce Base64.
            }
            if (byte < 0 || byte > 127)
                m_failed = true;
            else
                encoded.append(char(byte));
        }
        return encoded;
    }
    bool decodeChunk()
    {
        auto encoded = m_payload.escaped ? escapedChunk() : rawChunk();
        if (m_failed || (hasInput() && encoded.contains('='))) {
            m_failed = true;
            return false;
        }
        auto decoded = QByteArray::fromBase64Encoding(std::move(encoded),
                                                      QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) {
            m_failed = true;
            return false;
        }
        m_decoded  = std::move(decoded.decoded);
        m_consumed = 0;
        return true;
    }
};
