#!/usr/bin/env python3
"""Final camera angle adjustment with correct UE rotation conventions."""
import json
import socket
import time
import math

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


def look_at(cam_pos, target_pos):
    """Calculate UE rotation [Pitch, Yaw, Roll] to look from cam_pos to target_pos."""
    dx = target_pos[0] - cam_pos[0]
    dy = target_pos[1] - cam_pos[1]
    dz = target_pos[2] - cam_pos[2]

    # Yaw: angle in XY plane, 0 = +X, 90 = +Y
    yaw = math.degrees(math.atan2(dy, dx))

    # Pitch: positive = look down, negative = look up
    horiz_dist = math.sqrt(dx * dx + dy * dy)
    pitch = math.degrees(math.atan2(dz, horiz_dist))

    return [round(pitch, 1), round(yaw, 1), 0]


def main():
    print("=== Final Camera Angle Test ===")

    # Ensure HexPrism exists
    cmd("hex.DestroyAll")
    time.sleep(0.5)
    cmd("hex.SpawnPrism 200 400 0 0 200")
    time.sleep(0.5)

    # Add lights
    for cls, loc, rot in [
        ("/Script/Engine.DirectionalLight", [0, 0, 1000], [-45, 0, 0]),
        ("/Script/Engine.SkyLight", [0, 0, 1000], [0, 0, 0]),
        ("/Script/Engine.ExponentialHeightFog", [0, 0, 0], [0, 0, 0]),
    ]:
        send("spawn_actor", {"className": cls, "location": loc, "rotation": rot})

    time.sleep(1)

    target = [0, 0, 200]  # HexPrism center

    # Predefined camera positions with correct look-at rotations
    views = [
        # Name, camera position
        ("front", [0, -600, 200]),
        ("iso", [500, -500, 500]),
        ("top", [0, 0, 900]),
        ("side", [600, 0, 200]),
        ("closeup_iso", [300, -300, 350]),
        ("three_quarter", [400, -200, 450]),
    ]

    for name, cam_pos in views:
        rot = look_at(cam_pos, target)
        print(f"\n{name}: cam={cam_pos}, rot={rot}")

        r = send("set_viewport_camera", {"location": cam_pos, "rotation": rot})
        print(f"  Camera set: {r.get('success', False)}")
        time.sleep(2)

        r = send("take_screenshot", {"filename": f"hex_final_{name}"})
        if r.get("success"):
            print(f"  Screenshot OK")
        else:
            print(f"  Screenshot FAIL: {r}")

    print("\n=== Done ===")


if __name__ == "__main__":
    main()
