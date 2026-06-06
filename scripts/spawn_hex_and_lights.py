#!/usr/bin/env python3
"""
Spawn a single HexPrism with lighting setup.
"""
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
        s.settimeout(10)
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
    print("=" * 55)
    print("  HexPrism + Lighting Setup")
    print("=" * 55)

    # 1. Clear previous hex actors
    print("\n[1] Clearing previous hex actors...")
    r = cmd("hex.DestroyAll")
    print(f"    DestroyAll: success={r.get('success', False)}")
    time.sleep(0.5)

    # 2. Spawn HexPrism
    print("\n[2] Spawning HexPrism (Radius=200, Height=400)...")
    r = cmd("hex.SpawnPrism 200 400 0 0 200")
    print(f"    HexPrism: success={r.get('success', False)}")
    if r.get("result"):
        print(f"    Details: {r.get('result', {})}")
    time.sleep(0.5)

    # 3. Add lighting
    print("\n[3] Adding lighting...")

    # Directional Light (main light)
    r = send("spawn_actor", {
        "className": "/Script/Engine.DirectionalLight",
        "location": [0, 0, 1000],
        "rotation": [-45, 0, 0]
    })
    print(f"    DirectionalLight: {'OK' if r.get('success') else 'FAIL'}")

    # SkyLight (ambient)
    r = send("spawn_actor", {
        "className": "/Script/Engine.SkyLight",
        "location": [0, 0, 1000]
    })
    print(f"    SkyLight: {'OK' if r.get('success') else 'FAIL'}")

    # ExponentialHeightFog (atmosphere)
    r = send("spawn_actor", {
        "className": "/Script/Engine.ExponentialHeightFog",
        "location": [0, 0, 0]
    })
    print(f"    ExponentialHeightFog: {'OK' if r.get('success') else 'FAIL'}")

    # Point Light (warm fill light)
    r = send("spawn_actor", {
        "className": "/Script/Engine.PointLight",
        "location": [500, -500, 300],
        "rotation": [0, 0, 0]
    })
    print(f"    PointLight (fill): {'OK' if r.get('success') else 'FAIL'}")
    time.sleep(0.5)

    # 4. Set viewport camera
    print("\n[4] Setting viewport camera...")
    r = send("set_viewport_camera", {
        "location": [800, -800, 600],
        "rotation": [-30, 45, 0]
    })
    print(f"    Camera: {'OK' if r.get('success') else 'FAIL'}")

    print("\n" + "=" * 55)
    print("  Done! Switch to UE Editor to view.")
    print("=" * 55)


if __name__ == "__main__":
    main()
