# Server API 使用說明 (Server API Reference)

`httplib23::Server` 提供高效能的 HTTP Server 實作，支援鏈式 (Fluent API) 路由與 API 文件標註。

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
| `set_header(key, value)` | 設定自訂 Response Header |
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
