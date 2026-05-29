"""M2: Expression System — Lerp + OneMinus + TextureSample demo."""
import json, socket, uuid

HOST, PORT = "127.0.0.1", 13377
results = []

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM); s.settimeout(20)
    s.connect((HOST, PORT))
    req = json.dumps({"id": "t", "method": m, "params": p or {}}) + "\n"
    s.sendall(req.encode("utf-8")); r = json.loads(s.recv(65536).decode("utf-8").strip()); s.close()
    return r

def step(name, method, params=None, expect_ok=True):
    try:
        r = cmd(method, params); ok = r.get("success", False)
        tag = "PASS" if ok == expect_ok else "FAIL"
        err = r.get("error", "");
        if err: err = f" ({err[:60]})"
        print(f"  [{tag}] {name}{err}")
        results.append((name, ok == expect_ok)); return r
    except Exception as e:
        print(f"  [FAIL] {name}: {e}"); results.append((name, False)); return {"success": False}

tag = uuid.uuid4().hex[:4]

print("=== M2 — Advanced Material Expressions ===\n")

# P1: Lerp blend between two jade colors
print("[P1] Lerp: blend Dark Jade → Light Jade by Roughness map")
step("M_Jade_Lerp", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_Lerp",
    "shadingModel": "subsurface_profile",
    "baseColor": [0.02, 0.30, 0.15],       # A: dark green jade
    "lerpColorB": [0.70, 0.85, 0.65],       # B: pale green highlight
    "lerpAlpha": 0.40,                       # blend factor
    "roughness": 0.10,
    "specular": 0.50,
    "fresnelExponent": 3.0,
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade_Lerp",
    "reuse": True
})

# P2: OneMinus for roughness mapping
print("\n[P2] OneMinus: convert Smoothness → Roughness")
step("M_Jade_OneMinus", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_OneMinus",
    "shadingModel": "subsurface_profile",
    "baseColor": [0.05, 0.42, 0.24],
    "metallic": 0.0,
    "oneMinusValue": 0.08,              # 0.08 smoothness → 0.92 roughness (matte)
    "specular": 0.50,
    "fresnelExponent": 4.0,
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade_OM",
    "reuse": True
})

# P3: TextureSample with engine default noise texture
print("\n[P3] TextureSample: noise texture on jade material")
step("M_Jade_Tex", "create_material", {
    "path": "/Game/Materials/Master/M_Jade_Tex",
    "shadingModel": "subsurface_profile",
    "baseColor": [0.05, 0.40, 0.22],
    "roughness": 0.10,
    "specular": 0.50,
    "texture": "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq.Good64x64TilingNoiseHighFreq",
    "subsurfaceProfile": "/Game/Materials/Profiles/SP_Jade_Tex",
    "reuse": True
})

# Create MIs and spheres for each
print("\n[P4] Create MIs + Spheres for all 3")
mi_list = [
    ("MI_Jade_Lerp", "M_Jade_Lerp", "JLerp", [-250, 0, 150]),
    ("MI_Jade_OneMinus", "M_Jade_OneMinus", "JOM", [0, 0, 150]),
    ("MI_Jade_Tex", "M_Jade_Tex", "JTex", [250, 0, 150]),
]
for mi, master, sname, loc in mi_list:
    step(f"MI {mi}", "create_material_instance", {
        "path": f"/Game/Materials/Instances/{mi}",
        "parentPath": f"/Game/Materials/Master/{master}.{master}"
    })
    sphere_name = f"{sname}_{tag}"
    step(f"sphere {sname}", "spawn_actor", {
        "className": "StaticMeshActor", "name": sphere_name, "location": loc
    })
    step(f"mesh {sname}", "set_static_mesh", {
        "actorName": sphere_name, "meshPath": "/Engine/BasicShapes/Sphere.Sphere"
    })
    step(f"scale {sname}", "set_actor_transform", {
        "name": sphere_name, "scale": [1.8, 1.8, 1.8]
    })
    step(f"mat {sname}", "set_material", {
        "actorName": sphere_name,
        "materialPath": f"/Game/Materials/Instances/{mi}.{mi}"
    })

# Lights
print("\n[Lights]")
step("sun", "spawn_actor", {
    "className": "DirectionalLight", "name": f"Sun_{tag}",
    "location": [0, 400, 800], "mobility": "Movable"
})
step("sun params", "set_light_parameters", {
    "actorName": f"Sun_{tag}", "intensity": 10.0, "color": [1.0, 0.98, 0.90]
})
step("auto-exp off", "run_console_command", {"cmd": "r.DefaultFeature.AutoExposure 0"})

# Screenshot
print(f"\n[Screenshot]")
step("focus", "focus_viewport", {"actorName": f"JOM_{tag}"})
r = step("screen", "take_screenshot", {"filename": f"m2_demo_{tag}"})
if r.get("success"):
    print(f"  path={r['result'].get('path','')} saved={r['result'].get('saved','')}")

passed = sum(1 for _, ok in results if ok)
total = len(results)
print(f"\n=== M2: {passed}/{total} passed ===")
for name, ok in results:
    if not ok: print(f"  FAILED: {name}")
