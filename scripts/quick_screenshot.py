"""Quick test: screenshot and save level"""
import json, socket, time

def cmd(method, params=None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(30.0)
    sock.connect(("127.0.0.1", 13377))
    req = json.dumps({"id": "t", "method": method, "params": params or {}}) + "\n"
    sock.sendall(req.encode("utf-8"))
    resp = json.loads(sock.recv(65536).decode("utf-8").strip())
    sock.close()
    return resp

# Wait for UE
print("Waiting...")
for i in range(30):
    try:
        s = socket.socket(); s.settimeout(1); s.connect(("127.0.0.1", 13377)); s.close()
        break
    except: time.sleep(2)

print("Connected!\n")

# Screenshot via console
print("Taking screenshot...")
r = cmd("run_console_command", {"command": "HighResShot 2x2"})
print(f"  HighResShot: {r}")

# Or get viewport camera info
r = cmd("get_viewport_camera", {})
print(f"  Camera: {r}")

# Try playing in editor briefly to trigger render
print("\nChecking level...")
r = cmd("get_current_level", {})
print(f"  Level: {r}")

# Try save again with longer timeout
print("\nSaving level...")
try:
    r = cmd("save_current_level", {})
    print(f"  Save: {r}")
except Exception as e:
    print(f"  Save error: {e}")

print("\nDone")
