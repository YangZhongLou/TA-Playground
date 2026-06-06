#!/usr/bin/env python3
"""
Hexagonal Geometry -- Full Orchestrated Test

Automatically starts UE Editor (if needed), waits for MCP, runs tests.

Usage:
    python scripts/hex_test_orchestrator.py
"""

import json
import os
import socket
import subprocess
import sys
import time

# Config
MCP_HOST = "127.0.0.1"
MCP_PORT = 13377
UE_EDITOR = r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
PROJECT = r"D:\Mine\unreal_projects\TA-Playground\TA-Playground.uproject"
MAX_WAIT_SECONDS = 180  # 3 min for editor to start


def check_port():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2)
        result = s.connect_ex((MCP_HOST, MCP_PORT))
        s.close()
        return result == 0
    except:
        return False


def check_mcp_alive():
    """Send a real request and verify response."""
    try:
        payload = {
            "jsonrpc": "2.0",
            "method": "check_unreal_connection",
            "params": {},
            "id": "health-check"
        }
        data = json.dumps(payload).encode("utf-8")
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            s.connect((MCP_HOST, MCP_PORT))
            s.sendall(data)
            resp = s.recv(4096)
            if not resp:
                return False
            parsed = json.loads(resp.decode("utf-8", errors="replace"))
            return parsed.get("success", False) and "result" in parsed
    except Exception:
        return False


def start_ue_editor():
    """Launch UE Editor in background."""
    cmd = [UE_EDITOR, PROJECT]
    print(f"[START] Launching UE Editor...")
    print(f"        {UE_EDITOR}")
    print(f"        Project: {PROJECT}")
    try:
        # Use CREATE_NEW_CONSOLE to avoid blocking, hide window
        proc = subprocess.Popen(
            cmd,
            creationflags=subprocess.CREATE_NEW_CONSOLE,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        print(f"        PID: {proc.pid}")
        return proc
    except Exception as e:
        print(f"[ERROR] Failed to start UE Editor: {e}")
        return None


def wait_for_mcp(timeout=MAX_WAIT_SECONDS):
    """Poll MCP port until it's alive or timeout."""
    print(f"[WAIT] Waiting for MCP on {MCP_HOST}:{MCP_PORT} (max {timeout}s)...")
    start = time.time()
    dots = 0
    while time.time() - start < timeout:
        if check_mcp_alive():
            elapsed = time.time() - start
            print(f"\n[OK] MCP is alive after {elapsed:.1f}s")
            return True
        time.sleep(2)
        dots += 1
        if dots % 10 == 0:
            print(f"  ... waited {int(time.time() - start)}s")
        else:
            print(".", end="", flush=True)
    print(f"\n[TIMEOUT] MCP did not become ready within {timeout}s")
    return False


def run_tests():
    """Run the automated test suite via MCP."""
    print("\n" + "=" * 60)
    print("  RUNNING HEX GEOMETRY TESTS")
    print("=" * 60)

    import hex_auto_test
    return hex_auto_test.run_full_test()


def main():
    print("=" * 60)
    print("  HEX TEST ORCHESTRATOR")
    print("=" * 60)

    # Phase 1: Check if MCP is already alive
    if check_mcp_alive():
        print("[OK] MCP already running. Skipping editor launch.")
    else:
        print("[INFO] MCP not detected. Need to start UE Editor.")

        # Phase 2: Start UE Editor
        proc = start_ue_editor()
        if not proc:
            print("\n[FAIL] Could not auto-start UE Editor.")
            print("[HINT] Please start manually:")
            print(f"       \"{UE_EDITOR}\" \"{PROJECT}\"")
            print("\nThen re-run this script.")
            return 1

        # Phase 3: Wait for MCP
        if not wait_for_mcp():
            print("\n[FAIL] MCP did not start. Possible causes:")
            print("       - UE Editor is still loading (be patient)")
            print("       - UnrealMCP plugin failed to load")
            print("       - Port conflict on 13377")
            print("\nCheck UE Editor Output Log for errors.")
            return 1

    # Phase 4: Run tests
    return run_tests()


if __name__ == "__main__":
    sys.exit(main())
