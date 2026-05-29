"""Final Jade Material Pipeline — all steps with fixed create_material_instance."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(30.0)
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

# ── Step 1: Create M_Jade_Master via console ──
print("\n[1] Creating M_Jade_Master...")
run("run_console_command", {"command": "Editor.CreateNewMaterial /Game/Materials/Master/M_Jade_Master"})
time.sleep(2)

# ── Step 2: Create MI_Jade_Green (fixed create_material_instance) ──
print("\n[2] Creating MI_Jade_Green (constant instance)...")
r = run("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})

# ── Step 3: Spawn sphere ──
print("\n[3] Spawning sphere...")
run("spawn_actor", {"className": "StaticMeshActor", "name": "Sphere_Jade", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "Sphere_Jade", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "Sphere_Jade", "scale": [2.0, 2.0, 2.0]})

# ── Step 4: Apply MI and set PBR params ──
print("\n[4] Applying MI_Jade_Green and tuning PBR...")
run("set_material", {"actorName": "Sphere_Jade", "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"})

# Set base params via MID (works on any parent material)
jade_base_color = [0.05, 0.35, 0.20]
run("set_material_parameter", {"actorName": "Sphere_Jade", "parameterName": "BaseColor", "vectorValue": jade_base_color})
run("set_material_parameter", {"actorName": "Sphere_Jade", "parameterName": "Roughness", "scalarValue": 0.08})
run("set_material_parameter", {"actorName": "Sphere_Jade", "parameterName": "Specular", "scalarValue": 0.55})
run("set_material_parameter", {"actorName": "Sphere_Jade", "parameterName": "Metallic", "scalarValue": 0.0})

# ── Step 5: Lighting ──
print("\n[5] Setting up cinematic lighting...")
run("spawn_actor", {"className": "PointLight", "name": "Key", "location": [500, 300, 400]})
run("set_light_parameters", {"actorName": "Key", "intensity": 100.0, "color": [1.0, 0.95, 0.85]})

run("spawn_actor", {"className": "PointLight", "name": "Rim", "location": [-400, -300, 250]})
run("set_light_parameters", {"actorName": "Rim", "intensity": 60.0, "color": [0.3, 0.6, 1.0]})

# ── Step 6: Finalize ──
print("\n[6] Finalizing...")
run("focus_viewport", {"actorName": "Sphere_Jade"})
# Take screenshot via console
r = run("run_console_command", {"command": "Editor.TakeScreenshotAtMaxResolution jade_final"})
# Or the regular take_screenshot
run("take_screenshot", {"filename": "jade_final"})

r = run("get_actor_list", {})
print(f"  Scene actors: {r.get('result','')}")

print("\n=== Jade Material Pipeline Complete! ===")
