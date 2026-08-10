# 系統架構、安全性與效能優化 (Technical Architecture & Security)

本文說明 `httplib23` 的底層網路架構、IOCP 事件驅動模型、安全性防護機制與 Scatter-Gather I/O 等效能優化設計。

---

## 1. 核心架構與連線狀態機 (Architecture Diagram)

```text
 Client Connections
        │
        ▼
   [ Accept Loop ]
        │  (CreateIoCompletionPort)
        ▼
   [ ConnectionSession State Machine ] ◄── Cumulative Buffer Assembly (TCP Un-sticking)
        │
        ▼
   [ Windows IOCP Kernel Queue ]
        │  (GetQueuedCompletionStatus)
        ▼
 [ IOCP Worker Threads ] ◄── (Zero-Copy Header & Body Parsing)
        │
        ▼
 [ Bounded Thread Pool ] ◄── (Backpressure 503 protection)
        │
        ▼
  [ HTTP Request Processor & Router Match ] ──► [ OpenAPI / Scalar UI ]
        │
        ▼
 [ Scatter-Gather WSASend ] ──► (WSABUF[0] Header + WSABUF[1] Body)
```

---

## 2. 安全性與漏洞防護 (Security Enhancements)

### 2.1 TCP 封包黏包與拆包處理 (TCP Stream Fragmentation)
- 導入 `ConnectionSession` 狀態機，每條 TCP 連線擁有獨立的累積接收緩衝區 (`rx_buffer`)。
- 先檢測 `\r\n\r\n` 確保標頭完整接收，再依據 `Content-Length` 解析確保完整的 Body 到齊後才進行處理。

### 2.2 防範 DoS 與記憶體爆彈 (Unbounded Resource Protection)
- **Header & Body 大小限制**:
  - `MAX_HEADER_SIZE` = 64 KB (超過返回 `413 Payload Too Large`)
  - `MAX_BODY_SIZE` = 10 MB (超過返回 `413 Payload Too Large`)
- **ThreadPool 背壓機制 (Backpressure)**:
  - 任務佇列設置有界上限 `MAX_QUEUE_SIZE` = 10,000。當佇列滿載時自動拒絕連線並返回 `503 Service Unavailable`。

### 2.3 Slowloris 慢速連線防護 (Watchdog Timer)
- 內建後台 Watchdog 執行緒，定期檢查所有活動中的連線 Session。
- 若連線未在指定時間（`CONNECTION_TIMEOUT` = 15 秒）內完成 Request 傳送，自動將 Socket 斷開。

### 2.4 CRLF 注入過濾 (CRLF Injection Prevention)
- `Response::set_header()` 在設定 Header key/value 時會主動檢查並過濾 `\r` 與 `\n` 字元，防止 HTTP Response Splitting 攻擊。

### 2.5 修正 IOCP 資源關閉程序 (Safe Shutdown Sequence)
- 伺服器關閉時，先向 IOCP 發送退出信號喚醒並 join worker 執行緒，接著清空連線 Session，最後才銷毀 IOCP Handle，杜絕競態條件。

---

## 3. 效能優化 (Performance Optimizations)

### 3.1 Zero-Copy HTTP 標頭解析
- 解析請求時，全自動採用 `std::string_view` 切片與 `std::from_chars` 解析，大幅減少記憶體配置與字串複製。

### 3.2 Scatter-Gather I/O (`WSASend`)
- 回應 Response 時，利用 `WSASend` 的 `WSABUF` 陣列功能（`WSABUF[0]` 放 Status+Header，`WSABUF[1]` 放 Body），直接將兩塊區域送出，避免手動將 Header 與大型 Body 拼接到同一塊記憶體的字串複製開銷。
