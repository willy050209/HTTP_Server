// filepath: src/test_runner.cpp
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <print>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdint>

#include "../include/httplib23.hpp"

/// <summary>
/// 測試 Utility 純函數（URL 解碼、編碼與 Query 解析）。
/// </summary>
void test_utils() {
    std::println("[TEST] Running Utility Tests...");
    
    const std::string text = "Hello World! @C++23";
    const std::string encoded = httplib23::detail::url_encode(text);
    const std::string decoded = httplib23::detail::url_decode(encoded);
    assert(decoded == text);

    const auto query_map = httplib23::detail::parse_query_string("name=Alice&age=25&city=Taipei%20City");
    assert(query_map.at("name") == "Alice");
    assert(query_map.at("age") == "25");
    assert(query_map.at("city") == "Taipei City");

    std::println("  -> Utility tests passed!");
}

/// <summary>
/// 測試 Router 路徑匹配與動態參數解析。
/// </summary>
void test_router() {
    std::println("[TEST] Running Router Matching Tests...");
    
    httplib23::Router router;
    router.add_route(httplib23::Method::GET, "/api/v1/user/{id}", "Get user by ID").handler = [](const httplib23::Request&, httplib23::Response& res) noexcept {
        res.set_json(R"({"status":"ok"})");
    };

    httplib23::HandlerFunc handler;
    std::unordered_map<std::string, std::string> params;
    const bool matched = router.match(httplib23::Method::GET, "/api/v1/user/10086", handler, params);
    
    assert(matched == true);
    assert(params.at("id") == "10086");

    std::println("  -> Router matching tests passed!");
}

/// <summary>
/// 測試 Fail-Fast API 與 CRLF 防護例外擲出。
/// </summary>
void test_security_exceptions() {
    std::println("[TEST] Running Security & Exception Tests...");

    httplib23::Router router;
    
    // 1. 驗證非法 Pattern 不以 '/' 開頭會擲出 std::invalid_argument
    bool invalid_path_caught = false;
    try {
        router.add_route(httplib23::Method::GET, "invalid_path");
    } catch (const std::invalid_argument&) {
        invalid_path_caught = true;
    }
    assert(invalid_path_caught == true);

    // 2. 驗證 CRLF 注入防護
    httplib23::Response res;
    bool crlf_caught = false;
    try {
        res.set_header("X-Custom-Header", "admin=true\r\nSet-Cookie: session=hacked");
    } catch (const std::invalid_argument&) {
        crlf_caught = true;
    }
    assert(crlf_caught == true);

    std::println("  -> Security & Exception tests passed!");
}

/// <summary>
/// 測試 OpenAPI Spec JSON 生成器。
/// </summary>
void test_openapi_gen() {
    std::println("[TEST] Running OpenAPI Generator Tests...");
    
    httplib23::Router router;
    auto& route = router.add_route(httplib23::Method::GET, "/api/v1/products/{category}", "Get products by category");
    route.meta.tags.push_back("Product");
    route.meta.description = "Retrieves all products within a specific category";
    route.meta.responses.push_back(httplib23::RouteResponseDoc{.status_code = 200, .description = "Success"});

    const std::string spec = httplib23::OpenApiGenerator::generate_spec(router.get_routes(), "Test Service", "2.0.0");
    assert(spec.find("openapi") != std::string::npos);
    assert(spec.find("Product") != std::string::npos);
    assert(spec.find("category") != std::string::npos);

    std::println("  -> OpenAPI Generator tests passed!");
}

/// <summary>
/// 測試 Server 與 Client 整合及高併發壓測與 Slowloris 防護。
/// </summary>
void test_server_client_integration() {
    std::println("[TEST] Running Server/Client Integration & High-Concurrency Tests...");

    httplib23::Server server;

    server.Get("/ping", "Health Check")
        .tag("System")
        .summary("Ping endpoint")
        .response(200, "Pong response")
        .handle([](const httplib23::Request&, httplib23::Response& res) noexcept {
            res.set_content("pong", "text/plain");
        });

    server.Get("/users/{id}", "Get user details")
        .tag("User")
        .summary("Fetch single user details")
        .response(200, "User JSON object")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            const auto id = req.get_path_param("id").value_or("0");
            res.set_json(std::format(R"({{"id":{},"name":"User_{}"}})", id, id));
        });

    server.Post("/echo", "Echo Request Body")
        .tag("Test")
        .handle([](const httplib23::Request& req, httplib23::Response& res) noexcept {
            res.set_json(std::format(R"({{"echo":"{}"}})", req.body));
        });

    const uint16_t port = 18080;
    const bool started = server.listen("127.0.0.1", port);
    assert(started == true);
    std::println("  -> Server started on port {}", port);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    httplib23::Client client("127.0.0.1", port);

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

    // 4. Test OpenAPI Spec Endpoint /openapi.json
    const auto res_openapi = client.Get("/openapi.json");
    assert(res_openapi.has_value());
    assert(res_openapi->status == 200);
    assert(res_openapi->body.find("openapi") != std::string::npos);

    // 5. Test Scalar UI Endpoint /docs
    const auto res_docs = client.Get("/docs");
    assert(res_docs.has_value());
    assert(res_docs->status == 200);
    assert(res_docs->body.find("Scalar API Documentation") != std::string::npos);

    // 6. High Concurrency Stress Test: 50 Threads x 10 requests = 500 requests
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
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    std::println("  [STRESS RESULT] success_count: {} / {}", success_count.load(), NUM_THREADS * REQS_PER_THREAD);
    std::fflush(stdout);
    assert(success_count == NUM_THREADS * REQS_PER_THREAD);
    std::println("  -> High Concurrency Stress Test passed! All {} requests succeeded!", success_count.load());

    server.stop();
    std::println("  -> Server stopped gracefully.");
}

int main() {
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    std::println("========================================================");
    std::println("Running httplib23 C++23 Comprehensive Test Suite");
    std::println("========================================================");

    test_utils();
    test_router();
    test_security_exceptions();
    test_openapi_gen();
    test_server_client_integration();

    std::println("\n[ALL TESTS PASSED SUCCESSFULLY!]");
    return 0;
}
