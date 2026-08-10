# Client API 使用說明 (Client API Reference)

`httplib23::Client` 提供簡潔且現代化的 HTTP Client 功能，傳回 C++23 `std::expected<Response, std::string>` 物件以優雅處理成功回應與錯誤。

---

## 初始化 (Initialization)

```cpp
// 支援直接傳入完整 URL 或 Host + Port
httplib23::Client client("http://127.0.0.1:8080");

// 或指定主機名與連接埠
httplib23::Client client_custom("api.example.com", 443);
```

---

## 發送 HTTP 請求 (Sending Requests)

### 1. GET 請求

```cpp
auto res = client.Get("/api/v1/users/42");
if (res) {
    std::cout << "Status Code: " << res->status << "\n";
    std::cout << "Body: " << res->body << "\n";
} else {
    std::cerr << "Request failed: " << res.error() << "\n";
}
```

### 2. POST 請求

```cpp
std::string json_payload = R"({"name": "Charlie", "email": "charlie@example.com"})";

auto res = client.Post("/api/v1/users", json_payload, "application/json");
if (res) {
    std::cout << "Created user status: " << res->status << "\n";
}
```

### 3. PUT / DELETE 請求

```cpp
// PUT 請求
auto put_res = client.Put("/api/v1/users/42", R"({"name":"Updated Charlie"})");

// DELETE 請求
auto del_res = client.Delete("/api/v1/users/42");
```

### 4. 帶有自訂 Headers 的請求

```cpp
httplib23::HeaderMap headers;
headers["Authorization"] = "Bearer token_1234567890";
headers["Accept"] = "application/json";

auto res = client.Get("/api/v1/profile", headers);
```

---

## 錯誤處理與安全機制

`httplib23::Client` 回傳類型為 `std::expected<Response, std::string>`：

- `res.has_value()` 或 `if (res)`: 判斷網路請求是否成功送出並收到伺服器回應。
- `res.value()` 或 `res->`: 獲取回應的 `httplib23::Response` 物件（包含 `status`, `headers`, `body`）。
- `res.error()`: 獲取網路連線失敗或 Host 解析失敗的錯誤原因描述。
