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
    if (str == "GET") return Method::GET;
    if (str == "POST") return Method::POST;
    if (str == "PUT") return Method::PUT;
    if (str == "DELETE") return Method::DELETE;
    if (str == "PATCH") return Method::PATCH;
    if (str == "OPTIONS") return Method::OPTIONS;
    if (str == "HEAD") return Method::HEAD;
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
/// 修剪字串前後空白字元。
/// </summary>
/// <param name="str">待修剪字串。</param>
/// <returns>修剪後之字串。</returns>
[[nodiscard]] inline std::string trim(const std::string_view str) noexcept {
    const size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    const size_t end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
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
        headers["Content-Type"] = std::string(content_type);
        headers["Content-Length"] = std::to_string(body.size());
    }

    /// <summary>
    /// 設定 Response JSON 內容。
    /// </summary>
    void set_json(const std::string_view json_str) noexcept {
        set_content(json_str, "application/json");
    }

    /// <summary>
    /// 設定自訂 Response Header。
    /// </summary>
    void set_header(std::string key, std::string value) noexcept {
        headers[std::move(key)] = std::move(value);
    }

    /// <summary>
    /// 設定重導向 Response。
    /// </summary>
    void set_redirect(const std::string_view location, const int32_t redirect_status = 302) noexcept {
        status = redirect_status;
        headers["Location"] = std::string(location);
        set_content("", "text/plain");
    }

    /// <summary>
    /// 將 Response 序列化為 HTTP 協定串流。
    /// </summary>
    [[nodiscard]] std::string serialize() const noexcept {
        std::string res;
        res.reserve(256 + body.size());
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
        res += body;
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

// ============================================================================
// 7. Thread Pool Engine
// ============================================================================

/// <summary>
/// 高效能 Worker 執行緒池。
/// </summary>
class ThreadPool {
private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop{false};

public:
    explicit ThreadPool(size_t threads = std::thread::hardware_concurrency()) {
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

    void enqueue(std::function<void()> task) {
        if (!task) {
            throw std::invalid_argument("Task cannot be empty");
        }
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_tasks.push(std::move(task));
        }
        m_cv.notify_one();
    }
};

// ============================================================================
// 8. IOCP Server Implementation
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
    std::vector<std::thread> m_iocp_threads;

    enum class IOOperation : uint8_t { READ, WRITE };

    struct PerIoData {
        WSAOVERLAPPED overlapped;
        WSABUF wsa_buf;
        char buffer[8192];
        IOOperation op_type;
        SOCKET socket;
        std::vector<char> dynamic_send_buf;
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
        m_router.add_route(Method::GET, "/openapi.json", "Get OpenAPI 3.0 JSON Specification").handler = [this](const Request&, Response& res) {
            res.set_json(OpenApiGenerator::generate_spec(m_router.get_routes()));
        };

        m_router.add_route(Method::GET, "/docs", "Interactive Scalar API Documentation").handler = [](const Request&, Response& res) {
            res.set_content(ScalarDocGenerator::generate_html("/openapi.json"), "text/html; charset=utf-8");
        };

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
        return true;
    }

    void stop() noexcept {
        if (!m_running) return;
        m_running = false;

        if (m_listen_socket != INVALID_SOCKET) {
            closesocket(m_listen_socket);
            m_listen_socket = INVALID_SOCKET;
        }

        if (m_iocp != INVALID_HANDLE_VALUE) {
            for (size_t i = 0; i < m_iocp_threads.size(); ++i) {
                PostQueuedCompletionStatus(m_iocp, 0, 0, NULL);
            }
            CloseHandle(m_iocp);
            m_iocp = INVALID_HANDLE_VALUE;
        }

        if (m_accept_thread.joinable()) m_accept_thread.join();
        for (auto& th : m_iocp_threads) {
            if (th.joinable()) th.join();
        }
        m_iocp_threads.clear();
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

            auto io_data = std::make_unique<PerIoData>();
            ZeroMemory(&io_data->overlapped, sizeof(OVERLAPPED));
            io_data->op_type = IOOperation::READ;
            io_data->socket = client_socket;
            io_data->wsa_buf.buf = io_data->buffer;
            io_data->wsa_buf.len = sizeof(io_data->buffer);

            DWORD flags = 0;
            DWORD bytes_recv = 0;
            PerIoData* raw_ptr = io_data.release();
            if (WSARecv(client_socket, &raw_ptr->wsa_buf, 1, &bytes_recv, &flags, &raw_ptr->overlapped, NULL) == SOCKET_ERROR) {
                if (WSAGetLastError() != ERROR_IO_PENDING) {
                    closesocket(client_socket);
                    delete raw_ptr;
                }
            }
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
                    closesocket(io_data->socket);
                    delete io_data;
                }
                continue;
            }

            PerIoData* io_data = CONTAINING_RECORD(overlapped, PerIoData, overlapped);
            const SOCKET client_socket = io_data->socket;

            if (io_data->op_type == IOOperation::READ) {
                std::string raw_request(io_data->buffer, bytes_transferred);
                delete io_data;

                m_pool->enqueue([this, client_socket, request_str = std::move(raw_request)]() {
                    handle_http_request(client_socket, request_str);
                });
            } else if (io_data->op_type == IOOperation::WRITE) {
                closesocket(client_socket);
                delete io_data;
            }
        }
    }

    void handle_http_request(const SOCKET client_socket, const std::string& request_str) noexcept {
        Request req;
        Response res;

        std::istringstream stream(request_str);
        std::string request_line;
        if (std::getline(stream, request_line)) {
            request_line = detail::trim(request_line);
            std::istringstream line_stream(request_line);
            std::string method_str, target, version;
            line_stream >> method_str >> target >> version;
            req.method = string_to_method(method_str);
            req.raw_target = target;

            const size_t query_pos = target.find('?');
            if (query_pos != std::string::npos) {
                req.path = target.substr(0, query_pos);
                req.query_params = detail::parse_query_string(target.substr(query_pos + 1));
            } else {
                req.path = target;
            }
        }

        std::string header_line;
        while (std::getline(stream, header_line)) {
            header_line = detail::trim(header_line);
            if (header_line.empty()) break;
            const size_t colon = header_line.find(':');
            if (colon != std::string::npos) {
                std::string key = detail::trim(header_line.substr(0, colon));
                std::string value = detail::trim(header_line.substr(colon + 1));
                req.headers[key] = value;
            }
        }

        if (const auto len_opt = req.get_header("Content-Length")) {
            int32_t len = 0;
            const auto [ptr, ec] = std::from_chars(len_opt->data(), len_opt->data() + len_opt->size(), len);
            if (ec == std::errc{} && len > 0) {
                std::string body_data;
                body_data.resize(len);
                stream.read(body_data.data(), len);
                req.body = std::move(body_data);
            }
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
                handler(req, res);
            } else {
                res.status = 404;
                res.set_content("404 Not Found", "text/plain");
            }
        }

        const std::string response_payload = res.serialize();
        auto send_io = std::make_unique<PerIoData>();
        ZeroMemory(&send_io->overlapped, sizeof(OVERLAPPED));
        send_io->op_type = IOOperation::WRITE;
        send_io->socket = client_socket;
        
        send_io->dynamic_send_buf.resize(response_payload.size());
        std::copy(response_payload.begin(), response_payload.end(), send_io->dynamic_send_buf.begin());
        send_io->wsa_buf.buf = send_io->dynamic_send_buf.data();
        send_io->wsa_buf.len = static_cast<ULONG>(send_io->dynamic_send_buf.size());

        DWORD bytes_sent = 0;
        PerIoData* raw_send_ptr = send_io.release();
        if (WSASend(client_socket, &raw_send_ptr->wsa_buf, 1, &bytes_sent, 0, &raw_send_ptr->overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != ERROR_IO_PENDING) {
                closesocket(client_socket);
                delete raw_send_ptr;
            }
        }
    }
};

// ============================================================================
// 9. HTTP Client Engine
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
            const size_t colon = m_host.find(':');
            if (colon != std::string::npos) {
                m_port = static_cast<uint16_t>(std::stoi(m_host.substr(colon + 1)));
                m_host = m_host.substr(0, colon);
            }
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
        const SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            return std::unexpected("Failed to create socket");
        }

        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        const std::string port_str = std::to_string(m_port);
        if (getaddrinfo(m_host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
            closesocket(sock);
            return std::unexpected("Failed to resolve host address");
        }

        if (connect(sock, res->ai_addr, static_cast<int32_t>(res->ai_addrlen)) == SOCKET_ERROR) {
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
            status_line = detail::trim(status_line);
            std::istringstream line_stream(status_line);
            std::string http_ver;
            line_stream >> http_ver >> response.status;
        }

        std::string header_line;
        while (std::getline(stream, header_line)) {
            header_line = detail::trim(header_line);
            if (header_line.empty()) break;
            const size_t colon = header_line.find(':');
            if (colon != std::string::npos) {
                std::string key = detail::trim(header_line.substr(0, colon));
                std::string val = detail::trim(header_line.substr(colon + 1));
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
