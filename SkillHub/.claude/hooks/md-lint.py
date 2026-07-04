"""Markdown linter hook — calls markdownlint-cli2, formats output for Claude Code."""
import re, sys, json, os, subprocess
from datetime import datetime

LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'md-lint.log')

def log(msg):
    with open(LOG, 'a', encoding='utf-8') as f:
        f.write(f'[{datetime.now().isoformat()}] {msg}\n')

data = json.load(sys.stdin)
filepath = data.get('tool_input', {}).get('file_path', '')
log(f'CALLED: filepath={filepath}')

if not filepath.endswith('.md'):
    log(f'SKIP: not .md')
    sys.exit(0)

result = subprocess.run(
    f'npx markdownlint-cli2 "{filepath}" --no-banner --no-progress',
    capture_output=True, text=True, timeout=30, shell=True
)
log(f'LINT: exit={result.returncode} issues={len(result.stderr)} chars')

output = result.stderr.strip() if result.stderr else ''
if result.returncode == 0 or not output:
    rpt = f'## Lint: {os.path.basename(filepath)} — clean ✓'
else:
    basename = os.path.basename(filepath)
    lines = []
    for line in output.split('\n'):
        # Replace any path variant ending with the filename
        line = re.sub(r'\S*' + re.escape(basename), basename, line)
        lines.append(line)
    rpt = f'## Lint: {os.path.basename(filepath)}\n\n{chr(10).join(lines)}'

print(json.dumps({
    'hookSpecificOutput': {
        'hookEventName': 'PostToolUse',
        'additionalContext': rpt
    }
}))
