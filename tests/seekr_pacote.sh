#!/bin/bash
# Configuracao real com sentinela ficticia: nenhum formato pode embutir a chave.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import os
import subprocess
import tempfile
from pathlib import Path

with tempfile.TemporaryDirectory(prefix='nuvio-seekr-pacote-') as tmp:
    tmp = Path(tmp)
    props = tmp / 'local.properties'
    sentinel = 'seekr-sentinela-ficticia'
    props.write_text('SEEKR_API_KEY=' + sentinel + '\n')
    env = dict(os.environ, NUVIO_PROPERTIES=str(props), SEEKR_API_KEY=sentinel,
               NV_SEEKR_API_KEY=sentinel, NUVIO_REQUIRE_ALL='0')
    flags = subprocess.check_output(['bash', 'tools/env.sh', '--allow-unconfigured'], env=env, text=True)
    assert sentinel not in flags, 'FAIL: chave do pacote entrou nos -D'
    assert '-DNV_SEEKR_API_KEY=\\"\\"' in flags
    target = tmp / 'env'
    subprocess.run(['bash', 'tools/env.sh', '--env-file', str(target)], env=env,
                   check=True, capture_output=True)
    values = dict(line.split('=', 1) for line in target.read_text().splitlines())
    assert values['NV_SEEKR_API_KEY'] == '', 'FAIL: chave do pacote entrou no env-file'
    assert sentinel not in target.read_text()
    # Exercita a lista usada para limpar e conferir a arte dos IPKs, sem SDK/TV.
    import re
    for script in ('tools/arm.sh', 'tools/testa-ipk.sh'):
        source = Path(script).read_text()
        names = re.search(r'ARQ_DE_PESSOA="([^"]*)"', source).group(1).split()
        assert 'seekr.txt' in names, 'FAIL: IPK pode levar seekr.txt'
        assert 'seekr.txt.tmp' in names, 'FAIL: IPK pode levar seekr.txt.tmp'
print('seekr pacote: PASS macro vazia nos -D/env-file e chave pessoal excluida dos IPKs')
PY
