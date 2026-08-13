# 快速入門與建置指南 (Getting Started & Build Guide)

歡迎使用 `httplib23`！本文檔將引導您如何在專案中引入 `httplib23` 並建置第一個 C++23 HTTP 服務。

---

## 系統需求 (Prerequisites)

- **作業系統**: Windows 10 / 11 / Server (相容原生 IOCP 與 WinSock2)
- **編譯器**: MSVC (Microsoft Visual C++ Optimizing Compiler Version 19.38+) 或 VS 2022 / VS 2026
- **C++ 標準**: C++23 模式 (MSVC 旗標 `/std:c++latest`)

---

## 專案引入方法 (Single-Header Integration)

`httplib23` 為單檔包含 (Single-header) 設計，您只需將 [`include/httplib23.hpp`](../include/httplib23.hpp) 複製到專案標頭檔目錄中：

```cpp
#include "httplib23.hpp"
```

連結 Windows Socket 函式庫 (`ws2_32.lib`) 即可直接編譯使用。

---

## 第一個 HTTP Server

建立 `main.cpp`：

```cpp
#include <print>
#include "httplib23.hpp"

int main() {
    httplib23::Server server;

    // 開啟 Swagger UI (/docs) 與 Scalar UI (/scalar) 文件端點
    server.enable_docs(true);

    server.Get("/", "Home endpoint").handle([](const httplib23::Request&, httplib23::Response& res) {
        res.set_content("Welcome to httplib23!", "text/plain; charset=utf-8");
    });

    std::println("Server running on http://127.0.0.1:8080");
    std::println(" - Swagger UI: http://127.0.0.1:8080/docs");
    std::println(" - Scalar UI : http://127.0.0.1:8080/scalar");

    if (server.listen("127.0.0.1", 8080)) {
        std::cin.get();
        server.stop();
    }
    return 0;
}
```

---

## 編譯命令 (MSVC Command Line)

打開 Developer Command Prompt for VS 並執行：

```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 main.cpp ws2_32.lib
```

說明：
- `/std:c++latest`: 啟用 C++23 語言特性與標準庫。
- `/W4 /WX`: 最高等級警報，並將警告視為錯誤。
- `/EHsc`: 啟用標準 C++ 例外處理。
- `/utf-8`: 設定原始碼與執行階段字元集為 UTF-8。
- `ws2_32.lib`: 連結 Windows Socket 系統 API 庫。

---

## 執行測試套件 (Automated Test Suite)

專案包含完整的單元測試、高併發壓測與記憶體洩漏檢測：

```cmd
build_and_test.bat
```
