#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <format>
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socket_t = SOCKET;
    inline constexpr socket_t invalid_sock = INVALID_SOCKET;
    inline void close_sock(socket_t s) { ::closesocket(s); }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    using socket_t = int;
    inline constexpr socket_t invalid_sock = -1;
    inline void close_sock(socket_t s) { ::close(s); }
#endif

struct ThreadResult {
    std::vector<double> latencies_ms;
    uint64_t success_count = 0;
    uint64_t fail_count = 0;
    uint64_t bytes_transferred = 0;
};

void run_worker(const std::string host, const int port, const std::string path,
                const std::chrono::steady_clock::time_point end_time,
                ThreadResult& result) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    const std::string request = std::format(
        "GET {} HTTP/1.1\r\n"
        "Host: {}:{}\r\n"
        "User-Agent: BenchmarkRunner/2.0\r\n"
        "Accept: */*\r\n"
        "Connection: keep-alive\r\n\r\n",
        path, host, port
    );

    result.latencies_ms.reserve(100000);
    char buf[4096];
    socket_t sock = invalid_sock;
    std::string rx_buffer;
    rx_buffer.reserve(8192);

    auto connect_new_socket = [&]() -> bool {
        if (sock != invalid_sock) {
            close_sock(sock);
            sock = invalid_sock;
        }
        sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == invalid_sock) return false;

        int flag = 1;
        ::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&flag), sizeof(flag));

        linger sl{ 1, 0 };
        ::setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&sl), sizeof(sl));

#if defined(_WIN32) || defined(_WIN64)
        DWORD timeout_ms = 2000;
        ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
        ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
        struct timeval tv{ 2, 0 };
        ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif

        if (::connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
            close_sock(sock);
            sock = invalid_sock;
            return false;
        }
        rx_buffer.clear();
        return true;
    };

    while (std::chrono::steady_clock::now() < end_time) {
        if (sock == invalid_sock) {
            if (!connect_new_socket()) {
                result.fail_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                continue;
            }
        }

        const auto t0 = std::chrono::high_resolution_clock::now();

        // Send HTTP request
        int sent = ::send(sock, request.c_str(), static_cast<int>(request.size()), 0);
        if (sent <= 0) {
            connect_new_socket();
            result.fail_count++;
            continue;
        }

        // Receive response
        bool completed = false;
        size_t content_len = 0;
        size_t header_len = 0;
        bool header_parsed = false;
        bool keep_alive = true;

        while (true) {
            if (!header_parsed) {
                size_t pos = rx_buffer.find("\r\n\r\n");
                if (pos != std::string::npos) {
                    header_len = pos + 4;
                    header_parsed = true;
                    std::string_view headers(rx_buffer.data(), pos);
                    size_t cl_pos = headers.find("Content-Length: ");
                    if (cl_pos == std::string_view::npos) cl_pos = headers.find("content-length: ");
                    if (cl_pos != std::string_view::npos) {
                        size_t end_line = headers.find("\r\n", cl_pos);
                        if (end_line != std::string_view::npos) {
                            try {
                                content_len = std::stoul(std::string(headers.substr(cl_pos + 16, end_line - (cl_pos + 16))));
                            } catch (...) {
                                content_len = 0;
                            }
                        }
                    }
                    if (headers.find("Connection: close") != std::string_view::npos ||
                        headers.find("connection: close") != std::string_view::npos) {
                        keep_alive = false;
                    }
                }
            }

            if (header_parsed && rx_buffer.size() >= header_len + content_len) {
                completed = true;
                break;
            }

            int bytes = ::recv(sock, buf, sizeof(buf), 0);
            if (bytes <= 0) {
                break;
            }
            rx_buffer.append(buf, bytes);
        }

        const auto t1 = std::chrono::high_resolution_clock::now();
        const double duration_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (completed && (rx_buffer.find("200 OK") != std::string::npos || rx_buffer.find("HTTP/1.1 200") != std::string::npos)) {
            result.success_count++;
            const size_t total_req_size = header_len + content_len;
            result.bytes_transferred += total_req_size;
            result.latencies_ms.push_back(duration_ms);
            rx_buffer.erase(0, total_req_size);

            if (!keep_alive) {
                close_sock(sock);
                sock = invalid_sock;
            }
        } else {
            result.fail_count++;
            close_sock(sock);
            sock = invalid_sock;
        }
    }

    if (sock != invalid_sock) {
        close_sock(sock);
    }
}

int main(int argc, char* argv[]) {
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    std::string host = "127.0.0.1";
    int port = 8080;
    std::string path = "/plaintext";
    int concurrency = 50;
    int duration_sec = 10;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--path" && i + 1 < argc) path = argv[++i];
        else if (arg == "--threads" && i + 1 < argc) concurrency = std::stoi(argv[++i]);
        else if (arg == "--duration" && i + 1 < argc) duration_sec = std::stoi(argv[++i]);
    }

    std::vector<ThreadResult> results(concurrency);
    std::vector<std::thread> threads;
    threads.reserve(concurrency);

    const auto start_time = std::chrono::steady_clock::now();
    const auto end_time = start_time + std::chrono::seconds(duration_sec);

    for (int i = 0; i < concurrency; ++i) {
        threads.emplace_back(run_worker, host, port, path, end_time, std::ref(results[i]));
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    const auto actual_end_time = std::chrono::steady_clock::now();
    const double elapsed_seconds = std::chrono::duration<double>(actual_end_time - start_time).count();

    // Aggregate statistics
    std::vector<double> all_latencies;
    uint64_t total_success = 0;
    uint64_t total_fail = 0;
    uint64_t total_bytes = 0;

    for (const auto& r : results) {
        total_success += r.success_count;
        total_fail += r.fail_count;
        total_bytes += r.bytes_transferred;
        all_latencies.insert(all_latencies.end(), r.latencies_ms.begin(), r.latencies_ms.end());
    }

    std::sort(all_latencies.begin(), all_latencies.end());

    double avg_lat = 0.0;
    if (!all_latencies.empty()) {
        avg_lat = std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0) / all_latencies.size();
    }

    auto get_pct = [&](double pct) -> double {
        if (all_latencies.empty()) return 0.0;
        size_t idx = static_cast<size_t>(pct * all_latencies.size() / 100.0);
        if (idx >= all_latencies.size()) idx = all_latencies.size() - 1;
        return all_latencies[idx];
    };

    const double min_lat = all_latencies.empty() ? 0.0 : all_latencies.front();
    const double p50_lat = get_pct(50.0);
    const double p90_lat = get_pct(90.0);
    const double p99_lat = get_pct(99.0);
    const double max_lat = all_latencies.empty() ? 0.0 : all_latencies.back();
    const double rps = total_success / elapsed_seconds;
    const double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / elapsed_seconds;

    // Output JSON format
    std::cout << std::format(
        R"({{
  "host": "{}",
  "port": {},
  "path": "{}",
  "concurrency": {},
  "duration_sec": {:.2f},
  "total_requests": {},
  "successful_requests": {},
  "failed_requests": {},
  "rps": {:.2f},
  "mb_per_sec": {:.2f},
  "latency_ms": {{
    "min": {:.3f},
    "avg": {:.3f},
    "p50": {:.3f},
    "p90": {:.3f},
    "p99": {:.3f},
    "max": {:.3f}
  }}
}})",
        host, port, path, concurrency, elapsed_seconds,
        total_success + total_fail, total_success, total_fail,
        rps, mb_per_sec,
        min_lat, avg_lat, p50_lat, p90_lat, p99_lat, max_lat
    ) << std::endl;

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
    return 0;
}
