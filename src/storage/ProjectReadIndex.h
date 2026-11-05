#pragma once
#include <QHash>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QVector>
#include <functional>

// Index the layer array without retaining its PNG/Base64 strings. The ordinary
// Qt parser validates metadata values, and pixel strings retain file offsets.
class ProjectReadIndex
{
public:
    struct Span {
        qint64 offset = 0, size = 0;
    };
    struct Payload : Span {
        bool escaped = false;
    };
    struct LayerEntry {
        QJsonObject             metadata;
        QHash<QString, Payload> payloads;
        Payload                 payload(const QString &_name) const
        {
            return payloads.value(_name);
        }
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
    LayerEntry layer(int _index, bool &_ok)
    {
        _ok = false;
        if (_index < 0 || _index >= layers.size())
            return {};
        const auto span = layers[_index];
        position        = span.offset;
        LayerEntry entry;
        if (!take('{'))
            return {};
        if (!take('}')) {
            while (!failed) {
                space();
                if (peek() != '"')
                    return {};
                const auto key = parse(value()).value("value").toString();
                if (!take(':'))
                    return {};
                const bool payload = key == "png" || key == "mask" || key == "source";
                space();
                if (payload && peek() == '"')
                    entry.payloads.insert(key, payloadString());
                else {
                    const auto object = parse(value());
                    if (failed || object.isEmpty())
                        return {};
                    if (payload)
                        entry.payloads.remove(key);
                    else
                        entry.metadata.insert(key, object.value("value"));
                }
                if (take('}'))
                    break;
                if (!take(','))
                    return {};
            }
        }
        _ok = !failed && position == span.offset + span.size;
        return _ok ? entry : LayerEntry{};
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
    Payload payloadString()
    {
        ++position; // Opening quote.
        Payload result{{position, 0}, false};
        while (!failed && position < length) {
            const uchar c = uchar(next());
            if (c == '"') {
                result.size = position - result.offset - 1;
                return result;
            }
            if (c < 0x20)
                break;
            if (c == '\\') {
                result.escaped    = true;
                const char escape = next();
                if (escape == 'u') {
                    for (int i = 0; i < 4; ++i) {
                        const char hex = next();
                        if (!((hex >= '0' && hex <= '9') || (hex >= 'a' && hex <= 'f') ||
                              (hex >= 'A' && hex <= 'F'))) {
                            failed = true;
                            break;
                        }
                    }
                } else if (escape != '"' && escape != '\\' && escape != '/' && escape != 'b' &&
                           escape != 'f' && escape != 'n' && escape != 'r' && escape != 't')
                    break;
            } else if (c >= 0x80) {
                // Metadata is parsed by Qt. For payload strings, validate UTF-8
                // here even when the layer never uses the resulting PNG.
                int trailing = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
                if (c < 0xc2 || c > 0xf4)
                    break;
                for (int i = 0; i < trailing; ++i) {
                    const uchar byte = uchar(next());
                    if (byte < 0x80 || byte > 0xbf ||
                        (i == 0 && ((c == 0xe0 && byte < 0xa0) || (c == 0xed && byte >= 0xa0) ||
                                    (c == 0xf0 && byte < 0x90) || (c == 0xf4 && byte >= 0x90)))) {
                        failed = true;
                        break;
                    }
                }
            }
        }
        failed = true;
        return {};
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
