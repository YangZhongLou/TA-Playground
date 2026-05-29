"""Jade Material Pipeline v2 — uses new create_material command (no dialogs)."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20.0)
    s.connect((HOST, PORT))
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

# Wait
print("Waiting for UE...")
for i in range(30):
    try:
        s = socket.socket(); s.settimeout(1); s.connect((HOST, PORT)); s.close()
        break
    except: time.sleep(2)

# ── Step 1: Create M_Jade_Master (new command, no dialog) ──
print("\n[1] Creating M_Jade_Master (no dialog)...")
r = run("create_material", {"path": "/Game/Materials/Master/M_Jade_Master"})
print(f"  -> {r.get('result','')}")

# ── Step 2: Create MI_Jade_Green ──
print("\n[2] Creating MI_Jade_Green...")
r = run("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
})
if not r.get("success"):
    print("  Fallback: engine DefaultMaterial")
    r = run("create_material_instance", {
        "path": "/Game/Materials/Instances/MI_Jade_Green",
        "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
    })

# ── Step 3: Spawn sphere ──
print("\n[3] Spawning display sphere...")
run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "JadeSphere", "scale": [2.0, 2.0, 2.0]})

# ── Step 4: Apply material and set Jade PBR params ──
print("\n[4] Applying material + PBR parameters...")
run("set_material", {
    "actorName": "JadeSphere",
    "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
})
# Set via Dynamic Material Instance
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "BaseColor", "vectorValue": [0.05, 0.35, 0.20]})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Specular", "scalarValue": 0.55})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Metallic", "scalarValue": 0.0})

# ── Step 5: Cinematic lighting ──
print("\n[5] Setting up lights...")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [500, 300, 400]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 100.0, "color": [1.0, 0.95, 0.85]})

run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-400, -300, 250]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 60.0, "color": [0.3, 0.6, 1.0]})

# SkyLight intensity via console (set_light_parameters doesn't handle SkyLightComponent)
run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 800]})

# ── Step 6: Finalize ──
print("\n[6] Finalize...")
run("focus_viewport", {"actorName": "JadeSphere"})
run("save_current_level", {})

print("\n=== Jade Material Pipeline v2 Complete! ===")
print("Check UE Editor for: JadeSphere with green jade material + 3-point lighting")
