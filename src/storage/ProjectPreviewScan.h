#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <istream>
#include <string>

namespace PhotoCrap::Preview
{
struct Entry {
    std::string   json;
    std::uint64_t offset = 0, length = 0, bytesRead = 0;
    bool          escaped = false;
};

// Qt-free, bounded prefix reader shared by the app and native Finder extensions.
// Large pixel strings are scanned as spans, never accumulated in memory.
class Scan
{
public:
    explicit Scan(std::istream &input) : m_input(input)
    {
    }
    Entry read(bool legacy = false)
    {
        Entry result;
        if (peek() == 0xef) {
            if (next() != 0xef || next() != 0xbb || next() != 0xbf)
                return {};
        }
        if (!take('{'))
            return {};
        bool project = false;
        while (!m_failed && !take('}')) {
            const auto key = string(256);
            if (m_failed || !take(':'))
                return {};
            if (key == "format") {
                project = string(64) == "PhotoCrap";
                if (!project)
                    return {};
            } else if (key == "preview") {
                result.json = value(1024 * 1024);
                if (m_failed)
                    return {};
                if (project) {
                    result.bytesRead = m_read;
                    return result;
                }
            } else if (key == "layers") {
                if (!legacy || !project || !take('['))
                    return {};
                // An old file has no final composite. Return the first raster
                // source only; callers must identify this as an approximation.
                for (int layer = 0; layer < 512 && !take(']'); ++layer) {
                    if (!take('{'))
                        return {};
                    while (!m_failed && !take('}')) {
                        const auto field = string(256);
                        if (m_failed || !take(':'))
                            return {};
                        if (field == "png" && peekSpace() == '"') {
                            next();
                            result.offset = m_position;
                            bool escaped  = false;
                            skipString(escaped);
                            result.length  = m_position - result.offset - 1;
                            result.escaped = escaped;
                            if (m_failed)
                                return {};
                            if (result.length) {
                                result.bytesRead = m_read;
                                return result;
                            }
                        } else
                            value();
                        if (take('}'))
                            break;
                        if (!take(','))
                            return {};
                    }
                    if (take(']'))
                        break;
                    if (!take(','))
                        return {};
                }
                return {};
            } else
                value();
            if (take('}'))
                break;
            if (!take(','))
                return {};
        }
        if (!project || m_failed || result.json.empty())
            return {};
        result.bytesRead = m_read;
        return result;
    }

private:
    std::istream &                        m_input;
    std::array<char, 16384>               m_buffer{};
    std::size_t                           m_cursor = 0, m_count = 0;
    std::uint64_t                         m_position = 0, m_read = 0;
    bool                                  m_failed = false;
    std::chrono::steady_clock::time_point m_start  = std::chrono::steady_clock::now();
    int                                   peek()
    {
        if (m_failed)
            return -1;
        if (m_cursor == m_count) {
            if (m_read >= 512ULL * 1024 * 1024 ||
                std::chrono::steady_clock::now() - m_start > std::chrono::seconds(5)) {
                m_failed = true;
                return -1;
            }
            m_input.read(m_buffer.data(), m_buffer.size());
            m_count  = std::size_t(m_input.gcount());
            m_cursor = 0;
            m_read += m_count;
            if (!m_count) {
                m_failed = true;
                return -1;
            }
        }
        return static_cast<unsigned char>(m_buffer[m_cursor]);
    }
    int next()
    {
        const int c = peek();
        if (c >= 0) {
            ++m_cursor;
            ++m_position;
        }
        return c;
    }
    int peekSpace()
    {
        int c = peek();
        while (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            next();
            c = peek();
        }
        return c;
    }
    bool take(int c)
    {
        if (peekSpace() != c)
            return false;
        next();
        return true;
    }
    static int hex(int c)
    {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                      : -1;
    }
    void skipString(bool &escaped, std::string *text = nullptr, std::size_t limit = 0)
    {
        while (!m_failed) {
            int c = next();
            if (c == '"')
                return;
            if (c < 0x20)
                break;
            if (c == '\\') {
                escaped = true;
                c       = next();
                if (c == 'u') {
                    c = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int digit = hex(next());
                        if (digit < 0) {
                            m_failed = true;
                            return;
                        }
                        c = c * 16 + digit;
                    }
                } else if (c != '/' && c != '"' && c != '\\' && c != 'b' && c != 'f' && c != 'n' &&
                           c != 'r' && c != 't')
                    break;
            }
            if (text) {
                if (text->size() >= limit)
                    break;
                text->push_back(c < 128 ? char(c) : '?');
            }
        }
        m_failed = true;
    }
    std::string string(std::size_t limit)
    {
        if (!take('"')) {
            m_failed = true;
            return {};
        }
        std::string result;
        bool        escaped = false;
        skipString(escaped, &result, limit);
        return result;
    }
    std::string value(std::size_t limit = 0)
    {
        peekSpace();
        std::string         result;
        std::array<int, 64> closing{};
        int                 depth  = 0;
        bool                quoted = false, escaped = false, started = false;
        while (!m_failed) {
            const int c = peek();
            if (c < 0)
                break;
            if (started && !quoted && depth == 0 &&
                (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\r' || c == '\n' ||
                 c == '\t'))
                return result;
            next();
            started = true;
            if (limit) {
                if (result.size() >= limit)
                    break;
                result.push_back(char(c));
            }
            if (quoted) {
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"') {
                    quoted = false;
                    if (!depth)
                        return result;
                } else if (c < 0x20)
                    break;
            } else if (c == '"')
                quoted = true;
            else if (c == '{' || c == '[') {
                if (depth == int(closing.size()))
                    break;
                closing[depth++] = c == '{' ? '}' : ']';
            } else if (c == '}' || c == ']') {
                if (!depth || closing[--depth] != c)
                    break;
                if (!depth)
                    return result;
            }
        }
        m_failed = true;
        return {};
    }
};
} // namespace PhotoCrap::Preview
