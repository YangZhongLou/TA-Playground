#!/usr/bin/env python3
"""
MCP Command Health Check — Tests every UnrealMCP command and reports status.

Usage:
    1. Start UE Editor with TA-Playground.uproject
    2. python scripts/test_mcp.py

Requirements: UE Editor running with UnrealMCP plugin loaded (port 13377)
"""

import json
import socket
import sys
import time
from dataclasses import dataclass
from typing import Optional

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


@dataclass
class TestResult:
    name: str
    success: bool
    error: str = ""
    response_time_ms: float = 0.0


def mcp_request(method: str, params: dict, timeout: float = 10.0) -> tuple[bool, dict, float]:
    """Send MCP request, return (success, response_dict, time_ms)."""
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"test-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    start = time.time()

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(timeout)
            sock.connect((MCP_HOST, MCP_PORT))
            sock.sendall(data)
            response = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if len(chunk) < 4096:
                    break
            elapsed = (time.time() - start) * 1000
            resp = json.loads(response.decode("utf-8", errors="replace")) if response else {}
            return resp.get("success", False), resp, elapsed
    except Exception as e:
        return False, {"error": str(e)}, (time.time() - start) * 1000


def test_command(name: str, method: str, params: dict) -> TestResult:
    success, resp, elapsed = mcp_request(method, params)
    error = resp.get("error", "") if not success else ""
    if "success" not in resp and "error" not in resp:
        error = "No response or connection refused"
    return TestResult(name, success, error, elapsed)


def run_tests():
    results: list[TestResult] = []

    # --- Connection test ---
    print("Testing MCP connection...")
    r = test_command("get_editor_info", "get_editor_info", {})
    results.append(r)
    if not r.success:
        print(f"  FAIL: Cannot connect to MCP server at {MCP_HOST}:{MCP_PORT}")
        print(f"  Error: {r.error}")
        print("\nMake sure UE Editor is running with TA-Playground.uproject loaded.")
        sys.exit(1)
    print(f"  OK: Connected ({r.response_time_ms:.1f}ms)")

    # --- Actor commands ---
    print("\n--- Actor Commands ---")
    tests = [
        ("spawn_actor", "spawn_actor", {"className": "AHexPrism", "name": "TestHexPrism", "location": [0, 0, 500]}),
        ("get_actor_list", "get_actor_list", {}),
        ("get_actor_property", "get_actor_property", {"actorName": "TestHexPrism", "propertyName": "Radius"}),
        ("set_actor_property", "set_actor_property", {"actorName": "TestHexPrism", "propertyName": "Radius", "value": {"FloatValue": 150.0}}),
        ("find_actors_by_class", "find_actors_by_class", {"className": "HexPrism", "exactMatch": False}),
        ("set_actor_transform", "set_actor_transform", {"name": "TestHexPrism", "location": [100, 100, 500]}),
        ("add_actor_tag", "add_actor_tag", {"actorName": "TestHexPrism", "tag": "test-tag"}),
        ("duplicate_actor", "duplicate_actor", {"name": "TestHexPrism", "newName": "TestHexPrism2"}),
        ("destroy_actor", "destroy_actor", {"name": "TestHexPrism"}),
        ("destroy_actor", "destroy_actor", {"name": "TestHexPrism2"}),
    ]
    for name, method, params in tests:
        r = test_command(name, method, params)
        results.append(r)
        status = "OK" if r.success else "FAIL"
        print(f"  [{status}] {name:25s} ({r.response_time_ms:6.1f}ms) {r.error if r.error else ''}")
        time.sleep(0.2)

    # --- Editor commands ---
    print("\n--- Editor Commands ---")
    tests = [
        ("get_current_level", "get_current_level", {}),
        ("save_current_level", "save_current_level", {}),
        ("get_viewport_camera", "get_viewport_camera", {}),
        ("get_logs", "get_logs", {"count": 10}),
        ("execute_editor_command", "execute_editor_command", {"command": "hex.DestroyAll"}),
        ("run_console_command", "run_console_command", {"command": "stat fps"}),
    ]
    for name, method, params in tests:
        r = test_command(name, method, params)
        results.append(r)
        status = "OK" if r.success else "FAIL"
        print(f"  [{status}] {name:25s} ({r.response_time_ms:6.1f}ms) {r.error if r.error else ''}")
        time.sleep(0.2)

    # --- Asset commands ---
    print("\n--- Asset Commands ---")
    tests = [
        ("get_asset_list", "get_asset_list", {"path": "/Game", "type": ""}),
        ("get_asset_info", "get_asset_info", {"path": "/Engine/BasicShapes/BasicShapeMaterial"}),
    ]
    for name, method, params in tests:
        r = test_command(name, method, params)
        results.append(r)
        status = "OK" if r.success else "FAIL"
        print(f"  [{status}] {name:25s} ({r.response_time_ms:6.1f}ms) {r.error if r.error else ''}")

    # --- Risky commands (may fail) ---
    print("\n--- Risky Commands (may fail, known issues) ---")
    tests = [
        ("open_level", "open_level", {"path": "/Game/Maps/TestMap"}),
        ("take_screenshot", "take_screenshot", {"path": "D:/Temp/mcp_test.png", "width": 1920, "height": 1080}),
    ]
    for name, method, params in tests:
        r = test_command(name, method, params)
        results.append(r)
        status = "OK" if r.success else "WARN"
        print(f"  [{status}] {name:25s} ({r.response_time_ms:6.1f}ms) {r.error if r.error else ''}")

    # --- Hex commands ---
    print("\n--- Hex Commands ---")
    tests = [
        ("hex.SpawnPrism", "execute_editor_command", {"command": "hex.SpawnPrism 100 200 0 0 300"}),
        ("hex.SpawnGrid", "execute_editor_command", {"command": "hex.SpawnGrid 50 3 5"}),
        ("hex.SpawnTerrain", "execute_editor_command", {"command": "hex.SpawnTerrain 80 3 100 0.1"}),
        ("hex.DestroyAll", "execute_editor_command", {"command": "hex.DestroyAll"}),
    ]
    for name, method, params in tests:
        r = test_command(name, method, params)
        results.append(r)
        status = "OK" if r.success else "FAIL"
        print(f"  [{status}] {name:25s} ({r.response_time_ms:6.1f}ms) {r.error if r.error else ''}")
        time.sleep(0.3)

    # --- Summary ---
    passed = sum(1 for r in results if r.success)
    failed = sum(1 for r in results if not r.success)
    total = len(results)

    print(f"\n{'='*50}")
    print(f"Results: {passed}/{total} passed, {failed} failed/warn")
    print(f"{'='*50}")

    if failed > 0:
        print("\nFailed commands:")
        for r in results:
            if not r.success:
                print(f"  - {r.name}: {r.error}")

    return failed == 0


if __name__ == "__main__":
    ok = run_tests()
    sys.exit(0 if ok else 1)
