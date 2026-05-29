"""P4: Jade Variant Showcase — White/Emerald/Amethyst side by side."""
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

# Variant presets: (name, baseColor, roughness, specular, x_offset)
variants = [
    ("MI_Jade_White",   "白玉", [0.88, 0.90, 0.82], 0.06, 0.35, -300),
    ("MI_Jade_Emerald", "翡翠", [0.05, 0.42, 0.24], 0.10, 0.50, 0),
    ("MI_Jade_Amethyst","紫罗兰", [0.55, 0.15, 0.45], 0.08, 0.40, 300),
]

print("=== Jade P4 — Variant Showcase ===\n")

# Task 1: Ensure M_Jade_P3 exists (reuse from P3)
print("[1] Confirm M_Jade_P3 master")
step("ensure M_Jade_P3", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_P3",
    "shadingModel": "subsurface_profile",
    "baseColor": [0.05, 0.40, 0.22],
    "roughness": 0.10,
    "specular": 0.50,
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade_P3",
    "fresnelExponent": 3.0,
    "proceduralClouds": True,
    "reuse": True
})

# Task 2: Create all MIs + spheres
spheres = []
for mi_name, label, bc, rough, spec, x in variants:
    print(f"\n[2] {label} ({mi_name}) at X={x}")
    # Create MI
    step(f"MI {mi_name}", "create_material_instance", {
        "path": f"/Game/Materials/Instances/{mi_name}",
        "parentPath": "/Game/Materials/Master/M_Jade_P3.M_Jade_P3"
    })
    # Sphere
    sname = f"Sphere_{mi_name}_{tag}"
    step(f"spawn {label}", "spawn_actor", {
        "className": "StaticMeshActor", "name": sname,
        "location": [x, 0, 150]
    })
    step(f"mesh {label}", "set_static_mesh", {
        "actorName": sname, "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
    })
    step(f"scale {label}", "set_actor_transform", {
        "name": sname, "scale": [1.8, 1.8, 1.8]
    })
    step(f"apply {label}", "set_material", {
        "actorName": sname,
        "materialPath": f"/Game/Materials/Instances/{mi_name}.{mi_name}"
    })
    # Variant-specific params
    for pname, ptype, pval in [
        ("BaseColor", "v", bc),
        ("Roughness", "s", rough),
        ("Specular", "s", spec),
        ("Opacity", "s", 0.88),
    ]:
        params = {"actorName": sname, "parameterName": pname}
        if ptype == "s": params["scalarValue"] = pval
        else: params["vectorValue"] = pval
        step(f"{label} {pname}", "set_material_parameter", params)
    spheres.append(sname)

# Task 3: Scene lighting
print(f"\n[3] Scene lighting")
step("sun", "spawn_actor", {
    "className": "DirectionalLight", "name": f"Sun_{tag}",
    "location": [0, 400, 800], "mobility": "Movable"
})
step("sun params", "set_light_parameters", {
    "actorName": f"Sun_{tag}", "intensity": 10.0, "color": [1.0, 0.98, 0.90]
})
step("back rim", "spawn_actor", {
    "className": "PointLight", "name": f"BackRim_{tag}",
    "location": [0, -400, 200], "mobility": "Movable"
})
step("back rim params", "set_light_parameters", {
    "actorName": f"BackRim_{tag}", "intensity": 5000.0, "color": [0.5, 0.7, 1.0]
})
step("auto-exp off", "run_console_command", {"cmd": "r.DefaultFeature.AutoExposure 0"})

# Task 4: Screenshot — focus on center sphere
print(f"\n[4] Focus + Screenshot")
step("focus", "focus_viewport", {"actorName": spheres[1]})  # center = emerald
r = step("screenshot", "take_screenshot", {"filename": f"p4_showcase_{tag}"})
if r.get("success"):
    result = r.get("result", {})
    print(f"       path={result.get('path','')} saved={result.get('saved','')}")

passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n=== P4: {passed}/{total} passed ===")
if passed != total:
    for name, ok in results:
        if not ok: print(f"  FAILED: {name}")
