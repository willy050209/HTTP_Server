# C++23 Modern Light-weight High-Concurrency Single-Header HTTP Library (`httplib23.hpp`)

## 專案目標 (Project Objective)
建立一個基於 C++23 的輕量化、高併發、零外部相依性 (Zero-dependency)、單檔包含 (Single-header `httplib23.hpp`) 的現代化 HTTP Server & Client 函式庫。同時整合自動化 OpenAPI 3.0 Spec 產生與整合 Scalar API Documentation 網頁介面。

---

## 架構設計 (Architecture Overview)
1. **單檔結構 (Single-header design)**: 全部位於 `httplib23.hpp` 標頭檔中，採用 `namespace httplib23` 隔離。
2. **高效能併發網路引擎**:
   - Windows 平台原生 IOCP (I/O Completion Ports) 搭配動態 Worker Thread Pool。
   - 非阻塞 I/O，支援 HTTP/1.1 Keep-Alive 長連線與 Multiplexing 分派。
3. **現代 C++23 語法特徵**:
   - `std::string_view`, `std::span`, `std::expected` / `std::optional`, `std::jthread`, `std::stop_token`, `std::source_location`, Concepts, Lambda 開發介面。
4. **API 與 Routing 風格 (Crow + cpp-httplib Fluent Blend)**:
   - 支援經典導向 `svr.Get(...)`, `svr.Post(...)` 與 Fluent API 鏈式定義 API Metadata（`tag`, `summary`, `description`, `response` 等）。
   - Path Parameter 解析（例如 `/users/{id}`）。
5. **自動化 API 文件系統 (Scalar UI & OpenAPI 3.0)**:
   - 內建 OpenAPI 3.0 JSON 產生器，自動將註冊之 Routes 轉換為規範的 API Specification。
   - `/openapi.json` 自動對外提供 JSON Spec。
   - `/docs` 自動提供內建 Scalar API Reference 互動式網頁介面。
6. **HTTP Client 實作**:
   - 支援同步/非阻塞 client 請求 (`GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `HEAD`)，支援自訂 Header、Query string、JSON Payload 與連線逾時處理。

---

## 任務拆分與進度追蹤 (Task Breakdown & Progress Tracking)

### [x] Phase 1: 專案基礎與基礎元件開發 (Infrastructure & Base Components)
- [x] 1.1 初始化 Git 儲存庫與 MSVC 編譯測試腳本 (`build_and_test.bat`)
- [x] 1.2 建立核心數據結構與 Util 模組（HTTP Status Code, Headers Map, MIME types, URI Parser, Query String Encoder/Decoder, Base64/JSON Helper）
- [x] 1.3 實作 C++23 Request / Response 封裝物件

### [x] Phase 2: IOCP 網路引擎與 Socket 抽象層 (IOCP & Socket Core Engine)
- [x] 2.1 WinSock2 初始化與原生 Socket/IOCP 封裝 (`IoCompletionPort`, Socket Wrapper)
- [x] 2.2 實作 Thread Pool Worker 事件驅動循環 (GetQueuedCompletionStatus)
- [x] 2.3 實作 HTTP/1.1 串流解析器 (Stream Parser & Request Line / Header / Chunked Body Parsing)

### [x] Phase 3: Server 端與 Router / Fluent API 實作 (HTTP Server & Modern Router)
- [x] 3.1 實作核心 Router，支援精確匹配與動態路徑參數 (`/api/users/{id}`)
- [x] 3.2 實作 Fluent Route Metadata API (`tag`, `summary`, `response`, `description`, `handle`)
- [x] 3.3 實作 Middleware 鏈式處理解析 (CORS, Request Logger, Error Handler)
- [x] 3.4 實作 Server 主循環、靜態檔案 Serving、Keep-Alive 超時管理與優雅關閉 (Graceful Shutdown)

### [x] Phase 4: 自動化 API 文件生成 (OpenAPI 3.0 & Scalar UI Integration)
- [x] 4.1 實作 Route Metadata -> OpenAPI 3.0 JSON 轉換器
- [x] 4.2 內建路由 `/openapi.json` 與 `/docs` (Scalar UI) HTML Rendering 服務

### [x] Phase 5: HTTP Client 實作 (HTTP Client Engine)
- [x] 5.1 實作 `httplib23::Client` Socket 建立與連線處理
- [x] 5.2 實作 HTTP Request 送出與 Response 接收/解析器 (支援 Auto-redirect, Custom Headers, Timeout)

### [x] Phase 6: 自動化測試與品質驗證 (Comprehensive Testing & Quality Assurance)
- [x] 6.1 單元測試 (Unit Tests)：HTTP 協定解析、URI 匹配、OpenAPI JSON 格式測試
- [x] 6.2 整合與高併發測試 (Integration & Load Tests)：Server/Client 互通、500+ 多執行緒高併發壓測 (50 Threads x 10 Reqs)
- [x] 6.3 品質檢查：
  - [x] MSVC 最高警告等級編譯無警告 (`/W4 /WX /std:c++latest`)
  - [x] CRT Debug Heap 記憶體洩漏檢測 (`_CrtDumpMemoryLeaks()`)
  - [x] 功能完整測試通過
- [x] 6.4 Git Commit 提交與發布

---

## 測試規劃與結果 (Test Results)
1. **編譯測試**: 使用 MSVC `cl.exe /std:c++latest /W4 /WX /EHsc /utf-8` 編譯，確保 0 Warning 0 Error。(通過)
2. **記憶體洩漏測試**: 透過 `#define _CRTDBG_MAP_ALLOC` 與 `_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);` 檢測，無記憶體洩漏。(通過)
3. **功能性測試**: GET/POST/PUT/DELETE API、Path Variables (`/users/{id}`)、OpenAPI `/openapi.json`、`/docs` Scalar UI、Client GET/POST。(通過)
4. **高併發測試**: 50 個執行緒並行發送 500 個連續 HTTP 請求，IOCP Thread Pool 在極高併發下 100% 成功回應。(通過)
