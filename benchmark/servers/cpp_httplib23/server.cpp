#include "../../../include/httplib23.hpp"
#include <string>
#include <format>
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    // Disable logging during performance benchmark for maximum throughput
    httplib23::Logger::instance().set_level(httplib23::LogLevel::OFF);

    httplib23::Server server;
    server.enable_docs(false); // Disable Swagger/Scalar UI endpoints during pure performance benchmark

    // 1. Plaintext Endpoint
    server.Get("/plaintext", "Plaintext benchmark")
        .handle([](const httplib23::Request&, httplib23::Response& res) {
            res.set_content("Hello, World!", "text/plain");
        });

    // 2. JSON Endpoint
    server.Get("/json", "JSON benchmark")
        .handle([](const httplib23::Request&, httplib23::Response& res) {
            res.set_json(R"({"message":"Hello, World!","timestamp":1700000000})");
        });

    // 3. Dynamic Path Param Endpoint
    server.Get("/users/{id}", "User Profile benchmark")
        .handle([](const httplib23::Request& req, httplib23::Response& res) {
            const auto id_opt = req.get_path_param("id");
            const std::string_view id = id_opt.value_or("0");
            res.set_json(std::format(R"({{"id":{},"name":"User_{}","status":"active"}})", id, id));
        });

    std::cout << "[httplib23] Benchmark server listening on http://127.0.0.1:" << port << std::endl;
    if (server.listen("127.0.0.1", port)) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        server.stop();
    }

    return 0;
}
