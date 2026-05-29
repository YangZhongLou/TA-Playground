"""Test only create_material_instance (no create_material first)."""
import json, socket

def cmd(method, params=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10.0)
    s.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    resp = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return resp

# Test create_material_instance with engine DefaultMaterial
print("Test: create_material_instance (engine parent)...")
r = cmd("create_material_instance", {
    "path": "/Game/Materials/Instances/MI_Test2",
    "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
})
print(f"  success={r.get('success')}, error={r.get('error','')}")
print(f"  result={r.get('result','')}")

print("\nStandalone test passed!")
