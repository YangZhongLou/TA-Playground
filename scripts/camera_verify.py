#!/usr/bin/env python3
"""Adjust camera angles and take screenshots for vertex order verification."""
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
    print("  Camera Adjustment + Vertex Order Verification")
    print("=" * 60)

    # Toggle wireframe mode to see mesh structure
    print("\n[1] Switching to wireframe mode...")
    r = cmd("viewmode wireframe")
    print(f"    Wireframe: {r.get('success', False)}")
    time.sleep(1)

    # HexPrism at [0,0,200], Radius=200, Height=400
    # Bounding: X,Y +/- ~346, Z: 0 to 400
    views = [
        ("iso_good", [600, -600, 600], [-30, 45, 0]),
        ("iso_far", [1000, -1000, 800], [-25, 45, 0]),
        ("front_close", [0, -600, 200], [0, 0, 0]),
        ("top_down", [0, 0, 900], [-90, 0, 0]),
        ("side", [600, 0, 200], [0, 90, 0]),
    ]

    for name, loc, rot in views:
        print(f"\n[Camera] {name}: loc={loc}, rot={rot}")
        r = send("set_viewport_camera", {"location": loc, "rotation": rot})
        print(f"    Set: {'OK' if r.get('success') else 'FAIL'}")
        time.sleep(1)

        r = send("take_screenshot", {"filename": f"hex_{name}"})
        if r.get("success"):
            path = r.get("result", {}).get("path", "")
            print(f"    Screenshot: {path}")
        else:
            print(f"    Screenshot failed: {r}")

    # Switch back to lit mode
    print("\n[2] Switching back to lit mode...")
    r = cmd("viewmode lit")
    print(f"    Lit: {r.get('success', False)}")

    # Take lit screenshots too
    print("\n[3] Taking lit screenshots...")
    for name, loc, rot in views[:3]:
        r = send("set_viewport_camera", {"location": loc, "rotation": rot})
        time.sleep(1)
        r = send("take_screenshot", {"filename": f"hex_{name}_lit"})
        if r.get("success"):
            print(f"    {name}_lit: OK")
        else:
            print(f"    {name}_lit: FAIL")

    print("\n" + "=" * 60)
    print("  Done!")
    print("=" * 60)


if __name__ == "__main__":
    main()
