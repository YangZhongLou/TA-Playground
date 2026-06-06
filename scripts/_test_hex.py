import json, socket, sys, time, os

def cmd(m, p, timeout=15.0):
    payload = json.dumps({'jsonrpc':'2.0','method':m,'params':p,'id':'t'}).encode()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(timeout); s.connect(('127.0.0.1',13377)); s.sendall(payload)
        return json.loads(s.recv(8192).decode(errors='replace'))

# Try spawning via console command
print('=== hex.SpawnPrism ===')
r = cmd('execute_editor_command', {'command': 'hex.SpawnPrism 150 250 0 0 300'})
print(json.dumps(r, indent=2, ensure_ascii=False))
time.sleep(0.5)

# Screenshot
screenshot_dir = 'D:/Mine/unreal_projects/TA-Playground/Saved/Screenshots'
os.makedirs(screenshot_dir, exist_ok=True)
screenshot_path = screenshot_dir + '/hex_test.png'
print('\n=== Screenshot -> ' + screenshot_path + ' ===')
r = cmd('take_screenshot', {'path': screenshot_path, 'width': 1920, 'height': 1080}, timeout=30.0)
print(json.dumps(r, indent=2, ensure_ascii=False))

# Check actors
print('\n=== Actor List (hex only) ===')
r = cmd('get_actor_list', {})
actors = r.get('result', {}).get('actors', [])
hex_actors = [a for a in actors if 'Hex' in a.get('class','') or 'Hex' in a.get('name','') or 'Prism' in a.get('name','')]
print('Hex actors found: ' + str(len(hex_actors)))
for a in hex_actors:
    print('  ' + a['name'] + ' (' + a['class'] + ')')
if not hex_actors:
    print('(none)')
    print('Latest actors:')
    for a in actors[-5:]:
        print('  ' + a['name'] + ' (' + a['class'] + ')')
