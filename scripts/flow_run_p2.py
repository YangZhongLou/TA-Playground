"""P2: Subsurface Scattering — M_Jade_SSS + MI_Jade_SSS + Backlight + Screenshot."""
import json, socket, uuid

HOST, PORT = "127.0.0.1", 13377
results = []

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20)
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
sphere = f"JadeSSS_{tag}"
back = f"BackLight_{tag}"
key = f"KeySSS_{tag}"

print("=== Jade P2 — Subsurface Scattering ===\n")

# Task 1: Create SubsurfaceProfile + M_Jade_SSS
print("[1] M_Jade_SSS (subsurface_profile + SubsurfaceProfile)")
step("create M_Jade_SSS", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_SSS",
    "shadingModel": "subsurface_profile",
    "blendMode": "opaque",
    "baseColor": [0.05, 0.40, 0.22],
    "metallic": 0.0,
    "roughness": 0.12,
    "specular": 0.45,
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade",
    "reuse": True
})
r = cmd("get_asset_info", {"path": "/Game/Materials/Master/M_Jade_SSS"})
if r.get("success"):
    print(f"       M_Jade_SSS: {r['result'].get('class','?')}")

# Task 2: Create MI_Jade_SSS
print(f"\n[2] MI_Jade_SSS (parent = M_Jade_SSS)")
step("create MI_SSS", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_SSS",
    "parentPath": "/Game/Materials/Master/M_Jade_SSS.M_Jade_SSS"
})

# Task 3: Sphere + Material
print(f"\n[3] Sphere ({sphere}) + SSS Material")
step("spawn", "spawn_actor", {
    "className": "StaticMeshActor", "name": sphere, "location": [0, 0, 150]
})
step("mesh", "set_static_mesh", {
    "actorName": sphere, "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
})
step("scale x2", "set_actor_transform", {
    "name": sphere, "scale": [2.0, 2.0, 2.0]
})
step("apply MI_SSS", "set_material", {
    "actorName": sphere,
    "materialPath": "/Game/Materials/Instances/MI_Jade_SSS.MI_Jade_SSS"
})
# Tune opacity for SSS depth
for pname, ptype, pval in [
    ("BaseColor", "v", [0.05, 0.40, 0.22]),
    ("Roughness", "s", 0.12),
    ("Specular", "s", 0.45),
    ("Metallic", "s", 0.0),
    ("Opacity", "s", 0.85),
]:
    params = {"actorName": sphere, "parameterName": pname}
    if ptype == "s": params["scalarValue"] = pval
    else: params["vectorValue"] = pval
    step(f"param {pname}={pval}", "set_material_parameter", params)

# Task 4: Backlight setup — strong light behind sphere
print(f"\n[4] Backlight ({back}) + Key ({key})")
step("backlight", "spawn_actor", {
    "className": "PointLight", "name": back,
    "location": [0, 0, -600], "mobility": "Movable"
})
step("backlight params", "set_light_parameters", {
    "actorName": back, "intensity": 8000.0, "color": [1.0, 0.95, 0.85]
})
step("keylight", "spawn_actor", {
    "className": "DirectionalLight", "name": key,
    "location": [500, 300, 600], "mobility": "Movable"
})
step("keylight params", "set_light_parameters", {
    "actorName": key, "intensity": 8.0, "color": [1.0, 0.95, 0.85]
})
# Auto-exposure off
step("auto-exp off", "run_console_command", {"cmd": "r.DefaultFeature.AutoExposure 0"})

# Task 5: Screenshot
print(f"\n[5] Focus + Screenshot")
step("focus", "focus_viewport", {"actorName": sphere})
r = step("screenshot", "take_screenshot", {"filename": f"p2_sss_{sphere}"})
if r.get("success"):
    result = r.get("result", {})
    print(f"       path={result.get('path','')} saved={result.get('saved','')}")

passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n=== P2: {passed}/{total} passed ===")
if passed != total:
    for name, ok in results:
        if not ok: print(f"  FAILED: {name}")
