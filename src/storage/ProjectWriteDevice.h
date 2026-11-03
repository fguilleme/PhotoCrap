#pragma once
#include <QIODevice>

// Sequential adapters used by the atomic project writer. Single-use and large
// payloads stream directly; repeated small payloads may be captured within a
// byte budget without accumulating the whole encoded project.
class LimitedWriteDevice final : public QIODevice
{
public:
    LimitedWriteDevice(QIODevice &_target, qint64 _limit);
    bool isSequential() const override
    {
        return true;
    }
    bool limitExceeded() const
    {
        return m_limitExceeded;
    }
    qint64 writtenBytes() const
    {
        return m_written;
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }
    qint64 writeData(const char *_data, qint64 _size) override;

private:
    QIODevice &m_target;
    qint64     m_limit, m_written = 0;
    bool       m_limitExceeded = false, m_failed = false;
};

class Base64WriteDevice final : public QIODevice
{
public:
    explicit Base64WriteDevice(QIODevice &_target);
    bool isSequential() const override
    {
        return true;
    }
    bool finish();

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }
    qint64 writeData(const char *_data, qint64 _size) override;

private:
    bool       flush(const char *_data, qsizetype _size);
    QIODevice &m_target;
    QByteArray m_tail;
    bool       m_failed = false, m_finished = false;
};

// Best-effort bounded capture while forwarding the entire stream. Exceeding
// the capture limit discards the recording, never truncates the destination.
class RecordingWriteDevice final : public QIODevice
{
public:
    RecordingWriteDevice(QIODevice &_target, qint64 _limit);
    bool isSequential() const override
    {
        return true;
    }
    bool recordingComplete() const
    {
        return m_recording && !m_failed;
    }
    QByteArray recording() const
    {
        return recordingComplete() ? m_bytes : QByteArray{};
    }

protected:
    qint64 readData(char *, qint64) override
    {
        return -1;
    }
    qint64 writeData(const char *_data, qint64 _size) override;

private:
    QIODevice &m_target;
    qint64     m_limit;
    QByteArray m_bytes;
    bool       m_recording = true, m_failed = false;
};
