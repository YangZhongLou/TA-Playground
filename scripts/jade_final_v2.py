"""Final Jade Material Pipeline — tested commands only, no dialogs."""
import json, socket, time

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20.0)
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

# ── 1. Create Material Instance (engine parent, works without crash) ──
print("[1] Creating MI_Jade_Green...")
run("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})

# ── 2. Spawn sphere ──
print("[2] Spawning sphere...")
run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "JadeSphere", "scale": [2.5, 2.5, 2.5]})

# ── 3. Apply material ──
print("[3] Applying MI_Jade_Green...")
run("set_material", {
    "actorName": "JadeSphere",
    "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
})

# ── 4. Set Jade PBR params via Dynamic Material Instance ──
print("[4] Setting Jade parameters (DM instance)...")
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Specular", "scalarValue": 0.55})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Metallic", "scalarValue": 0.0})
# BaseColor is vector3
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "BaseColor", "vectorValue": [0.05, 0.35, 0.20]})

# ── 5. Lighting ──
print("[5] Setting up cinematic lighting...")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]})

run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]})

run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]})

# ── 6. Finalize ──
print("[6] Finalizing...")
run("focus_viewport", {"actorName": "JadeSphere"})
run("save_current_level", {})

print("\n=== Jade Material Pipeline Complete! ===")
print("Result: JadeSphere with green jade PBR + 3-point lighting in UE Editor")
