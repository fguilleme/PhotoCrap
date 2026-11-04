#pragma once
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QVector>
#include <functional>

// Index the layer array without retaining its PNG/Base64 strings. Only the
// metadata and byte ranges survive the scan; callers decode one layer at a time.
// The ordinary Qt JSON parser still validates every metadata/layer value.
class ProjectReadIndex
{
public:
    struct Span {
        qint64 offset = 0, size = 0;
    };
    QJsonObject   metadata;
    QVector<Span> layers;
    qint64        largestRead = 0;
    bool          read(QIODevice &_device, const std::function<bool()> &_continue = {})
    {
        metadata = {};
        layers.clear();
        largestRead = 0;
        device      = &_device;
        keepGoing   = _continue;
        position    = 0;
        length      = _device.size();
        bufferStart = -1;
        buffer.clear();
        failed = false;
        if (_device.isSequential() || !_device.isReadable())
            return false;
        if (peek() == char(0xef)) {
            if (next() != char(0xef) || next() != char(0xbb) || next() != char(0xbf))
                return false;
        }
        if (!take('{'))
            return false;
        if (take('}'))
            return end();
        while (!failed) {
            space();
            if (peek() != '"')
                return false;
            const auto keySpan = value();
            const auto keyJson = parse(keySpan);
            if (failed || keyJson.isEmpty())
                return false;
            const auto key = keyJson.value("value").toString();
            if (!take(':'))
                return false;
            if (key == "layers") {
                layers.clear();
                if (!take('['))
                    return false;
                if (!take(']')) {
                    while (!failed) {
                        if (layers.size() >= 512)
                            return false;
                        const auto span = value();
                        if (failed || span.size <= 0)
                            return false;
                        layers.append(span);
                        if (take(']'))
                            break;
                        if (!take(','))
                            return false;
                    }
                }
            } else {
                const auto object = parse(value());
                if (failed || object.isEmpty())
                    return false;
                metadata.insert(key, object.value("value"));
            }
            if (take('}'))
                return end();
            if (!take(','))
                return false;
        }
        return false;
    }
    QJsonObject layer(QIODevice &_device, int _index, bool &_ok)
    {
        _ok = false;
        if (_index < 0 || _index >= layers.size())
            return {};
        device                = &_device;
        const auto      bytes = raw(layers[_index]);
        QJsonParseError error;
        const auto      json = QJsonDocument::fromJson(bytes, &error);
        _ok = !failed && error.error == QJsonParseError::NoError && json.isObject();
        return _ok ? json.object() : QJsonObject{};
    }

private:
    QIODevice            *device = nullptr;
    std::function<bool()> keepGoing;
    qint64                position = 0, length = 0, bufferStart = -1;
    QByteArray            buffer;
    bool                  failed = false;
    char                  peek()
    {
        if (failed || position >= length)
            return 0;
        if (position < bufferStart || position >= bufferStart + buffer.size()) {
            if ((keepGoing && !keepGoing()) || !device->seek(position)) {
                failed = true;
                return 0;
            }
            buffer      = device->read(16 * 1024);
            bufferStart = position;
            largestRead = qMax(largestRead, qint64(buffer.size()));
            if (buffer.isEmpty()) {
                failed = true;
                return 0;
            }
        }
        return buffer.at(position - bufferStart);
    }
    char next()
    {
        const char c = peek();
        if (!failed && position < length)
            ++position;
        return c;
    }
    void space()
    {
        while (!failed && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n'))
            ++position;
    }
    bool take(char _c)
    {
        space();
        if (peek() != _c || failed)
            return false;
        ++position;
        return true;
    }
    bool end()
    {
        space();
        return !failed && position == length;
    }
    Span value()
    {
        space();
        const qint64 start = position;
        const char   first = peek();
        if (!first) {
            failed = true;
            return {};
        }
        const bool quoted = first == '"';
        if (quoted || first == '{' || first == '[') {
            QVector<char> closing;
            bool          string = false, escaped = false;
            while (!failed && position < length) {
                const char c = next();
                if (string) {
                    if (escaped)
                        escaped = false;
                    else if (c == '\\')
                        escaped = true;
                    else if (c == '"') {
                        string = false;
                        if (quoted && closing.isEmpty())
                            return {start, position - start};
                    }
                } else if (c == '"')
                    string = true;
                else if (c == '{' || c == '[') {
                    closing.append(c == '{' ? '}' : ']');
                    if (closing.size() > 1024)
                        break;
                } else if (c == '}' || c == ']') {
                    if (closing.isEmpty() || closing.takeLast() != c)
                        break;
                    if (closing.isEmpty())
                        return {start, position - start};
                }
            }
            failed = true;
            return {};
        }
        while (!failed && position < length) {
            const char c = peek();
            if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\n' || c == '\r' || c == '\t')
                break;
            ++position;
        }
        if (position == start)
            failed = true;
        return {start, position - start};
    }
    QByteArray raw(const Span &_span)
    {
        if (failed || (keepGoing && !keepGoing()) || !device->seek(_span.offset)) {
            failed = true;
            return {};
        }
        auto bytes  = device->read(_span.size);
        largestRead = qMax(largestRead, qint64(bytes.size()));
        if (bytes.size() != _span.size)
            failed = true;
        return bytes;
    }
    QJsonObject parse(const Span &_span)
    {
        const auto      bytes = raw(_span);
        QJsonParseError error;
        const auto      json = QJsonDocument::fromJson("{\"value\":" + bytes + "}", &error);
        if (error.error != QJsonParseError::NoError || !json.isObject())
            failed = true;
        return json.object();
    }
};
