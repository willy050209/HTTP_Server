#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <chrono>

#include "httplib23.hpp"

void test_utils() {
    std::cout << "[TEST] Running Utility Tests...\n";
    
    // URL Encoding & Decoding
    std::string text = "Hello World! @C++23";
    std::string encoded = httplib23::detail::url_encode(text);
    std::string decoded = httplib23::detail::url_decode(encoded);
    assert(decoded == text);

    // Query String Parsing
    auto query_map = httplib23::detail::parse_query_string("name=Alice&age=25&city=Taipei%20City");
    assert(query_map["name"] == "Alice");
    assert(query_map["age"] == "25");
    assert(query_map["city"] == "Taipei City");

    std::cout << "  -> Utility tests passed!\n";
}

void test_router() {
    std::cout << "[TEST] Running Router Matching Tests...\n";
    
    httplib23::Router router;
    router.add_route(httplib23::Method::GET, "/api/v1/user/{id}", "Get user by ID").handler = [](const httplib23::Request&, httplib23::Response& res) {
        res.set_json(R"({"status":"ok"})");
    };

    httplib23::HandlerFunc handler;
    std::unordered_map<std::string, std::string> params;
    bool matched = router.match(httplib23::Method::GET, "/api/v1/user/10086", handler, params);
    
    assert(matched == true);
    assert(params["id"] == "10086");

    std::cout << "  -> Router matching tests passed!\n";
}

void test_openapi_gen() {
    std::cout << "[TEST] Running OpenAPI Generator Tests...\n";
    
    httplib23::Router router;
    auto& route = router.add_route(httplib23::Method::GET, "/api/v1/products/{category}", "Get products by category");
    route.meta.tags.push_back("Product");
    route.meta.description = "Retrieves all products within a specific category";
    route.meta.responses.push_back(httplib23::RouteResponseDoc{.status_code = 200, .description = "Success"});

    std::string spec = httplib23::OpenApiGenerator::generate_spec(router.get_routes(), "Test Service", "2.0.0");
    assert(spec.find("openapi") != std::string::npos);
    assert(spec.find("Product") != std::string::npos);
    assert(spec.find("category") != std::string::npos);

    std::cout << "  -> OpenAPI Generator tests passed!\n";
}

void test_server_client_integration() {
    std::cout << "[TEST] Running Server/Client Integration & High-Concurrency Tests...\n";

    httplib23::Server server;

    // Fluent API Route definition
    server.Get("/ping", "Health Check")
        .tag("System")
        .summary("Ping endpoint")
        .response(200, "Pong response")
        .handle([](const httplib23::Request&, httplib23::Response& res) {
            res.set_content("pong", "text/plain");
        });

    server.Get("/users/{id}", "Get user details")
        .tag("User")
        .summary("Fetch single user details")
        .response(200, "User JSON object")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            auto id = req.get_path_param("id").value_or("0");
            res.set_json(std::format(R"({{"id":{},"name":"User_{}"}})", id, id));
        });

    server.Post("/echo", "Echo Request Body")
        .tag("Test")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            res.set_json(std::format(R"({{"echo":"{}"}})", req.body));
        });

    // Start Server on port 18080
    int port = 18080;
    bool started = server.listen("127.0.0.1", port);
    assert(started == true);
    std::cout << "  -> Server started on port " << port << "\n";

    // Give server time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Test Client Requests
    httplib23::Client client("127.0.0.1", port);

    // 1. Test GET /ping
    auto res_ping = client.Get("/ping");
    assert(res_ping.has_value());
    assert(res_ping->status == 200);
    assert(res_ping->body == "pong");

    // 2. Test GET /users/42
    auto res_user = client.Get("/users/42");
    assert(res_user.has_value());
    assert(res_user->status == 200);
    assert(res_user->body.find("\"id\":42") != std::string::npos);

    // 3. Test POST /echo
    auto res_echo = client.Post("/echo", "Hello C++23");
    assert(res_echo.has_value());
    assert(res_echo->status == 200);
    assert(res_echo->body.find("Hello C++23") != std::string::npos);

    // 4. Test OpenAPI Spec Endpoint /openapi.json
    auto res_openapi = client.Get("/openapi.json");
    assert(res_openapi.has_value());
    assert(res_openapi->status == 200);
    assert(res_openapi->body.find("openapi") != std::string::npos);

    // 5. Test Scalar UI Endpoint /docs
    auto res_docs = client.Get("/docs");
    assert(res_docs.has_value());
    assert(res_docs->status == 200);
    assert(res_docs->body.find("Scalar API Documentation") != std::string::npos);

    // 6. High Concurrency Stress Test: 50 Concurrent Threads making 500 total requests
    std::cout << "  -> Starting High Concurrency Stress Test (50 threads x 10 requests = 500 requests)...\n";
    constexpr int NUM_THREADS = 50;
    constexpr int REQS_PER_THREAD = 10;
    std::atomic<int> success_count{0};

    std::vector<std::thread> workers;
    for (int t = 0; t < NUM_THREADS; ++t) {
        workers.emplace_back([port, &success_count]() {
            httplib23::Client thread_client("127.0.0.1", port);
            for (int r = 0; r < REQS_PER_THREAD; ++r) {
                auto res = thread_client.Get("/ping");
                if (res.has_value() && res->status == 200 && res->body == "pong") {
                    success_count++;
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    assert(success_count == NUM_THREADS * REQS_PER_THREAD);
    std::cout << "  -> High Concurrency Stress Test passed! All " << success_count << " requests succeeded!\n";

    // Graceful Stop Server
    server.stop();
    std::cout << "  -> Server stopped gracefully.\n";
}

int main() {
    // Enable CRT Memory Leak Check on exit
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    std::cout << "========================================================\n";
    std::cout << "Running httplib23 C++23 Comprehensive Test Suite\n";
    std::cout << "========================================================\n";

    test_utils();
    test_router();
    test_openapi_gen();
    test_server_client_integration();

    std::cout << "\n[ALL TESTS PASSED SUCCESSFULLY!]\n";
    return 0;
}
