# 快速入門與建置指南 (Getting Started)

本指南說明如何將 `httplib23.hpp` 整合至專案中，以及如何使用 MSVC 進行 C++23 環境編譯。

---

## 系統需求 (Prerequisites)

- **作業系統**: Windows 10 / Windows 11 / Windows Server
- **編譯器**: MSVC (Microsoft Visual C++) 2022 / 2026 或以上，需支援 C++23 (`/std:c++latest` 或 `/std:c++23`)
- **SDK**: Windows SDK (提供 Winsock2 / `ws2_32.lib`)

---

## 專案整合 (Integration)

`httplib23` 為單檔包含 (Single-header) 標頭檔，您只需將 [`httplib23.hpp`](../httplib23.hpp) 複製到您的專案目錄中並直接包含：

```cpp
#include "httplib23.hpp"
```

由於 Winsock2 需要連結 `ws2_32.lib`，`httplib23.hpp` 內部已預設包含 `#pragma comment(lib, "ws2_32.lib")`，因此在 MSVC 下無需額外在專案設定中指定 `.lib` 檔。

---

## 編譯命令 (Compilation)

### 命令列編譯 (MSVC `cl.exe`)

開啟 Visual Studio Developer Command Prompt 或於腳本中載入 `vcvars64.bat`：

```cmd
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 main.cpp
```

- `/std:c++latest`: 啟用 C++23/最新 C++ 標準特徵
- `/W4 /WX`: 啟用最高等級警告並視為編譯錯誤
- `/EHsc`: 啟用 C++ 異常處理
- `/utf-8`: 設定原始碼與執行字元集為 UTF-8

### 執行編譯與測試腳本

專案內提供自動編譯與執行腳本 [`build_and_test.bat`](../build_and_test.bat)：

```cmd
.\build_and_test.bat
```

該腳本會使用 MSVC 編譯測試程式 [`test_runner.cpp`](../test_runner.cpp) 並執行：
1. 基礎 Utility 單元測試
2. Router 導向匹配測試
3. OpenAPI 生成與格式檢查
4. Server / Client 整合測試
5. 50 個 Thread 併發 500 個 HTTP 請求的高併發壓測
6. CRT Debug Heap 記憶體洩漏自動檢測
