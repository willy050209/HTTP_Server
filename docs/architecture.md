# 系統架構與 IOCP 高併發設計 (Technical Architecture)

本文說明 `httplib23` 的底層網路架構、IOCP 事件驅動模型與 Worker Thread Pool 的設計原理。

---

## 1. 核心架構視圖 (Architecture Diagram)

```text
 Client Connections
        │
        ▼
   [ Accept Loop ]
        │  (CreateIoCompletionPort)
        ▼
   [ Windows IOCP Kernel Queue ]
        │  (GetQueuedCompletionStatus)
        ▼
 [ IOCP Worker Threads ] ◄── (Non-blocking Socket I/O)
        │
        ▼
  [ Thread Pool Task Queue ]
        │
        ▼
  [ HTTP Request Processor & Router Match ] ──► [ OpenAPI / Scalar UI ]
        │
        ▼
 [ Response Serializer ] ──► (WSASend Asynchronous Completion)
```

---

## 2. Windows IOCP (I/O Completion Ports) 網路模型

在 Windows 平台上，IOCP 是性能最高、擴展性最強的非阻塞事件通知機制。

- **多執行緒事件通知**: 透過 `CreateIoCompletionPort` 與原生 `WSARecv` / `WSASend`，內核直接在 Completion Port 上維護 I/O 完成佇列。
- **無輪詢代價 (Zero Polling Overhead)**: 工作執行緒呼叫 `GetQueuedCompletionStatus` 處於高效休眠狀態，當網卡接收資料並完成時由 OS 直接喚醒。
- **重疊 I/O 結構 (`PerIoData`)**:
  ```cpp
  struct PerIoData {
      WSAOVERLAPPED overlapped;
      WSABUF wsa_buf;
      char buffer[8192];
      IOOperation op_type;
      SOCKET socket;
      char* dynamic_send_buf = nullptr;
  };
  ```

---

## 3. Worker Thread Pool 任務併發分派

為了防範單一長時間運行的 Route Handler 阻塞 IOCP 的 I/O 接收循環，`httplib23` 將 **I/O 數據接收** 與 **HTTP Handler 執行** 解耦：

1. **IOCP Worker Threads**: 僅負責從 Kernel 完成佇列中取出原生的連線 packets。
2. **Task Thread Pool**: IOCP 收到 complete packet 後，將 HTTP Request 解析與 Handler 任務封裝為 Task enqueue 至 Thread Pool 中併發執行。
3. **Async Response Send**: Handler 完成 Response 構建後，調用 `WSASend` 提交非阻塞發送，完成發送後觸發 `IOOperation::WRITE` 自動清理資源並關閉 Socket。

---

## 4. 記憶體管理與安全防護

- **零記憶體洩漏 guarantee**: 所有非同步 IO 的 `PerIoData` 記憶體空間均在 Completion 點由 IOCP worker 安全回收與刪除。
- **CRT Debug Heap 監控**: 於 Debug 模式下包含 `_CrtDumpMemoryLeaks()`，確保在伺服器極高併發壓力下程式停止時無任何 Dangling Pointers 或 Memory Leaks。
