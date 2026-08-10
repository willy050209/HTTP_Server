# OpenAPI 3.0 & Scalar UI 整合說明 (OpenAPI & Scalar Documentation)

`httplib23` 內建自動化 OpenAPI 規範產生器與整合 **Scalar API Reference** 的線上文件網頁介面。

---

## 自動產生的內建 Endpoints

當啟動 `httplib23::Server` 時，系統會自動幫您註冊以下兩個標準路由（您無需手動手寫註冊）：

1. **`/openapi.json`**:
   - 傳回根據所有動態與靜態註冊路由自動產生的符合 **OpenAPI 3.0.3 Specification** 規範的 JSON 規格內容。
2. **`/docs`**:
   - 傳回嵌入 **Scalar UI** (`@scalar/api-reference`) 的 HTML 介面，網頁自動載入 `/openapi.json` 並顯示互動式測試介面。

---

## 搭配 Fluent API 標註 API 文件

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

啟動 Server 後，在瀏覽器打開 `http://127.0.0.1:8080/docs`，即可看到專屬且具備 Modern 視覺設計的 **Scalar API Reference** 介面：

- 檢視所有 API 的分類 (Tags) 與路徑結構
- 測試 HTTP 請求發送 (Try it out)
- 檢視產生的範例 Request / Response Schema
