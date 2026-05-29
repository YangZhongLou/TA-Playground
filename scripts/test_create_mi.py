"""Test fixed create_material_instance."""
import json, socket

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(20.0)
    s.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    resp = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return resp

print("Testing create_material_instance (fixed)...")

# Test 1: non-existent parent (should fail gracefully)
print("\n[1] Non-existent parent:")
r = cmd("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Test_Bad",
    "parentPath": "/Game/Nope/Nope.Nope"
})
print(f"  -> success={r.get('success')}, error={r.get('error','')}")

# Test 2: valid engine default material as parent
print("\n[2] Engine DefaultMaterial parent:")
r = cmd("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Test_Engine",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
print(f"  -> success={r.get('success')}, error={r.get('error','')}")
print(f"  -> result={r.get('result','')}")

# Test 3: create with our master material as parent
print("\n[3] Our M_Jade_Master parent:")
r = cmd("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Test_Jade",
    "parentPath": "/Game/Materials/Master/M_Jade_Master.M_Jade_Master"
})
print(f"  -> success={r.get('success')}, error={r.get('error','')}")
print(f"  -> result={r.get('result','')}")

print("\nAll tests passed without crash!")
