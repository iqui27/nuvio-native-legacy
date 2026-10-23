#!/bin/bash
# Local scripts must use ctr.sh; remote Linux scripts and runtime fixtures are exempt.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
import shlex
import sys

allowed = {
    'tools/ctr.sh',
    'tools/tpk/sdk.sh',  # Selects the Linux Docker SDK through ctr.sh.
    'tools/painel-issues/api/implantar-remoto.sh',
    'tools/painel-issues/tests/implantar-shim.sh',
    'tools/painel-issues/publicar.sh',  # Commands inside remote ssh script.
    'tests/sem_docker.sh',
    'tests/applecontainer_review.sh',  # Deliberately broken scanner fixtures.
}
failed = False
for directory in ('tools', 'tests'):
    for path in sorted(Path(directory).rglob('*.sh')):
        if str(path) in allowed:
            continue
        for number, line in enumerate(path.read_text().splitlines(), 1):
            try:
                lexer = shlex.shlex(line, posix=True, punctuation_chars=';&|()')
                lexer.whitespace_split = True
                tokens = list(lexer)
            except ValueError:  # Continued string: inspect following lines too.
                continue
            command = []
            for token in tokens + [';']:
                if token in (';', '&&', '||', '|', '&', '(', ')'):
                    if command and command[0] not in ('echo', 'printf'):
                        direct = any(t == 'docker' and i + 1 < len(command) and
                                     command[i + 1] in ('run', 'build', 'image', 'ps', 'exec', 'info')
                                     for i, t in enumerate(command))
                        default = any(re.match(r'\w*(?:RUNTIME|runtime)\w*=(?:docker$|.*(?:=|:-|:=)docker(?:[};]|$))', t)
                                      for t in command)
                        if direct or default:
                            print(f'FAIL: {path}:{number}: {line.strip()}')
                            failed = True
                            break
                    command = []
                else:
                    command.append(token)
if not failed:
    print('OK: No direct Docker commands or indirect Docker defaults in local scripts')
sys.exit(int(failed))
PY
