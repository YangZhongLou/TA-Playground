#!/usr/bin/env python3
"""
HexPrism Camera Presets

One-click camera angle presets for HexPrism screenshots.

Usage:
    python scripts/camera_presets.py <preset_name>

Available presets:
    top       - Best top-down view (tune_top_far)
    tilt      - 3D tilted view
    iso       - Isometric game view
    side      - Side profile
    front     - Front view
"""

import json
import socket
import sys

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


PRESETS = {
    "top": {
        "name": "Best Top-Down",
        "description": "Full hexagon outline with inner shadow (tune_top_far)",
        "location": [0, 0, 1200],
        "rotation": [-85, 0, 0],
        "zoom": 1800.0,
    },
    "tilt": {
        "name": "3D Tilted",
        "description": "Three faces visible, strong 3D feel",
        "location": [0, 100, 800],
        "rotation": [-80, 10, 0],
        "zoom": 1500.0,
    },
    "iso": {
        "name": "Isometric",
        "description": "Classic isometric game camera",
        "location": [400, -400, 500],
        "rotation": [-35, 45, 0],
        "zoom": 2000.0,
    },
    "side": {
        "name": "Side Profile",
        "description": "Shows height of the prism",
        "location": [800, 0, 300],
        "rotation": [-10, 90, 0],
        "zoom": 1500.0,
    },
    "front": {
        "name": "Front View",
        "description": "Straight-on front view",
        "location": [0, 800, 300],
        "rotation": [-10, 0, 0],
        "zoom": 1500.0,
    },
}


def mcp_request(method, params, timeout=10.0):
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"preset-{method}",
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


def apply_preset(name):
    preset = PRESETS.get(name)
    if not preset:
        print(f"Unknown preset: {name}")
        print(f"Available: {', '.join(PRESETS.keys())}")
        return False

    print(f"Applying preset: {preset['name']}")
    print(f"  {preset['description']}")
    print(f"  Location: {preset['location']}")
    print(f"  Rotation: {preset['rotation']}")

    # Set viewport camera
    r = mcp_request(
        "set_viewport_camera",
        {"location": preset["location"], "rotation": preset["rotation"]},
    )
    if not r.get("success"):
        print(f"  [FAIL] set_viewport_camera: {r}")
        return False
    print("  [OK] Viewport camera set")

    # Sync IsometricCamera
    r = mcp_request(
        "set_runtime_camera_transform",
        {"location": preset["location"], "zoom": preset["zoom"]},
    )
    if r.get("success"):
        print("  [OK] IsometricCamera synced")

    print(f"\nPreset '{name}' applied. Switch to UE Editor to view.")
    return True


def list_presets():
    print("Available camera presets:\n")
    for key, preset in PRESETS.items():
        print(f"  {key:8s} - {preset['name']}")
        print(f"           {preset['description']}")
        print(f"           loc={preset['location']} rot={preset['rotation']}")
        print()


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "list"):
        list_presets()
        print("Usage: python camera_presets.py <preset_name>")
        return 0

    preset_name = sys.argv[1]
    if apply_preset(preset_name):
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
