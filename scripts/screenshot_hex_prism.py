#!/usr/bin/env python3
"""Spawn AHexPrism and take a screenshot via UnrealMCP."""

import json
import socket
import sys
import time

HOST = "127.0.0.1"
PORT = 13377


def send_command(method: str, params: dict, timeout: float = 15.0) -> dict:
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": f"cmd-{int(time.time() * 1000)}",
    }
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    start = time.time()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        sock.connect((HOST, PORT))
        sock.sendall(data)
        response = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response += chunk
            if len(chunk) < 4096:
                break
        elapsed = (time.time() - start) * 1000
        resp = json.loads(response.decode("utf-8", errors="replace")) if response else {}
        return resp


def main():
    # 1. Check connection
    print("1. Checking connection...")
    resp = send_command("get_editor_info", {})
    if not resp.get("success"):
        print(f"   FAIL: {resp.get('error', 'no response')}")
        sys.exit(1)
    print(f"   OK: {resp.get('result', {})}")

    # 2. Destroy any existing test actors
    print("\n2. Cleaning up old test actors...")
    resp = send_command("execute_editor_command", {"command": "hex.DestroyAll"})
    print(f"   DestroyAll: {'OK' if resp.get('success') else resp.get('error')}")
    time.sleep(0.5)

    # 3. Spawn AHexPrism
    print("\n3. Spawning AHexPrism...")
    resp = send_command("spawn_actor", {
        "className": "AHexPrism",
        "name": "TestHexPrism",
        "location": [0, 0, 300]
    })
    if resp.get("success"):
        print(f"   OK: spawned TestHexPrism")
    else:
        print(f"   FAIL: {resp.get('error', 'unknown error')}")
        sys.exit(1)
    time.sleep(0.5)

    # 4. Set properties to make it visible
    print("\n4. Configuring prism properties...")
    resp = send_command("set_actor_property", {
        "actorName": "TestHexPrism",
        "propertyName": "Radius",
        "value": {"FloatValue": 150.0}
    })
    print(f"   Radius=150: {'OK' if resp.get('success') else resp.get('error')}")

    resp = send_command("set_actor_property", {
        "actorName": "TestHexPrism",
        "propertyName": "Height",
        "value": {"FloatValue": 250.0}
    })
    print(f"   Height=250: {'OK' if resp.get('success') else resp.get('error')}")
    time.sleep(0.3)

    # 5. Take screenshot
    import os
    screenshot_path = "D:/Mine/unreal_projects/TA-Playground/Saved/Screenshots/hex_prism_test.png"
    os.makedirs(os.path.dirname(screenshot_path), exist_ok=True)
    print(f"\n5. Taking screenshot -> {screenshot_path}")
    resp = send_command("take_screenshot", {
        "path": screenshot_path,
        "width": 1920,
        "height": 1080
    }, timeout=30.0)
    if resp.get("success"):
        print(f"   OK: Screenshot saved")
    else:
        print(f"   WARN: {resp.get('error', 'screenshot may have failed')}")

    print("\nDone!")


if __name__ == "__main__":
    main()
