#!/usr/bin/env python3
"""Re-spawn HexPrism with fixed winding order and take screenshots."""
import json
import socket
import time

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


def send(method, params):
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"v-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload).encode()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(15)
        s.connect((MCP_HOST, MCP_PORT))
        s.sendall(data)
        chunks = []
        while True:
            try:
                chunk = s.recv(65536)
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


def cmd(c):
    return send("execute_editor_command", {"command": c})


def main():
    print("=" * 60)
    print("  HexPrism Fix Verification")
    print("=" * 60)

    # 1. Clear and re-spawn
    print("\n[1] Clearing old hex actors...")
    r = cmd("hex.DestroyAll")
    print(f"    DestroyAll: {r.get('success', False)}")
    time.sleep(0.5)

    print("\n[2] Spawning new HexPrism (fixed winding order)...")
    r = cmd("hex.SpawnPrism 200 400 0 0 200")
    print(f"    SpawnPrism: {r.get('success', False)}")
    time.sleep(0.5)

    # 3. Ensure lighting exists
    print("\n[3] Adding lighting...")
    lights = [
        ("/Script/Engine.DirectionalLight", [0, 0, 1000], [-45, 0, 0]),
        ("/Script/Engine.SkyLight", [0, 0, 1000], [0, 0, 0]),
        ("/Script/Engine.ExponentialHeightFog", [0, 0, 0], [0, 0, 0]),
        ("/Script/Engine.PointLight", [500, -500, 300], [0, 0, 0]),
    ]
    for cls, loc, rot in lights:
        r = send("spawn_actor", {"className": cls, "location": loc, "rotation": rot})
        name = cls.split(".")[-1]
        print(f"    {name}: {'OK' if r.get('success') else 'FAIL'}")
    time.sleep(1)

    # 4. Camera views - carefully positioned to see the full prism
    # Prism at [0,0,200], R=200, H=400
    # Bounds: X,Y approx +/- 200, Z from 0 to 400
    views = [
        # Name, location, rotation
        # Isometric view - classic 3/4 view
        ("iso", [500, -500, 500], [-35, 45, 0]),
        # Front view - slightly elevated
        ("front", [0, -600, 300], [-15, 0, 0]),
        # Top view
        ("top", [0, 0, 800], [-90, 0, 0]),
        # Side view
        ("side", [600, 0, 300], [-15, 90, 0]),
        # Close-up iso
        ("closeup", [400, -400, 400], [-30, 45, 0]),
    ]

    print("\n[4] Taking screenshots from multiple angles...")
    for name, loc, rot in views:
        r = send("set_viewport_camera", {"location": loc, "rotation": rot})
        if not r.get("success"):
            print(f"    Camera {name}: FAILED to set")
            continue
        time.sleep(1.5)  # Wait for viewport to settle

        r = send("take_screenshot", {"filename": f"hex_fixed_{name}"})
        if r.get("success"):
            path = r.get("result", {}).get("path", "")
            print(f"    {name}: {path}")
        else:
            print(f"    {name}: screenshot FAILED - {r}")

    print("\n" + "=" * 60)
    print("  Done! Check screenshots.")
    print("=" * 60)


if __name__ == "__main__":
    main()
