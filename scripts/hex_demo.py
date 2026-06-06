#!/usr/bin/env python3
"""
Hexagonal Geometry System — MCP Demo Script

Validates the full Phase 1~3 pipeline by sending commands to UnrealMCP
via TCP (127.0.0.1:13377). Each command is a JSON RPC-style request.

Usage:
    python scripts/hex_demo.py [prism|grid|terrain|destroy]

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


def mcp_request(method: str, params: dict) -> dict:
    """Send a JSON-RPC style request to UnrealMCP and return the response."""
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"hex-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(10.0)
        sock.connect((MCP_HOST, MCP_PORT))
        sock.sendall(data)

        # Receive response
        chunks = []
        while True:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                # Heuristic: most responses fit in one recv
                if len(chunk) < 4096:
                    break
            except socket.timeout:
                break

        response = b"".join(chunks).decode("utf-8", errors="replace")
        return json.loads(response) if response else {}


def execute_command(cmd: str) -> dict:
    """Execute an editor console command via MCP."""
    return mcp_request("execute_editor_command", {"command": cmd})


def destroy_all():
    """Remove all hex actors."""
    print("[1/1] Destroying all hex actors...")
    resp = execute_command("hex.DestroyAll")
    print(f"      → {resp.get('success', False)}")


def demo_prism():
    """Phase 1: Spawn a hexagonal prism."""
    print("\n=== Phase 1: Hex Prism ===")
    print("[1/1] Spawning hex prism (R=200, H=400)...")
    resp = execute_command("hex.SpawnPrism 200 400 0 0 200")
    print(f"      → success={resp.get('success', False)}")


def demo_grid():
    """Phase 2: Spawn a hexagonal grid."""
    print("\n=== Phase 2: Hex Grid ===")
    print("[1/1] Spawning 5-ring hex grid...")
    resp = execute_command("hex.SpawnGrid 80 5 5")
    print(f"      → success={resp.get('success', False)}")


def demo_terrain():
    """Phase 3: Spawn procedural hex terrain."""
    print("\n=== Phase 3: Hex Terrain ===")
    print("[1/1] Spawning procedural terrain (radius=5, cell=100)...")
    resp = execute_command("hex.SpawnTerrain 100 5 150 0.08")
    print(f"      → success={resp.get('success', False)}")


def demo_full_pipeline():
    """Run the complete pipeline: destroy → prism → grid → terrain."""
    destroy_all()
    time.sleep(0.5)

    demo_prism()
    time.sleep(0.5)

    demo_grid()
    time.sleep(0.5)

    demo_terrain()

    print("\n=== Pipeline complete ===")
    print("Switch to UE Editor to view the results.")


def main():
    if len(sys.argv) < 2:
        demo_full_pipeline()
        return

    cmd = sys.argv[1].lower()
    if cmd == "destroy":
        destroy_all()
    elif cmd == "prism":
        demo_prism()
    elif cmd == "grid":
        demo_grid()
    elif cmd == "terrain":
        demo_terrain()
    else:
        print(f"Unknown command: {cmd}")
        print("Usage: python hex_demo.py [prism|grid|terrain|destroy]")
        sys.exit(1)


if __name__ == "__main__":
    main()
