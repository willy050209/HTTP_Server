import subprocess
import time
import json
import os
import sys
import urllib.request
import psutil

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER_EXE = os.path.join(BASE_DIR, "runner", "bench_runner_v2.exe")
RESULTS_DIR = os.path.join(BASE_DIR, "results")
os.makedirs(RESULTS_DIR, exist_ok=True)

SERVERS = [
    {
        "name": "httplib23 (C++23 IOCP)",
        "id": "cpp_httplib23",
        "port": 8181,
        "cmd": [os.path.join(BASE_DIR, "servers", "cpp_httplib23", "server_cpp.exe"), "8181"],
        "cwd": os.path.join(BASE_DIR, "servers", "cpp_httplib23")
    },
    {
        "name": "ASP.NET Core (C# .NET 10 Kestrel)",
        "id": "dotnet_aspnet",
        "port": 8182,
        "cmd": [
            "dotnet",
            os.path.join(BASE_DIR, "servers", "dotnet_aspnet", "bin", "Release", "net10.0", "AspNetServer.dll")
            if os.path.exists(os.path.join(BASE_DIR, "servers", "dotnet_aspnet", "bin", "Release", "net10.0", "AspNetServer.dll"))
            else os.path.join(BASE_DIR, "servers", "dotnet_aspnet", "bin", "Release", "net9.0", "AspNetServer.dll"),
            "--urls", "http://127.0.0.1:8182"
        ],
        "cwd": os.path.join(BASE_DIR, "servers", "dotnet_aspnet")
    },
    {
        "name": "FastAPI (Python 3.12 Uvicorn)",
        "id": "python_fastapi",
        "port": 8183,
        "cmd": [
            os.path.join(BASE_DIR, "venv", "Scripts", "python.exe"),
            "-m", "uvicorn", "main:app",
            "--host", "127.0.0.1",
            "--port", "8183",
            "--log-level", "warning",
            "--no-access-log"
        ],
        "cwd": os.path.join(BASE_DIR, "servers", "python_fastapi")
    }
]

def kill_port_owners(port):
    for proc in psutil.process_iter(['pid', 'name']):
        try:
            for conn in proc.net_connections(kind='inet'):
                if conn.laddr.port == port:
                    proc.kill()
        except Exception:
            pass

SCENARIOS = [
    {"name": "Plaintext (/plaintext)", "path": "/plaintext"},
    {"name": "JSON Serialization (/json)", "path": "/json"},
    {"name": "Dynamic Route (/users/123)", "path": "/users/123"}
]

CONCURRENCIES = [10, 25, 50]
DURATION_SEC = 5

def wait_for_server(port, timeout=15):
    start = time.time()
    url = f"http://127.0.0.1:{port}/plaintext"
    while time.time() - start < timeout:
        try:
            with urllib.request.urlopen(url, timeout=1) as resp:
                if resp.status == 200:
                    return True
        except Exception:
            pass
        time.sleep(0.3)
    return False

def get_process_stats(proc):
    try:
        p = psutil.Process(proc.pid)
        mem_mb = p.memory_info().rss / (1024 * 1024)
        cpu_pct = p.cpu_percent(interval=None)
        # Also sum children processes if any
        for child in p.children(recursive=True):
            mem_mb += child.memory_info().rss / (1024 * 1024)
        return mem_mb, cpu_pct
    except Exception:
        return 0.0, 0.0

def run_bench(port, path, threads, duration):
    cmd = [
        RUNNER_EXE,
        "--host", "127.0.0.1",
        "--port", str(port),
        "--path", path,
        "--threads", str(threads),
        "--duration", str(duration)
    ]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f"[ERROR] Runner failed: {res.stderr}")
        return None
    try:
        return json.loads(res.stdout)
    except json.JSONDecodeError:
        print(f"[ERROR] Invalid JSON from runner:\n{res.stdout}")
        return None

def main():
    print("================================================================================")
    print("      Starting Comprehensive HTTP Server Performance Benchmark Suite           ")
    print("================================================================================")
    print(f"Scenarios: {[s['name'] for s in SCENARIOS]}")
    print(f"Concurrencies: {CONCURRENCIES} threads")
    print(f"Duration per test: {DURATION_SEC} seconds\n")

    all_data = {}

    for server in SERVERS:
        sname = server["name"]
        sid = server["id"]
        sport = server["port"]
        print(f"\n================================================================================")
        print(f" [TESTING] Starting Server: {sname} on port {sport}")
        print(f"================================================================================")

        kill_port_owners(sport)
        time.sleep(0.5)

        proc = subprocess.Popen(server["cmd"], cwd=server["cwd"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if not wait_for_server(sport):
            print(f"[FAIL] Could not connect to {sname} on port {sport}!")
            proc.kill()
            continue

        print(f" -> {sname} is healthy and responsive.")

        # Warm up
        print(" -> Warming up server (2 seconds)...")
        run_bench(sport, "/plaintext", 10, 2)

        idle_mem, _ = get_process_stats(proc)
        print(f" -> Idle Memory RSS: {idle_mem:.2f} MB")

        server_results = {
            "server_name": sname,
            "idle_mem_mb": idle_mem,
            "peak_mem_mb": idle_mem,
            "scenarios": {}
        }

        peak_mem = idle_mem

        for scenario in SCENARIOS:
            sc_name = scenario["name"]
            sc_path = scenario["path"]
            print(f"\n --- Scenario: {sc_name} ---")
            server_results["scenarios"][sc_name] = {}

            for c in CONCURRENCIES:
                print(f"   Executing Concurrency {c:3d} threads x {DURATION_SEC}s ... ", end="", flush=True)
                data = run_bench(sport, sc_path, c, DURATION_SEC)
                current_mem, _ = get_process_stats(proc)
                if current_mem > peak_mem:
                    peak_mem = current_mem

                if data:
                    rps = data.get("rps", 0.0)
                    p50 = data.get("latency_ms", {}).get("p50", 0.0)
                    p99 = data.get("latency_ms", {}).get("p99", 0.0)
                    print(f"RPS: {rps:10.2f} | Latency p50: {p50:6.3f} ms | p99: {p99:6.3f} ms | RSS: {current_mem:5.1f} MB")
                    server_results["scenarios"][sc_name][str(c)] = data
                else:
                    print("FAILED")

        server_results["peak_mem_mb"] = peak_mem
        all_data[sid] = server_results

        # Gracefully stop server
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()

        print(f" -> Server {sname} stopped. Peak Memory RSS: {peak_mem:.2f} MB\n")
        time.sleep(1)

    # Save to JSON
    output_file = os.path.join(RESULTS_DIR, "benchmark_data.json")
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(all_data, f, indent=2, ensure_ascii=False)

    print("\n================================================================================")
    print("                     BENCHMARK SUMMARY RESULTS TABLE                            ")
    print("================================================================================")

    # Print Summary Tables for 50 Concurrency
    print(f"\n[Summary: 50 Concurrency - High Throughput Comparison]")
    print(f"{'Framework':<28} | {'Scenario':<26} | {'RPS':>12} | {'p50 (ms)':>10} | {'p99 (ms)':>10} | {'Memory':>10}")
    print("-" * 105)
    for sid, sinfo in all_data.items():
        sname = sinfo["server_name"]
        for sc_name, sc_data in sinfo["scenarios"].items():
            d50 = sc_data.get("50", {})
            rps = d50.get("rps", 0)
            p50 = d50.get("latency_ms", {}).get("p50", 0)
            p99 = d50.get("latency_ms", {}).get("p99", 0)
            mem = f"{sinfo['peak_mem_mb']:.1f} MB"
            print(f"{sname:<28} | {sc_name:<26} | {rps:>12.2f} | {p50:>10.3f} | {p99:>10.3f} | {mem:>10}")

    print(f"\nRaw results saved to: {output_file}")

if __name__ == "__main__":
    main()
