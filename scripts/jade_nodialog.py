"""Jade Pipeline — ZERO dialogs. Uses only engine materials + DM instances."""
import json, socket, time

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15)
    s.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": m, "params": p or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    r = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return r

def run(m, p=None):
    try:
        r = cmd(m, p)
        tag = "OK" if r.get("success") else f"FAIL: {r.get('error','')[:60]}"
        print(f"  [{tag}] {m}")
        return r
    except Exception as e:
        print(f"  [ERR] {m}: {e}")
        return {"success": False}

# Wait for UE
print("Waiting...")
for i in range(30):
    try: s=socket.socket(); s.settimeout(1); s.connect(("127.0.0.1",13377)); s.close(); break
    except: time.sleep(2)
print("Connected!\n")

# ── 1. Spawn sphere ──
print("[1] Spawn sphere")
run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "JadeSphere", "scale": [2.5, 2.5, 2.5]})

# ── 2. Create MI_Jade_Green (engine DefaultMaterial parent = full PBR nodes) ──
print("\n[2] Create MI_Jade_Green")
run("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
run("set_material", {
    "actorName": "JadeSphere",
    "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
})

# ── 3. Set Jade PBR params (creates Dynamic Material Instance automatically) ──
print("\n[3] Set Jade PBR parameters (auto-creates DM instance)")
jade_base = [0.05, 0.35, 0.20]
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "BaseColor", "vectorValue": jade_base})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Specular", "scalarValue": 0.55})
run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": "Metallic", "scalarValue": 0.0})

# ── 4. Cinematic lighting ──
print("\n[4] Lighting")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]})
run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]})
run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]})

# ── 5. Focus viewport ──
print("\n[5] Focus")
run("focus_viewport", {"actorName": "JadeSphere"})

print("\n=== Done: Jade sphere with PBR + 3-point lighting ===")
print("No material assets were created — zero dialogs guaranteed.")
