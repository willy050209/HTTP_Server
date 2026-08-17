// filepath: include/httplib23.hpp
#ifndef HTTPLIB23_HPP
#define HTTPLIB23_HPP

// ============================================================================
// Platform Detection & Socket Abstraction Layer
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define HTTPLIB23_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
        #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #endif

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
    #include <crtdbg.h>

    #pragma comment(lib, "ws2_32.lib")

    using socket_t = SOCKET;
    using ssize_t = std::ptrdiff_t;
    inline constexpr socket_t invalid_socket = INVALID_SOCKET;
    inline constexpr int socket_error_val = SOCKET_ERROR;

    inline void close_socket(socket_t s) noexcept {
        if (s != invalid_socket) {
            ::closesocket(s);
        }
    }

    inline int get_last_socket_error() noexcept {
        return ::WSAGetLastError();
    }

    inline bool set_nonblocking(socket_t s) noexcept {
        u_long mode = 1;
        return ::ioctlsocket(s, FIONBIO, &mode) == 0;
    }
#else
    #define HTTPLIB23_PLATFORM_POSIX

    #include <sys/socket.h>
    #include <sys/types.h>
    #include <sys/uio.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    #include <errno.h>

    #if defined(__APPLE__) || defined(__MACH__)
        #define HTTPLIB23_PLATFORM_MACOS
        #include <sys/event.h>
        #include <sys/time.h>
    #elif defined(__linux__)
        #define HTTPLIB23_PLATFORM_LINUX
        #include <sys/epoll.h>
    #endif

    using socket_t = int;
    inline constexpr socket_t invalid_socket = -1;
    inline constexpr int socket_error_val = -1;

    inline void close_socket(socket_t s) noexcept {
        if (s != invalid_socket) {
            ::close(s);
        }
    }

    inline int get_last_socket_error() noexcept {
        return errno;
    }

    inline bool set_nonblocking(socket_t s) noexcept {
        const int flags = ::fcntl(s, F_GETFL, 0);
        if (flags == -1) return false;
        return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
    }
#endif

#ifdef DELETE
#undef DELETE
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <memory>
#include <sstream>
#include <charconv>
#include <expected>
#include <optional>
#include <span>
#include <chrono>
#include <algorithm>
#include <queue>
#include <concepts>
#include <type_traits>
#include <format>
#include <print>
#include <source_location>
#include <cstdint>
#include <stdexcept>

namespace httplib23 {

// ============================================================================
// NetworkContext (RAII Network Lifecycle Manager)
// ============================================================================

struct NetworkContext {
    NetworkContext() noexcept {
#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    ~NetworkContext() noexcept {
#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        WSACleanup();
#endif
    }
};

// ============================================================================
// Security Limits & Constants
// ============================================================================

static constexpr size_t MAX_HEADER_SIZE = 64 * 1024;            // 64 KB Max Header Size
static constexpr size_t MAX_BODY_SIZE = 10 * 1024 * 1024;       // 10 MB Max Body Size
static constexpr size_t MAX_QUEUE_SIZE = 10000;                // Max Bounded Queue Size
static constexpr std::chrono::seconds CONNECTION_TIMEOUT{15};  // 15s Slowloris Timeout

// ============================================================================
// 1. Core HTTP Enums & Types
// ============================================================================

/// <summary>
/// 定義 HTTP 協定狀態碼。
/// </summary>
enum class StatusCode : int32_t {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    Conflict = 409,
    PayloadTooLarge = 413,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503
};

/// <summary>
/// 根據 HTTP 狀態碼獲得標準文字描述。
/// </summary>
[[nodiscard]] inline constexpr std::string_view get_status_message(const int32_t code) noexcept {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default:  return "Unknown Status";
    }
}

[[nodiscard]] inline constexpr std::string_view get_status_message(const StatusCode status) noexcept {
    return get_status_message(static_cast<int32_t>(status));
}

/// <summary>
/// 定義 HTTP 請求動詞。
/// </summary>
enum class Method : uint8_t {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS,
    HEAD,
    UNKNOWN
};

[[nodiscard]] inline constexpr std::string_view method_to_string(const Method method) noexcept {
    switch (method) {
        case Method::GET:     return "GET";
        case Method::POST:    return "POST";
        case Method::PUT:     return "PUT";
        case Method::DELETE:  return "DELETE";
        case Method::PATCH:   return "PATCH";
        case Method::OPTIONS: return "OPTIONS";
        case Method::HEAD:    return "HEAD";
        default:              return "UNKNOWN";
    }
}

[[nodiscard]] inline constexpr Method string_to_method(const std::string_view str) noexcept {
    if (str == "GET")       return Method::GET;
    if (str == "POST")      return Method::POST;
    if (str == "PUT")       return Method::PUT;
    if (str == "DELETE")    return Method::DELETE;
    if (str == "PATCH")     return Method::PATCH;
    if (str == "OPTIONS")   return Method::OPTIONS;
    if (str == "HEAD")      return Method::HEAD;
    return Method::UNKNOWN;
}

/// <summary>
/// 不分大小寫的 HTTP Header 比較器。
/// </summary>
struct CaseInsensitiveCompare {
    [[nodiscard]] bool operator()(const std::string_view a, const std::string_view b) const noexcept {
        return std::ranges::equal(a, b, [](const char c1, const char c2) noexcept {
            return std::tolower(static_cast<uint8_t>(c1)) == std::tolower(static_cast<uint8_t>(c2));
        });
    }
};

/// <summary>
/// 不分大小寫的 HTTP Header Hash 計算器。
/// </summary>
struct CaseInsensitiveHash {
    [[nodiscard]] size_t operator()(const std::string_view str) const noexcept {
        size_t h = 0;
        for (const char c : str) {
            h = h * 31 + static_cast<size_t>(std::tolower(static_cast<uint8_t>(c)));
        }
        return h;
    }
};

using HeaderMap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveCompare>;

// ============================================================================
// 2. Utility Pure Functions
// ============================================================================

namespace detail {

[[nodiscard]] inline constexpr bool contains_crlf(const std::string_view str) noexcept {
    return str.find('\r') != std::string_view::npos || str.find('\n') != std::string_view::npos;
}

[[nodiscard]] inline constexpr std::string_view trim(const std::string_view str) noexcept {
    const size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return "";
    const size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

[[nodiscard]] inline std::string url_encode(const std::string_view str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (const char c : str) {
        const auto uc = static_cast<uint8_t>(c);
        if (std::isalnum(uc) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase << static_cast<int32_t>(uc);
        }
    }
    return escaped.str();
}

[[nodiscard]] inline std::string url_decode(const std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.size()) {
                const std::string_view hex_sv = str.substr(i + 1, 2);
                int32_t value = 0;
                const auto [ptr, ec] = std::from_chars(hex_sv.data(), hex_sv.data() + 2, value, 16);
                if (ec == std::errc{}) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

[[nodiscard]] inline std::map<std::string, std::string> parse_query_string(const std::string_view query_str) {
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start < query_str.size()) {
        const size_t end = query_str.find('&', start);
        const std::string_view pair = query_str.substr(start, (end == std::string_view::npos ? query_str.size() : end) - start);
        const size_t eq = pair.find('=');
        if (eq != std::string_view::npos) {
            std::string key = url_decode(pair.substr(0, eq));
            std::string val = url_decode(pair.substr(eq + 1));
            params[std::move(key)] = std::move(val);
        } else if (!pair.empty()) {
            std::string key = url_decode(pair);
            params[std::move(key)] = "";
        }
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return params;
}

[[nodiscard]] inline std::string escape_json(const std::string_view str) {
    std::string out;
    out.reserve(str.size() + 16);
    for (const char c : str) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<uint8_t>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<uint8_t>(c));
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

} // namespace detail

// ============================================================================
// 2.1 Asynchronous Logging Engine (Producer-Consumer Pattern)
// ============================================================================

enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3,
    OFF   = 4
};

[[nodiscard]] inline constexpr std::string_view level_to_string(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF:   return "OFF";
        default:              return "LOG";
    }
}

class Logger {
public:
    using LogCallback = std::function<void(LogLevel level, std::string_view msg)>;

private:
    std::atomic<LogLevel> m_level{LogLevel::INFO};
    LogCallback m_custom_sink = nullptr;
    std::queue<std::pair<LogLevel, std::string>> m_log_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_bg_thread;
    std::atomic<bool> m_running{true};

    [[nodiscard]] static std::string_view extract_filename(const std::string_view path) noexcept {
        const size_t pos = path.find_last_of("/\\");
        if (pos != std::string_view::npos) return path.substr(pos + 1);
        return path;
    }

    [[nodiscard]] static std::string get_current_timestamp() noexcept {
        const auto now = std::chrono::system_clock::now();
        return std::format("{:%Y-%m-%d %H:%M:%S}", now);
    }

public:
    static Logger& instance() noexcept {
        static Logger log;
        return log;
    }

    void set_level(const LogLevel level) noexcept {
        m_level.store(level, std::memory_order_relaxed);
    }

    [[nodiscard]] LogLevel get_level() const noexcept {
        return m_level.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool is_enabled(const LogLevel level) const noexcept {
        return level >= m_level.load(std::memory_order_relaxed);
    }

    void set_sink(LogCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_custom_sink = std::move(callback);
    }

    void log(const LogLevel level,
             const std::source_location& loc,
             const std::string_view msg) {
        if (!is_enabled(level)) return;

        std::string timestamp = get_current_timestamp();
        std::string final_log = std::format("[{}] [{}] [{}:{}] {}\n",
            timestamp,
            level_to_string(level),
            extract_filename(loc.file_name()),
            loc.line(),
            msg
        );

        push_to_queue(level, std::move(final_log));
    }

    void push_to_queue(const LogLevel level, std::string final_log) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_log_queue.emplace(level, std::move(final_log));
        }
        m_cv.notify_one();
    }

private:
    Logger() {
        m_bg_thread = std::thread([this]() {
            while (true) {
                std::pair<LogLevel, std::string> log_item;
                LogCallback current_sink = nullptr;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this]() {
                        return !m_running || !m_log_queue.empty();
                    });
                    if (!m_running && m_log_queue.empty()) break;

                    log_item = std::move(m_log_queue.front());
                    m_log_queue.pop();
                    current_sink = m_custom_sink;
                }

                if (current_sink) {
                    current_sink(log_item.first, log_item.second);
                } else {
                    std::print("{}", log_item.second);
                    std::fflush(stdout);
                }
            }
        });
    }

    ~Logger() {
        m_running = false;
        m_cv.notify_all();
        if (m_bg_thread.joinable()) {
            m_bg_thread.join();
        }
    }
};

template <typename... Args>
struct log_location_fmt {
    std::format_string<Args...> fmt;
    std::source_location loc;

    template <typename S>
        requires std::convertible_to<const S&, std::string_view>
    consteval log_location_fmt(const S& s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

template <typename... Args>
inline void log_debug(
    log_location_fmt<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
    if (!Logger::instance().is_enabled(LogLevel::DEBUG)) return;
    std::string formatted_msg = std::format(fmt_loc.fmt, std::forward<Args>(args)...);
    Logger::instance().log(LogLevel::DEBUG, fmt_loc.loc, formatted_msg);
}

template <typename... Args>
inline void log_info(
    log_location_fmt<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
    if (!Logger::instance().is_enabled(LogLevel::INFO)) return;
    std::string formatted_msg = std::format(fmt_loc.fmt, std::forward<Args>(args)...);
    Logger::instance().log(LogLevel::INFO, fmt_loc.loc, formatted_msg);
}

template <typename... Args>
inline void log_warn(
    log_location_fmt<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
    if (!Logger::instance().is_enabled(LogLevel::WARN)) return;
    std::string formatted_msg = std::format(fmt_loc.fmt, std::forward<Args>(args)...);
    Logger::instance().log(LogLevel::WARN, fmt_loc.loc, formatted_msg);
}

template <typename... Args>
inline void log_error(
    log_location_fmt<std::type_identity_t<Args>...> fmt_loc,
    Args&&... args) {
    if (!Logger::instance().is_enabled(LogLevel::ERROR)) return;
    std::string formatted_msg = std::format(fmt_loc.fmt, std::forward<Args>(args)...);
    Logger::instance().log(LogLevel::ERROR, fmt_loc.loc, formatted_msg);
}

// ============================================================================
// 3. Request & Response Objects
// ============================================================================

struct Request {
    Method method = Method::GET;
    std::string path;
    std::string raw_target;
    HeaderMap headers;
    std::string body;
    std::map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;

    [[nodiscard]] std::optional<std::string> get_header(const std::string_view key) const noexcept {
        const auto it = headers.find(std::string(key));
        if (it != headers.end()) return it->second;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> get_param(const std::string_view key) const noexcept {
        const auto it = query_params.find(std::string(key));
        if (it != query_params.end()) return it->second;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> get_path_param(const std::string_view key) const noexcept {
        const auto it = path_params.find(std::string(key));
        if (it != path_params.end()) return it->second;
        return std::nullopt;
    }
};

struct Response {
    int32_t status = 200;
    HeaderMap headers;
    std::string body;

    void set_content(const std::string_view content, const std::string_view content_type = "text/plain") {
        if (detail::contains_crlf(content_type)) {
            throw std::invalid_argument("CRLF injection detected in Content-Type");
        }
        body = std::string(content);
        headers["Content-Type"] = std::string(content_type);
    }

    void set_json(std::string json_content) {
        set_content(std::move(json_content), "application/json; charset=utf-8");
    }

    void set_header(std::string key, std::string value) {
        if (detail::contains_crlf(key) || detail::contains_crlf(value)) {
            throw std::invalid_argument("CRLF injection detected in Header key or value");
        }
        headers[std::move(key)] = std::move(value);
    }

    void set_redirect(const std::string_view location, const int32_t redirect_status = 302) {
        if (detail::contains_crlf(location)) {
            throw std::invalid_argument("CRLF injection detected in Redirect Location");
        }
        status = redirect_status;
        headers["Location"] = std::string(location);
    }

    [[nodiscard]] std::string serialize_headers() const {
        std::string res;
        res.reserve(256 + headers.size() * 64);
        res += std::format("HTTP/1.1 {} {}\r\n", status, get_status_message(status));

        bool has_content_length = false;
        bool has_server = false;
        bool has_connection = false;

        for (const auto& [k, v] : headers) {
            if (CaseInsensitiveCompare{}(k, "Content-Length")) has_content_length = true;
            if (CaseInsensitiveCompare{}(k, "Server")) has_server = true;
            if (CaseInsensitiveCompare{}(k, "Connection")) has_connection = true;
            res += std::format("{}: {}\r\n", k, v);
        }

        if (!has_content_length) {
            res += std::format("Content-Length: {}\r\n", body.size());
        }
        if (!has_server) {
            res += "Server: httplib23/1.0 (C++23 Modern Engine)\r\n";
        }
        if (!has_connection) {
            res += "Connection: close\r\n";
        }
        res += "\r\n";
        return res;
    }
};

using HandlerFunc = std::function<void(const Request&, Response&)>;

// ============================================================================
// 4. OpenAPI & Swagger UI / Scalar UI Documentation Generator
// ============================================================================

struct ParameterMeta {
    std::string name;
    std::string description;
    bool required = true;
    std::string in_type = "query"; // "query", "path", "header"
    std::string data_type = "string";
};

struct ResponseMeta {
    int32_t status_code = 200;
    std::string description;
    std::string content_type = "application/json";
};

struct RouteMeta {
    Method method = Method::GET;
    std::string pattern;
    std::string summary;
    std::string description;
    std::vector<std::string> tags;
    std::vector<ParameterMeta> parameters;
    std::vector<ResponseMeta> responses;
};

struct RouteEntry {
    RouteMeta meta;
    HandlerFunc handler;
};

class FluentRoute {
private:
    RouteMeta& m_meta;
    HandlerFunc& m_handler;

public:
    FluentRoute(RouteMeta& meta, HandlerFunc& handler) noexcept : m_meta(meta), m_handler(handler) {}

    FluentRoute& tag(std::string tag_name) {
        m_meta.tags.push_back(std::move(tag_name));
        return *this;
    }

    FluentRoute& summary(std::string sum) {
        m_meta.summary = std::move(sum);
        return *this;
    }

    FluentRoute& description(std::string desc) {
        m_meta.description = std::move(desc);
        return *this;
    }

    FluentRoute& param(std::string name, std::string description = "", bool required = true, std::string in_type = "query", std::string data_type = "string") {
        m_meta.parameters.push_back(ParameterMeta{
            .name = std::move(name),
            .description = std::move(description),
            .required = required,
            .in_type = std::move(in_type),
            .data_type = std::move(data_type)
        });
        return *this;
    }

    FluentRoute& response(int32_t status_code, std::string description, std::string content_type = "application/json") {
        m_meta.responses.push_back(ResponseMeta{
            .status_code = status_code,
            .description = std::move(description),
            .content_type = std::move(content_type)
        });
        return *this;
    }

    void handle(HandlerFunc fn) {
        m_handler = std::move(fn);
    }
};

class OpenApiGenerator {
public:
    [[nodiscard]] static std::string generate_spec(const std::vector<RouteEntry>& routes, const std::string_view title = "httplib23 API", const std::string_view version = "1.0.0") {
        std::string json;
        json.reserve(4096);
        json += std::format(R"({{
  "openapi": "3.0.3",
  "info": {{
    "title": "{}",
    "version": "{}"
  }},
  "paths": {{
)", detail::escape_json(title), detail::escape_json(version));

        std::map<std::string, std::vector<const RouteEntry*>> path_map;
        for (const auto& entry : routes) {
            path_map[entry.meta.pattern].push_back(&entry);
        }

        bool first_path = true;
        for (const auto& [path, entry_list] : path_map) {
            if (!first_path) json += ",\n";
            first_path = false;

            std::string openapi_path = path;
            size_t pos = 0;
            while ((pos = openapi_path.find(':', pos)) != std::string::npos) {
                size_t end = openapi_path.find('/', pos);
                if (end == std::string::npos) end = openapi_path.size();
                std::string param_name = openapi_path.substr(pos + 1, end - pos - 1);
                openapi_path.replace(pos, end - pos, std::format("{{{}}}", param_name));
                pos += param_name.size() + 2;
            }

            json += std::format("    \"{}\": {{\n", detail::escape_json(openapi_path));
            bool first_method = true;
            for (const auto* entry_ptr : entry_list) {
                const auto& meta = entry_ptr->meta;
                if (!first_method) json += ",\n";
                first_method = false;

                const std::string method_lower = [] (std::string_view s) {
                    std::string res;
                    for (char c : s) res += static_cast<char>(std::tolower(static_cast<uint8_t>(c)));
                    return res;
                }(method_to_string(meta.method));

                json += std::format("      \"{}\": {{\n", method_lower);
                json += std::format("        \"summary\": \"{}\",\n", detail::escape_json(meta.summary));
                json += std::format("        \"description\": \"{}\"", detail::escape_json(meta.description));

                if (!meta.tags.empty()) {
                    json += ",\n        \"tags\": [";
                    for (size_t i = 0; i < meta.tags.size(); ++i) {
                        if (i > 0) json += ", ";
                        json += std::format("\"{}\"", detail::escape_json(meta.tags[i]));
                    }
                    json += "]";
                }

                if (!meta.parameters.empty()) {
                    json += ",\n        \"parameters\": [\n";
                    for (size_t i = 0; i < meta.parameters.size(); ++i) {
                        const auto& p = meta.parameters[i];
                        if (i > 0) json += ",\n";
                        json += std::format(R"(          {{
            "name": "{}",
            "in": "{}",
            "required": {},
            "description": "{}",
            "schema": {{ "type": "{}" }}
          }})", detail::escape_json(p.name), detail::escape_json(p.in_type), (p.required ? "true" : "false"), detail::escape_json(p.description), detail::escape_json(p.data_type));
                    }
                    json += "\n        ]";
                }

                json += ",\n        \"responses\": {\n";
                if (meta.responses.empty()) {
                    json += "          \"200\": { \"description\": \"OK\" }\n";
                } else {
                    for (size_t i = 0; i < meta.responses.size(); ++i) {
                        const auto& r = meta.responses[i];
                        if (i > 0) json += ",\n";
                        json += std::format("          \"{}\": {{ \"description\": \"{}\" }}", r.status_code, detail::escape_json(r.description));
                    }
                    json += "\n";
                }
                json += "        }\n";
                json += "      }";
            }
            json += "\n    }";
        }
        json += "\n  }\n}";
        return json;
    }
};

class SwaggerDocGenerator {
public:
    [[nodiscard]] static std::string generate_html(const std::string_view openapi_url, const std::string_view title = "Swagger UI") {
        return std::format(R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>{}</title>
  <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css" />
  <style>
    html {{ box-sizing: border-box; overflow-y: scroll; }}
    *, *:before, *:after {{ box-sizing: inherit; }}
    body {{ margin:0; background: #fafafa; }}
  </style>
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js" charset="UTF-8"></script>
  <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-standalone-preset.js" charset="UTF-8"></script>
  <script>
    window.onload = function() {{
      window.ui = SwaggerUIBundle({{
        url: "{}",
        dom_id: '#swagger-ui',
        deepLinking: true,
        presets: [
          SwaggerUIBundle.presets.apis,
          SwaggerUIStandalonePreset
        ],
        plugins: [
          SwaggerUIBundle.plugins.DownloadUrl
        ],
        layout: "StandaloneLayout"
      }});
    }};
  </script>
</body>
</html>)", title, openapi_url);
    }
};

class ScalarDocGenerator {
public:
    [[nodiscard]] static std::string generate_html(const std::string_view openapi_url, const std::string_view title = "Scalar API Reference") {
        return std::format(R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>{}</title>
</head>
<body>
  <script id="api-reference" data-url="{}"></script>
  <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
</body>
</html>)", title, openapi_url);
    }
};

// ============================================================================
// 5. High Performance Router Engine
// ============================================================================

class Router {
private:
    struct Segment {
        std::string name;
        bool is_param = false;
    };

    struct RouteNode {
        std::string path_segment;
        bool is_param = false;
        std::string param_name;
        std::unordered_map<Method, RouteEntry> handlers;
        std::vector<std::unique_ptr<RouteNode>> children;
    };

    RouteNode m_root;
    std::vector<RouteEntry> m_routes_flat;

    [[nodiscard]] static std::vector<Segment> parse_path(const std::string_view path) {
        std::vector<Segment> segments;
        size_t start = 0;
        while (start < path.size()) {
            while (start < path.size() && path[start] == '/') ++start;
            if (start >= path.size()) break;
            const size_t end = path.find('/', start);
            const std::string_view seg_sv = path.substr(start, (end == std::string_view::npos ? path.size() : end) - start);
            
            Segment seg;
            if (seg_sv.starts_with('{') && seg_sv.ends_with('}')) {
                seg.is_param = true;
                seg.name = std::string(seg_sv.substr(1, seg_sv.size() - 2));
            } else if (seg_sv.starts_with(':')) {
                seg.is_param = true;
                seg.name = std::string(seg_sv.substr(1));
            } else {
                seg.is_param = false;
                seg.name = std::string(seg_sv);
            }
            segments.push_back(std::move(seg));
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
        return segments;
    }

public:
    RouteEntry& add_route(const Method method, std::string path, std::string summary = "") {
        if (path.empty() || path[0] != '/') {
            throw std::invalid_argument("Route path must start with '/'");
        }
        const auto segments = parse_path(path);
        RouteNode* current = &m_root;

        for (const auto& seg : segments) {
            RouteNode* found = nullptr;
            for (auto& child : current->children) {
                if (child->is_param == seg.is_param && (seg.is_param || child->path_segment == seg.name)) {
                    found = child.get();
                    break;
                }
            }

            if (!found) {
                auto new_node = std::make_unique<RouteNode>();
                new_node->is_param = seg.is_param;
                if (seg.is_param) {
                    new_node->param_name = seg.name;
                } else {
                    new_node->path_segment = seg.name;
                }
                found = new_node.get();
                current->children.push_back(std::move(new_node));
            }
            current = found;
        }

        RouteEntry entry;
        entry.meta.method = method;
        entry.meta.pattern = path;
        entry.meta.summary = std::move(summary);
        
        current->handlers[method] = entry;
        m_routes_flat.push_back(entry);
        return current->handlers[method];
    }

    [[nodiscard]] bool match(const Method method, const std::string_view path, HandlerFunc& out_handler, std::unordered_map<std::string, std::string>& out_params) const {
        const auto segments = parse_path(path);
        return match_recursive(&m_root, segments, 0, method, out_handler, out_params);
    }

    [[nodiscard]] std::vector<RouteEntry> get_routes() const {
        std::vector<RouteEntry> routes;
        collect_routes_recursive(&m_root, routes);
        return routes;
    }

private:
    void collect_routes_recursive(const RouteNode* node, std::vector<RouteEntry>& routes) const {
        for (const auto& [method, entry] : node->handlers) {
            routes.push_back(entry);
        }
        for (const auto& child : node->children) {
            collect_routes_recursive(child.get(), routes);
        }
    }

private:
    bool match_recursive(const RouteNode* current, const std::vector<Segment>& segments, const size_t index, const Method method, HandlerFunc& out_handler, std::unordered_map<std::string, std::string>& out_params) const {
        if (index == segments.size()) {
            const auto it = current->handlers.find(method);
            if (it != current->handlers.end()) {
                out_handler = it->second.handler;
                return true;
            }
            return false;
        }

        const auto& seg = segments[index];
        for (const auto& child : current->children) {
            if (!child->is_param && child->path_segment == seg.name) {
                if (match_recursive(child.get(), segments, index + 1, method, out_handler, out_params)) {
                    return true;
                }
            } else if (child->is_param) {
                out_params[child->param_name] = seg.name;
                if (match_recursive(child.get(), segments, index + 1, method, out_handler, out_params)) {
                    return true;
                }
                out_params.erase(child->param_name);
            }
        }
        return false;
    }
};

// ============================================================================
// 6. Cross-Platform I/O Multiplexer Engine Abstraction
// ============================================================================

namespace detail {

class IOMultiplexer {
public:
    virtual ~IOMultiplexer() = default;
    virtual bool add_socket(socket_t sock) = 0;
    virtual bool remove_socket(socket_t sock) = 0;
    virtual void poll_events(int timeout_ms, std::function<void(socket_t sock, bool is_read, bool is_write)> callback) = 0;
};

#if defined(HTTPLIB23_PLATFORM_LINUX)
class EpollMultiplexer : public IOMultiplexer {
private:
    int epoll_fd = -1;

public:
    EpollMultiplexer() {
        epoll_fd = ::epoll_create1(0);
    }
    ~EpollMultiplexer() override {
        if (epoll_fd != -1) {
            ::close(epoll_fd);
        }
    }

    bool add_socket(socket_t sock) override {
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET; // Edge-Triggered
        ev.data.fd = sock;
        return ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &ev) == 0;
    }

    bool remove_socket(socket_t sock) override {
        return ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock, nullptr) == 0;
    }

    void poll_events(int timeout_ms, std::function<void(socket_t sock, bool is_read, bool is_write)> callback) override {
        constexpr int MAX_EVENTS = 64;
        epoll_event events[MAX_EVENTS];
        const int nfds = ::epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
        for (int i = 0; i < nfds; ++i) {
            const socket_t sock = events[i].data.fd;
            const bool is_read = (events[i].events & EPOLLIN) != 0;
            const bool is_write = (events[i].events & EPOLLOUT) != 0;
            callback(sock, is_read, is_write);
        }
    }
};
#elif defined(HTTPLIB23_PLATFORM_MACOS)
class KqueueMultiplexer : public IOMultiplexer {
private:
    int kq_fd = -1;

public:
    KqueueMultiplexer() {
        kq_fd = ::kqueue();
    }
    ~KqueueMultiplexer() override {
        if (kq_fd != -1) {
            ::close(kq_fd);
        }
    }

    bool add_socket(socket_t sock) override {
        struct kevent ev;
        EV_SET(&ev, sock, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
        return ::kevent(kq_fd, &ev, 1, nullptr, 0, nullptr) == 0;
    }

    bool remove_socket(socket_t sock) override {
        struct kevent ev;
        EV_SET(&ev, sock, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
        return ::kevent(kq_fd, &ev, 1, nullptr, 0, nullptr) == 0;
    }

    void poll_events(int timeout_ms, std::function<void(socket_t sock, bool is_read, bool is_write)> callback) override {
        constexpr int MAX_EVENTS = 64;
        struct kevent events[MAX_EVENTS];
        struct timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;

        const int nev = ::kevent(kq_fd, nullptr, 0, events, MAX_EVENTS, (timeout_ms >= 0 ? &ts : nullptr));
        for (int i = 0; i < nev; ++i) {
            const socket_t sock = static_cast<socket_t>(events[i].ident);
            const bool is_read = (events[i].filter == EVFILT_READ);
            const bool is_write = (events[i].filter == EVFILT_WRITE);
            callback(sock, is_read, is_write);
        }
    }
};
#endif

} // namespace detail

struct DocOptions {
    bool enabled = true;         // 是否開啟全域 API 文件功能
    bool enable_swagger = true;  // 是否單獨啟用 Swagger UI 頁面
    bool enable_scalar = true;   // 是否單獨啟用 Scalar UI 頁面
    std::string openapi_path = "/openapi.json";
    std::string swagger_path = "/docs";
    std::string scalar_path = "/scalar";
    std::string title = "httplib23 API Documentation";
    std::string version = "1.0.0";
};

// ============================================================================
// 7. Thread Pool Engine (With Bounded Queue Size)
// ============================================================================

class ThreadPool {
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{false};
    size_t m_max_queue_size = MAX_QUEUE_SIZE;

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency(), const size_t max_queue_size = MAX_QUEUE_SIZE)
        : m_max_queue_size(max_queue_size) {
        threads = std::max<size_t>(threads, 2);
        for (size_t i = 0; i < threads; ++i) {
            m_workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->m_queue_mutex);
                        this->m_cv.wait(lock, [this]() {
                            return this->m_stop || !this->m_tasks.empty();
                        });
                        if (this->m_stop && this->m_tasks.empty()) return;
                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        m_stop = true;
        m_cv.notify_all();
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) worker.join();
        }
    }

    [[nodiscard]] bool enqueue(std::function<void()> task) noexcept {
        if (!task) return false;
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            if (m_tasks.size() >= m_max_queue_size) {
                return false;
            }
            m_tasks.push(std::move(task));
        }
        m_cv.notify_one();
        return true;
    }
};

// ============================================================================
// 8. Connection Session State Machine (Data-Race-Free Atomic Timestamp)
// ============================================================================

struct ConnectionSession {
    socket_t socket = invalid_socket;
    std::vector<char> rx_buffer;
    std::atomic<int64_t> last_active_ms{
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    };
    bool header_parsed = false;
    size_t content_length = 0;
    size_t header_length = 0;

    void touch() noexcept {
        last_active_ms.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count(),
            std::memory_order_relaxed
        );
    }

    [[nodiscard]] int64_t get_last_active_ms() const noexcept {
        return last_active_ms.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// 9. Server Implementation (IOCP / epoll / kqueue)
// ============================================================================

class Server {
public:
    using MiddlewareFunc = std::function<bool(Request&, Response&)>;

private:
    NetworkContext m_net_ctx;
    socket_t m_listen_socket = invalid_socket;
    std::atomic<bool> m_running{false};
    Router m_router;
    std::vector<MiddlewareFunc> m_middlewares;
    std::unique_ptr<ThreadPool> m_pool;
    std::thread m_accept_thread;
    std::thread m_watchdog_thread;
    DocOptions m_doc_options;

    std::mutex m_session_mutex;
    std::unordered_map<socket_t, std::shared_ptr<ConnectionSession>> m_sessions;

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
    HANDLE m_iocp = INVALID_HANDLE_VALUE;
    std::vector<std::thread> m_iocp_threads;

    enum class IOOperation : uint8_t { READ, WRITE };

    struct PerIoData {
        WSAOVERLAPPED overlapped;
        WSABUF wsa_bufs[2];
        char buffer[8192];
        IOOperation op_type;
        socket_t socket;
        std::string send_header_buf;
        std::string send_body_buf;
    };
#else
    std::unique_ptr<detail::IOMultiplexer> m_multiplexer;
    std::thread m_poll_thread;
#endif

public:
    Server() = default;

    ~Server() {
        stop();
    }

    Server& set_doc_options(DocOptions options) noexcept {
        m_doc_options = std::move(options);
        return *this;
    }

    Server& enable_docs(const bool enable = true) noexcept {
        m_doc_options.enabled = enable;
        return *this;
    }

    Server& enable_swagger(const bool enable = true) noexcept {
        m_doc_options.enable_swagger = enable;
        return *this;
    }

    Server& enable_scalar(const bool enable = true) noexcept {
        m_doc_options.enable_scalar = enable;
        return *this;
    }

    [[nodiscard]] const DocOptions& get_doc_options() const noexcept {
        return m_doc_options;
    }

    FluentRoute Get(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::GET, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Post(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::POST, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Put(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::PUT, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Delete(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::DELETE, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Patch(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::PATCH, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Options(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::OPTIONS, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    FluentRoute Head(std::string path, std::string summary = "") {
        auto& entry = m_router.add_route(Method::HEAD, std::move(path), std::move(summary));
        return FluentRoute(entry.meta, entry.handler);
    }

    void Use(MiddlewareFunc mw) {
        if (!mw) {
            throw std::invalid_argument("Middleware cannot be empty");
        }
        m_middlewares.push_back(std::move(mw));
    }

    bool listen(const std::string& host, const uint16_t port) {
        if (m_doc_options.enabled) {
            if (!m_doc_options.openapi_path.empty()) {
                const std::string title = m_doc_options.title;
                const std::string version = m_doc_options.version;
                m_router.add_route(Method::GET, m_doc_options.openapi_path, "Get OpenAPI 3.0 JSON Specification").handler = [this, title, version](const Request&, Response& res) {
                    res.set_json(OpenApiGenerator::generate_spec(m_router.get_routes(), title, version));
                };
            }

            if (m_doc_options.enable_swagger && !m_doc_options.swagger_path.empty() && !m_doc_options.openapi_path.empty()) {
                const std::string openapi_url = m_doc_options.openapi_path;
                const std::string title = m_doc_options.title + " - Swagger UI";
                m_router.add_route(Method::GET, m_doc_options.swagger_path, "Interactive Swagger API Documentation").handler = [openapi_url, title](const Request&, Response& res) {
                    res.set_content(SwaggerDocGenerator::generate_html(openapi_url, title), "text/html; charset=utf-8");
                };
            }

            if (m_doc_options.enable_scalar && !m_doc_options.scalar_path.empty() && !m_doc_options.openapi_path.empty()) {
                const std::string openapi_url = m_doc_options.openapi_path;
                const std::string title = m_doc_options.title + " - Scalar UI";
                m_router.add_route(Method::GET, m_doc_options.scalar_path, "Interactive Scalar API Documentation").handler = [openapi_url, title](const Request&, Response& res) {
                    res.set_content(ScalarDocGenerator::generate_html(openapi_url, title), "text/html; charset=utf-8");
                };
            }
        }

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (m_iocp == NULL) return false;

        m_listen_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (m_listen_socket == invalid_socket) return false;

        BOOL reuse = TRUE;
        setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == socket_error_val) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
            return false;
        }

        if (::listen(m_listen_socket, SOMAXCONN) == socket_error_val) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
            return false;
        }

        m_running = true;
        m_pool = std::make_unique<ThreadPool>();

        const size_t thread_count = std::max<size_t>(2, std::thread::hardware_concurrency());
        for (size_t i = 0; i < thread_count; ++i) {
            m_iocp_threads.emplace_back([this]() { iocp_worker_loop(); });
        }

        m_accept_thread = std::thread([this]() { accept_loop(); });
        m_watchdog_thread = std::thread([this]() { timeout_watchdog_loop(); });
        return true;
#else
        m_listen_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listen_socket == invalid_socket) return false;

        int reuse = 1;
        ::setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        set_nonblocking(m_listen_socket);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        if (::bind(m_listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == socket_error_val) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
            return false;
        }

        if (::listen(m_listen_socket, SOMAXCONN) == socket_error_val) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
            return false;
        }

#if defined(HTTPLIB23_PLATFORM_LINUX)
        m_multiplexer = std::make_unique<detail::EpollMultiplexer>();
#elif defined(HTTPLIB23_PLATFORM_MACOS)
        m_multiplexer = std::make_unique<detail::KqueueMultiplexer>();
#endif
        if (!m_multiplexer || !m_multiplexer->add_socket(m_listen_socket)) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
            return false;
        }

        m_running = true;
        m_pool = std::make_unique<ThreadPool>();
        m_poll_thread = std::thread([this]() { posix_poll_loop(); });
        m_watchdog_thread = std::thread([this]() { timeout_watchdog_loop(); });
        return true;
#endif
    }

    void stop() noexcept {
        if (!m_running) return;
        m_running = false;

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        if (m_iocp != INVALID_HANDLE_VALUE) {
            for (size_t i = 0; i < m_iocp_threads.size(); ++i) {
                PostQueuedCompletionStatus(m_iocp, 0, 0, NULL);
            }
        }
#endif

        if (m_listen_socket != invalid_socket) {
            close_socket(m_listen_socket);
            m_listen_socket = invalid_socket;
        }

        if (m_accept_thread.joinable()) m_accept_thread.join();
        if (m_watchdog_thread.joinable()) m_watchdog_thread.join();

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        for (auto& th : m_iocp_threads) {
            if (th.joinable()) th.join();
        }
        m_iocp_threads.clear();
#else
        if (m_poll_thread.joinable()) m_poll_thread.join();
#endif

        {
            std::lock_guard<std::mutex> lock(m_session_mutex);
            for (const auto& [sock, session] : m_sessions) {
                close_socket(sock);
            }
            m_sessions.clear();
        }

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        if (m_iocp != INVALID_HANDLE_VALUE) {
            CloseHandle(m_iocp);
            m_iocp = INVALID_HANDLE_VALUE;
        }
#else
        m_multiplexer.reset();
#endif

        m_pool.reset();
    }

private:
#if defined(HTTPLIB23_PLATFORM_WINDOWS)
    void accept_loop() noexcept {
        while (m_running) {
            sockaddr_in client_addr{};
            int32_t addr_len = sizeof(client_addr);
            const socket_t client_socket = accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
            if (client_socket == invalid_socket) {
                if (!m_running) break;
                continue;
            }

            if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), m_iocp, static_cast<ULONG_PTR>(client_socket), 0) == NULL) {
                close_socket(client_socket);
                continue;
            }

            auto session = std::make_shared<ConnectionSession>();
            session->socket = client_socket;
            {
                std::lock_guard<std::mutex> lock(m_session_mutex);
                m_sessions[client_socket] = session;
            }

            auto io_data = std::make_unique<PerIoData>();
            ZeroMemory(&io_data->overlapped, sizeof(OVERLAPPED));
            io_data->op_type = IOOperation::READ;
            io_data->socket = client_socket;
            io_data->wsa_bufs[0].buf = io_data->buffer;
            io_data->wsa_bufs[0].len = sizeof(io_data->buffer);

            DWORD flags = 0;
            DWORD bytes_recv = 0;
            PerIoData* raw_ptr = io_data.release();
            if (WSARecv(client_socket, &raw_ptr->wsa_bufs[0], 1, &bytes_recv, &flags, &raw_ptr->overlapped, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    remove_session(client_socket);
                    delete raw_ptr;
                }
            }
        }
    }
#else
    void posix_poll_loop() noexcept {
        while (m_running) {
            m_multiplexer->poll_events(50, [this](socket_t fd, bool is_read, bool /*is_write*/) {
                if (!m_running) return;

                if (fd == m_listen_socket) {
                    if (is_read) {
                        while (true) {
                            sockaddr_in client_addr{};
                            socklen_t addr_len = sizeof(client_addr);
                            const socket_t client_sock = ::accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
                            if (client_sock == invalid_socket) {
                                break;
                            }

                            set_nonblocking(client_sock);
#if defined(HTTPLIB23_PLATFORM_MACOS)
                            int opt = 1;
                            ::setsockopt(client_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

                            auto session = std::make_shared<ConnectionSession>();
                            session->socket = client_sock;
                            {
                                std::lock_guard<std::mutex> lock(m_session_mutex);
                                m_sessions[client_sock] = session;
                            }

                            m_multiplexer->add_socket(client_sock);
                        }
                    }
                } else {
                    if (is_read) {
                        std::shared_ptr<ConnectionSession> session;
                        {
                            std::lock_guard<std::mutex> lock(m_session_mutex);
                            const auto it = m_sessions.find(fd);
                            if (it != m_sessions.end()) {
                                session = it->second;
                            }
                        }

                        if (!session) return;

                        session->touch();
                        char buf[8192];
                        bool client_closed = false;

                        while (true) {
                            const ssize_t bytes_read = ::recv(fd, buf, sizeof(buf), 0);
                            if (bytes_read > 0) {
                                session->rx_buffer.insert(session->rx_buffer.end(), buf, buf + bytes_read);
                            } else if (bytes_read == 0) {
                                client_closed = true;
                                break;
                            } else {
                                const int err = get_last_socket_error();
                                if (err == EAGAIN || err == EWOULDBLOCK) {
                                    break;
                                } else {
                                    client_closed = true;
                                    break;
                                }
                            }
                        }

                        if (client_closed) {
                            remove_session(fd);
                            return;
                        }

                        bool should_disconnect = false;
                        while (true) {
                            const std::string_view rx_sv(session->rx_buffer.data(), session->rx_buffer.size());
                            
                            if (!session->header_parsed) {
                                const size_t header_end = rx_sv.find("\r\n\r\n");
                                if (header_end != std::string_view::npos) {
                                    session->header_length = header_end + 4;
                                    session->header_parsed = true;
                                    const std::string_view header_sv = rx_sv.substr(0, header_end);
                                    session->content_length = parse_content_length(header_sv);

                                    if (session->content_length > MAX_BODY_SIZE || session->header_length > MAX_HEADER_SIZE) {
                                        send_error_response(fd, 413, "Payload Too Large");
                                        should_disconnect = true;
                                        break;
                                    }
                                } else {
                                    if (session->rx_buffer.size() > MAX_HEADER_SIZE) {
                                        send_error_response(fd, 413, "Header Size Exceeds Limit");
                                        should_disconnect = true;
                                    }
                                    break;
                                }
                            }

                            if (session->header_parsed) {
                                const size_t total_expected = session->header_length + session->content_length;
                                if (session->rx_buffer.size() >= total_expected) {
                                    std::string full_request(session->rx_buffer.data(), total_expected);
                                    session->rx_buffer.erase(session->rx_buffer.begin(), session->rx_buffer.begin() + total_expected);
                                    session->header_parsed = false;
                                    session->content_length = 0;
                                    session->header_length = 0;

                                    const bool queued = m_pool->enqueue([this, fd, req_str = std::move(full_request)]() {
                                        handle_http_request(fd, req_str);
                                    });

                                    if (!queued) {
                                        send_error_response(fd, 503, "Service Unavailable - Server Busy");
                                        should_disconnect = true;
                                        break;
                                    }
                                } else {
                                    break;
                                }
                            }
                        }

                        if (should_disconnect) {
                            remove_session(fd);
                        }
                    }
                }
            });
        }
    }
#endif

    void timeout_watchdog_loop() noexcept {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!m_running) break;

            const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            const int64_t timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(CONNECTION_TIMEOUT).count();

            std::vector<socket_t> timed_out_sockets;
            {
                std::lock_guard<std::mutex> lock(m_session_mutex);
                for (const auto& [sock, session] : m_sessions) {
                    if (now_ms - session->get_last_active_ms() > timeout_ms) {
                        timed_out_sockets.push_back(sock);
                    }
                }
            }

            for (const socket_t sock : timed_out_sockets) {
                remove_session(sock);
            }
        }
    }

    void remove_session(const socket_t sock) noexcept {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        if (m_sessions.contains(sock)) {
#if defined(HTTPLIB23_PLATFORM_POSIX)
            if (m_multiplexer) {
                m_multiplexer->remove_socket(sock);
            }
#endif
            char dummy[512];
            set_nonblocking(sock);
            while (::recv(sock, dummy, sizeof(dummy), 0) > 0) {}
            close_socket(sock);
            m_sessions.erase(sock);
        }
    }

#if defined(HTTPLIB23_PLATFORM_WINDOWS)
    void iocp_worker_loop() noexcept {
        DWORD bytes_transferred = 0;
        ULONG_PTR completion_key = 0;
        LPOVERLAPPED overlapped = NULL;

        while (m_running) {
            const BOOL result = GetQueuedCompletionStatus(
                m_iocp,
                &bytes_transferred,
                &completion_key,
                &overlapped,
                INFINITE
            );

            if (!m_running && overlapped == NULL) break;

            if (!result || bytes_transferred == 0) {
                if (overlapped) {
                    PerIoData* io_data = CONTAINING_RECORD(overlapped, PerIoData, overlapped);
                    remove_session(io_data->socket);
                    delete io_data;
                }
                continue;
            }

            PerIoData* io_data = CONTAINING_RECORD(overlapped, PerIoData, overlapped);
            const socket_t client_socket = io_data->socket;

            if (io_data->op_type == IOOperation::READ) {
                std::shared_ptr<ConnectionSession> session;
                {
                    std::lock_guard<std::mutex> lock(m_session_mutex);
                    const auto it = m_sessions.find(client_socket);
                    if (it != m_sessions.end()) {
                        session = it->second;
                    }
                }

                if (!session) {
                    delete io_data;
                    continue;
                }

                session->touch();
                session->rx_buffer.insert(session->rx_buffer.end(), io_data->buffer, io_data->buffer + bytes_transferred);
                delete io_data;

                bool should_disconnect = false;
                while (true) {
                    const std::string_view rx_sv(session->rx_buffer.data(), session->rx_buffer.size());
                    
                    if (!session->header_parsed) {
                        const size_t header_end = rx_sv.find("\r\n\r\n");
                        if (header_end != std::string_view::npos) {
                            session->header_length = header_end + 4;
                            session->header_parsed = true;
                            const std::string_view header_sv = rx_sv.substr(0, header_end);
                            session->content_length = parse_content_length(header_sv);

                            if (session->content_length > MAX_BODY_SIZE || session->header_length > MAX_HEADER_SIZE) {
                                send_error_response(client_socket, 413, "Payload Too Large");
                                should_disconnect = true;
                                break;
                            }
                        } else {
                            if (session->rx_buffer.size() > MAX_HEADER_SIZE) {
                                send_error_response(client_socket, 413, "Header Size Exceeds Limit");
                                should_disconnect = true;
                            }
                            break;
                        }
                    }

                    if (session->header_parsed) {
                        const size_t total_expected = session->header_length + session->content_length;
                        if (session->rx_buffer.size() >= total_expected) {
                            std::string full_request(session->rx_buffer.data(), total_expected);
                            session->rx_buffer.erase(session->rx_buffer.begin(), session->rx_buffer.begin() + total_expected);
                            session->header_parsed = false;
                            session->content_length = 0;
                            session->header_length = 0;

                            const bool queued = m_pool->enqueue([this, client_socket, req_str = std::move(full_request)]() {
                                handle_http_request(client_socket, req_str);
                            });

                            if (!queued) {
                                send_error_response(client_socket, 503, "Service Unavailable - Server Busy");
                                should_disconnect = true;
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                }

                if (should_disconnect) {
                    remove_session(client_socket);
                    continue;
                }

                auto next_io = std::make_unique<PerIoData>();
                ZeroMemory(&next_io->overlapped, sizeof(OVERLAPPED));
                next_io->op_type = IOOperation::READ;
                next_io->socket = client_socket;
                next_io->wsa_bufs[0].buf = next_io->buffer;
                next_io->wsa_bufs[0].len = sizeof(next_io->buffer);

                DWORD flags = 0;
                DWORD next_recv = 0;
                PerIoData* raw_next = next_io.release();
                if (WSARecv(client_socket, &raw_next->wsa_bufs[0], 1, &next_recv, &flags, &raw_next->overlapped, NULL) == SOCKET_ERROR) {
                    if (WSAGetLastError() != ERROR_IO_PENDING) {
                        remove_session(client_socket);
                        delete raw_next;
                    }
                }
            } else if (io_data->op_type == IOOperation::WRITE) {
                shutdown(client_socket, SD_SEND);
                remove_session(client_socket);
                delete io_data;
            }
        }
    }
#endif

    [[nodiscard]] static size_t parse_content_length(const std::string_view header_sv) noexcept {
        size_t pos = 0;
        while (pos < header_sv.size()) {
            size_t line_end = header_sv.find("\r\n", pos);
            if (line_end == std::string_view::npos) line_end = header_sv.size();
            const std::string_view line = header_sv.substr(pos, line_end - pos);
            const size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                const std::string_view key = detail::trim(line.substr(0, colon));
                if (CaseInsensitiveCompare{}(key, "Content-Length")) {
                    const std::string_view val = detail::trim(line.substr(colon + 1));
                    size_t len = 0;
                    const auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), len);
                    if (ec == std::errc{}) return len;
                }
            }
            pos = line_end + 2;
        }
        return 0;
    }

    void send_error_response(const socket_t client_socket, const int32_t status_code, const std::string_view msg) noexcept {
        Response res;
        res.status = status_code;
        res.set_content(msg, "text/plain");
        send_response_scatter(client_socket, res);
    }

    void send_response_scatter(const socket_t client_socket, const Response& res) noexcept {
#if defined(HTTPLIB23_PLATFORM_WINDOWS)
        auto send_io = std::make_unique<PerIoData>();
        ZeroMemory(&send_io->overlapped, sizeof(OVERLAPPED));
        send_io->op_type = IOOperation::WRITE;
        send_io->socket = client_socket;

        send_io->send_header_buf = res.serialize_headers();
        send_io->send_body_buf = res.body;

        send_io->wsa_bufs[0].buf = send_io->send_header_buf.data();
        send_io->wsa_bufs[0].len = static_cast<ULONG>(send_io->send_header_buf.size());

        send_io->wsa_bufs[1].buf = send_io->send_body_buf.data();
        send_io->wsa_bufs[1].len = static_cast<ULONG>(send_io->send_body_buf.size());

        DWORD bytes_sent = 0;
        PerIoData* raw_send = send_io.release();
        if (WSASend(client_socket, raw_send->wsa_bufs, 2, &bytes_sent, 0, &raw_send->overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != ERROR_IO_PENDING) {
                remove_session(client_socket);
                delete raw_send;
            }
        }
#else
        std::string header_buf = res.serialize_headers();
        struct iovec iov[2];
        iov[0].iov_base = const_cast<char*>(header_buf.data());
        iov[0].iov_len = header_buf.size();
        iov[1].iov_base = const_cast<char*>(res.body.data());
        iov[1].iov_len = res.body.size();

        ::writev(client_socket, iov, 2);
        ::shutdown(client_socket, SHUT_WR);
        remove_session(client_socket);
#endif
    }

    void handle_http_request(const socket_t client_socket, const std::string& request_str) noexcept {
        Request req;
        Response res;

        const std::string_view req_sv(request_str);
        const size_t req_line_end = req_sv.find("\r\n");
        if (req_line_end != std::string_view::npos) {
            const std::string_view req_line = req_sv.substr(0, req_line_end);
            const size_t sp1 = req_line.find(' ');
            if (sp1 != std::string_view::npos) {
                const size_t sp2 = req_line.find(' ', sp1 + 1);
                if (sp2 != std::string_view::npos) {
                    const std::string_view method_str = req_line.substr(0, sp1);
                    const std::string_view target = req_line.substr(sp1 + 1, sp2 - sp1 - 1);
                    
                    req.method = string_to_method(method_str);
                    req.raw_target = std::string(target);

                    const size_t query_pos = target.find('?');
                    if (query_pos != std::string_view::npos) {
                        req.path = std::string(target.substr(0, query_pos));
                        req.query_params = detail::parse_query_string(target.substr(query_pos + 1));
                    } else {
                        req.path = std::string(target);
                    }
                }
            }
        }

        size_t header_pos = req_line_end + 2;
        const size_t header_end_all = req_sv.find("\r\n\r\n");
        while (header_pos < header_end_all && header_pos != std::string_view::npos) {
            size_t line_end = req_sv.find("\r\n", header_pos);
            if (line_end == std::string_view::npos || line_end > header_end_all) line_end = header_end_all;
            const std::string_view header_line = req_sv.substr(header_pos, line_end - header_pos);
            const size_t colon = header_line.find(':');
            if (colon != std::string_view::npos) {
                std::string key(detail::trim(header_line.substr(0, colon)));
                std::string val(detail::trim(header_line.substr(colon + 1)));
                req.headers[std::move(key)] = std::move(val);
            }
            header_pos = line_end + 2;
        }

        if (header_end_all != std::string_view::npos && header_end_all + 4 <= req_sv.size()) {
            req.body = std::string(req_sv.substr(header_end_all + 4));
        }

        bool proceed = true;
        for (const auto& mw : m_middlewares) {
            if (!mw(req, res)) {
                proceed = false;
                break;
            }
        }

        if (proceed) {
            HandlerFunc handler;
            std::unordered_map<std::string, std::string> path_params;
            if (m_router.match(req.method, req.path, handler, path_params)) {
                req.path_params = std::move(path_params);
                try {
                    handler(req, res);
                } catch (const std::exception& ex) {
                    res.status = 500;
                    res.set_content(std::format("500 Internal Server Error: {}", ex.what()), "text/plain; charset=utf-8");
                } catch (...) {
                    res.status = 500;
                    res.set_content("500 Internal Server Error: Unknown Exception", "text/plain; charset=utf-8");
                }
            } else {
                res.status = 404;
                res.set_content("404 Not Found", "text/plain");
            }
        }

        send_response_scatter(client_socket, res);
    }
};

// ============================================================================
// 10. HTTP Client Engine
// ============================================================================

class Client {
private:
    std::string m_host;
    uint16_t m_port = 80;
    NetworkContext m_net_ctx;

public:
    explicit Client(std::string host_or_url, const uint16_t port = 80) : m_host(std::move(host_or_url)), m_port(port) {
        if (m_host.starts_with("http://")) {
            m_host = m_host.substr(7);
        } else if (m_host.starts_with("https://")) {
            m_host = m_host.substr(8);
        }

        const size_t slash_pos = m_host.find('/');
        if (slash_pos != std::string::npos) {
            m_host = m_host.substr(0, slash_pos);
        }

        const size_t colon = m_host.find(':');
        if (colon != std::string::npos) {
            uint16_t parsed_port = 0;
            const std::string_view port_sv(m_host.data() + colon + 1, m_host.size() - colon - 1);
            const auto [ptr, ec] = std::from_chars(port_sv.data(), port_sv.data() + port_sv.size(), parsed_port);
            if (ec == std::errc{}) {
                m_port = parsed_port;
            }
            m_host = m_host.substr(0, colon);
        }
    }

    ~Client() = default;

    [[nodiscard]] std::expected<Response, std::string> Get(const std::string_view path, const HeaderMap& headers = {}) noexcept {
        return send_request(Method::GET, path, "", "", headers);
    }

    [[nodiscard]] std::expected<Response, std::string> Post(const std::string_view path, const std::string_view body, const std::string_view content_type = "application/json", const HeaderMap& headers = {}) noexcept {
        return send_request(Method::POST, path, body, content_type, headers);
    }

    [[nodiscard]] std::expected<Response, std::string> Put(const std::string_view path, const std::string_view body, const std::string_view content_type = "application/json", const HeaderMap& headers = {}) noexcept {
        return send_request(Method::PUT, path, body, content_type, headers);
    }

    [[nodiscard]] std::expected<Response, std::string> Delete(const std::string_view path, const HeaderMap& headers = {}) noexcept {
        return send_request(Method::DELETE, path, "", "", headers);
    }

    [[nodiscard]] std::expected<Response, std::string> send_request(const Method method, const std::string_view path, const std::string_view body = "", const std::string_view content_type = "", const HeaderMap& custom_headers = {}) noexcept {
        for (int32_t attempt = 0; attempt < 10; ++attempt) {
            auto res = send_request_once(method, path, body, content_type, custom_headers);
            if (res.has_value()) return res;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return std::unexpected("Failed to receive response after retries");
    }

private:
    [[nodiscard]] std::expected<Response, std::string> send_request_once(const Method method, const std::string_view path, const std::string_view body, const std::string_view content_type, const HeaderMap& custom_headers) noexcept {
        const socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == invalid_socket) {
            return std::unexpected("Failed to create socket");
        }

        int reuse = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        const std::string port_str = std::to_string(m_port);
        if (::getaddrinfo(m_host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
            close_socket(sock);
            return std::unexpected("Failed to resolve host address");
        }

        bool connected = false;
        for (int32_t retry = 0; retry < 10; ++retry) {
            if (::connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) != socket_error_val) {
                connected = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!connected) {
            ::freeaddrinfo(res);
            close_socket(sock);
            return std::unexpected("Failed to connect to host");
        }
        ::freeaddrinfo(res);

        std::string req_str;
        req_str += std::format("{} {} HTTP/1.1\r\n", method_to_string(method), path);
        req_str += std::format("Host: {}\r\n", m_host);
        req_str += "User-Agent: httplib23-Client/1.0\r\n";
        req_str += "Connection: close\r\n";

        if (!body.empty()) {
            if (!content_type.empty()) req_str += std::format("Content-Type: {}\r\n", content_type);
            req_str += std::format("Content-Length: {}\r\n", body.size());
        }

        for (const auto& [k, v] : custom_headers) {
            req_str += std::format("{}: {}\r\n", k, v);
        }
        req_str += "\r\n";
        req_str += body;

#if defined(HTTPLIB23_PLATFORM_LINUX)
        const ssize_t send_res = ::send(sock, req_str.c_str(), static_cast<int>(req_str.size()), MSG_NOSIGNAL);
#else
        const ssize_t send_res = ::send(sock, req_str.c_str(), static_cast<int>(req_str.size()), 0);
#endif
        if (send_res == socket_error_val) {
            close_socket(sock);
            return std::unexpected("Failed to send HTTP request");
        }

        std::string raw_response;
        char buffer[4096];
        ssize_t bytes_read = 0;
        while ((bytes_read = ::recv(sock, buffer, static_cast<int>(sizeof(buffer)), 0)) > 0) {
            raw_response.append(buffer, bytes_read);
        }
        close_socket(sock);

        if (raw_response.empty()) {
            return std::unexpected("Empty response received from server");
        }

        Response response;
        std::istringstream stream(raw_response);
        std::string status_line;
        if (std::getline(stream, status_line)) {
            status_line = std::string(detail::trim(status_line));
            std::istringstream line_stream(status_line);
            std::string http_ver;
            line_stream >> http_ver >> response.status;
        }

        std::string header_line;
        while (std::getline(stream, header_line)) {
            header_line = std::string(detail::trim(header_line));
            if (header_line.empty()) break;
            const size_t colon = header_line.find(':');
            if (colon != std::string_view::npos) {
                std::string key(detail::trim(header_line.substr(0, colon)));
                std::string val(detail::trim(header_line.substr(colon + 1)));
                response.headers[key] = val;
            }
        }

        std::string body_content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        response.body = std::move(body_content);

        return response;
    }
};

} // namespace httplib23

#endif // HTTPLIB23_HPP
