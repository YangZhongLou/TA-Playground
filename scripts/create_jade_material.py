"""Create Jade Material display scene in UE via UnrealMCP."""
import json, socket, sys

class UEClient:
    def __init__(self, host="127.0.0.1", port=13377):
        self.host, self.port = host, port
        self.sock = None
    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(30.0)
        self.sock.connect((self.host, self.port))
    def disconnect(self):
        if self.sock: self.sock.close(); self.sock = None
    def cmd(self, method, params=None):
        msg = json.dumps({"id": method, "method": method, "params": params or {}}) + "\n"
        self.sock.sendall(msg.encode("utf-8"))
        resp = json.loads(self.sock.recv(65536).decode("utf-8").strip())
        ok = "OK" if resp.get("success") else f"FAIL: {resp.get('error','')[:80]}"
        print(f"  [{ok}] {method}")
        return resp
    def console(self, c): return self.cmd("run_console_command", {"command": c})


def main():
    ue = UEClient()
    ue.connect()
    print("Connected to UE Editor\n")

    # ── Create material via console ──
    print("=== Creating M_Jade_Master ===")
    r = ue.console("Editor.CreateNewMaterial /Game/Materials/Master/M_Jade_Master")
    # Check if created
    r = ue.cmd("get_asset_info", {"assetPath": "/Game/Materials/Master/M_Jade_Master"})
    print(f"  Asset info: {r}")

    # ── Create material instance ──
    print("\n=== Creating MI_Jade_Green ===")
    r = ue.cmd("create_material_instance", {
        "path": "/Game/Materials/Instances/MI_Jade_Green",
        "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
    })
    if not r.get("success"):
        # Fallback: use engine default material as parent
        print("  Fallback: using engine default material")
        r = ue.cmd("create_material_instance", {
            "path": "/Game/Materials/Instances/MI_Jade_Green",
            "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
        })

    # ── Spawn sphere ──
    print("\n=== Spawning Display Sphere ===")
    r = ue.cmd("spawn_actor", {
        "className": "StaticMeshActor",
        "name": "JadeDisplay_Sphere",
        "location": [0, 0, 150]
    })
    # Set mesh
    r = ue.cmd("set_static_mesh", {
        "actorName": "JadeDisplay_Sphere",
        "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
    })
    # Apply material
    print("\n=== Applying Jade Material ===")
    r = ue.cmd("set_material", {
        "actorName": "JadeDisplay_Sphere",
        "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
    })

    # ── Setup lights ──
    print("\n=== Setting up Lighting ===")
    # Add point lights for rim/fill effects
    ue.cmd("spawn_actor", {
        "className": "PointLight",
        "name": "KeyLight_Jade",
        "location": [300, 200, 300]
    })
    ue.cmd("set_light_parameters", {
        "actorName": "KeyLight_Jade",
        "intensity": 50.0,
        "color": [1.0, 0.95, 0.85]
    })
    ue.cmd("spawn_actor", {
        "className": "PointLight",
        "name": "RimLight_Jade",
        "location": [-300, -100, 250]
    })
    ue.cmd("set_light_parameters", {
        "actorName": "RimLight_Jade",
        "intensity": 30.0,
        "color": [0.5, 0.8, 1.0]
    })
    ue.cmd("spawn_actor", {
        "className": "SkyLight",
        "name": "SkyLight_Jade",
        "location": [0, 0, 600]
    })
    ue.cmd("set_light_parameters", {
        "actorName": "SkyLight_Jade",
        "intensity": 2.0
    })

    # ── Focus and screenshot ──
    ue.cmd("focus_viewport", {"actorName": "JadeDisplay_Sphere"})
    print("\n=== Screenshot ===")
    r = ue.cmd("take_screenshot", {"filename": "jade_material_p1"})
    print(f"  Screenshot: {r}")
    ue.cmd("save_current_level", {})

    ue.disconnect()
    print("\nJade Material P1 complete!")

if __name__ == "__main__":
    main()
