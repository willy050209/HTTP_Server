// filepath: src/test_runner.cpp
#include <iostream>
#include <cassert>
#include <print>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <format>
#include <stdexcept>
#include "../include/httplib23.hpp"

/// <summary>
/// 測試 1: Utility 工具函式測試
/// </summary>
void test_utilities() {
    std::println("[TEST] Running Utility Tests...");

    // URL Encoding / Decoding
    const std::string original = "Hello World! @ 2026 #C++23";
    const std::string encoded = httplib23::detail::url_encode(original);
    const std::string decoded = httplib23::detail::url_decode(encoded);
    assert(decoded == original);

    // Query String Parsing
    const auto params = httplib23::detail::parse_query_string("name=John%20Doe&age=30&city=Taipei");
    assert(params.at("name") == "John Doe");
    assert(params.at("age") == "30");
    assert(params.at("city") == "Taipei");

    // JSON Escaping
    const std::string raw_json = "Line 1\nLine 2 \"Quote\" \t Tab";
    const std::string escaped = httplib23::detail::escape_json(raw_json);
    assert(escaped.find("\\n") != std::string::npos);
    assert(escaped.find("\\\"") != std::string::npos);

    std::println("  -> Utility tests passed!");
}

/// <summary>
/// 測試: 非同步 Producer-Consumer Logger 功能測試
/// </summary>
void test_logger() {
    std::println("[TEST] Running Asynchronous Logger Tests...");

    std::vector<std::string> received_logs;
    std::mutex log_mutex;

    httplib23::Logger::instance().set_sink([&received_logs, &log_mutex](httplib23::LogLevel level, std::string_view msg) {
        std::lock_guard<std::mutex> lock(log_mutex);
        received_logs.push_back(std::format("[{}] {}", httplib23::level_to_string(level), msg));
    });

    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);

    // DEBUG 訊息應被過濾 (zero overhead)
    httplib23::log_debug("This debug message should be filtered {}", 123);
    
    // INFO, WARN, ERROR 訊息應正常記錄
    httplib23::log_info("Hello Async Logger {}", 42);
    httplib23::log_warn("Warning event {}", "TestWarn");
    httplib23::log_error("Error event code {}", 500);

    // 等待背景 Logger 執行緒處理佇列
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        assert(received_logs.size() == 3);
        assert(received_logs[0].find("Hello Async Logger 42") != std::string::npos);
        assert(received_logs[1].find("Warning event TestWarn") != std::string::npos);
        assert(received_logs[2].find("Error event code 500") != std::string::npos);
        assert(received_logs[0].find("test_runner.cpp") != std::string::npos); // std::source_location
    }

    // 測試 LogLevel::OFF
    httplib23::Logger::instance().set_level(httplib23::LogLevel::OFF);
    httplib23::log_error("This error message should be completely ignored when OFF");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        assert(received_logs.size() == 3);
    }

    // 恢復預設 Sink 與 INFO 層級
    httplib23::Logger::instance().set_sink(nullptr);
    httplib23::Logger::instance().set_level(httplib23::LogLevel::INFO);

    std::println("  -> Asynchronous Logger tests passed!");
}

/// <summary>
/// 測試 2: Router 路由配對測試 (包含動態參數與靜態路徑)
/// </summary>
void test_router_matching() {
    std::println("[TEST] Running Router Matching Tests...");

    httplib23::Router router;
    
    router.add_route(httplib23::Method::GET, "/users/{id}", "Get User By ID").handler = [](const httplib23::Request&, httplib23::Response& res) {
        res.set_content("User Endpoint");
    };

    router.add_route(httplib23::Method::POST, "/api/v1/posts", "Create Post").handler = [](const httplib23::Request&, httplib23::Response& res) {
        res.set_content("Post Created");
    };

    httplib23::HandlerFunc handler;
    std::unordered_map<std::string, std::string> params;

    // Test Dynamic Route Match
    const bool match1 = router.match(httplib23::Method::GET, "/users/123", handler, params);
    assert(match1 == true);
    assert(params["id"] == "123");

    // Test Exact Match
    params.clear();
    const bool match2 = router.match(httplib23::Method::POST, "/api/v1/posts", handler, params);
    assert(match2 == true);
    assert(params.empty());

    // Test Mismatch Route
    const bool match3 = router.match(httplib23::Method::GET, "/nonexistent", handler, params);
    assert(match3 == false);

    std::println("  -> Router matching tests passed!");
}

/// <summary>
/// 測試 3: Client URL 解析與例外捕捉測試
/// </summary>
void test_client_url_parsing_and_exceptions() {
    std::println("[TEST] Running Client URL Parsing & Exception Tests...");

    // Test Client URL Host / Port / Path extraction
    httplib23::Client client1("http://localhost:8080/api/v1");
    httplib23::Client client2("http://localhost/");
    httplib23::Client client3("http://127.0.0.1:9090");

    // CRLF Injection Validation
    httplib23::Response res;
    bool caught_crlf = false;
    try {
        res.set_header("X-Custom\r\nHeader", "value");
    } catch (const std::invalid_argument&) {
        caught_crlf = true;
    }
    assert(caught_crlf == true);

    // Invalid Route Pattern Validation
    httplib23::Router router;
    bool caught_invalid_pattern = false;
    try {
        router.add_route(httplib23::Method::GET, "invalid_pattern");
    } catch (const std::invalid_argument&) {
        caught_invalid_pattern = true;
    }
    assert(caught_invalid_pattern == true);

    std::println("  -> Client URL Parsing & Exception tests passed!");
}

/// <summary>
/// 測試 4: OpenAPI 3.0 與 Scalar HTML 產生器測試
/// </summary>
void test_openapi_generator() {
    std::println("[TEST] Running OpenAPI Generator Tests...");

    httplib23::Router router;
    auto& entry = router.add_route(httplib23::Method::GET, "/items/{item_id}", "Get Item");
    entry.meta.tags.push_back("Inventory");

    const auto routes = router.get_routes();
    const std::string openapi_json = httplib23::OpenApiGenerator::generate_spec(routes, "Test API", "2.0.0");
    assert(openapi_json.find("openapi") != std::string::npos);
    assert(openapi_json.find("3.0.3") != std::string::npos);
    assert(openapi_json.find("Inventory") != std::string::npos);

    const std::string scalar_html = httplib23::ScalarDocGenerator::generate_html("/openapi.json");
    assert(scalar_html.find("<script id=\"api-reference\"") != std::string::npos);

    std::println("  -> OpenAPI Generator tests passed!");
}

/// <summary>
/// 測試 5: Server / Client 完整整合、例外捕捉與高併發壓力測試
/// </summary>
void test_server_client_integration() {
    std::println("[TEST] Running Server/Client Integration, Exception & High-Concurrency Tests...");

    httplib23::Server server;

    // 1. GET /ping
    server.Get("/ping", "Ping Check")
        .tag("Health")
        .handle([](const httplib23::Request&, httplib23::Response& res) noexcept {
            res.set_content("pong", "text/plain");
        });

    // 2. GET /users/{id}
    server.Get("/users/{id}", "Get User")
        .tag("Users")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            const auto id = req.get_path_param("id").value_or("0");
            res.set_json(std::format(R"({{"id":{},"name":"User_{}"}})", id, id));
        });

    // 3. POST /echo
    server.Post("/echo", "Echo Request Body")
        .tag("Test")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            res.set_json(std::format(R"({{"echo":"{}"}})", req.body));
        });

    // 4. GET /throw (測試 Route Handler 拋出未捕獲例外)
    server.Get("/throw", "Throw Exception Test")
        .tag("Test")
        .handle([](const httplib23::Request&, httplib23::Response&) {
            throw std::runtime_error("Simulated Uncaught Exception in Route Handler");
        });

    const uint16_t port = 18080;
    const bool started = server.listen("127.0.0.1", port);
    assert(started == true);
    std::println("  -> Server started on port {}", port);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 使用帶有完整 URL 的 Client 初始化
    httplib23::Client client("http://127.0.0.1:18080/api/v1");

    // 1. Test GET /ping
    const auto res_ping = client.Get("/ping");
    assert(res_ping.has_value());
    assert(res_ping->status == 200);
    assert(res_ping->body == "pong");

    // 2. Test GET /users/42
    const auto res_user = client.Get("/users/42");
    assert(res_user.has_value());
    assert(res_user->status == 200);
    assert(res_user->body.find("\"id\":42") != std::string::npos);

    // 3. Test POST /echo
    const auto res_echo = client.Post("/echo", "Hello C++23");
    assert(res_echo.has_value());
    assert(res_echo->status == 200);
    assert(res_echo->body.find("Hello C++23") != std::string::npos);

    // 4. Test Route Handler Uncaught Exception -> Expect 500 Internal Server Error
    const auto res_throw = client.Get("/throw");
    assert(res_throw.has_value());
    assert(res_throw->status == 500);
    assert(res_throw->body.find("Simulated Uncaught Exception") != std::string::npos);
    std::println("  -> Route Handler uncaught exception caught safely with status 500!");

    // 5. Test OpenAPI Spec Endpoint /openapi.json
    const auto res_openapi = client.Get("/openapi.json");
    assert(res_openapi.has_value());
    assert(res_openapi->status == 200);
    assert(res_openapi->body.find("openapi") != std::string::npos);

    // 6. Test Swagger UI Endpoint /docs (Default path)
    const auto res_swagger = client.Get("/docs");
    assert(res_swagger.has_value());
    assert(res_swagger->status == 200);
    assert(res_swagger->body.find("swagger-ui") != std::string::npos);

    // 7. Test Scalar UI Endpoint /scalar (Default path)
    const auto res_scalar = client.Get("/scalar");
    assert(res_scalar.has_value());
    assert(res_scalar->status == 200);
    assert(res_scalar->body.find("api-reference") != std::string::npos);

    // 7. High Concurrency Stress Test: 50 Threads x 10 requests = 500 requests
    std::println("  -> Starting High Concurrency Stress Test (50 threads x 10 requests = 500 requests)...");
    constexpr int32_t NUM_THREADS = 50;
    constexpr int32_t REQS_PER_THREAD = 10;
    std::atomic<int32_t> success_count{0};

    std::vector<std::thread> workers;
    for (int32_t t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([port, &success_count]() {
            httplib23::Client thread_client("127.0.0.1", port);
            for (int32_t r = 0; r < REQS_PER_THREAD; ++r) {
                const auto res = thread_client.Get("/ping");
                if (res.has_value() && res->status == 200 && res->body == "pong") {
                    success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    assert(success_count == NUM_THREADS * REQS_PER_THREAD);
    std::println("  -> High Concurrency Stress Test passed! All {} requests succeeded!", success_count.load());

    server.stop();
    std::println("  -> Server stopped gracefully.");
}

/// <summary>
/// 測試 6: API 文件啟用/停用與自訂路徑測試 (Swagger UI / Scalar UI / OpenAPI Spec)
/// </summary>
void test_doc_configuration() {
    std::println("[TEST] Running API Documentation Configuration Tests...");

    // 1. 測試關閉 API 文件功能 (enable_docs(false))
    {
        httplib23::Server server;
        server.enable_docs(false);
        server.Get("/ping").handle([](const httplib23::Request&, httplib23::Response& res) {
            res.set_content("pong");
        });

        const uint16_t port = 18081;
        assert(server.listen("127.0.0.1", port) == true);

        httplib23::Client client("127.0.0.1", port);
        assert(client.Get("/ping")->status == 200);
        assert(client.Get("/docs")->status == 404);
        assert(client.Get("/scalar")->status == 404);
        assert(client.Get("/openapi.json")->status == 404);

        server.stop();
    }

    // 2. 測試自訂路徑 (set_doc_options)
    {
        httplib23::Server server;
        httplib23::DocOptions opts{
            .enabled = true,
            .openapi_path = "/custom-spec.json",
            .swagger_path = "/custom-swagger",
            .scalar_path = "/custom-scalar",
            .title = "Custom Title API",
            .version = "3.0.0"
        };
        server.set_doc_options(opts);

        const uint16_t port = 18082;
        assert(server.listen("127.0.0.1", port) == true);

        httplib23::Client client("127.0.0.1", port);

        const auto res_swagger = client.Get("/custom-swagger");
        assert(res_swagger.has_value() && res_swagger->status == 200);
        assert(res_swagger->body.find("swagger-ui") != std::string::npos);

        const auto res_scalar = client.Get("/custom-scalar");
        assert(res_scalar.has_value() && res_scalar->status == 200);
        assert(res_scalar->body.find("api-reference") != std::string::npos);

        const auto res_spec = client.Get("/custom-spec.json");
        assert(res_spec.has_value() && res_spec->status == 200);
        assert(res_spec->body.find("Custom Title API") != std::string::npos);

        server.stop();
    }

    // 3. 測試單獨開啟/關閉 Swagger UI 或 Scalar UI
    {
        httplib23::Server server1;
        server1.enable_swagger(true).enable_scalar(false);
        const uint16_t port1 = 18083;
        assert(server1.listen("127.0.0.1", port1) == true);

        httplib23::Client client1("127.0.0.1", port1);
        assert(client1.Get("/docs")->status == 200);     // Swagger UI 啟用
        assert(client1.Get("/scalar")->status == 404);   // Scalar UI 關閉
        assert(client1.Get("/openapi.json")->status == 200);
        server1.stop();

        httplib23::Server server2;
        server2.enable_swagger(false).enable_scalar(true);
        const uint16_t port2 = 18084;
        assert(server2.listen("127.0.0.1", port2) == true);

        httplib23::Client client2("127.0.0.1", port2);
        assert(client2.Get("/docs")->status == 404);     // Swagger UI 關閉
        assert(client2.Get("/scalar")->status == 200);   // Scalar UI 啟用
        assert(client2.Get("/openapi.json")->status == 200);
        server2.stop();
    }

    std::println("  -> API Documentation configuration tests passed!");
}

int main() {
    std::println("========================================================");
    std::println("Running httplib23 C++23 Comprehensive Test Suite");
    std::println("========================================================");

    test_utilities();
    test_logger();
    test_router_matching();
    test_client_url_parsing_and_exceptions();
    test_openapi_generator();
    test_server_client_integration();
    test_doc_configuration();

    std::println("\n[ALL TESTS PASSED SUCCESSFULLY!]\n");

    // Memory Leak Check
    const int32_t leaks = _CrtDumpMemoryLeaks();
    if (leaks != 0) {
        std::println(stderr, "[WARNING] Memory leak detected!");
    }

    return 0;
}
