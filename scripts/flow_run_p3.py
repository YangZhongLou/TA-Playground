"""P3: Internal Details — Fresnel rim glow + procedural clouds texture."""
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
sphere = f"JadeP3_{tag}"
key = f"KeyP3_{tag}"
rim = f"RimP3_{tag}"

print("=== Jade P3 — Fresnel + Procedural Clouds ===\n")

# Task 1: M_Jade_P3 with Fresnel + Clouds
print("[1] M_Jade_P3 (Fresnel exponent=3.0 + procedural clouds)")
step("create M_Jade_P3", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_P3",
    "shadingModel": "subsurface_profile",
    "blendMode": "opaque",
    "baseColor": [0.05, 0.40, 0.22],
    "metallic": 0.0,
    "roughness": 0.10,
    "specular": 0.50,
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade_P3",
    "fresnelExponent": 3.0,
    "proceduralClouds": True,
    "reuse": True
})

# Task 2: MI_Jade_P3
print(f"\n[2] MI_Jade_P3")
step("create MI_P3", "create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_P3",
    "parentPath": "/Game/Materials/Master/M_Jade_P3.M_Jade_P3"
})

# Task 3: Sphere
print(f"\n[3] Sphere ({sphere})")
step("spawn", "spawn_actor", {
    "className": "StaticMeshActor", "name": sphere, "location": [0, 0, 150]
})
step("mesh", "set_static_mesh", {
    "actorName": sphere, "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
})
step("scale x2", "set_actor_transform", {
    "name": sphere, "scale": [2.0, 2.0, 2.0]
})
step("apply MI_P3", "set_material", {
    "actorName": sphere,
    "materialPath": "/Game/Materials/Instances/MI_Jade_P3.MI_Jade_P3"
})

# Tune params
for pname, ptype, pval in [
    ("BaseColor", "v", [0.05, 0.40, 0.22]),
    ("Roughness", "s", 0.10),
    ("Specular", "s", 0.50),
    ("Opacity", "s", 0.88),
]:
    params = {"actorName": sphere, "parameterName": pname}
    if ptype == "s": params["scalarValue"] = pval
    else: params["vectorValue"] = pval
    step(f"param {pname}={pval}", "set_material_parameter", params)

# Task 4: Lights
print(f"\n[4] Lights")
step("keylight", "spawn_actor", {
    "className": "DirectionalLight", "name": key,
    "location": [500, 300, 600], "mobility": "Movable"
})
step("key params", "set_light_parameters", {
    "actorName": key, "intensity": 8.0, "color": [1.0, 0.95, 0.85]
})
step("rimlight", "spawn_actor", {
    "className": "PointLight", "name": rim,
    "location": [-400, 200, 400], "mobility": "Movable"
})
step("rim params", "set_light_parameters", {
    "actorName": rim, "intensity": 3000.0, "color": [0.3, 0.5, 1.0]
})
step("auto-exp off", "run_console_command", {"cmd": "r.DefaultFeature.AutoExposure 0"})

# Task 5: Screenshot
print(f"\n[5] Focus + Screenshot")
step("focus", "focus_viewport", {"actorName": sphere})
r = step("screenshot", "take_screenshot", {"filename": f"p3_detail_{sphere}"})
if r.get("success"):
    result = r.get("result", {})
    print(f"       path={result.get('path','')} saved={result.get('saved','')}")

passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n=== P3: {passed}/{total} passed ===")
if passed != total:
    for name, ok in results:
        if not ok: print(f"  FAILED: {name}")
