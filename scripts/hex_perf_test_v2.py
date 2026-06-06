#!/usr/bin/env python3
"""
HexPrism Bulk Performance Test v2

Single MCP connection, cumulative spawn to avoid DestroyAll overhead.
"""
import json
import psutil
import socket
import time
import sys

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


class MCPClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(60)
        self.sock.connect((MCP_HOST, MCP_PORT))
        self.req_id = 0

    def send(self, method, params, timeout=60):
        self.req_id += 1
        payload = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params,
            "id": f"pv2-{self.req_id}",
        }
        self.sock.settimeout(timeout)
        data = json.dumps(payload).encode()
        self.sock.sendall(data)

        chunks = []
        while True:
            try:
                chunk = self.sock.recv(65536)
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

        resp = b"".join(chunks).decode("utf-8", errors="replace")
        return json.loads(resp) if resp else {}

    def close(self):
        self.sock.close()


def get_mem():
    for p in psutil.process_iter(["pid", "name"]):
        if p.info["name"] == "UnrealEditor.exe":
            return psutil.Process(p.info["pid"]).memory_info().rss / (1024 * 1024)
    return 0.0


def count_hex(mcp):
    r = mcp.send("get_actor_list", {})
    actors = r.get("result", {}).get("actors", [])
    return sum(1 for a in actors if "HexPrism" in a.get("class", ""))


def spawn_range(mcp, start_idx, end_idx, spacing=200):
    """Spawn prisms from start_idx to end_idx (exclusive)."""
    spawned = 0
    failed = 0

    for i in range(start_idx, end_idx):
        x = (i % 50 - 25) * spacing
        y = (i // 50 - 25) * spacing
        r = mcp.send(
            "execute_editor_command",
            {"command": f"hex.SpawnPrism 80 160 {x} {y} 0"},
            timeout=15,
        )
        if r.get("success"):
            spawned += 1
        else:
            failed += 1

        # Throttle every 5 spawns
        if (i + 1) % 5 == 0:
            time.sleep(0.15)

        # Progress every 100
        if (i + 1) % 100 == 0:
            print(f"      ... {i + 1} total spawned")

    return {"spawned": spawned, "failed": failed}


def main():
    # Target series: cumulative
    targets = [100, 500, 1000, 2000, 5000]
    if len(sys.argv) > 1:
        max_target = int(sys.argv[1])
        targets = [t for t in targets if t <= max_target]
        if max_target not in targets:
            targets.append(max_target)

    print("=" * 60)
    print("  HexPrism Bulk Performance Test v2")
    print("  Cumulative spawn (single connection)")
    print("=" * 60)
    print(f"  Targets: {targets}")

    mcp = MCPClient()

    # Verify
    r = mcp.send("check_unreal_connection", {})
    if not r.get("success"):
        print("[FAIL] MCP not responding")
        mcp.close()
        return 1

    # Initial cleanup
    print("\n[INIT] Cleanup...")
    mcp.send("execute_editor_command", {"command": "hex.DestroyAll"})
    time.sleep(3)
    initial_count = count_hex(mcp)
    print(f"       Hex actors after cleanup: {initial_count}")

    mem_baseline = get_mem()
    print(f"       Memory baseline: {mem_baseline:.1f} MB")

    results = []
    current_count = initial_count

    for target in targets:
        to_spawn = target - current_count
        if to_spawn <= 0:
            continue

        print(f"\n{'='*60}")
        print(f"  BATCH: {to_spawn} more prisms (total will be {target})")
        print(f"{'='*60}")

        mem_before = get_mem()
        start = time.time()

        result = spawn_range(mcp, current_count, target)

        elapsed = time.time() - start
        mem_after = get_mem()
        mem_delta = mem_after - mem_before

        # Verify
        time.sleep(2)
        verified = count_hex(mcp)

        print(f"  Time: {elapsed:.1f}s")
        print(f"  Rate: {result['spawned'] / elapsed:.1f}/s")
        print(f"  Memory delta: +{mem_delta:.1f} MB")
        print(f"  Verified count: {verified}")
        if result["spawned"] > 0:
            print(f"  Per-actor memory: {mem_delta / result['spawned']:.2f} MB")

        results.append({
            "target": target,
            "spawned": result["spawned"],
            "failed": result["failed"],
            "elapsed": elapsed,
            "rate": result["spawned"] / elapsed if elapsed > 0 else 0,
            "mem_delta": mem_delta,
            "per_actor_mb": mem_delta / result["spawned"] if result["spawned"] > 0 else 0,
            "verified": verified,
        })

        current_count = verified

        # Safety: if spawn rate drops below 1/s or memory spikes, stop
        if result["spawned"] / elapsed < 1.0 if elapsed > 0 else True:
            print("\n  [WARN] Spawn rate dropped below 1/s, stopping for safety")
            break

        # Rest between batches
        if target < targets[-1]:
            print(f"  Resting 5s before next batch...")
            time.sleep(5)

    # Summary
    print("\n" + "=" * 60)
    print("  PERFORMANCE SUMMARY")
    print("=" * 60)
    print(f"  {'Target':>8} | {'Time(s)':>8} | {'Rate/s':>8} | {'Mem(MB)':>10} | {'Per-Actor':>10}")
    print("  " + "-" * 54)
    for r in results:
        print(
            f"  {r['target']:>8} | "
            f"{r['elapsed']:>8.1f} | "
            f"{r['rate']:>8.1f} | "
            f"{r['mem_delta']:>10.1f} | "
            f"{r['per_actor_mb']:>10.2f}"
        )

    # Final cleanup
    print("\n[FINAL] Cleanup...")
    mcp.send("execute_editor_command", {"command": "hex.DestroyAll"})
    mcp.close()
    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
