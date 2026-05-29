"""Cleanup existing assets, then run Jade pipeline."""
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

def run(method, params=None):
    try:
        r = cmd(method, params)
        ok = "OK" if r.get("success") else f"FAIL: {r.get('error','')[:80]}"
        print(f"  [{ok}] {method}")
        return r
    except Exception as e:
        print(f"  [ERR] {method}: {e}")
        return {"success": False}

# Wait for UE
print("Waiting for UE...")
for i in range(30):
    try:
        s = socket.socket(); s.settimeout(1); s.connect(("127.0.0.1", 13377)); s.close()
        break
    except: time.sleep(2)

print("Connected!\n")

# ── 0. Delete existing conflicting assets ──
print("[0] Cleaning up old assets...")
for path in [
    "/Game/Materials/Instances/MI_Jade_Green",
    "/Game/Materials/Instances/MI_Test_Engine",
    "/Game/Materials/Instances/MI_Test2",
    "/Game/Materials/Instances/MI_Test_Jade",
    "/Game/Materials/Instances/MI_Test_Bad",
    "/Game/Materials/Master/M_Jade_Master",
]:
    run("delete_asset", {"path": path})

# ── 1. Create MI with unique name ──
import uuid
suffix = uuid.uuid4().hex[:6]
mi_name = f"MI_Jade_Green_{suffix}"
mi_path = f"/Game/Materials/Instances/{mi_name}"
print(f"\n[1] Creating {mi_name}...")
r = run("create_material_instance", {
    "path": mi_path,
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
if not r.get("success"):
    print("FATAL: Cannot create material instance. Aborting.")
    exit(1)

# ── 2. Spawn sphere ──
print("[2] Spawning sphere...")
run("spawn_actor", {"className": "StaticMeshActor", "name": f"JadeSphere_{suffix}", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": f"JadeSphere_{suffix}", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": f"JadeSphere_{suffix}", "scale": [2.5, 2.5, 2.5]})

# ── 3. Apply material ──
print("[3] Applying material...")
run("set_material", {
    "actorName": f"JadeSphere_{suffix}",
    "materialPath": f"{mi_path}.{mi_name}"
})

# ── 4. Jade PBR params ──
print("[4] Setting Jade PBR parameters...")
run("set_material_parameter", {"actorName": f"JadeSphere_{suffix}", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": f"JadeSphere_{suffix}", "parameterName": "Specular", "scalarValue": 0.5})
run("set_material_parameter", {"actorName": f"JadeSphere_{suffix}", "parameterName": "Metallic", "scalarValue": 0.0})
run("set_material_parameter", {"actorName": f"JadeSphere_{suffix}", "parameterName": "BaseColor", "vectorValue": [0.05, 0.35, 0.20]})

# ── 5. Lighting ──
print("[5] Cinematic lighting...")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]})

run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]})

run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]})

# ── 6. Finalize ──
print("[6] Finalizing...")
run("focus_viewport", {"actorName": f"JadeSphere_{suffix}"})
run("save_current_level", {})

print(f"\n=== Jade Material Pipeline Complete! ===")
print(f"Material Instance: {mi_name}")
print(f"Sphere: JadeSphere_{suffix}")
print("Check UE Editor viewport")
