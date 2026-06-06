#!/usr/bin/env python3
"""Verify top and bottom cap visibility."""
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
    # Re-spawn prism
    cmd("hex.DestroyAll")
    time.sleep(0.5)
    cmd("hex.SpawnPrism 200 400 0 0 200")
    time.sleep(0.5)

    # Add lights
    for cls, loc, rot in [
        ("/Script/Engine.DirectionalLight", [0, 0, 1000], [-45, 0, 0]),
        ("/Script/Engine.SkyLight", [0, 0, 1000], [0, 0, 0]),
    ]:
        send("spawn_actor", {"className": cls, "location": loc, "rotation": rot})

    time.sleep(1)

    views = [
        # Name, location, rotation, description
        ("top_down", [0, 0, 900], [90, 0, 0], "Top view (Pitch=+90, looking DOWN)"),
        ("bottom_up", [0, 0, -100], [-90, 0, 0], "Bottom view (Pitch=-90, looking UP)"),
        ("front", [0, -600, 200], [0, 90, 0], "Front view"),
        ("iso", [500, -500, 500], [23, 135, 0], "ISO view"),
        ("side", [600, 0, 200], [0, 180, 0], "Side view"),
    ]

    for name, loc, rot, desc in views:
        print(f"\n{desc}")
        r = send("set_viewport_camera", {"location": loc, "rotation": rot})
        print(f"  Camera set: {r.get('success', False)}")
        time.sleep(2)
        r = send("take_screenshot", {"filename": f"hex_check_{name}"})
        print(f"  Screenshot: {r.get('success', False)}")

    print("\nDone!")


if __name__ == "__main__":
    main()
