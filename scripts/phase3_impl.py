"""Phase 3: Implement — execute each task, verify result."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15)
    s.connect((HOST, PORT))
    req = json.dumps({"id": "t", "method": m, "params": p or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    r = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return r

def verify(name, method, params=None, expect_ok=True):
    try:
        r = cmd(method, params)
        ok = r.get("success", False)
        result = r.get("result", "")
        if ok == expect_ok:
            print(f"  [PASS] {name}")
            return True, result
        else:
            print(f"  [FAIL] {name}: expected success={expect_ok}, got {ok}: {r.get('error','')}")
            return False, None
    except Exception as e:
        print(f"  [FAIL] {name}: {e}")
        return False, None

print("Phase 3: Implement\n")

# ── Task #14: Create MI_Jade_Green ──
print("[Task #14] Create MI_Jade_Green")
passed, result = verify("create material instance", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
if not passed:
    print("  ABORT: Cannot create MI")
    exit(1)
# Verify it exists
passed, _ = verify("verify MI exists in asset registry", "get_asset_info", {
    "path": "/Game/Materials/Instances/MI_Jade_Green"
})
mi_path = "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"

# ── Task #15: Spawn sphere + apply MI ──
print("\n[Task #15] Spawn sphere + apply MI")
# First destroy existing JadeSphere to prevent name conflict crash
verify("destroy old JadeSphere", "destroy_actor", {"name": "JadeSphere"}, expect_ok=False)
verify("destroy old KeyLight", "destroy_actor", {"name": "KeyLight"}, expect_ok=False)
verify("destroy old RimLight", "destroy_actor", {"name": "RimLight"}, expect_ok=False)
verify("destroy old SkyLight", "destroy_actor", {"name": "SkyLight"}, expect_ok=False)
passed, _ = verify("spawn sphere", "spawn_actor", {
    "className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]
})
passed, _ = verify("set sphere mesh", "set_static_mesh", {
    "actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
})
passed, _ = verify("scale sphere", "set_actor_transform", {
    "name": "JadeSphere", "scale": [2.5, 2.5, 2.5]
})
passed, _ = verify("apply MI to sphere", "set_material", {
    "actorName": "JadeSphere", "materialPath": mi_path
})

# ── Task #16: Set PBR params ──
print("\n[Task #16] Set Jade PBR parameters")
params = [
    ("BaseColor", "v", [0.05, 0.35, 0.20]),
    ("Roughness", "s", 0.08),
    ("Specular", "s", 0.55),
    ("Metallic", "s", 0.0),
]
for name, ptype, val in params:
    p = {"actorName": "JadeSphere", "parameterName": name}
    if ptype == "s": p["scalarValue"] = val
    else: p["vectorValue"] = val
    passed, _ = verify(f"set {name}={val}", "set_material_parameter", p)

# ── Task #17: Lighting ──
print("\n[Task #17] Setup three-point lighting")
passed, _ = verify("KeyLight", "spawn_actor", {
    "className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]
})
passed, _ = verify("KeyLight intensity", "set_light_parameters", {
    "actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]
})
passed, _ = verify("RimLight", "spawn_actor", {
    "className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]
})
passed, _ = verify("RimLight intensity", "set_light_parameters", {
    "actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]
})
passed, _ = verify("SkyLight", "spawn_actor", {
    "className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]
})

# ── Task #18: Focus ──
print("\n[Task #18] Focus viewport")
passed, _ = verify("focus on JadeSphere", "focus_viewport", {
    "actorName": "JadeSphere"
})

print(f"\n=== Phase 3 Complete ===")
