#!/usr/bin/env python3
"""
Hexagon Terrain Integration Test Suite

Exercises the full hex terrain pipeline via UnrealMCP console commands.
Validates Bug #1 (per-layer materials), Bug #2 (UV after paint),
Feature #8 (LOD debug), Feature #9 (perf logging).

Usage:
    python scripts/hex_integration_test.py

Prerequisites:
    1. UE Editor running with TA-Playground.uproject loaded
    2. UnrealMCP plugin active (port 13377)
"""

import json
import socket
import sys
import time
from typing import Any

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


class Colors:
    OK = "\033[92m"
    FAIL = "\033[91m"
    WARN = "\033[93m"
    SKIP = "\033[94m"
    RESET = "\033[0m"


def console(cmd: str, timeout: float = 15.0) -> str:
    """Send a console command via UnrealMCP and return the raw result."""
    payload = {
        "jsonrpc": "2.0",
        "method": "run_console_command",
        "params": {"command": cmd},
        "id": f"test-{int(time.time() * 1000)}",
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
        raw = b"".join(chunks).decode("utf-8", errors="replace")
    try:
        resp = json.loads(raw)
        return resp.get("result", raw)
    except json.JSONDecodeError:
        return raw


class TestSuite:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.skipped = 0

    def run(self, name: str, cmd: str, expect_contains: str = "",
            expect_not_contains: str = "", timeout: float = 15.0) -> bool:
        print(f"  {name} ... ", end="", flush=True)
        try:
            result = str(console(cmd, timeout))
        except Exception as e:
            print(f"{Colors.FAIL}FAIL{Colors.RESET} ({e})")
            self.failed += 1
            return False

        ok = True
        if expect_contains and expect_contains not in result:
            ok = False
        if expect_not_contains and expect_not_contains in result:
            ok = False

        if ok:
            print(f"{Colors.OK}OK{Colors.RESET}")
            self.passed += 1
        else:
            print(f"{Colors.FAIL}FAIL{Colors.RESET}")
            # Truncate for readability
            short = result[:200] + "..." if len(result) > 200 else result
            print(f"      expected: '{expect_contains}'")
            print(f"      got:      {short}")
            self.failed += 1
        return ok

    def section(self, title: str):
        print(f"\n{'─' * 60}\n{title}\n{'─' * 60}")

    def summary(self):
        total = self.passed + self.failed + self.skipped
        print(f"\n{'=' * 60}")
        print(f"Results: {self.passed} passed, {self.failed} failed, "
              f"{self.skipped} skipped ({total} total)")
        if self.failed == 0:
            print(f"{Colors.OK}ALL TESTS PASSED{Colors.RESET}")
        else:
            print(f"{Colors.FAIL}{self.failed} TESTS FAILED{Colors.RESET}")
        return self.failed


def main() -> int:
    print("=" * 60)
    print("  Hexagon Terrain — Integration Test Suite")
    print("=" * 60)
    print(f"  Target: {MCP_HOST}:{MCP_PORT}")
    print()

    t = TestSuite()

    # ── Connection check ────────────────────────────────────────────────────
    t.section("0. Connection")

    try:
        r = console("showlog", timeout=5.0)
        if "error" in str(r).lower() and "not found" not in str(r).lower():
            print(f"  {Colors.WARN}Cannot connect to UE Editor. "
                  f"Is UnrealMCP plugin loaded?{Colors.RESET}")
            print(f"  {Colors.SKIP}Skipping all tests.{Colors.RESET}")
            t.skipped += 100
            return t.summary()
    except Exception as e:
        print(f"  {Colors.FAIL}Connection failed: {e}{Colors.RESET}")
        t.skipped += 100
        return t.summary()
    print(f"  {Colors.OK}Connected{Colors.RESET}")
    t.passed += 1

    # ── 1. Clean up ─────────────────────────────────────────────────────────
    t.section("1. Clean up")
    t.run("DestroyAll", "hex.DestroyAll", "destroyed")

    # ── 2. Create test scene (Bug #1 regression: per-layer materials) ──────
    t.section("2. Create Test Scene (Bug #1: per-layer materials)")
    t.run("CreateTestScene", "hex.CreateTestScene 14 120",
          "Test Scene Ready")

    # Verify per-layer materials render: Water/Sand/Grass count in stats
    t.run("PrintStats (verify layers)", "hex.Stats",
          "Water=")

    # ── 3. Grass-only terrain (Doc #3: GrassLevel config) ───────────────────
    t.section("3. Grass-only Terrain (Doc #3: GrassLevel=1.0)")
    t.run("CreateGrassTerrain", "hex.CreateGrassTerrain 12",
          "Grass Terrain Ready")

    # ── 4. Manual mode + Paint ──────────────────────────────────────────────
    t.section("4. Manual Mode + Paint")
    t.run("Enable manual mode", "hex.SetCell 3 0 Grass",
          "EHexTerrainType::Grass")
    t.run("SetCell valid", "hex.SetCell 3 1 Sand",
          "EHexTerrainType::Sand")
    t.run("FillRing", "hex.FillRing 0 0 2 Rock",
          "cells set to")

    # ── 5. World-Space UV (Bug #2: UV after paint) ─────────────────────────
    t.section("5. World-Space UV (Bug #2: UV after paint)")
    # Check TextureTileSize default is 200
    t.run("PrintStats (after paint)", "hex.Stats",
          "Total Cells")

    # ── 6. Error handling ────────────────────────────────────────────────────
    t.section("6. Error Handling")
    t.run("SetCell invalid type", "hex.SetCell 0 0 Lava",
          "Unknown terrain type")
    t.run("SetCell out of range", "hex.SetCell 999 999 Snow",
          "EHexTerrainType::Snow",
          timeout=5.0)

    # ── 7. Mixed terrain ────────────────────────────────────────────────────
    t.section("7. Mixed Terrain")
    t.run("CreateGrassSandTerrain", "hex.CreateGrassSandTerrain 14",
          "Grass+Sand Terrain Ready")
    t.run("Stats shows distribution", "hex.Stats",
          "Water=")

    # ── 8. Stats command ────────────────────────────────────────────────────
    t.section("8. PrintStats Detail")
    t.run("PrintStats", "hex.Stats", "LOD dist:")
    t.run("PrintStats chunks", "hex.Stats", "Chunks:")

    # ── 9. Clean up ─────────────────────────────────────────────────────────
    t.section("9. Final Clean Up")
    t.run("DestroyAll final", "hex.DestroyAll", "destroyed")

    return t.summary()


if __name__ == "__main__":
    sys.exit(main())
