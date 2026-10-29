#!/bin/bash
# Regression checks for production/test readiness parity and initial-window coverage.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re
import subprocess

failures = []
args = ['cc', '-E', '-P', '-Isrc', 'src/mkvass.c']
normal = subprocess.check_output(args)
legacy = subprocess.check_output(args + ['-DMKVASS_TESTE_DRENAR_PREBUSCA'])
if normal != legacy or '-DMKVASS_TESTE_DRENAR_PREBUSCA' in open('tests/mkvass.sh').read():
    failures.append('prefetch readiness changes under the legacy test define')

source = open('tests/mkvass.c').read()
match = re.search(r'ok\(([^;]+), "cabecalho, Cues e os blocos da janela lidos antes do video"\);', source)
assert match, 'initial-window assertion not found'
condition = match[1]
# The fixture has 41 cues, four starting within [0, 12]. Every incomplete
# prefix must fail, including the one-block case accepted by the old check.
program = '''
int main(void) {
  int tot = 41, nEsp = 41, nJan = 4, col;
  for (col = 0; col < nJan; col++) if (%s) return 1;
  col = nJan;
  return !(%s);
}
''' % (condition, condition)
import os
import tempfile
with tempfile.TemporaryDirectory() as directory:
    binary = os.path.join(directory, 'window')
    subprocess.run(['cc', '-x', 'c', '-', '-o', binary], input=program.encode(), check=True)
    if subprocess.run([binary]).returncode:
        failures.append('initial-window assertion accepts missing cues or rejects the complete window')
for failure in failures:
    print('FAIL:', failure)
assert not failures
print('prefetch readiness parity and initial-window regression: ok')
PY
