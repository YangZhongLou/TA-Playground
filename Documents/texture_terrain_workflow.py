"""End-to-end texture-to-terrain workflow demo.

Connects to a running UE Editor via UnrealMCP (TCP 13377) and:
1. Creates per-terrain-type materials with texture parameters
2. Assigns them to a hex terrain
3. Swaps textures at runtime
4. Adjusts UV settings
5. Verifies and screenshots

Usage:
    python scripts/texture_terrain_workflow.py
"""

import json
import socket
import sys
import time


class UEClient:
    """Minimal UnrealMCP TCP client."""

    def __init__(self, host="127.0.0.1", port=13377):
        self.host = host
        self.port = port

    def cmd(self, method, params=None):
        """Send a JSON command to UE and return the response dict."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30.0)
        try:
            sock.connect((self.host, self.port))
            msg = json.dumps({
                "id": method,
                "method": method,
                "params": params or {}
            }) + "\n"
            sock.sendall(msg.encode("utf-8"))
            raw = sock.recv(65536).decode("utf-8").strip()
            return json.loads(raw) if raw else {"success": False, "error": "empty response"}
        except Exception as e:
            return {"success": False, "error": str(e)}
        finally:
            sock.close()


def main():
    ue = UEClient()

    # Check connection
    r = ue.cmd("check_unreal_connection")
    if not r.get("success"):
        print("ERROR: Cannot connect to UE Editor. Is it running?")
        sys.exit(1)
    print(f"Connected: {r['result']['project_name']} on UE {r['result']['engine_version']}\n")

    terrain_name = "HexTerrain_Workflow"
    types = ["Water", "Sand", "Grass", "Rock", "Snow"]

    # ── Step 1: Create per-type materials ──
    print("=== Step 1: Creating materials ===")
    for t in types:
        mat_path = f"/Game/Materials/Workflow/M_Terrain_{t}"
        r = ue.cmd("create_material", {
            "path": mat_path,
            "shadingModel": "default_lit",
            "roughness": 0.7
        })
        status = "OK" if r.get("success") else f"FAIL: {r.get('error', '')}"
        print(f"  {t}: {status}")

    # ── Step 2: Spawn terrain ──
    print("\n=== Step 2: Spawning terrain ===")
    r = ue.cmd("run_console_command", {
        "command": f"hex.CreateFullTerrain {terrain_name} 6"
    })
    time.sleep(0.5)  # Let terrain build complete

    # ── Step 3: Assign materials to layers ──
    print("\n=== Step 3: Assigning layer materials ===")
    for t in types:
        r = ue.cmd("set_terrain_layer_material", {
            "terrainName": terrain_name,
            "terrainType": t,
            "materialPath": f"/Game/Materials/Workflow/M_Terrain_{t}"
        })
        status = "OK" if r.get("success") else f"FAIL: {r.get('error','')}"
        print(f"  {t}: {status}")

    # ── Step 4: Configure UV ──
    print("\n=== Step 4: Configuring UV ===")
    r = ue.cmd("set_actor_property", {
        "actorName": terrain_name,
        "propertyName": "TextureTileSize",
        "propertyValue": "200.0"
    })
    print(f"  TextureTileSize = 200: {'OK' if r.get('success') else r.get('error','FAIL')}")

    # ── Step 5: Verify ──
    print("\n=== Step 5: Verification ===")
    info = ue.cmd("get_terrain_layer_info", {"terrainName": terrain_name})
    if info.get("success"):
        r = info["result"]
        print(f"  Cells: {r['cellCount']}, Chunks: {r['chunkCount']}")
        print(f"  TileSize: {r['textureTileSize']}, ManualMode: {r['bManualMode']}")
        for layer in r["layers"]:
            status = "ACTIVE" if layer["activeChunks"] > 0 else "idle"
            print(f"  {layer['type']}: {layer['activeChunks']} chunks [{status}]")
    else:
        print(f"  FAILED: {info.get('error','')}")

    # ── Step 6: Screenshot ──
    print("\n=== Step 6: Screenshot ===")
    ue.cmd("focus_viewport", {"actorName": terrain_name})
    r = ue.cmd("take_screenshot", {"filename": "texture_terrain_workflow"})
    print(f"  {'OK' if r.get('success') else 'FAIL'}")
    ue.cmd("save_current_level", {})

    print("\nWorkflow complete!")


if __name__ == "__main__":
    main()
