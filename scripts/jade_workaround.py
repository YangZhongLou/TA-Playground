"""Jade Material pipeline — avoiding crash-prone create_material_instance.
Uses set_material + set_material_parameter for dynamic material instances."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377

def cmd(method, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(20.0)
    sock.connect((HOST, PORT))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    sock.sendall(req.encode("utf-8"))
    resp = json.loads(sock.recv(65536).decode("utf-8").strip())
    sock.close()
    return resp

def run(method, params=None):
    try:
        r = cmd(method, params)
        ok = "OK" if r.get("success") else f"FAIL: {r.get('error','')[:80]}"
        print(f"  [{ok}] {method}")
        return r
    except Exception as e:
        print(f"  [ERR] {method}: {e}")
        return {"success": False, "error": str(e)}

# Wait for port
print("Waiting for UE...")
for i in range(60):
    try:
        s = socket.socket(); s.settimeout(1); s.connect((HOST, PORT)); s.close()
        print("UE ready!\n")
        break
    except:
        time.sleep(2)

# ── Create materials via console ──
print("=== Create Master Material ===")
run("run_console_command", {"command": "Editor.CreateNewMaterial /Game/Materials/Master/M_Jade_Master"})
time.sleep(1)

# ── Spawn sphere ──
print("\n=== Setup Display ===")
run("spawn_actor", {"className": "StaticMeshActor", "name": "JadeSphere", "location": [0, 0, 150]})
run("set_static_mesh", {"actorName": "JadeSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
run("set_actor_transform", {"name": "JadeSphere", "scale": [2.0, 2.0, 2.0]})

# Apply engine default material to sphere
print("\n=== Apply Base Material ===")
r = run("set_material", {
    "actorName": "JadeSphere",
    "materialPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})

# Set material parameters (creates Dynamic Material Instance)
print("\n=== Set Jade Parameters ===")
params = [
    ("Roughness", "scalar", 0.08),
    ("Specular", "scalar", 0.5),
    ("Metallic", "scalar", 0.0),
    ("BaseColor", "vector", [0.05, 0.35, 0.20]),
]
for name, ptype, val in params:
    if ptype == "scalar":
        run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": name, "scalarValue": val})
    else:
        run("set_material_parameter", {"actorName": "JadeSphere", "parameterName": name, "vectorValue": val})

# ── Lights ──
print("\n=== Lighting ===")
run("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [500, 300, 400]})
run("set_light_parameters", {"actorName": "KeyLight", "intensity": 80.0, "color": [1.0, 0.95, 0.85]})

run("spawn_actor", {"className": "PointLight", "name": "RimLight", "location": [-400, -200, 300]})
run("set_light_parameters", {"actorName": "RimLight", "intensity": 40.0, "color": [0.4, 0.7, 1.0]})

run("spawn_actor", {"className": "SkyLight", "name": "SkyLight", "location": [0, 0, 800]})
run("set_light_parameters", {"actorName": "SkyLight", "intensity": 5.0})

# ── Finalize ──
print("\n=== Finalize ===")
run("focus_viewport", {"actorName": "JadeSphere"})
r = run("take_screenshot", {"filename": "jade_material_result"})
print(f"  Screenshot: {r.get('result', r.get('error',''))}")
run("save_current_level", {})

print("\nJade Material Pipeline Complete!")
