"""Markdown linter hook for Kimi CLI — plain-text output."""
import json
import os
import re
import subprocess
import sys
from datetime import datetime

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'md-lint.log')


def log(msg):
    with open(LOG, 'a', encoding='utf-8') as f:
        f.write(f'[{datetime.now().isoformat()}] {msg}\n')


try:
    sys.stdout.reconfigure(encoding='utf-8')
except AttributeError:
    pass

try:
    data = json.load(sys.stdin)
except json.JSONDecodeError as e:
    log(f'ERR: invalid JSON input: {e}')
    sys.exit(0)

filepath = data.get('tool_input', {}).get('file_path', '')
tool_name = data.get('tool_name', '')
log(f'CALLED: tool={tool_name} filepath={filepath}')

if not filepath.endswith('.md'):
    log('SKIP: not .md')
    sys.exit(0)

if tool_name not in ('WriteFile', 'Edit'):
    log(f'SKIP: tool={tool_name}')
    sys.exit(0)

result = subprocess.run(
    f'npx markdownlint-cli2 "{filepath}" --no-banner --no-progress',
    capture_output=True, text=True, encoding='utf-8', errors='replace',
    timeout=30, shell=True
)
stderr_len = len(result.stderr) if result.stderr else 0
log(f'LINT: exit={result.returncode} issues={stderr_len} chars')

output = result.stderr.strip() if result.stderr else ''
basename = os.path.basename(filepath)
if result.returncode == 0 or not output:
    print(f'## Lint: {basename} — clean ✓')
else:
    lines = []
    for line in output.split('\n'):
        line = re.sub(r'\S*' + re.escape(basename), basename, line)
        lines.append(line)
    print(f'## Lint: {basename}\n')
    print('\n'.join(lines))
