"""Dev-Flow pipeline — custom M_Jade_Master + MI_Jade_Green + Sphere + Lights + Screenshot."""
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
sun = f"SunLight_{tag}"

print("=== Jade P1 — Custom M_Jade_Master ===\n")

# Task 0: Create M_Jade_Master with PBR nodes
print("[0] M_Jade_Master (custom parent material)")
step("create M_Jade_Master", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_Master",
    "shadingModel": "default_lit",
    "blendMode": "opaque",
    "baseColor": [0.05, 0.35, 0.20],
    "metallic": 0.0,
    "roughness": 0.08,
    "specular": 0.55,
    "reuse": True
})
r = step("verify M_Jade_Master exists", "get_asset_info", {
    "path": "/Game/Materials/Master/M_Jade_Master"
})
if r.get("success"):
    info = r.get("result", {})
    print(f"       class={info.get('class','?')} path={info.get('path','?')}")

# Task 1: Create MI_Jade_Green from M_Jade_Master
print(f"\n[1] MI_Jade_Green (parent = M_Jade_Master)")
step("create MI", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
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

# Task 3: Lights — no SkyLight (captures empty scene = black)
# Use DirectionalLight (sun) + PointLights for 3-point setup
print(f"\n[3] Lights ({sun}, {key}, {rim})")
step("SunLight", "spawn_actor", {
    "className": "DirectionalLight", "name": sun, "location": [800, 400, 800],
    "mobility": "Movable"
})
step("SunLight params", "set_light_parameters", {
    "actorName": sun, "intensity": 15.0, "color": [1.0, 0.95, 0.85]
})
step("SunLight angle", "set_actor_transform", {
    "name": sun, "rotation": [-45, -30, 0]
})

step("KeyLight", "spawn_actor", {
    "className": "PointLight", "name": key, "location": [600, 400, 500],
    "mobility": "Movable"
})
step("KeyLight params", "set_light_parameters", {
    "actorName": key, "intensity": 5000.0, "color": [1.0, 0.95, 0.85]
})
step("RimLight", "spawn_actor", {
    "className": "PointLight", "name": rim, "location": [-500, -300, 300],
    "mobility": "Movable"
})
step("RimLight params", "set_light_parameters", {
    "actorName": rim, "intensity": 3000.0, "color": [0.4, 0.6, 1.0]
})

# Fix auto-exposure which can make scene black
step("auto-exposure off", "run_console_command", {
    "cmd": "r.DefaultFeature.AutoExposure 0"
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
