// filepath: example/multi_port_server.cpp
#include <print>
#include <iostream>
#include <cstdint>
#include <chrono>
#include <format>
#include <thread>
#include "../include/httplib23.hpp"

/// <summary>
/// 取得目前系統時間之 ISO 8601 格式字串純函數。
/// </summary>
/// <returns>格式化系統時間字串。</returns>
[[nodiscard]] inline std::string get_current_system_time() noexcept {
    const auto now = std::chrono::system_clock::now();
    return std::format("{:%Y-%m-%d %H:%M:%S UTC}", now);
}

/// <summary>
/// 多埠 HTTP 伺服器範例進入點。
/// 包含：
/// - Port 8080: 回傳 "Hello World!" API
/// - Port 8081: 回傳目前系統時間 API
/// </summary>
/// <returns>程式執行狀態碼。</returns>
int main() {
    // ------------------------------------------------------------------------
    // Server 1: 監聽 Port 8080，回傳 "Hello World!"
    // ------------------------------------------------------------------------
    httplib23::Server server_hello;

    server_hello.Get("/", "Hello World Endpoint")
        .tag("General")
        .summary("Returns a greeting message")
        .response(200, "Hello World text response", "text/plain")
        .handle([](const httplib23::Request&, httplib23::Response& res) noexcept {
            res.set_content("Hello World!", "text/plain; charset=utf-8");
        });

    // ------------------------------------------------------------------------
    // Server 2: 監聽 Port 8081，回傳目前系統時間
    // ------------------------------------------------------------------------
    httplib23::Server server_time;

    server_time.Get("/", "System Time Endpoint")
        .tag("Time")
        .summary("Returns current UTC system time")
        .response(200, "JSON payload containing current timestamp", "application/json")
        .handle([](const httplib23::Request&, httplib23::Response& res) noexcept {
            const std::string time_str = get_current_system_time();
            res.set_json(std::format(R"({{"status":"success", "timestamp":"{}"}})", time_str));
        });

    const uint16_t port_hello = 8080;
    const uint16_t port_time = 8081;

    std::println("========================================================");
    std::println("Starting httplib23 Multi-Port HTTP Servers:");
    std::println(" 1. Hello World Server : http://127.0.0.1:{}", port_hello);
    std::println("    - Interactive Docs  : http://127.0.0.1:{}/docs", port_hello);
    std::println(" 2. System Time Server : http://127.0.0.1:{}", port_time);
    std::println("    - Interactive Docs  : http://127.0.0.1:{}/docs", port_time);
    std::println("========================================================");

    // 在獨立執行緒啟動第一台 Server (Port 8080)
    std::thread thread_hello([&server_hello, port_hello]() {
        if (!server_hello.listen("127.0.0.1", port_hello)) {
            std::println(stderr, "[ERROR] Failed to start Hello World Server on port {}", port_hello);
        }
    });

    // 在獨立執行緒啟動第二台 Server (Port 8081)
    std::thread thread_time([&server_time, port_time]() {
        if (!server_time.listen("127.0.0.1", port_time)) {
            std::println(stderr, "[ERROR] Failed to start System Time Server on port {}", port_time);
        }
    });

    std::println("Servers are running! Press Enter to stop...");
    std::cin.get();

    // 關閉伺服器並等待執行緒回歸
    server_hello.stop();
    server_time.stop();

    if (thread_hello.joinable()) thread_hello.join();
    if (thread_time.joinable()) thread_time.join();

    std::println("Servers shut down gracefully.");
    return 0;
}
