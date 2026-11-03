#pragma once
#include <QIODevice>

// Sequential adapters used by the atomic project writer. Image payloads never
// accumulate as a complete PNG, Base64 string or JSON document in memory.
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
