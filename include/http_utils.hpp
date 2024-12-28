#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <string_view>
#include "spdlog/spdlog.h"

using StringMap = std::map<std::string, std::string>;

struct http11_header_parser {
    std::string m_header;
    std::string m_header_line;
    StringMap m_header_keys;
    std::string m_body;
    bool m_header_finished = false;

    void reset_state() {
        m_header.clear();
        m_header_line.clear();
        m_header_keys.clear();
        m_body.clear();
        m_header_finished = false;
    }

    [[nodiscard]] bool header_finished() const {
        return m_header_finished;
    }

    void _extract_headers() {
        std::string_view header = m_header;
        size_t pos = header.find("\r\n", 0, 2);
        m_header_line = std::string(header.substr(0, pos));
        while (pos != std::string::npos) {
            pos += 2;  // skip "\r\n"
            size_t next_pos = header.find("\r\n", pos, 2);
            size_t line_len = std::string::npos;
            if (next_pos != std::string::npos) {
                line_len = next_pos - pos;
            }
            std::string_view line = header.substr(pos, line_len);
            size_t colon = line.find(": ", 0, 2);
            if (colon != std::string::npos) {
                std::string key = std::string(line.substr(0, colon));
                std::string_view value = line.substr(colon + 2);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                m_header_keys.insert_or_assign(std::move(key), value);
            }
            pos = next_pos;
        }
        for (const auto& [key, value] : m_header_keys) {
            spdlog::info("Header key: {}, value: {}", key, value);
        }
    }

    void push_chunk(const std::string& chunk) {
        assert(!m_header_finished);
        spdlog::info("Pushing chunk in http11_header_parser: {}", chunk);
        size_t old_size = m_header.size();
        m_header.append(chunk);
        std::string_view header = m_header;
        if (old_size < 4) old_size = 4;
        old_size -= 4;
        size_t header_len = header.find("\r\n\r\n", old_size, 4);
        if (header_len != std::string::npos) {
            m_header_finished = true;
            m_body = header.substr(header_len + 4);
            m_header.resize(header_len);
            _extract_headers();
        }
        spdlog::info("Header finished: {}", m_header);
    }

    std::string& header_line() {
        return m_header_line;
    }

    StringMap& header_keys() {
        return m_header_keys;
    }

    std::string& header() {
        return m_header;
    }

    std::string& body() {
        return m_body;
    }
};

template <class HeaderParser = http11_header_parser>
struct http_request_parser {
    HeaderParser m_header_parser;
    size_t m_content_length = 0;
    size_t body_accumulated_size = 0;
    bool m_body_finished = false;

    void reset_state() {
        m_header_parser.reset_state();
        m_content_length = 0;
        m_body_finished = false;
    }

    [[nodiscard]] bool request_finished() const {
        return m_body_finished;
    }

    size_t _extract_content_length() {
        auto& headers = m_header_parser.header_keys();
        auto it = headers.find("content-length");
        if (it != headers.end()) {
            return std::stoul(it->second);
        }
        return 0;
    }

    std::string& header() {
        return m_header_parser.header();
    }

    std::string& body() {
        return m_header_parser.body();
    }

    void push_chunk(const std::string& chunk) {
        assert(!m_body_finished);
        if (!m_header_parser.header_finished()) {
            m_header_parser.push_chunk(chunk);
            if (m_header_parser.header_finished()) {
                body_accumulated_size = body().size();
                m_content_length = _extract_content_length();
                if (body_accumulated_size >= m_content_length) {
                    m_body_finished = true;
                }
            }
        } else {
            body().append(chunk);
            body_accumulated_size += chunk.size();
            if (body_accumulated_size >= m_content_length) {
                m_body_finished = true;
            }
        }
    }
};

#endif /* HTTP_UTILS_HPP */
