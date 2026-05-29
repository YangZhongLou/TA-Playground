"""Phase 4 Tests — verify all MCP commands work correctly."""
import json, socket, time

HOST, PORT = "127.0.0.1", 13377
passed = 0
failed = 0

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    s.connect((HOST, PORT))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    resp = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return resp

def check(name, method, params=None, expect_ok=True):
    global passed, failed
    try:
        r = cmd(method, params)
        ok = r.get("success", False)
        if ok == expect_ok:
            print(f"  PASS: {name} -> success={ok}")
            passed += 1
        else:
            print(f"  FAIL: {name} -> expected success={expect_ok}, got {ok}: {r.get('error','')}")
            failed += 1
        return r
    except Exception as e:
        print(f"  FAIL: {name} -> {e}")
        failed += 1
        return {"success": False}

# Wait
print("Connecting to UE...")
for i in range(10):
    try:
        s = socket.socket(); s.settimeout(1); s.connect((HOST, PORT)); s.close()
        break
    except: time.sleep(2)

# ── Test 1: create_material ──
print("\n=== Test 1: create_material ===")
check("valid path", "create_material", {"path": "/Game/Materials/Master/Test_Mat"})
check("duplicate path (should fail)", "create_material", {"path": "/Game/Materials/Master/Test_Mat"}, expect_ok=False)

# ── Test 2: create_material_instance ──
print("\n=== Test 2: create_material_instance ===")
check("MIC valid", "create_material_instance", {"path": "/Game/Materials/Instances/Test_MIC", "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"})
check("MIC reuse existing", "create_material_instance", {"path": "/Game/Materials/Instances/Test_MIC", "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"})
check("MIC bad parent", "create_material_instance", {"path": "/Game/Materials/Instances/BadMIC", "parentPath": "/Game/Nope/Nope.Nope"}, expect_ok=False)

# ── Test 3: set_material_parameter ──
print("\n=== Test 3: set_material_parameter ===")
# Spawn actor, apply MIC, then set params
check("spawn test actor", "spawn_actor", {"className": "StaticMeshActor", "name": "TestActor", "location": [0, 0, 100]})
check("set mesh", "set_static_mesh", {"actorName": "TestActor", "meshPath": "/Engine/BasicShapes/Sphere.Sphere"})
check("apply MIC", "set_material", {"actorName": "TestActor", "materialPath": "/Game/Materials/Instances/Test_MIC.Test_MIC"})
check("set scalar param", "set_material_parameter", {"actorName": "TestActor", "parameterName": "Roughness", "scalarValue": 0.5})
check("set vector param", "set_material_parameter", {"actorName": "TestActor", "parameterName": "BaseColor", "vectorValue": [0.1, 0.5, 0.3]})
check("nonexistent actor", "set_material_parameter", {"actorName": "NopeActor", "parameterName": "Roughness", "scalarValue": 0.5}, expect_ok=False)

# ── Test 4: save_current_level ──
print("\n=== Test 4: save_current_level ===")
r = check("save level (auto-name temp)", "save_current_level", {})
# Verify it's saved by checking get_current_level
check("verify saved path", "get_current_level", {})

# ── Summary ──
total = passed + failed
print(f"\n{'='*40}")
print(f"Results: {passed}/{total} passed, {failed} failed")
if failed == 0:
    print("ALL TESTS PASSED")
else:
    print(f"{failed} TESTS FAILED")
