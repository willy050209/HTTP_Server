// filepath: main.cpp
#include <iostream>
#include <cstdint>
#include <format>
#include "include/httplib23.hpp"

/// <summary>
/// 主程式進入點，初始化 HTTP Server 並啟動服務。
/// </summary>
/// <returns>執行結果狀態碼。</returns>
int main() {
    httplib23::Server server;

    // 1. Health check endpoint
    server.Get("/ping", "Health Check Endpoint")
        .tag("Health")
        .summary("Simple ping/pong check")
        .response(200, "Returns pong text")
        .handle([](const httplib23::Request&, httplib23::Response& res) noexcept {
            res.set_content("pong", "text/plain");
        });

    // 2. User API with path parameters and metadata
    server.Get("/api/v1/users/{id}", "Get User Details")
        .tag("User")
        .summary("Fetch user info by user ID")
        .param("id", "Unique user ID", true, "path", "integer")
        .response(200, "User object found", "application/json")
        .response(404, "User not found", "application/json")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            const auto user_id = req.get_path_param("id").value_or("0");
            res.set_json(std::format(R"({{"id":{}, "name":"Alice", "role":"admin"}})", user_id));
        });

    // 3. Post echo endpoint
    server.Post("/api/v1/echo", "Echo Post Request")
        .tag("Utility")
        .summary("Echoes received payload back")
        .response(200, "Echoed response")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            res.set_json(std::format(R"({{"status":"success", "received": "{}"}})", req.body));
        });

    const uint16_t port = 8080;
    httplib23::log_info("========================================================");
    httplib23::log_info("Starting httplib23 HTTP Server on http://127.0.0.1:{}", port);
    httplib23::log_info(" - Swagger UI API Docs:       http://127.0.0.1:{}/docs", port);
    httplib23::log_info(" - Scalar UI API Docs:        http://127.0.0.1:{}/scalar", port);
    httplib23::log_info(" - OpenAPI 3.0 Specification: http://127.0.0.1:{}/openapi.json", port);
    httplib23::log_info("========================================================");

    if (server.listen("127.0.0.1", port)) {
        httplib23::log_info("Server is running! Press Enter to stop...");
        std::cin.get();
        server.stop();
        httplib23::log_info("Server stopped gracefully.");
    } else {
        httplib23::log_error("Failed to start server on port {}", port);
        return 1;
    }

    return 0;
}
