# httplib23

[![Cross-Platform Build & Test](https://github.com/willy050209/HTTP_Server/actions/workflows/ci.yml/badge.svg)](https://github.com/willy050209/HTTP_Server/actions/workflows/ci.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

一個基於 **C++23** 建立的高效能、跨平台 (Windows / Linux / macOS)、零外部相依性 (Zero Dependencies)、單檔包含 (Single-header `httplib23.hpp`) 的現代化 HTTP Server & Client 函式庫。

底層非同步 I/O 引擎根據 OS 自動採用原生最高效能架構：
- 🪟 **Windows**: **IOCP** (I/O Completion Ports)
- 🐧 **Linux**: **epoll** (Edge-Triggered 非阻塞 Multiplexer)
- 🍎 **macOS**: **kqueue** (Event Multiplexer)

內建 **Producer-Consumer 非同步 Logger**、**OpenAPI 3.0** JSON 規格產生與 **Swagger UI** / **Scalar UI** 雙互動式網頁文件服務。

---

## 核心特徵 (Features)

- ⚡ **單檔包含 (Single-header)**: 僅需引入 `httplib23.hpp` 即可使用，完全零第三方依賴。
- 🚀 **跨平台高效能 I/O Multiplexer Engine**:
  - **Windows (IOCP)**: `CreateIoCompletionPort` + `WSARecv` / `WSASend`。
  - **Linux (epoll)**: `epoll_create1` + `EPOLLIN | EPOLLET` 邊緣觸發模式。
  - **macOS (kqueue)**: `kqueue` + `EVFILT_READ` 事件驅動。
  - **Scatter-Gather I/O**: Windows 下 `WSABUF[2]`，POSIX 下 `writev` 零拷貝標頭與 Payload。
- 📝 **Producer-Consumer 非同步 Logger Engine**:
  - **高效非阻塞**: 工作執行緒（Producer）僅需將格式化 Log 入列，由獨立背景 Consumer 執行緒批次輸出。
  - **零開銷過濾 (Log Level & OFF)**: 提供 `DEBUG`, `INFO`, `WARN`, `ERROR`, `OFF`，過濾時早期 Return 零開銷。
  - **零巨集 & `std::source_location`**: 使用 C++20 `std::source_location` 自動取得調用檔名與行號。
  - **自訂 Sink**: 支援傳入 Lambda Callback 自訂輸出至 Console、檔案或日誌中心 (Loki/ELK)。
- 💎 **現代 C++23 風格**: 使用 `std::string_view`, `std::expected`, `std::span`, `std::format`, `std::print`, Concepts, Lambda 及 Fluent API。
- 📜 **自動化與可自訂 API 文件 (Swagger UI & Scalar UI)**:
  - **`/docs`**: 預設嵌入 **Swagger UI** 互動式線上測試與文件網頁 (路徑可自訂)。
  - **`/scalar`**: 預設嵌入 **Scalar UI** (`@scalar/api-reference`) 現代化網頁介面 (路徑可自訂)。
  - **`/openapi.json`**: 自動將註冊之 Routes 轉換為規範的 OpenAPI 3.0.3 Spec JSON (路徑可自訂)。
  - **靈活單獨開關**: 支援全域 (`enable_docs`) 或單獨開啟/關閉 Swagger UI (`enable_swagger`) 與 Scalar UI (`enable_scalar`)。
- 🌐 **全功能 HTTP Server & Client**: 支援 `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`, `HEAD` 請求、動態路徑參數 (`/users/{id}`)、Query String 解析與 Middleware 擴充。
- 🤖 **GitHub Actions CI/CD 集成**: 自動驗證 Windows (MSVC), Linux (GCC/Clang), macOS (AppleClang) 三平台建置與全測試套件。

---

## 快速開始 (Quick Start)

### 1. HTTP Server & Async Logger 範例

```cpp
#include "httplib23.hpp"

int main(int argc, char* argv[]) {
    // 1. 配置非同步 Logger (支援 DEBUG/INFO/WARN/ERROR/OFF 與自訂 Sink)
    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);
    httplib23::log_info("伺服器正在準備啟動...");

    httplib23::Server server;

    // 2. 配置 API 文件選項 (支援單獨開啟/關閉 Swagger UI 或 Scalar UI)
    server.set_doc_options({
        .enabled = true,
        .enable_swagger = true,       // 啟用 Swagger UI
        .enable_scalar = true,        // 啟用 Scalar UI
        .openapi_path = "/openapi.json",
        .swagger_path = "/docs",      // Swagger UI 存取路徑
        .scalar_path = "/scalar",     // Scalar UI 存取路徑
        .title = "My Cross-Platform C++23 API",
        .version = "1.0.0"
    });

    // 3. 定義 GET API 並加入 OpenAPI 文件元資料 (Fluent API)
    server.Get("/api/v1/users/{id}", "取得用戶詳情")
        .tag("User")
        .summary("依據 User ID 獲取用戶資料")
        .param("id", "使用者唯一 ID", true, "path", "integer")
        .response(200, "成功取得用戶資訊", "application/json")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            auto id = req.get_path_param("id").value_or("0");
            httplib23::log_info("Handling request for user ID: {}", id);
            res.set_json(std::format(R"({{"id":{}, "name":"Alice", "role":"Admin"}})", id));
        });

    httplib23::log_info("伺服器啟動於 http://127.0.0.1:8080");
    httplib23::log_info(" - Swagger UI 文件: http://127.0.0.1:8080/docs");
    httplib23::log_info(" - Scalar UI 文件 : http://127.0.0.1:8080/scalar");

    if (server.listen("127.0.0.1", 8080)) {
        std::cin.get();
        server.stop();
    }
    return 0;
}
```

---

## 跨平台編譯與建置 (Cross-Platform Compilation)

### 🪟 Windows (MSVC)
```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude main.cpp ws2_32.lib /Fe:server.exe
build_and_test.bat
```

### 🐧 Linux (GCC / Clang C++23)
```bash
g++ -std=c++23 -O2 -pthread main.cpp -Iinclude -o server
./server --test-mode
g++ -std=c++23 -O2 -pthread src/test_runner.cpp -Iinclude -o test_runner
./test_runner
```

### 🍎 macOS (AppleClang C++23)
```bash
clang++ -std=c++23 -O2 main.cpp -Iinclude -o server
./server --test-mode
clang++ -std=c++23 -O2 src/test_runner.cpp -Iinclude -o test_runner
./test_runner
```

---

## 詳細文檔目錄 (Documentation)

請參考 [`docs/`](docs/) 目錄查看完整的說明文件：

- 📘 [快速入門與跨平台建置指南](docs/getting_started.md)
- 📙 [Server API 介面與 Async Logger 說明](docs/server_api.md)
- 📗 [Client API 介面說明](docs/client_api.md)
- 📙 [OpenAPI 3.0, Swagger UI & Scalar UI 整合說明](docs/openapi_scalar.md)
- 🔬 [跨平台系統架構 (IOCP / epoll / kqueue)](docs/architecture.md)

---

## 授權 (License)

本專案採用 [MIT License](LICENSE) 授權。
