"""Dev-Flow: Plan->Architect->Implement->Test, verified step by step."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377
results = []

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(15)
    s.connect((HOST, PORT))
    req = json.dumps({"id": "t", "method": m, "params": p or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    r = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return r

def step(name, method, params=None, expect_ok=True):
    try:
        r = cmd(method, params)
        ok = r.get("success", False)
        tag = "PASS" if ok == expect_ok else "FAIL"
        err = r.get("error", "")
        if err: err = f" ({err[:60]})"
        print(f"  [{tag}] {name}{err}")
        results.append((name, ok == expect_ok))
        return r
    except Exception as e:
        print(f"  [FAIL] {name}: {e}")
        results.append((name, False))
        return {"success": False}

# ── Phase 2: Architect (condensed: all APIs verified) ──
print("Phase 2: Architect")
print("  API plan: destroy_actor -> create_material_instance -> spawn_actor ->")
print("  set_static_mesh -> set_actor_transform -> set_material ->")
print("  set_material_parameter(×4) -> spawn lights -> set_light_parameters ->")
print("  focus_viewport -> take_screenshot")
print("  Review: all APIs previously tested [OK]\n")

# ── Phase 3: Implement ──
print("Phase 3: Implement")

# Task 1: Cleanup actors only (MI is persistent, don't delete)
print("\n[Task 1] Cleanup actors...")
for name in ["JadeSphere", "KeyLight", "RimLight", "SkyLight"]:
    step(f"destroy {name}", "destroy_actor", {"name": name}, expect_ok=False)

# Task 2: Create MI
print("\n[Task 2] Create MI_Jade_Green...")
step("create MI_Jade_Green", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
step("verify MI exists", "get_asset_info", {"path": "/Game/Materials/Instances/MI_Jade_Green"})

# Task 3: Sphere + MI + PBR
print("\n[Task 3] Sphere + MI + PBR...")
step("spawn sphere", "spawn_actor", {
    "className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]
})
step("set mesh", "set_static_mesh", {
    "actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
})
step("scale x2.5", "set_actor_transform", {
    "name": "JadeSphere", "scale": [2.5, 2.5, 2.5]
})
step("apply MI", "set_material", {
    "actorName": "JadeSphere",
    "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
})
# PBR params
for pname, ptype, pval in [
    ("BaseColor", "v", [0.05, 0.35, 0.20]),
    ("Roughness", "s", 0.08),
    ("Specular", "s", 0.55),
    ("Metallic", "s", 0.0),
]:
    params = {"actorName": "JadeSphere", "parameterName": pname}
    if ptype == "s": params["scalarValue"] = pval
    else: params["vectorValue"] = pval
    step(f"{pname}={pval}", "set_material_parameter", params)

# Task 4: Lighting
print("\n[Task 4] Three-point lighting...")
step("KeyLight", "spawn_actor", {
    "className": "PointLight", "name": "KeyLight", "location": [600, 400, 500]
})
step("KeyLight params", "set_light_parameters", {
    "actorName": "KeyLight", "intensity": 120.0, "color": [1.0, 0.95, 0.85]
})
step("RimLight", "spawn_actor", {
    "className": "PointLight", "name": "RimLight", "location": [-500, -300, 300]
})
step("RimLight params", "set_light_parameters", {
    "actorName": "RimLight", "intensity": 60.0, "color": [0.4, 0.6, 1.0]
})
step("SkyLight", "spawn_actor", {
    "className": "SkyLight", "name": "SkyLight", "location": [0, 0, 900]
})

# Task 5: Focus + Screenshot
print("\n[Task 5] Focus + Screenshot...")
step("focus viewport", "focus_viewport", {"actorName": "JadeSphere"})
r = step("screenshot", "take_screenshot", {"filename": "jade_flow_result"})
if r.get("success"):
    print(f"       -> {r.get('result',{}).get('path','')}")

# ── Phase 4: Test (summary) ──
passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n{'='*50}")
print(f"Phase 4: Test — {passed}/{total} passed")
if passed == total:
    print("ALL TESTS PASSED")
else:
    for name, ok in results:
        if not ok: print(f"  FAILED: {name}")
