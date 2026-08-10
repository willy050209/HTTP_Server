# httplib23

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![MSVC](https://img.shields.io/badge/Compiler-MSVC-purple.svg)](https://visualstudio.microsoft.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

一個基於 **C++23** 建立的輕量化、高併發、零外部相依性 (Zero-dependency)、單檔包含 (Single-header `httplib23.hpp`) 的現代化 HTTP Server & Client 函式庫，同時內建 **OpenAPI 3.0** JSON 規格產生與 **Scalar API Reference** 互動式網頁文件服務。

---

## 核心特徵 (Features)

- ⚡ **單檔包含 (Single-header)**: 僅需引入 `httplib23.hpp` 即可使用，完全零第三方依賴。
- 🚀 **高效能 IOCP 併發架構**: 基於 Windows 原生 **I/O Completion Ports (IOCP)** 與 Worker Thread Pool，支援極高連線併發處理。
- 💎 **現代 C++23 風格**: 使用 `std::string_view`, `std::expected`, `std::span`, `std::format`, Concepts, Lambda 及 Fluent API。
- 📜 **自動化 API 文件 (Scalar UI)**:
  - `/openapi.json`: 自動將註冊之 Routes 轉換為規範的 OpenAPI 3.0.3 Spec JSON。
  - `/docs`: 自動產生美觀的 **Scalar API Reference** 互動式線上測試與文件網頁。
- 🌐 **全功能 HTTP Server & Client**: 支援 `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`, `HEAD` 請求、動態路徑參數 (`/users/{id}`)、Query String 解析與 Middleware 擴充。

---

## 快速開始 (Quick Start)

### 1. HTTP Server 範例

```cpp
#include <iostream>
#include "httplib23.hpp"

int main() {
    httplib23::Server server;

    // 定義 GET API 並加入 OpenAPI / Scalar 文件元資料 (Fluent API)
    server.Get("/api/v1/users/{id}", "取得用戶詳情")
        .tag("User")
        .summary("依據 User ID 獲取用戶資料")
        .param("id", "使用者唯一 ID", true, "path", "integer")
        .response(200, "成功取得用戶資訊", "application/json")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            auto id = req.get_path_param("id").value_or("0");
            res.set_json(std::format(R"({{"id":{}, "name":"Alice", "role":"Admin"}})", id));
        });

    // 定義 POST API
    server.Post("/api/v1/echo", "Echo 回傳 payload")
        .tag("Utility")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            res.set_json(std::format(R"({{"status":"ok", "body":"{}"}})", req.body));
        });

    std::cout << "伺服器啟動於 http://127.0.0.1:8080\n";
    std::cout << "互動式 Scalar 文件: http://127.0.0.1:8080/docs\n";

    if (server.listen("127.0.0.1", 8080)) {
        std::cin.get();
        server.stop();
    }
    return 0;
}
```

### 2. HTTP Client 範例

```cpp
#include <iostream>
#include "httplib23.hpp"

int main() {
    httplib23::Client client("http://127.0.0.1:8080");

    // 發送 GET 請求
    auto res = client.Get("/api/v1/users/42");
    if (res) {
        std::cout << "HTTP Status: " << res->status << "\n";
        std::cout << "Response Body: " << res->body << "\n";
    } else {
        std::cerr << "Request failed: " << res.error() << "\n";
    }

    return 0;
}
```

---

## 編譯與測試 (Build & Test)

使用 MSVC (C++23 `/std:c++latest`) 編譯：

```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 main.cpp ws2_32.lib
```

執行完整測試套件（含高併發測試與記憶體洩漏檢測）：

```cmd
build_and_test.bat
```

---

## 詳細文檔目錄 (Documentation)

請參考 [`docs/`](docs/) 目錄查看完整的說明文件：

- 📘 [快速入門與建置指南](docs/getting_started.md)
- 📙 [Server API 介面說明](docs/server_api.md)
- 📗 [Client API 介面說明](docs/client_api.md)
- 📙 [OpenAPI 3.0 & Scalar UI 整合說明](docs/openapi_scalar.md)
- 🔬 [系統架構與 IOCP 高併發設計](docs/architecture.md)

---

## 授權 (License)

本專案採用 MIT License 授權。
