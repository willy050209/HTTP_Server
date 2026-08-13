// filepath: include/httplib23.hpp
#ifndef HTTPLIB23_HPP
#define HTTPLIB23_HPP

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

#ifdef DELETE
#undef DELETE
#endif

#pragma comment(lib, "ws2_32.lib")

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
#include <format>
#include <cstdint>
#include <stdexcept>
#include <crtdbg.h>

namespace httplib23 {

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
/// <param name="code">HTTP 狀態碼數值。</param>
/// <returns>對應之狀態字串 view。</returns>
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

/// <summary>
/// 根據 StatusCode 列舉獲得標準文字描述。
/// </summary>
/// <param name="status">StatusCode 列舉值。</param>
/// <returns>對應之狀態字串 view。</returns>
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

/// <summary>
/// 將 Method 列舉轉為字串描述。
/// </summary>
/// <param name="method">Method 列舉值。</param>
/// <returns>HTTP 動詞字串。</returns>
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

/// <summary>
/// 將 HTTP 動詞字串轉為 Method 列舉。
/// </summary>
/// <param name="str">HTTP 動詞字串。</param>
/// <returns>Method 列舉值。</returns>
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

/// <summary>
/// 檢查 Header Key 或 Value 是否包含 CRLF 注入字元。
/// </summary>
/// <param name="str">待檢查字串。</param>
/// <returns>若包含 CRLF 傳回 true。</returns>
[[nodiscard]] inline constexpr bool contains_crlf(const std::string_view str) noexcept {
    return str.find('\r') != std::string_view::npos || str.find('\n') != std::string_view::npos;
}

/// <summary>
/// 修剪字串前後空白字元 (Zero-copy std::string_view)。
/// </summary>
/// <param name="str">待修剪字串。</param>
/// <returns>修剪後之 string_view。</returns>
[[nodiscard]] inline constexpr std::string_view trim(const std::string_view str) noexcept {
    const size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    const size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

/// <summary>
/// URL 解碼純函數。
/// </summary>
/// <param name="str">URL 編碼字串。</param>
/// <returns>解碼後字串。</returns>
[[nodiscard]] inline std::string url_decode(const std::string_view str) noexcept {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.size()) {
                int32_t hex = 0;
                const auto [ptr, ec] = std::from_chars(str.data() + i + 1, str.data() + i + 3, hex, 16);
                if (ec == std::errc{}) {
                    result.push_back(static_cast<char>(hex));
                    i += 2;
                    continue;
                }
            }
        } else if (str[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(str[i]);
        }
    }
    return result;
}

/// <summary>
/// URL 編碼純函數。
/// </summary>
/// <param name="str">未編碼字串。</param>
/// <returns>URL 編碼字串。</returns>
[[nodiscard]] inline std::string url_encode(const std::string_view str) noexcept {
    std::string result;
    for (const uint8_t c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result.push_back(static_cast<char>(c));
        } else {
            result += std::format("%{:02X}", c);
        }
    }
    return result;
}

/// <summary>
/// 解析 Query String 鍵值對。
/// </summary>
/// <param name="query">URL query 部分。</param>
/// <returns>解析後鍵值對字典。</returns>
[[nodiscard]] inline std::unordered_map<std::string, std::string> parse_query_string(const std::string_view query) noexcept {
    std::unordered_map<std::string, std::string> params;
    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string_view::npos) end = query.size();
        const std::string_view pair = query.substr(start, end - start);
        const size_t eq = pair.find('=');
        if (eq != std::string_view::npos) {
            std::string key = url_decode(pair.substr(0, eq));
            std::string val = url_decode(pair.substr(eq + 1));
            params[std::move(key)] = std::move(val);
        } else if (!pair.empty()) {
            params[url_decode(pair)] = "";
        }
        start = end + 1;
    }
    return params;
}

/// <summary>
/// 轉義 JSON 特殊字元。
/// </summary>
/// <param name="str">未轉義字串。</param>
/// <returns>JSON 安全轉義字串。</returns>
[[nodiscard]] inline std::string escape_json(const std::string_view str) noexcept {
    std::string out;
    out.reserve(str.size() + 8);
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
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

} // namespace detail

// ============================================================================
// 3. Request & Response Objects
// ============================================================================

/// <summary>
/// HTTP 請求物件。
/// </summary>
struct Request {
    Method method = Method::GET;
    std::string path;
    std::string raw_target;
    HeaderMap headers;
    std::string body;
    std::unordered_map<std::string, std::string> query_params;
    std::unordered_map<std::string, std::string> path_params;
    std::string remote_addr;
    uint16_t remote_port = 0;

    /// <summary>
    /// 獲取指定 Header。
    /// </summary>
    [[nodiscard]] std::optional<std::string> get_header(const std::string_view key) const noexcept {
        const auto it = headers.find(std::string(key));
        if (it != headers.end()) return it->second;
        return std::nullopt;
    }

    /// <summary>
    /// 檢查 Header 是否存在。
    /// </summary>
    [[nodiscard]] bool has_header(const std::string_view key) const noexcept {
        return headers.contains(std::string(key));
    }

    /// <summary>
    /// 獲取 Query 參數。
    /// </summary>
    [[nodiscard]] std::optional<std::string> get_param(const std::string_view key) const noexcept {
        const auto it = query_params.find(std::string(key));
        if (it != query_params.end()) return it->second;
        return std::nullopt;
    }

    /// <summary>
    /// 獲取 Path 動態參數。
    /// </summary>
    [[nodiscard]] std::optional<std::string> get_path_param(const std::string_view key) const noexcept {
        const auto it = path_params.find(std::string(key));
        if (it != path_params.end()) return it->second;
        return std::nullopt;
    }
};

/// <summary>
/// HTTP 回應物件。
/// </summary>
struct Response {
    int32_t status = 200;
    HeaderMap headers;
    std::string body;

    /// <summary>
    /// 設定 Response 文字內容。
    /// </summary>
    void set_content(const std::string_view content, const std::string_view content_type = "text/plain; charset=utf-8") noexcept {
        body = std::string(content);
        set_header("Content-Type", std::string(content_type));
        set_header("Content-Length", std::to_string(body.size()));
    }

    /// <summary>
    /// 設定 Response JSON 內容。
    /// </summary>
    void set_json(const std::string_view json_str) noexcept {
        set_content(json_str, "application/json");
    }

    /// <summary>
    /// 設定自訂 Response Header（含 CRLF 注入過濾檢查）。
    /// </summary>
    /// <exception cref="std::invalid_argument">當 Header key 或 value 包含 CRLF 時擲出。</exception>
    void set_header(std::string key, std::string value) {
        if (detail::contains_crlf(key) || detail::contains_crlf(value)) {
            throw std::invalid_argument("CRLF injection detected in HTTP header key or value");
        }
        headers[std::move(key)] = std::move(value);
    }

    /// <summary>
    /// 設定重導向 Response。
    /// </summary>
    void set_redirect(const std::string_view location, const int32_t redirect_status = 302) {
        status = redirect_status;
        set_header("Location", std::string(location));
        set_content("", "text/plain");
    }

    /// <summary>
    /// 將 Header 序列化為 HTTP 協定標頭字串（適用於 Scatter-Gather I/O）。
    /// </summary>
    [[nodiscard]] std::string serialize_headers() const noexcept {
        std::string res;
        res.reserve(256);
        const std::string_view msg = get_status_message(status);
        res += std::format("HTTP/1.1 {} {}\r\n", status, msg);
        
        HeaderMap final_headers = headers;
        if (!final_headers.contains("Content-Length")) {
            final_headers["Content-Length"] = std::to_string(body.size());
        }
        if (!final_headers.contains("Connection")) {
            final_headers["Connection"] = "close";
        }
        
        for (const auto& [k, v] : final_headers) {
            res += k;
            res += ": ";
            res += v;
            res += "\r\n";
        }
        res += "\r\n";
        return res;
    }
};

// ============================================================================
// 4. OpenAPI Metadata & Fluent Route API
// ============================================================================

/// <summary>
/// API 參數文件元資料。
/// </summary>
struct RouteParamDoc {
    std::string name;
    std::string description;
    bool required = true;
    std::string in_location = "path";
    std::string data_type = "string";
};

/// <summary>
/// API 回應文件元資料。
/// </summary>
struct RouteResponseDoc {
    int32_t status_code = 200;
    std::string description;
    std::string content_type = "application/json";
};

/// <summary>
/// API Route 元資料。
/// </summary>
struct RouteMetadata {
    std::string path;
    Method method = Method::GET;
    std::string summary;
    std::string description;
    std::vector<std::string> tags;
    std::vector<RouteParamDoc> parameters;
    std::vector<RouteResponseDoc> responses;
};

using HandlerFunc = std::function<void(const Request&, Response&)>;

/// <summary>
/// 流暢介面 Fluent API 包裝物件。
/// </summary>
class FluentRoute {
private:
    RouteMetadata& m_meta;
    HandlerFunc& m_handler;

public:
    FluentRoute(RouteMetadata& meta, HandlerFunc& handler)
        : m_meta(meta), m_handler(handler) {}

    FluentRoute& tag(std::string tag_name) noexcept {
        m_meta.tags.push_back(std::move(tag_name));
        return *this;
    }

    FluentRoute& summary(std::string sum_str) noexcept {
        m_meta.summary = std::move(sum_str);
        return *this;
    }

    FluentRoute& description(std::string desc_str) noexcept {
        m_meta.description = std::move(desc_str);
        return *this;
    }

    FluentRoute& param(std::string name, std::string desc, const bool required = true, std::string location = "path", std::string type = "string") noexcept {
        m_meta.parameters.push_back(RouteParamDoc{
            .name = std::move(name),
            .description = std::move(desc),
            .required = required,
            .in_location = std::move(location),
            .data_type = std::move(type)
        });
        return *this;
    }

    FluentRoute& response(const int32_t status_code, std::string desc, std::string content_type = "application/json") noexcept {
        m_meta.responses.push_back(RouteResponseDoc{
            .status_code = status_code,
            .description = std::move(desc),
            .content_type = std::move(content_type)
        });
        return *this;
    }

    FluentRoute& handle(HandlerFunc handler) {
        if (!handler) {
            throw std::invalid_argument("Route handler cannot be empty");
        }
        m_handler = std::move(handler);
        return *this;
    }

    void operator=(HandlerFunc handler) {
        handle(std::move(handler));
    }
};

// ============================================================================
// 5. Router & Path Matching
// ============================================================================

/// <summary>
/// HTTP 核心路由器。
/// </summary>
class Router {
public:
    struct RouteEntry {
        std::string pattern;
        Method route_method = Method::GET;
        RouteMetadata meta;
        HandlerFunc handler;
        std::vector<std::string> param_names;
    };

private:
    std::vector<RouteEntry> m_routes;

public:
    RouteEntry& add_route(const Method method, std::string pattern, std::string summary = "") {
        if (pattern.empty() || pattern[0] != '/') {
            throw std::invalid_argument("Route pattern must start with '/'");
        }

        RouteEntry entry;
        entry.route_method = method;
        entry.pattern = pattern;
        entry.meta.path = pattern;
        entry.meta.method = method;
        entry.meta.summary = std::move(summary);
        
        size_t i = 0;
        while (i < pattern.size()) {
            if (pattern[i] == '{') {
                const size_t end = pattern.find('}', i);
                if (end != std::string::npos) {
                    std::string param_name = pattern.substr(i + 1, end - i - 1);
                    entry.param_names.push_back(param_name);
                    entry.meta.parameters.push_back(RouteParamDoc{
                        .name = param_name,
                        .description = std::format("Parameter {}", param_name),
                        .required = true,
                        .in_location = "path",
                        .data_type = "string"
                    });
                    i = end + 1;
                    continue;
                }
            } else if (pattern[i] == ':') {
                size_t end = pattern.find_first_of("/?#", i);
                if (end == std::string::npos) end = pattern.size();
                std::string param_name = pattern.substr(i + 1, end - i - 1);
                entry.param_names.push_back(param_name);
                entry.meta.parameters.push_back(RouteParamDoc{
                    .name = param_name,
                    .description = std::format("Parameter {}", param_name),
                    .required = true,
                    .in_location = "path",
                    .data_type = "string"
                });
                i = end;
                continue;
            }
            i++;
        }
        
        m_routes.push_back(std::move(entry));
        return m_routes.back();
    }

    [[nodiscard]] bool match(const Method method, const std::string_view path, HandlerFunc& out_handler, std::unordered_map<std::string, std::string>& out_params) const noexcept {
        static auto split = [](const std::string_view s) noexcept {
            std::vector<std::string_view> parts;
            size_t start = 0;
            while (start < s.size()) {
                while (start < s.size() && s[start] == '/') start++;
                if (start >= s.size()) break;
                size_t end = s.find('/', start);
                if (end == std::string_view::npos) end = s.size();
                parts.push_back(s.substr(start, end - start));
                start = end;
            }
            return parts;
        };

        for (const auto& route : m_routes) {
            if (route.route_method != method) continue;
            
            if (route.param_names.empty()) {
                if (route.pattern == path) {
                    out_handler = route.handler;
                    return true;
                }
                continue;
            }

            const auto route_parts = split(route.pattern);
            const auto path_parts = split(path);

            if (route_parts.size() != path_parts.size()) continue;

            bool matched = true;
            std::unordered_map<std::string, std::string> temp_params;

            for (size_t k = 0; k < route_parts.size(); ++k) {
                const std::string_view r_part = route_parts[k];
                const std::string_view p_part = path_parts[k];

                if (r_part.starts_with('{') && r_part.ends_with('}')) {
                    std::string param_name(r_part.substr(1, r_part.size() - 2));
                    temp_params[param_name] = detail::url_decode(p_part);
                } else if (r_part.starts_with(':')) {
                    std::string param_name(r_part.substr(1));
                    temp_params[param_name] = detail::url_decode(p_part);
                } else if (r_part != p_part) {
                    matched = false;
                    break;
                }
            }

            if (matched) {
                out_handler = route.handler;
                out_params = std::move(temp_params);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const std::vector<RouteEntry>& get_routes() const noexcept {
        return m_routes;
    }
};

// ============================================================================
// 6. OpenAPI 3.0 & Scalar UI Generator
// ============================================================================

/// <summary>
/// OpenAPI 3.0.3 JSON 產生器。
/// </summary>
class OpenApiGenerator {
public:
    [[nodiscard]] static std::string generate_spec(const std::vector<Router::RouteEntry>& routes, const std::string_view title = "httplib23 API Document", const std::string_view version = "1.0.0") noexcept {
        std::string json;
        json.reserve(2048);
        json += "{\n";
        json += "  \"openapi\": \"3.0.3\",\n";
        json += "  \"info\": {\n";
        json += std::format("    \"title\": \"{}\",\n", detail::escape_json(title));
        json += std::format("    \"version\": \"{}\"\n", detail::escape_json(version));
        json += "  },\n";
        json += "  \"paths\": {\n";

        std::map<std::string, std::vector<const Router::RouteEntry*>> path_map;
        for (const auto& r : routes) {
            path_map[r.pattern].push_back(&r);
        }

        size_t path_idx = 0;
        for (const auto& [path_str, route_list] : path_map) {
            json += std::format("    \"{}\": {{\n", detail::escape_json(path_str));
            size_t route_idx = 0;
            for (const auto* r : route_list) {
                std::string method_lower = std::string(method_to_string(r->route_method));
                std::transform(method_lower.begin(), method_lower.end(), method_lower.begin(), [](const uint8_t c) noexcept -> char {
                    return static_cast<char>(std::tolower(c));
                });
                
                json += std::format("      \"{}\": {{\n", method_lower);
                if (!r->meta.summary.empty()) {
                    json += std::format("        \"summary\": \"{}\",\n", detail::escape_json(r->meta.summary));
                }
                if (!r->meta.description.empty()) {
                    json += std::format("        \"description\": \"{}\",\n", detail::escape_json(r->meta.description));
                }
                if (!r->meta.tags.empty()) {
                    json += "        \"tags\": [";
                    for (size_t ti = 0; ti < r->meta.tags.size(); ++ti) {
                        json += std::format("\"{}\"", detail::escape_json(r->meta.tags[ti]));
                        if (ti + 1 < r->meta.tags.size()) json += ", ";
                    }
                    json += "],\n";
                }

                if (!r->meta.parameters.empty()) {
                    json += "        \"parameters\": [\n";
                    for (size_t pi = 0; pi < r->meta.parameters.size(); ++pi) {
                        const auto& p = r->meta.parameters[pi];
                        json += "          {\n";
                        json += std::format("            \"name\": \"{}\",\n", detail::escape_json(p.name));
                        json += std::format("            \"in\": \"{}\",\n", detail::escape_json(p.in_location));
                        json += std::format("            \"required\": {},\n", p.required ? "true" : "false");
                        json += std::format("            \"description\": \"{}\",\n", detail::escape_json(p.description));
                        json += std::format("            \"schema\": {{ \"type\": \"{}\" }}\n", detail::escape_json(p.data_type));
                        json += "          }";
                        if (pi + 1 < r->meta.parameters.size()) json += ",";
                        json += "\n";
                    }
                    json += "        ],\n";
                }

                json += "        \"responses\": {\n";
                if (r->meta.responses.empty()) {
                    json += "          \"200\": { \"description\": \"OK\" }\n";
                } else {
                    for (size_t ri = 0; ri < r->meta.responses.size(); ++ri) {
                        const auto& resp = r->meta.responses[ri];
                        json += std::format("          \"{}\": {{\n", resp.status_code);
                        json += std::format("            \"description\": \"{}\"\n", detail::escape_json(resp.description));
                        json += "          }";
                        if (ri + 1 < r->meta.responses.size()) json += ",";
                        json += "\n";
                    }
                }
                json += "        }\n";

                json += "      }";
                if (route_idx + 1 < route_list.size()) json += ",";
                json += "\n";
                route_idx++;
            }
            json += "    }";
            if (path_idx + 1 < path_map.size()) json += ",";
            json += "\n";
            path_idx++;
        }

        json += "  }\n";
        json += "}\n";
        return json;
    }
};

/// <summary>
/// Swagger UI HTML 文件產生器。
/// </summary>
class SwaggerDocGenerator {
public:
    [[nodiscard]] static std::string generate_html(const std::string_view openapi_url = "/openapi.json", const std::string_view title = "Swagger UI") noexcept {
        return std::format(R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{}</title>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui.css" />
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5/swagger-ui-bundle.js"></script>
  <script>
    window.onload = () => {{
      window.ui = SwaggerUIBundle({{
        url: '{}',
        dom_id: '#swagger-ui',
        deepLinking: true,
        presets: [
          SwaggerUIBundle.presets.apis,
          SwaggerUIBundle.SwaggerUIStandalonePreset
        ],
      }});
    }};
  </script>
</body>
</html>)", title, openapi_url);
    }
};

/// <summary>
/// Scalar API Reference HTML 文件產生器。
/// </summary>
class ScalarDocGenerator {
public:
    [[nodiscard]] static std::string generate_html(const std::string_view openapi_url = "/openapi.json", const std::string_view title = "Scalar API Documentation") noexcept {
        return std::format(R"(<!DOCTYPE html>
<html>
<head>
  <title>{}</title>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <style>
    body {{ margin: 0; padding: 0; background-color: #0f172a; color: #f8fafc; font-family: system-ui, sans-serif; }}
  </style>
</head>
<body>
  <script id="api-reference" data-url="{}"></script>
  <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
</body>
</html>)", title, openapi_url);
    }
};

/// <summary>
/// API 文件生成選項配置結構體。
/// </summary>
struct DocOptions {
    bool enabled = true;
    std::string openapi_path = "/openapi.json";
    std::string swagger_path = "/docs";
    std::string scalar_path = "/scalar";
    std::string title = "httplib23 API Documentation";
    std::string version = "1.0.0";
};

// ============================================================================
// 7. Thread Pool Engine (With Bounded Queue Size)
// ============================================================================

/// <summary>
/// 高效能有界 Worker 執行緒池 (Bounded Queue Backpressure)。
/// </summary>
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

    /// <summary>
    /// 將 Task 放入佇列。若佇列已滿傳回 false。
    /// </summary>
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

/// <summary>
/// 每條 TCP 連線 Session 狀態機與累積 Receiving Buffer (Data-Race-Free)。
/// </summary>
struct ConnectionSession {
    SOCKET socket = INVALID_SOCKET;
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
// 9. IOCP Server Implementation
// ============================================================================

/// <summary>
/// 高併發 Windows IOCP HTTP 伺服器。
/// </summary>
class Server {
public:
    using MiddlewareFunc = std::function<bool(Request&, Response&)>;

private:
    HANDLE m_iocp = INVALID_HANDLE_VALUE;
    SOCKET m_listen_socket = INVALID_SOCKET;
    std::atomic<bool> m_running{false};
    Router m_router;
    std::vector<MiddlewareFunc> m_middlewares;
    std::unique_ptr<ThreadPool> m_pool;
    std::thread m_accept_thread;
    std::thread m_watchdog_thread;
    std::vector<std::thread> m_iocp_threads;
    DocOptions m_doc_options;

    std::mutex m_session_mutex;
    std::unordered_map<SOCKET, std::shared_ptr<ConnectionSession>> m_sessions;

    enum class IOOperation : uint8_t { READ, WRITE };

    struct PerIoData {
        WSAOVERLAPPED overlapped;
        WSABUF wsa_bufs[2];
        char buffer[8192];
        IOOperation op_type;
        SOCKET socket;
        std::string send_header_buf;
        std::string send_body_buf;
    };

public:
    Server() {
        WSADATA wsaData;
        const int32_t err = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (err != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~Server() {
        stop();
        WSACleanup();
    }

    /// <summary>
    /// 設定 API 文件生成與路由選項（可開啟/關閉文件、自訂 Swagger UI 與 Scalar UI 路徑）。
    /// </summary>
    Server& set_doc_options(DocOptions options) noexcept {
        m_doc_options = std::move(options);
        return *this;
    }

    /// <summary>
    /// 設定是否啟用 API 文件生成。
    /// </summary>
    Server& enable_docs(const bool enable = true) noexcept {
        m_doc_options.enabled = enable;
        return *this;
    }

    /// <summary>
    /// 獲取目前 API 文件配置選項。
    /// </summary>
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

            if (!m_doc_options.swagger_path.empty() && !m_doc_options.openapi_path.empty()) {
                const std::string openapi_url = m_doc_options.openapi_path;
                const std::string title = m_doc_options.title + " - Swagger UI";
                m_router.add_route(Method::GET, m_doc_options.swagger_path, "Interactive Swagger API Documentation").handler = [openapi_url, title](const Request&, Response& res) {
                    res.set_content(SwaggerDocGenerator::generate_html(openapi_url, title), "text/html; charset=utf-8");
                };
            }

            if (!m_doc_options.scalar_path.empty() && !m_doc_options.openapi_path.empty()) {
                const std::string openapi_url = m_doc_options.openapi_path;
                const std::string title = m_doc_options.title + " - Scalar UI";
                m_router.add_route(Method::GET, m_doc_options.scalar_path, "Interactive Scalar API Documentation").handler = [openapi_url, title](const Request&, Response& res) {
                    res.set_content(ScalarDocGenerator::generate_html(openapi_url, title), "text/html; charset=utf-8");
                };
            }
        }

        m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (m_iocp == NULL) return false;

        m_listen_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        if (m_listen_socket == INVALID_SOCKET) return false;

        BOOL reuse = TRUE;
        setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

        if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
            closesocket(m_listen_socket);
            m_listen_socket = INVALID_SOCKET;
            return false;
        }

        if (::listen(m_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(m_listen_socket);
            m_listen_socket = INVALID_SOCKET;
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
    }

    void stop() noexcept {
        if (!m_running) return;
        m_running = false;

        if (m_iocp != INVALID_HANDLE_VALUE) {
            for (size_t i = 0; i < m_iocp_threads.size(); ++i) {
                PostQueuedCompletionStatus(m_iocp, 0, 0, NULL);
            }
        }

        if (m_listen_socket != INVALID_SOCKET) {
            closesocket(m_listen_socket);
            m_listen_socket = INVALID_SOCKET;
        }

        if (m_accept_thread.joinable()) m_accept_thread.join();
        if (m_watchdog_thread.joinable()) m_watchdog_thread.join();
        for (auto& th : m_iocp_threads) {
            if (th.joinable()) th.join();
        }
        m_iocp_threads.clear();

        {
            std::lock_guard<std::mutex> lock(m_session_mutex);
            for (const auto& [sock, session] : m_sessions) {
                closesocket(sock);
            }
            m_sessions.clear();
        }

        if (m_iocp != INVALID_HANDLE_VALUE) {
            CloseHandle(m_iocp);
            m_iocp = INVALID_HANDLE_VALUE;
        }

        m_pool.reset();
    }

private:
    void accept_loop() noexcept {
        while (m_running) {
            sockaddr_in client_addr{};
            int32_t addr_len = sizeof(client_addr);
            const SOCKET client_socket = accept(m_listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
            if (client_socket == INVALID_SOCKET) {
                if (!m_running) break;
                continue;
            }

            if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket), m_iocp, static_cast<ULONG_PTR>(client_socket), 0) == NULL) {
                closesocket(client_socket);
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

    void timeout_watchdog_loop() noexcept {
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (!m_running) break;

            const int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            const int64_t timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(CONNECTION_TIMEOUT).count();

            std::vector<SOCKET> timed_out_sockets;
            {
                std::lock_guard<std::mutex> lock(m_session_mutex);
                for (const auto& [sock, session] : m_sessions) {
                    if (now_ms - session->get_last_active_ms() > timeout_ms) {
                        timed_out_sockets.push_back(sock);
                    }
                }
            }

            for (const SOCKET sock : timed_out_sockets) {
                remove_session(sock);
            }
        }
    }

    void remove_session(const SOCKET sock) noexcept {
        std::lock_guard<std::mutex> lock(m_session_mutex);
        if (m_sessions.contains(sock)) {
            char dummy[512];
            u_long mode = 1;
            ioctlsocket(sock, FIONBIO, &mode);
            while (recv(sock, dummy, sizeof(dummy), 0) > 0) {}
            closesocket(sock);
            m_sessions.erase(sock);
        }
    }

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
            const SOCKET client_socket = io_data->socket;

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

                // TCP Pipelining Loop: Process all complete requests in rx_buffer
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
                            break; // Wait for more header bytes
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
                            break; // Wait for more body bytes
                        }
                    }
                }

                if (should_disconnect) {
                    remove_session(client_socket);
                    continue;
                }

                // Post Next Async Read
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

    void send_error_response(const SOCKET client_socket, const int32_t status_code, const std::string_view msg) noexcept {
        Response res;
        res.status = status_code;
        res.set_content(msg, "text/plain");
        send_response_scatter(client_socket, res);
    }

    void send_response_scatter(const SOCKET client_socket, const Response& res) noexcept {
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
    }

    void handle_http_request(const SOCKET client_socket, const std::string& request_str) noexcept {
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

/// <summary>
/// HTTP Client 連線用戶端。
/// </summary>
class Client {
private:
    std::string m_host;
    uint16_t m_port = 80;

public:
    explicit Client(std::string host_or_url, const uint16_t port = 80) : m_host(std::move(host_or_url)), m_port(port) {
        if (m_host.starts_with("http://")) {
            m_host = m_host.substr(7);
        } else if (m_host.starts_with("https://")) {
            m_host = m_host.substr(8);
        }

        // Separate path component if present (e.g. "localhost:8080/api/v1" -> "localhost:8080")
        const size_t slash_pos = m_host.find('/');
        if (slash_pos != std::string::npos) {
            m_host = m_host.substr(0, slash_pos);
        }

        // Separate port if present (e.g. "localhost:8080" -> "localhost", 8080)
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

        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~Client() {
        WSACleanup();
    }

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
        const SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            return std::unexpected("Failed to create socket");
        }

        BOOL reuse = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        const std::string port_str = std::to_string(m_port);
        if (getaddrinfo(m_host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
            closesocket(sock);
            return std::unexpected("Failed to resolve host address");
        }

        bool connected = false;
        for (int32_t retry = 0; retry < 10; ++retry) {
            if (connect(sock, res->ai_addr, static_cast<int32_t>(res->ai_addrlen)) != SOCKET_ERROR) {
                connected = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!connected) {
            freeaddrinfo(res);
            closesocket(sock);
            return std::unexpected("Failed to connect to host");
        }
        freeaddrinfo(res);

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

        if (send(sock, req_str.c_str(), static_cast<int32_t>(req_str.size()), 0) == SOCKET_ERROR) {
            closesocket(sock);
            return std::unexpected("Failed to send HTTP request");
        }

        std::string raw_response;
        char buffer[4096];
        int32_t bytes_read = 0;
        while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
            raw_response.append(buffer, bytes_read);
        }
        closesocket(sock);

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
