import json, socket, time

def cmd(m, p=None):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": m, "params": p or {}}) + "\n"
    s.sendall(req.encode("utf-8"))
    r = json.loads(s.recv(65536).decode("utf-8").strip())
    s.close()
    return r

print("create_material...")
r = cmd("create_material", {"path": "/Game/Materials/Master/MT"})
print(f"  success={r.get('success')}")
print("waiting 3s..."); time.sleep(3)

print("create_material_instance...")
r = cmd("create_material_instance", {"path": "/Game/Materials/Instances/MIT", "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"})
print(f"  success={r.get('success')}")
print("waiting 3s..."); time.sleep(3)

print("Done — check UE for dialogs")
