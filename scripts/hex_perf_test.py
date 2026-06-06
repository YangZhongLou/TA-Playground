#!/usr/bin/env python3
"""
HexPrism Bulk Performance Test

Validates spawn performance for thousands of HexPrism actors.
Measures: spawn time, memory growth, Editor stability.

Usage:
    python scripts/hex_perf_test.py [max_count]

Default test series: 100, 500, 1000, 2000, 5000
"""

import json
import os
import psutil
import socket
import sys
import time

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


def mcp_request(method: str, params: dict, timeout: float = 30.0) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"perf-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload).encode("utf-8")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect((MCP_HOST, MCP_PORT))
        sock.sendall(data)

        chunks = []
        while True:
            try:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                chunks.append(chunk)
                try:
                    json.loads(b"".join(chunks).decode("utf-8", errors="replace"))
                    break
                except json.JSONDecodeError:
                    continue
            except socket.timeout:
                break

        response = b"".join(chunks).decode("utf-8", errors="replace")
        if not response:
            return {}
        try:
            return json.loads(response)
        except json.JSONDecodeError:
            return {"raw": response}


def execute_command(cmd: str) -> dict:
    return mcp_request("execute_editor_command", {"command": cmd})


def get_process_memory_mb() -> float:
    """Get UE Editor process memory in MB."""
    for proc in psutil.process_iter(["pid", "name"]):
        if proc.info["name"] == "UnrealEditor.exe":
            try:
                p = psutil.Process(proc.info["pid"])
                return p.memory_info().rss / (1024 * 1024)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                return 0.0
    return 0.0


def count_hex_actors() -> int:
    resp = mcp_request("get_actor_list", {})
    actors = resp.get("result", {}).get("actors", [])
    return sum(1 for a in actors if "HexPrism" in a.get("class", ""))


def spawn_prism_batch(count: int, radius: float = 100.0, height: float = 200.0) -> dict:
    """Spawn N prisms in a grid pattern to avoid overlap."""
    grid_size = int(count ** 0.5) + 1
    spacing = radius * 2.5

    start_time = time.time()
    spawned = 0
    failed = 0

    for i in range(count):
        x = (i % grid_size - grid_size / 2) * spacing
        y = (i // grid_size - grid_size / 2) * spacing
        z = 0.0

        resp = execute_command(f"hex.SpawnPrism {radius} {height} {x} {y} {z}")
        if resp.get("success"):
            spawned += 1
        else:
            failed += 1

        # Throttle: small delay every 10 spawns to let Editor breathe
        if (i + 1) % 10 == 0:
            time.sleep(0.05)

        # Batch progress report every 100
        if (i + 1) % 100 == 0:
            elapsed = time.time() - start_time
            rate = (i + 1) / elapsed if elapsed > 0 else 0
            print(f"      ... {i + 1}/{count} spawned ({rate:.1f}/s)")

    elapsed = time.time() - start_time
    return {
        "requested": count,
        "spawned": spawned,
        "failed": failed,
        "elapsed_sec": elapsed,
        "rate_per_sec": spawned / elapsed if elapsed > 0 else 0,
    }


def test_batch(count: int) -> dict:
    print(f"\n{'='*60}")
    print(f"  Batch: {count} HexPrisms")
    print(f"{'='*60}")

    # Pre-test cleanup
    print("  [1] Cleanup...")
    execute_command("hex.DestroyAll")
    time.sleep(0.5)

    # Baseline memory
    mem_before = get_process_memory_mb()
    print(f"  [2] Memory before: {mem_before:.1f} MB")

    # Baseline actor count
    count_before = count_hex_actors()
    print(f"  [3] Hex actors before: {count_before}")

    # Spawn batch
    print(f"  [4] Spawning {count} prisms...")
    result = spawn_prism_batch(count)

    # Post-spawn wait for Editor to stabilize
    print("  [5] Waiting for stabilization (3s)...")
    time.sleep(3)

    # Verify count
    count_after = count_hex_actors()
    print(f"  [6] Hex actors after: {count_after}")

    # Post-spawn memory
    mem_after = get_process_memory_mb()
    mem_delta = mem_after - mem_before
    print(f"  [7] Memory after: {mem_after:.1f} MB (delta: +{mem_delta:.1f} MB)")

    # Per-actor memory
    per_actor_mb = mem_delta / result["spawned"] if result["spawned"] > 0 else 0

    return {
        "batch_size": count,
        "spawned": result["spawned"],
        "failed": result["failed"],
        "elapsed_sec": result["elapsed_sec"],
        "rate_per_sec": result["rate_per_sec"],
        "mem_before_mb": mem_before,
        "mem_after_mb": mem_after,
        "mem_delta_mb": mem_delta,
        "per_actor_mb": per_actor_mb,
        "verified_count": count_after,
    }


def run_perf_test():
    max_count = int(sys.argv[1]) if len(sys.argv) > 1 else 5000

    # Determine test series based on max
    if max_count <= 100:
        batches = [100]
    elif max_count <= 500:
        batches = [100, 500]
    elif max_count <= 1000:
        batches = [100, 500, 1000]
    elif max_count <= 2000:
        batches = [100, 500, 1000, 2000]
    else:
        batches = [100, 500, 1000, 2000, 5000]

    # Filter to max
    batches = [b for b in batches if b <= max_count]
    if max_count not in batches:
        batches.append(max_count)

    print("=" * 60)
    print("  HexPrism Bulk Performance Test")
    print("=" * 60)
    print(f"  Test series: {batches}")
    print(f"  MCP: {MCP_HOST}:{MCP_PORT}")

    results = []
    for count in batches:
        try:
            result = test_batch(count)
            results.append(result)
        except Exception as e:
            print(f"  [ERROR] Batch {count} failed: {e}")
            results.append({"batch_size": count, "error": str(e)})
            break

    # Summary table
    print("\n" + "=" * 60)
    print("  Performance Summary")
    print("=" * 60)
    print(f"  {'Count':>8} | {'Time(s)':>8} | {'Rate(/s)':>10} | {'Mem(MB)':>10} | {'Per-Actor':>10}")
    print("  " + "-" * 58)
    for r in results:
        if "error" in r:
            print(f"  {r['batch_size']:>8} | FAILED: {r['error'][:40]}")
        else:
            print(
                f"  {r['batch_size']:>8} | "
                f"{r['elapsed_sec']:>8.2f} | "
                f"{r['rate_per_sec']:>10.1f} | "
                f"{r['mem_delta_mb']:>10.1f} | "
                f"{r['per_actor_mb']:>10.2f}"
            )

    # Final cleanup
    print("\n  [Final] Cleaning up...")
    execute_command("hex.DestroyAll")
    print("  Done.")

    return results


if __name__ == "__main__":
    run_perf_test()
