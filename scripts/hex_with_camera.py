#!/usr/bin/env python3
"""
Hex + IsometricCamera -- Full visual test pipeline.

Spawns hex geometry + lighting, sets up IsometricCamera, takes screenshots.
"""
import json
import socket
import time
import os

MCP_HOST = "127.0.0.1"
MCP_PORT = 13377


def send(method, params):
    payload = {"jsonrpc": "2.0", "method": method, "params": params, "id": f"v-{int(time.time()*1000)}"}
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
                except:
                    continue
            except socket.timeout:
                break
        resp = b"".join(chunks).decode("utf-8", errors="replace")
        return json.loads(resp) if resp else {}


def cmd(c):
    return send("execute_editor_command", {"command": c})


def screenshot(name):
    return send("take_screenshot", {"filename": name})


def run():
    print("=" * 60)
    print("  Hex + IsometricCamera Visual Test")
    print("=" * 60)

    # 1. Check MCP
    r = send("check_unreal_connection", {})
    if not r.get("success"):
        print("[FAIL] MCP not responding")
        return False
    print(f"[OK] MCP connected: {r.get('result', {}).get('engine_version')}")

    # 2. Clear existing hex actors
    print("\n[1] Clearing previous hex actors...")
    cmd("hex.DestroyAll")
    time.sleep(0.5)

    # 3. Spawn hex geometry
    print("\n[2] Spawning hex geometry...")
    results = []
    results.append(("Prism", cmd("hex.SpawnPrism 200 400 0 0 200")))
    time.sleep(0.3)
    results.append(("Grid", cmd("hex.SpawnGrid 100 3 5")))
    time.sleep(0.3)
    results.append(("Terrain", cmd("hex.SpawnTerrain 120 4 150 0.08")))
    time.sleep(0.3)
    for name, r in results:
        ok = r.get("success", False)
        print(f"  {'[OK]' if ok else '[FAIL]'} {name}")

    # 4. Add lighting via spawn_actor
    print("\n[3] Adding lighting...")
    # DirectionalLight
    r = send("spawn_actor", {
        "className": "/Script/Engine.DirectionalLight",
        "location": [0, 0, 1000],
        "rotation": [-45, 0, 0]
    })
    print(f"  DirectionalLight: {'OK' if r.get('success') else 'FAIL'}")
    # SkyLight
    r = send("spawn_actor", {
        "className": "/Script/Engine.SkyLight",
        "location": [0, 0, 1000]
    })
    print(f"  SkyLight: {'OK' if r.get('success') else 'FAIL'}")
    # ExponentialHeightFog for atmosphere
    r = send("spawn_actor", {
        "className": "/Script/Engine.ExponentialHeightFog",
        "location": [0, 0, 0]
    })
    print(f"  ExponentialHeightFog: {'OK' if r.get('success') else 'FAIL'}")
    time.sleep(0.5)

    # 5. Set viewport camera to isometric-like view
    print("\n[4] Setting camera view...")
    r = send("set_viewport_camera", {
        "location": [2000, -2000, 2000],
        "rotation": [-35, 45, 0]
    })
    print(f"  Camera set: {'OK' if r.get('success') else 'FAIL'}")
    time.sleep(1)

    # 6. Screenshot 1 - viewport
    print("\n[5] Screenshot 1: Viewport view")
    r = screenshot("hex_viewport")
    if r.get("success"):
        path = r.get("result", {}).get("path", "")
        print(f"  Saved: {path}")
        if os.path.exists(path):
            print(f"  Size: {os.path.getsize(path)} bytes")
    else:
        print(f"  FAIL: {r}")
    time.sleep(1)

    # 7. Try IsometricCamera -- spawn the pawn
    print("\n[6] Spawning IsometricCameraPawn...")
    r = send("spawn_actor", {
        "className": "/Script/IsometricCamera.IsometricCameraPawn",
        "location": [0, 0, 0],
        "rotation": [0, 0, 0]
    })
    print(f"  IsometricCameraPawn: {'OK' if r.get('success') else 'FAIL'}")
    if r.get("success"):
        print(f"  Name: {r.get('result', {}).get('name', '?')}")

    # 8. Set viewport to look from above
    print("\n[7] Overhead view...")
    r = send("set_viewport_camera", {
        "location": [0, 0, 3000],
        "rotation": [-90, 0, 0]
    })
    print(f"  Camera set: {'OK' if r.get('success') else 'FAIL'}")
    time.sleep(1)

    # 9. Screenshot 2 - overhead
    print("\n[8] Screenshot 2: Overhead view")
    r = screenshot("hex_overhead")
    if r.get("success"):
        path = r.get("result", {}).get("path", "")
        print(f"  Saved: {path}")
        if os.path.exists(path):
            print(f"  Size: {os.path.getsize(path)} bytes")
    else:
        print(f"  FAIL: {r}")

    # 10. Side view
    print("\n[9] Side view...")
    r = send("set_viewport_camera", {
        "location": [3000, 0, 500],
        "rotation": [0, 90, 0]
    })
    time.sleep(1)
    r = screenshot("hex_side")
    if r.get("success"):
        path = r.get("result", {}).get("path", "")
        print(f"  Saved: {path}")
        if os.path.exists(path):
            print(f"  Size: {os.path.getsize(path)} bytes")

    print("\n" + "=" * 60)
    print("  Test complete. Check screenshots at:")
    print("  D:\\Mine\\unreal_projects\\TA-Playground\\Saved\\Screenshots\\WindowsEditor\\")
    print("=" * 60)
    return True


if __name__ == "__main__":
    run()
