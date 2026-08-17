# 快速入門與跨平台建置指南 (Getting Started)

本指南引導您在 **Windows**, **Linux** 與 **macOS** 平台上使用 **C++23** 編譯並建置 `httplib23` 跨平台高效能 HTTP 伺服器與非同步日誌服務。

---

## 系統需求 (Prerequisites)

| 作業系統 | 需求編譯器 / 工具 | 原生多路復用引擎 | 系統庫依賴 |
| :--- | :--- | :--- | :--- |
| **Windows 10/11** | MSVC (VS 2022+ `/std:c++latest`) | **IOCP** (I/O Completion Ports) | `ws2_32.lib` |
| **Linux (Ubuntu/Debian/RHEL)** | GCC 13+ 或 Clang 16+ (`-std=c++23`) | **epoll** (`EPOLLIN \| EPOLLET`) | `pthread` |
| **macOS (Intel/Apple Silicon)** | AppleClang 15+ 或 LLVM Clang (`-std=c++23`) | **kqueue** (`EVFILT_READ`) | 原生 C++ 標準庫 |

---

## 專案結構 (Project Layout)

`httplib23` 是單檔包含 (Single-header) 的設計，您只需要將 `include/httplib23.hpp` 放入專案即可：

```text
HTTP_Server/
├── .github/
│   └── workflows/
│       └── ci.yml            # Cross-platform GitHub Actions Workflow
├── include/
│   └── httplib23.hpp        # 核心標頭檔 (包含 Server, Client, Logger, OpenAPI Spec)
├── example/
│   └── multi_port_server.cpp # 多埠伺服器範例
├── src/
│   └── test_runner.cpp      # 自動化綜合測試套件 (跨平台)
├── docs/                    # 詳細技術文件
├── build_and_test.bat       # Windows MSVC 一鍵編譯與測試批次檔
├── main.cpp                 # 預設 HTTP 伺服器與 CI --test-mode 測試進入點
├── README.md                # 專案說明文件
└── LICENSE                  # MIT 授權條款
```

---

## 最簡伺服器與非同步 Logger 範例

建立 `main.cpp`：

```cpp
#include "httplib23.hpp"

int main(int argc, char* argv[]) {
    // 1. 設定非同步 Logger 門檻與日誌輸出
    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);
    httplib23::log_info("正在初始化跨平台 HTTP 伺服器...");

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

## 跨平台手動編譯與執行指令

### 🪟 1. Windows (MSVC CLI)
開啟 **Developer Command Prompt for VS** 並執行：
```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude main.cpp ws2_32.lib /Fe:server.exe
server.exe
```

### 🐧 2. Linux (Ubuntu GCC / Clang)
```bash
g++ -std=c++23 -O2 -pthread main.cpp -Iinclude -o server
./server

# 執行自動化驗證測試
./server --test-mode
```

### 🍎 3. macOS (AppleClang / kqueue)
```bash
clang++ -std=c++23 -O2 main.cpp -Iinclude -o server
./server

# 執行自動化驗證測試
./server --test-mode
```

---

## 執行全套單元與高併發壓測 (Comprehensive Test Suite)

### Windows
```cmd
build_and_test.bat
```

### Linux / macOS
```bash
g++ -std=c++23 -O2 -pthread src/test_runner.cpp -Iinclude -o test_runner
./test_runner
```

測試套件將自動執行：
1. 工具純函數與 URL Encode/Decode 測試。
2. 非同步 Producer-Consumer Logger 門檻與 Callback 測試。
3. 高效能 Radix/Path 路由匹配測試。
4. OpenAPI 3.0, Swagger UI & Scalar UI 產生器測試。
5. 50 執行緒 x 10 請求 (共 500 次) 高併發壓力測試。
