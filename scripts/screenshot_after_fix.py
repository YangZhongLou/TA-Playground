#!/usr/bin/env python3
"""Re-spawn HexPrism after winding order fix and take screenshots."""
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
    print("=== Re-spawn HexPrism after fix ===")

    # Destroy old and spawn new
    cmd("hex.DestroyAll")
    time.sleep(0.5)

    r = cmd("hex.SpawnPrism 200 400 0 0 200")
    print(f"SpawnPrism: {r.get('success', False)}")
    time.sleep(1)

    # Add lights
    lights = [
        ("/Script/Engine.DirectionalLight", [0, 0, 1000], [-45, 0, 0]),
        ("/Script/Engine.SkyLight", [0, 0, 1000], [0, 0, 0]),
        ("/Script/Engine.ExponentialHeightFog", [0, 0, 0], [0, 0, 0]),
    ]
    for cls, loc, rot in lights:
        send("spawn_actor", {"className": cls, "location": loc, "rotation": rot})

    time.sleep(1)

    # Use HighResShot for screenshots (more reliable)
    views = [
        # (name, location, rotation) - None means don't move
        ("default", None, None),
        ("front", [0, -500, 200], [0, 90, 0]),
        ("iso", [400, -400, 400], [-25, 45, 0]),
        ("top", [0, 0, 800], [-90, 0, 0]),
        ("side", [500, 0, 200], [0, 90, 0]),
    ]

    for name, loc, rot in views:
        if loc is not None:
            print(f"Setting {name} view: loc={loc}, rot={rot}")
            r = send("set_viewport_camera", {"location": loc, "rotation": rot})
            print(f"  Camera set: {r.get('success', False)}")
            time.sleep(2)
        else:
            print("Using default camera position...")
            time.sleep(1)

        # Take screenshot via MCP take_screenshot
        r = send("take_screenshot", {"filename": f"hex_afterfix_{name}"})
        if r.get("success"):
            path = r.get("result", {}).get("path", "")
            print(f"  Screenshot: {path}")
        else:
            print(f"  Screenshot failed: {r}")

        # Also take HighResShot
        cmd(f"HighResShot 1 filename=hex_hrs_{name}")
        time.sleep(2)

    print("\nDone! Check Saved/Screenshots/WindowsEditor/")


if __name__ == "__main__":
    main()
