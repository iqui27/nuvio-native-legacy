#!/bin/bash
# Regression: the bug template must gate playback/source reports behind the
# pre-filing checklist (issue triage, owner 2026-10), and README must keep the
# matching TROUBLESHOOTING text.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import yaml

body = yaml.safe_load(Path('.github/ISSUE_TEMPLATE/bug.yml').read_text())
fields = body['body']
ids = [f.get('id') for f in fields]

# 1. Required checkbox block with the five pre-filing checks.
checks = [f for f in fields if f.get('id') == 'checks' and f['type'] == 'checkboxes']
assert len(checks) == 1, 'missing checkboxes field "checks"'
checks = checks[0]
labels = ' '.join(o['label'] for o in checks['attributes']['options'])
assert checks['validations']['required'] is True, 'checks not required'
for o in checks['attributes']['options']:
    assert o.get('required') is True, f'option not required: {o["label"][:40]}'
for term in ('Stremio', 'cached', 'speed test', 'This TV',
             'smaller than the recommended size', '6-character code'):
    assert term.lower() in labels.lower(), f'checklist missing: {term}'

# 2. Required fields the triage cannot work without.
for fid in ('platform', 'tv', 'version', 'what', 'log'):
    field = [f for f in fields if f.get('id') == fid][0]
    assert field['validations']['required'] is True, f'{fid} must be required'
    assert fid in ids

# 3. Platform dropdown (not free text).
platform = [f for f in fields if f.get('id') == 'platform'][0]
assert platform['type'] == 'dropdown' and platform['attributes']['options'], 'platform must be a dropdown'

# 4. "Send log" is explained with its path and code.
log = [f for f in fields if f.get('id') == 'log'][0]
assert 'Send log' in log['attributes']['description'], 'log field must point at Send log'
markdown = [f for f in fields if f['type'] == 'markdown'][0]['attributes']['value']
for term in ('add-on', 'debrid', '404', 'Speed test', 'This TV',
             '6-character code', 'Send log'):
    assert term.lower() in markdown.lower(), f'markdown preamble missing: {term}'

# 5. feature.yml untouched by this change.
feat = yaml.safe_load(Path('.github/ISSUE_TEMPLATE/feature.yml').read_text())
assert feat['body'], 'feature.yml lost its body'

# 6. README keeps the same checklist and the section it lives in.
readme = Path('README.md').read_text()
assert '## Reporting a problem\n' in readme, 'Reporting a problem section missing'
assert '### Troubleshoot first\n' in readme, 'troubleshoot step list missing'
section = readme.split('## Reporting a problem\n', 1)[1].split('\n---\n', 1)[0]
for term in ('Stremio', 'add-on', 'debrid', '404', 'Speed test', 'This TV',
             '6-character code', 'Send the log and keep its code', 'Bug report'):
    assert term.lower() in section.lower(), f'README troubleshooting missing: {term}'
print('issue triage checklist: ok')
PY
