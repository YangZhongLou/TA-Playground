"""Test UnrealMCP commands step by step, reconnecting each time."""
import json, socket, time, sys

def cmd(method, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(20.0)
    sock.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    sock.sendall(req.encode("utf-8"))
    resp = json.loads(sock.recv(65536).decode("utf-8").strip())
    sock.close()
    return resp

def test(method, params=None):
    try:
        r = cmd(method, params)
        ok = "OK" if r.get("success") else f"FAIL: {r.get('error','')[:100]}"
        print(f"  [{ok}] {method}")
        return r.get("success")
    except Exception as e:
        print(f"  [CRASH] {method}: {e}")
        return False

print("=== Step-by-step test ===\n")

# Test basic commands first
test("get_editor_info")
test("get_actor_list")

# Spawn sphere
ok = test("spawn_actor", {"className": "StaticMeshActor", "name": "TestSphere", "location": [0, 0, 200]})
if ok:
    test("set_static_mesh", {"actorName": "TestSphere", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
    test("set_actor_transform", {"name": "TestSphere", "scale": [2.0, 2.0, 2.0]})

# Try material
test("run_console_command", {"command": "Editor.CreateNewMaterial /Game/Materials/Master/M_Jade_Master"})
time.sleep(1)

# Create material instance
ok = test("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Jade_Green",
    "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
})
if not ok:
    print("  Trying fallback parent...")
    test("create_material_instance", {
        "path": "/Game/Materials/Instances/MI_Jade_Green",
        "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
    })

# Apply material
test("set_material", {"actorName": "TestSphere", "materialPath": "/Game/Materials/Instances/MI_Jade_Green.MI_Jade_Green"})

# Lights
test("spawn_actor", {"className": "PointLight", "name": "KeyLight", "location": [400, 300, 400]})
test("set_light_parameters", {"actorName": "KeyLight", "intensity": 80.0})

# Screenshot
test("focus_viewport", {"actorName": "TestSphere"})
test("take_screenshot", {"filename": "jade_test"})
test("save_current_level")

print("\nTests complete")
