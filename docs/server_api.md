# Server API 使用說明 (Server API Reference)

`httplib23::Server` 提供高效能的 HTTP Server 實作，支援鏈式 (Fluent API) 路由、非同步 Logger 與 API 文件標註。

---

## 初始化與啟動 (Lifecycle)

```cpp
httplib23::Server server;

// 啟動伺服器並監聽指定 IP 與 Port
bool is_started = server.listen("127.0.0.1", 8080);

// 優雅關閉伺服器 (Graceful Shutdown)
server.stop();
```

---

## 非同步日誌記錄器 (Asynchronous Producer-Consumer Logger)

`httplib23` 內建輕量、高併發、零外部相依的 **Producer-Consumer** 架構非同步 Logger：

### 核心特性
1. **Producer-Consumer 雙核架構**: Worker 執行緒（Producer）僅需將格式化訊息推入 Queue 後立即傳回，由專屬背景執行緒（Consumer）進行批次 I/O 寫入。
2. **零開銷層級過濾 (Log Level & OFF)**: 提供 `DEBUG`, `INFO`, `WARN`, `ERROR`, `OFF`。當層級設為 `OFF` 或低於目前門檻時，極早期 Return，完全不耗費字串格式化開銷。
3. **零巨集與 C++20 `std::source_location`**: 自動精準擷取呼叫者的檔名與行號。
4. **自訂 Sink (Hook / Callback)**: 可註冊自訂 Callback Lambda，寫入檔案或串接日誌收集系統 (如 ELK/Loki)。

### 日誌使用範例

```cpp
#include "httplib23.hpp"

int main() {
    // 1. 設定 Log Level 門檻
    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);

    // 2. 自訂 Log Sink (選擇性：預設輸出至 Console)
    httplib23::Logger::instance().set_sink([](httplib23::LogLevel level, std::string_view msg) {
        // 自訂將 Log 寫入檔案或日誌中心
    });

    // 3. 全域 Log 輔助函式 (自動捕捉檔名與行號)
    httplib23::log_info("Server listening on http://{}:{}", "127.0.0.1", 8080);
    httplib23::log_warn("High connection load detected: {} req/s", 5000);
    httplib23::log_error("Database connection failed, status code: {}", 500);

    // 4. 當設定為 LogLevel::OFF 時，達到極低零開銷
    httplib23::Logger::instance().set_level(httplib23::LogLevel::OFF);
    httplib23::log_debug("This will be filtered out instantly with zero overhead");

    return 0;
}
```

---

## API 文件產生與控制 (API Documentation Configuration)

`httplib23::Server` 內建自動產生的 **Swagger UI** (`/docs`)、**Scalar UI** (`/scalar`) 與 **OpenAPI 3.0 Spec** (`/openapi.json`)，支援全域或單獨開關控制：

```cpp
httplib23::Server server;

// 1. 設定自訂文件路徑、單獨開啟/關閉 Swagger UI 與 Scalar UI
server.set_doc_options({
    .enabled = true,
    .enable_swagger = true,       // 是否單獨啟用 Swagger UI
    .enable_scalar = true,        // 是否單獨啟用 Scalar UI
    .openapi_path = "/openapi.json",
    .swagger_path = "/docs",      // Swagger UI 預設位址 /docs
    .scalar_path = "/scalar",     // Scalar UI 預設位址 /scalar
    .title = "系統 API 文件",
    .version = "1.0.0"
});

// 2. 流暢 API 單獨控制 Swagger UI 或 Scalar UI
server.enable_swagger(true)   // 單獨開啟/關閉 Swagger UI
      .enable_scalar(false);  // 單獨開啟/關閉 Scalar UI

// 3. 在生產環境全域關閉 API 文件端點
server.enable_docs(false);
```

---

## 路由定義 (Routing & HTTP Methods)

支援常用 HTTP 動詞：`Get`, `Post`, `Put`, `Delete`, `Patch`, `Options`, `Head`。

### 1. 基本路由

```cpp
server.Get("/ping", "Health check endpoint")
    .handle([](const httplib23::Request& req, httplib23::Response& res) {
        res.set_content("pong", "text/plain");
    });
```

### 2. 流暢介面 (Fluent API) 與 OpenAPI 標註

可以在註冊路由時鏈式定義 API 文件元資料：

```cpp
server.Get("/api/v1/users/{id}", "取得用戶詳細資訊")
    .tag("User")
    .summary("根據 ID 獲取用戶詳情")
    .description("這是一個獲取單一用戶詳細資料的 API Endpoint")
    .param("id", "使用者 ID", true, "path", "integer")
    .response(200, "成功返回用戶", "application/json")
    .response(404, "找不到該用戶", "application/json")
    .handle([](const httplib23::Request& req, httplib23::Response& res) {
        auto user_id = req.get_path_param("id").value_or("0");
        res.set_json(std::format(R"({{"id":{}, "name":"Bob"}})", user_id));
    });
```

---

## 路徑參數解析 (Path Parameters)

支援 `{param_name}` 或 `:param_name` 格式的路徑佔位符：

```cpp
server.Get("/users/{id}/posts/{post_id}", "取得用戶文章")
    .handle([](const httplib23::Request& req, httplib23::Response& res) {
        auto user_id = req.get_path_param("id").value_or("");
        auto post_id = req.get_path_param("post_id").value_or("");
        
        res.set_json(std::format(R"({{"userId":"{}", "postId":"{}"}})", user_id, post_id));
    });
```

---

## Request 與 Response 物件說明

### `httplib23::Request`

| 成員變數 / 方法 | 類型 | 說明 |
| :--- | :--- | :--- |
| `method` | `Method` | HTTP 動詞 (`GET`, `POST`, `PUT`, etc.) |
| `path` | `std::string` | 請求路徑 (例如 `/api/v1/user`) |
| `raw_target` | `std::string` | 原始目標字串 (含 Query string) |
| `headers` | `HeaderMap` | Header 鍵值對 (不分大小寫) |
| `body` | `std::string` | 請求 Payload Body |
| `query_params` | `map` | 解析後之 Query 參數 |
| `path_params` | `map` | 解析後之路徑動態參數 |
| `get_header(key)` | `std::optional<string>` | 獲取指定 Header 值 |
| `get_param(key)` | `std::optional<string>` | 獲取指定 Query 參數值 |
| `get_path_param(key)` | `std::optional<string>` | 獲取指定 Path 參數值 |

### `httplib23::Response`

| 方法 | 說明 |
| :--- | :--- |
| `set_content(body, content_type)` | 設定回應 Body 與 Content-Type |
| `set_json(json_str)` | 設定 JSON 回應 (自動將 Content-Type 設為 `application/json`) |
| `set_header(key, value)` | 設定自訂 Response Header (含 CRLF 注入防護) |
| `set_redirect(url, status_code)` | 設定重導向 HTTP 回應 |
| `status` (變數) | HTTP 狀態碼 (預設為 `200`) |

---

## 中間件 (Middleware) 支援

可以使用 `server.Use(...)` 加入全域 Middleware：

```cpp
// CORS 中間件範例
server.Use([](httplib23::Request& req, httplib23::Response& res) -> bool {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    // 若為 OPTIONS 預檢請求，可直接結束回應
    if (req.method == httplib23::Method::OPTIONS) {
        res.status = 204;
        return false; // 終止後續 Route Handler 執行
    }
    return true; // 繼續執行
});
```
