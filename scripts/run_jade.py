"""Jade Pipeline — all dialogs eliminated."""
import json, socket, time

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15.0)
    s.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    resp = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return resp

def run(m, p=None):
    try:
        r = cmd(m, p)
        s = "OK" if r.get("success") else f"FAIL: {r.get('error','')[:60]}"
        print(f"  [{s}] {m}")
        return r
    except Exception as e:
        print(f"  [ERR] {m}: {e}")
        return {"success": False}

print("Waiting for UE...")
for i in range(30):
    try:
        s = socket.socket(); s.settimeout(1); s.connect(("127.0.0.1", 13377)); s.close()
        break
    except: time.sleep(2)

print("Connected!\n")

# ── STEP 1: Create Master Material ──
print("[1] create_material M_Jade_Master")
run("create_material", {"path": "/Game/Materials/Master/M_Jade_Master"})

# ── STEP 2: Create Material Instance ──
print("[2] create_material_instance MI_Jade_Green")
run("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
})

# ── STEP 3: Spawn Sphere ──
print("[3] Spawn sphere")
run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "JadeSphere", "scale": [2.5, 2.5, 2.5]})

# ── STEP 4: Apply MI & set PBR ──
print("[4] Apply material & PBR params")
mi_path = "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
run("set_material", {"actorName": "JadeSphere", "materialPath": mi_path})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "BaseColor", "vectorValue": [0.05, 0.35, 0.20]})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Specular", "scalarValue": 0.55})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Metallic", "scalarValue": 0.0})

# ── STEP 5: Lighting ──
print("[5] Lights")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]})
run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]})
run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]})

# ── STEP 6: Finalize ──
print("[6] Focus + save")
run("focus_viewport", {"actorName": "JadeSphere"})
run("save_current_level", {})

print("\n=== Jade Pipeline Complete ===")
