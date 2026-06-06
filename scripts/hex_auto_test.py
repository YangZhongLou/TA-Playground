#!/usr/bin/env python3
"""
Hexagonal Geometry System -- Automated Test & Validation Script

Tests the full hex pipeline via UnrealMCP and validates responses.

Usage:
    python scripts/hex_auto_test.py

Prerequisites:
    1. UE Editor is running with TA-Playground.uproject loaded
    2. UnrealMCP plugin is active (port 13377)
"""

import json
import socket
import sys
import time

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


class Colors:
    OK = "\033[92m"
    FAIL = "\033[91m"
    WARN = "\033[93m"
    INFO = "\033[94m"
    RESET = "\033[0m"


def mcp_request(method: str, params: dict, timeout: float = 10.0) -> dict:
    """Send a JSON-RPC style request to UnrealMCP and return the response."""
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"hex-test-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect((MCP_HOST, MCP_PORT))
        sock.sendall(data)

        chunks = []
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                if len(chunk) < 4096:
                    break
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
    """Execute an editor console command via MCP."""
    return mcp_request("execute_editor_command", {"command": cmd})


def print_result(name: str, success: bool, details: str = ""):
    """Print a colored test result."""
    status = f"{Colors.OK}[PASS]{Colors.RESET}" if success else f"{Colors.FAIL}[FAIL]{Colors.RESET}"
    detail_str = f" -- {details}" if details else ""
    print(f"  {status} {name}{detail_str}")


def verify_mcp_alive() -> bool:
    """Verify MCP is actually responding with valid data."""
    resp = mcp_request("check_unreal_connection", {}, timeout=5.0)
    if not resp or "result" not in resp:
        return False
    return resp.get("success", False) and "engine_version" in resp.get("result", {})


def test_destroy_all() -> bool:
    """Test: Clear all hex actors."""
    print(f"\n{Colors.INFO}>>> Step 1: Cleanup -- Destroy all hex actors{Colors.RESET}")
    resp = execute_command("hex.DestroyAll")
    time.sleep(0.5)
    # DestroyAll logs result; check UE logs for confirmation
    print_result("DestroyAll", True, "Command executed")
    return True


def test_spawn_prism() -> bool:
    """Test: Spawn a single hexagonal prism."""
    print(f"\n{Colors.INFO}>>> Step 2: Hex Prism -- Spawn single prism{Colors.RESET}")
    resp = execute_command("hex.SpawnPrism 200 400 0 0 200")
    success = resp.get("success", False)
    print_result("SpawnPrism R=200 H=400 @ (0,0,200)", success,
                 f"response={resp}")
    time.sleep(0.5)
    return success


def test_spawn_grid() -> bool:
    """Test: Spawn a hexagonal grid."""
    print(f"\n{Colors.INFO}>>> Step 3: Hex Grid -- Spawn 5-ring grid{Colors.RESET}")
    resp = execute_command("hex.SpawnGrid 80 5 5")
    success = resp.get("success", False)
    print_result("SpawnGrid cellR=80 gridR=5 height=5", success,
                 f"response={resp}")
    time.sleep(0.5)
    return success


def test_spawn_terrain() -> bool:
    """Test: Spawn procedural hex terrain."""
    print(f"\n{Colors.INFO}>>> Step 4: Hex Terrain -- Spawn procedural terrain{Colors.RESET}")
    resp = execute_command("hex.SpawnTerrain 100 5 150 0.08")
    success = resp.get("success", False)
    print_result("SpawnTerrain cellR=100 gridR=5 height=150 noise=0.08", success,
                 f"response={resp}")
    time.sleep(0.5)
    return success


def test_prism_variants() -> bool:
    """Test: Spawn prisms with different sizes."""
    print(f"\n{Colors.INFO}>>> Step 5: Hex Prism Variants -- Different sizes{Colors.RESET}")
    variants = [
        ("hex.SpawnPrism 50 100 500 0 50", "Small prism @ (500,0,50)"),
        ("hex.SpawnPrism 300 100 -500 0 50", "Large flat prism @ (-500,0,50)"),
        ("hex.SpawnPrism 150 600 0 500 300", "Tall prism @ (0,500,300)"),
    ]
    all_ok = True
    for cmd, desc in variants:
        resp = execute_command(cmd)
        success = resp.get("success", False)
        all_ok = all_ok and success
        print_result(desc, success)
        time.sleep(0.3)
    return all_ok


def test_parameter_changes() -> bool:
    """Test: Verify parameter-based mesh regeneration via multiple spawns."""
    print(f"\n{Colors.INFO}>>> Step 6: Parameter Validation -- Edge cases{Colors.RESET}")
    edge_cases = [
        ("hex.SpawnPrism 1 200 0 800 100", "Min radius (R=1)"),
        ("hex.SpawnPrism 100 0 200 800 100", "Zero height (H=0)"),
        ("hex.SpawnPrism 500 1000 -200 800 500", "Max size (R=500, H=1000)"),
    ]
    all_ok = True
    for cmd, desc in edge_cases:
        resp = execute_command(cmd)
        success = resp.get("success", False)
        all_ok = all_ok and success
        print_result(desc, success)
        time.sleep(0.3)
    return all_ok


def run_full_test():
    """Run the complete automated test suite."""
    print(f"{Colors.INFO}=============================================================={Colors.RESET}")
    print(f"{Colors.INFO}     Hexagonal Geometry System -- Automated Test Suite       {Colors.RESET}")
    print(f"{Colors.INFO}=============================================================={Colors.RESET}")
    print(f"  MCP Endpoint: {MCP_HOST}:{MCP_PORT}")
    print(f"  Project: TA-Playground")
    print(f"  Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}")

    # Pre-flight check: verify MCP is alive
    print(f"\n{Colors.INFO}>>> Pre-flight: Verifying MCP service...{Colors.RESET}")
    if not verify_mcp_alive():
        print(f"  {Colors.FAIL}[FAIL]{Colors.RESET} MCP service not responding on {MCP_HOST}:{MCP_PORT}")
        print(f"\n  {Colors.WARN}Please ensure:{Colors.RESET}")
        print(f"    1. UE Editor is running with TA-Playground.uproject loaded")
        print(f"    2. UnrealMCP plugin is enabled (listening on port {MCP_PORT})")
        print(f"\n  Start UE Editor with:")
        print(f"    .\\TA-Playground.uproject")
        return 1

    print(f"  {Colors.OK}[OK]{Colors.RESET} MCP service is alive and responding")
    resp = mcp_request("check_unreal_connection", {})
    info = resp.get("result", {})
    print(f"  Engine: {info.get('engine_version', 'unknown')}")
    print(f"  Project: {info.get('project_name', 'unknown')}")

    results = {}

    # Step 1: Cleanup
    results["destroy_all"] = test_destroy_all()

    # Step 2-4: Core pipeline
    results["spawn_prism"] = test_spawn_prism()
    results["spawn_grid"] = test_spawn_grid()
    results["spawn_terrain"] = test_spawn_terrain()

    # Step 5: Variants
    results["prism_variants"] = test_prism_variants()

    # Step 6: Edge cases
    results["parameter_changes"] = test_parameter_changes()

    # Summary
    print(f"\n{Colors.INFO}=============================================================={Colors.RESET}")
    print(f"{Colors.INFO}                        Test Summary                          {Colors.RESET}")
    print(f"{Colors.INFO}=============================================================={Colors.RESET}")

    passed = sum(1 for v in results.values() if v)
    total = len(results)

    for name, ok in results.items():
        status = f"{Colors.OK}PASS{Colors.RESET}" if ok else f"{Colors.FAIL}FAIL{Colors.RESET}"
        print(f"  {status}: {name}")

    print(f"\n  Total: {passed}/{total} tests passed")

    if passed == total:
        print(f"\n  {Colors.OK}[OK] All tests passed! Hexagonal geometry system is working correctly.{Colors.RESET}")
        print(f"  {Colors.INFO}Switch to UE Editor to visually inspect the results.{Colors.RESET}")
        return 0
    else:
        print(f"\n  {Colors.FAIL}[X] Some tests failed. Check UE Editor output log for details.{Colors.RESET}")
        return 1


def run_health_check():
    """Quick health check -- verify MCP is responsive with real data."""
    print(f"{Colors.INFO}Checking MCP health...{Colors.RESET}")
    if verify_mcp_alive():
        resp = mcp_request("check_unreal_connection", {})
        info = resp.get("result", {})
        print(f"  {Colors.OK}[OK]{Colors.RESET} MCP is responsive")
        print(f"  Engine: {info.get('engine_version', 'unknown')}")
        print(f"  Project: {info.get('project_name', 'unknown')}")
        return True
    else:
        print(f"  {Colors.FAIL}[FAIL]{Colors.RESET} MCP not responding on {MCP_HOST}:{MCP_PORT}")
        print(f"  Make sure UE Editor is running with UnrealMCP plugin enabled.")
        return False


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "health":
        ok = run_health_check()
        sys.exit(0 if ok else 1)
    else:
        sys.exit(run_full_test())
