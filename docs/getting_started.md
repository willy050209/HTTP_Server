# 快速入門與建置指南 (Getting Started)

本指南引導您使用 **MSVC (C++23)** 建置並使用 `httplib23` 建立輕量化、高併發的 HTTP 伺服器與非同步日誌服務。

---

## 系統需求 (Prerequisites)

1. **作業系統**: Windows 10 / Windows 11 / Windows Server (64-bit)。
2. **編譯器**: MSVC (Microsoft Visual Studio 2022 v17.x 或更新版本，支援 `/std:c++latest` C++23 語法)。
3. **系統程式庫**: `ws2_32.lib` (WinSock2)。

---

## 專案結構 (Project Layout)

`httplib23` 是單檔包含 (Single-header) 的設計，您只需要將 `include/httplib23.hpp` 放入專案即可：

```text
HTTP_Server/
├── include/
│   └── httplib23.hpp        # 核心標頭檔 (包含 Server, Client, Logger, OpenAPI Spec)
├── example/
│   └── multi_port_server.cpp # 多埠伺服器範例
├── src/
│   └── test_runner.cpp      # 自動化綜合測試套件
├── docs/                    # 詳細技術文件
├── build_and_test.bat       # 一鍵編譯與測試批次檔
├── README.md                # 專案說明文件
└── LICENSE                  # MIT 授權條款
```

---

## 最簡伺服器與非同步 Logger 範例

建立 `main.cpp`：

```cpp
#include "httplib23.hpp"

int main() {
    // 1. 設定非同步 Logger 門檻與日誌輸出
    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);
    httplib23::log_info("正在初始化 HTTP 伺服器...");

    httplib23::Server server;

    // 2. 註冊 HTTP GET 端點 (Fluent API)
    server.Get("/", "Home Page")
        .handle([](const httplib23::Request&, httplib23::Response& res) {
            res.set_content("<h1>Welcome to httplib23!</h1>", "text/html; charset=utf-8");
        });

    server.Get("/api/v1/time", "Get Server Time")
        .tag("System")
        .summary("獲取伺服器當前時間")
        .response(200, "成功返回時間點", "application/json")
        .handle([](const httplib23::Request&, httplib23::Response& res) {
            httplib23::log_info("處理 /api/v1/time 請求");
            res.set_json(std::format(R"({{"time":"{:%Y-%m-%d %H:%M:%S}"}})", std::chrono::system_clock::now()));
        });

    httplib23::log_info("伺服器啟動於 http://127.0.0.1:8080");
    httplib23::log_info(" - Swagger UI: http://127.0.0.1:8080/docs");
    httplib23::log_info(" - Scalar UI : http://127.0.0.1:8080/scalar");

    if (server.listen("127.0.0.1", 8080)) {
        std::cin.get();
        server.stop();
    }

    return 0;
}
```

---

## 手動編譯指令 (MSVC CLI)

開啟 **Developer Command Prompt for VS** 並執行：

```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude main.cpp ws2_32.lib /Fe:server.exe
```

編譯成功後執行：

```cmd
server.exe
```

在瀏覽器或 `curl` 開啟：
- 主頁: `http://127.0.0.1:8080/`
- 時間 API: `http://127.0.0.1:8080/api/v1/time`
- Swagger UI 測試介面: `http://127.0.0.1:8080/docs`
- Scalar UI 文件介面: `http://127.0.0.1:8080/scalar`

---

## 執行全套單元與高併發壓測

直接在命令列執行一鍵批次檔：

```cmd
build_and_test.bat
```

該腳本將自動：
1. 以 MSVC C++23 最高警告等級 `/W4 /WX` 編譯 `test_runner.cpp`。
2. 執行含 50 執行緒 x 10 請求的高併發 IOCP 壓力測試。
3. 執行非同步 Producer-Consumer Logger 功能測試。
4. 進行 `_CrtDumpMemoryLeaks()` 記憶體洩漏檢測。
