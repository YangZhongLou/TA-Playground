"""Jade Material Pipeline — connects to UE via UnrealMCP, executes step by step."""
import json, socket, sys, time

class UEClient:
    def __init__(self):
        self.sock = None
    def reconnect(self):
        if self.sock:
            try: self.sock.close()
            except: pass
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(30.0)
        self.sock.connect(("127.0.0.1", 13377))
    def cmd(self, method, params=None):
        req = json.dumps({"id": method, "method": method, "params": params or {}}) + "\n"
        self.sock.sendall(req.encode("utf-8"))
        data = self.sock.recv(65536).decode("utf-8").strip()
        return json.loads(data)
    def run(self, method, params=None):
        try:
            self.reconnect()
        except:
            print(f"  [RECONNECT] Reconnecting...")
            time.sleep(0.5)
            self.reconnect()
        r = self.cmd(method, params)
        ok = r.get("success")
        tag = "OK" if ok else f"FAIL"
        print(f"  [{tag}] {method}: {r.get('error','') if not ok else ''}")
        return r
    def console(self, c):
        return self.run("run_console_command", {"command": c})


def main():
    ue = UEClient()
    print("Jade Material Pipeline\n")

    # ── SPHERE ──
    print("[1] Spawn sphere...")
    ue.run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
    ue.run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
    ue.run("set_actor_transform", {"name": "JadeSphere", "scale": [2.0, 2.0, 2.0]})

    # ── MATERIAL ──
    print("\n[2] Create material...")
    ue.console("Editor.CreateNewMaterial /Game/Materials/Master/M_Jade_Master")
    time.sleep(2)

    print("\n[3] Create MI_Jade_Green...")
    r = ue.run("create_material_instance", {
        "path": "/Game/Materials/Instances/MI_Jade_Green",
        "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
    })
    if not r.get("success"):
        print("  Fallback: using engine DefaultMaterial")
        r = ue.run("create_material_instance", {
            "path": "/Game/Materials/Instances/MI_Jade_Green",
            "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
        })

    print("\n[4] Apply material to sphere...")
    ue.run("set_material", {"actorName": "JadeSphere", "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"})

    # ── LIGHTS ──
    print("\n[5] Setup lights...")
    ue.run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [400, 300, 400]})
    ue.run("set_light_parameters", {"actorName": "KeyLight", "intensity": 80.0, "color": [1.0, 0.95, 0.85]})

    ue.run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-400, -200, 300]})
    ue.run("set_light_parameters", {"actorName": "RimLight", "intensity": 40.0, "color": [0.5, 0.8, 1.0]})

    ue.run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 800]})
    ue.run("set_light_parameters", {"actorName": "SkyLight", "intensity": 3.0})

    # ── FINALIZE ──
    print("\n[6] Focus + screenshot + save...")
    ue.run("focus_viewport", {"actorName": "JadeSphere"})
    ue.run("save_current_level", {})
    r = ue.run("take_screenshot", {"filename": "jade_p1_result"})
    print(f"\n>>> Screenshot saved: {r.get('result','')}")

    ue.sock.close()
    print("\nPipeline complete!")

if __name__ == "__main__":
    main()
