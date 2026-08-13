# OpenAPI 3.0, Swagger UI & Scalar UI 整合說明 (API Documentation Options)

`httplib23` 內建自動化 OpenAPI 規範產生器，並同時整合 **Swagger UI** 與 **Scalar API Reference** 雙線上互動式文件介面，支援獨立/單獨啟用與自訂路由位址。

---

## 預設內建 Endpoints

當啟動 `httplib23::Server` 時，若未特別關閉 API 文件生成，系統預設會自動註冊以下三個標準路由：

1. **`/docs`** (Swagger UI):
   - 傳回嵌入 **Swagger UI** 官方前端介面的 HTML，提供業界標準的互動式測試與 API 測試工具。
2. **`/scalar`** (Scalar UI):
   - 傳回嵌入 **Scalar UI** (`@scalar/api-reference`) 的現代化視覺介面，提供優雅的視圖與範例程式碼。
3. **`/openapi.json`**:
   - 傳回根據所有動態與靜態註冊路由自動產生的符合 **OpenAPI 3.0.3 Specification** 規範的 JSON 規格內容。

---

## 控制 API 文件 (DocOptions, enable_swagger & enable_scalar)

您可以透過 `server.set_doc_options(...)` 或流暢 API 方法自由控制 API 文件的生成、單獨啟用與路由位址：

### 1. 單獨開啟 / 關閉 Swagger UI 或 Scalar UI

若您只希望提供 **Swagger UI** 或只希望提供 **Scalar UI**，可以使用單獨控制方法：

```cpp
httplib23::Server server;

// 僅啟用 Swagger UI (關閉 Scalar UI)
server.enable_swagger(true)
      .enable_scalar(false);

// 僅啟用 Scalar UI (關閉 Swagger UI)
server.enable_swagger(false)
      .enable_scalar(true);
```

### 2. 全域一鍵關閉 API 文件 (適用於生產環境)

在正式上線生產環境中，基於資安考量可隨時一鍵停用所有文件端點：

```cpp
httplib23::Server server;

// 停用所有 API 文件生成 (不會註冊 /docs, /scalar, /openapi.json)
server.enable_docs(false);
```

### 3. 自訂 API 文件路由與標題

使用者可隨意調整 Swagger UI、Scalar UI 或 OpenAPI spec 的存取路徑：

```cpp
httplib23::Server server;

server.set_doc_options({
    .enabled = true,                     // 是否啟用文件服務
    .enable_swagger = true,              // 是否單獨啟用 Swagger UI
    .enable_scalar = false,              // 是否單獨啟用 Scalar UI
    .openapi_path = "/api-spec.json",    // 自訂 OpenAPI Spec JSON 位址
    .swagger_path = "/swagger-docs",     // 自訂 Swagger UI 位址 (預設 /docs)
    .scalar_path = "/scalar-docs",       // 自訂 Scalar UI 位址 (預設 /scalar)
    .title = "企業級微服務 API 文件",
    .version = "2.1.0"
});
```

---

## 搭配 Fluent API 標註 API 文件元資料

您可以透過 `server.Get(...)`, `server.Post(...)` 傳回的 `FluentRoute` 鏈式物件，設定各個 API 的詳細資訊：

```cpp
server.Post("/api/v1/products", "建立新商品")
    .tag("Product")                            // 分類標籤 (Tags)
    .summary("新增商品至資料庫")                 // 摘要說明 (Summary)
    .description("提供詳細商品資訊以建立系統資料")  // 詳細描述 (Description)
    .param("category", "商品分類名稱", true, "query", "string") // Query 參數
    .response(201, "成功建立商品", "application/json")          // 201 Response 描述
    .response(400, "無效的商品格式", "application/json")        // 400 Response 描述
    .handle([](const httplib23::Request& req, httplib23::Response& res) {
        res.status = 201;
        res.set_json(R"({"status":"created"})");
    });
```

---

## 在瀏覽器中預覽

啟動 Server 後，您可以在瀏覽器開啟：

- **Swagger UI**: `http://127.0.0.1:8080/docs`
- **Scalar UI**: `http://127.0.0.1:8080/scalar`
- **OpenAPI 3.0 Spec JSON**: `http://127.0.0.1:8080/openapi.json`
