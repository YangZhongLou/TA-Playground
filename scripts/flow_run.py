"""Dev-Flow pipeline — unique names, no destroy, persistent MI."""
import json, socket, time, uuid

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

tag = uuid.uuid4().hex[:4]
sphere = f"JadeSphere_{tag}"
key = f"KeyLight_{tag}"
rim = f"RimLight_{tag}"
sky = f"SkyLight_{tag}"

print("=== Jade Pipeline ===\n")

# Task 1
print("[1] MI_Jade_Green")
step("create/reuse MI", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})

# Task 2
print(f"\n[2] Sphere ({sphere}) + PBR")
step("spawn sphere", "spawn_actor", {
    "className": "StaticMeshActor", "name": sphere, "location": [0, 0, 150]
})
step("set mesh", "set_static_mesh", {
    "actorName": sphere, "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
})
step("scale x2.5", "set_actor_transform", {
    "name": sphere, "scale": [2.5, 2.5, 2.5]
})
step("apply MI", "set_material", {
    "actorName": sphere,
    "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"
})
for pname, ptype, pval in [
    ("BaseColor", "v", [0.05, 0.35, 0.20]),
    ("Roughness", "s", 0.08),
    ("Specular", "s", 0.55),
    ("Metallic", "s", 0.0),
]:
    params = {"actorName": sphere, "parameterName": pname}
    if ptype == "s": params["scalarValue"] = pval
    else: params["vectorValue"] = pval
    step(f"{pname}={pval}", "set_material_parameter", params)

# Task 3
print(f"\n[3] Lights ({key}, {rim}, {sky})")
step("KeyLight", "spawn_actor", {
    "className": "PointLight", "name": key, "location": [600, 400, 500]
})
step("KeyLight params", "set_light_parameters", {
    "actorName": key, "intensity": 120.0, "color": [1.0, 0.95, 0.85]
})
step("RimLight", "spawn_actor", {
    "className": "PointLight", "name": rim, "location": [-500, -300, 300]
})
step("RimLight params", "set_light_parameters", {
    "actorName": rim, "intensity": 60.0, "color": [0.4, 0.6, 1.0]
})
step("SkyLight", "spawn_actor", {
    "className": "SkyLight", "name": sky, "location": [0, 0, 900]
})

# Task 4
print(f"\n[4] Focus + Screenshot")
step("focus viewport", "focus_viewport", {"actorName": sphere})
r = step("screenshot", "take_screenshot", {"filename": f"jade_{sphere}"})
if r.get("success"):
    result = r.get("result", {})
    print(f"       path={result.get('path','')} saved={result.get('saved','')}")

passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n=== {passed}/{total} passed ===")
if passed != total:
    for name, ok in results:
        if not ok: print(f"  FAILED: {name}")
