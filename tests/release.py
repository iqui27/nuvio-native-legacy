#!/usr/bin/env python3
"""Regressoes sem SDK: configuracao, isolamento, falha concorrente e cache de headers."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('release', ROOT/'tools/release.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)

(ROOT/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=ROOT/'build', prefix='release-test-') as tmp:
    tmp = Path(tmp)
    props = tmp/'local.properties'
    props.write_text('NUVIO_SUPABASE_URL=https://example.invalid\nNUVIO_SUPABASE_ANON_KEY=sentinel-do-not-print\nTV_LOGIN_WEB_BASE_URL=https://example.invalid\n')
    env = dict(os.environ, NUVIO_PROPERTIES=str(props), NUVIO_REQUIRE_ALL='1')
    p = subprocess.run(['bash', 'tools/env.sh', '--env-file', str(tmp/'env')], cwd=ROOT, env=env, capture_output=True, text=True)
    assert p.returncode == 3 and not (tmp/'env').exists()
    assert 'sentinel-do-not-print' not in p.stdout+p.stderr

    # Os scripts individuais exigem somente o core, como antes. Execute o
    # preflight real ate VER, sem entrar em SDKs, assinatura ou empacotamento.
    individual_env = dict(env, NUVIO_RELEASE_TREE=r.tree_state()[1])
    individual_env.pop('NUVIO_REQUIRE_ALL')
    for name in ('release-android.sh', 'release-samsung.sh'):
        script = ROOT/'tools'/name
        preflight = script.read_text().split('\nVER=', 1)[0]
        p = subprocess.run(['bash', '-c', preflight, str(script)], env=individual_env, capture_output=True, text=True)
        assert p.returncode == 0, p.stderr
        print(f'ok: {name} preflight core apenas, Social/Discord/Seekr vazios rc={p.returncode}')

    required = dict(NUVIO_SUPABASE_URL='https://example.invalid', NUVIO_SUPABASE_ANON_KEY='sentinel-do-not-print',
                    TV_LOGIN_WEB_BASE_URL='https://example.invalid', TRAKT_CLIENT_ID='synthetic',
                    TRAKT_CLIENT_SECRET='synthetic', SIMKL_CLIENT_ID='synthetic',
                    SIMKL_APP_NAME='Nuvio Native', TMDB_API_KEY='synthetic')
    complete = ''.join(f'{k}={v}\n' for k, v in required.items()) + 'NUVIO_REC_URL=\nDISCORD_CLIENT_ID=\nSEEKR_API_KEY=\n'
    props.write_text(complete)
    with patch.dict(os.environ, env):
        values = r.config()
    assert all(values[k] == '' for k in ('NV_REC_URL', 'NV_DISCORD_CLIENT_ID', 'NV_SEEKR_API_KEY'))

    # O bloco real do arm.sh deve preservar o argumento E a string C no @file.
    arm = (ROOT/'tools/arm.sh').read_text()
    block = arm[arm.index('  for k in NV_SUPABASE_URL'):arm.index('  done >> /tmp/arm-flags')] + '  done\n'
    flags = tmp/'arm-flags'
    flags.write_text(subprocess.check_output(['bash', '-c', block], env=dict(env, **values), text=True))
    source = tmp/'name.c'
    source.write_text('#include <string.h>\nint main(void){return strcmp(NV_SIMKL_APP, "Nuvio Native");}\n')
    p = subprocess.run(['clang', '@'+str(flags), str(source), '-o', str(tmp/'name')], capture_output=True, text=True)
    assert p.returncode == 0, p.stderr
    subprocess.run([str(tmp/'name')], check=True)
    print(f'ok: clang SIMKL_APP_NAME=Nuvio Native rc={p.returncode}; valor preservado')

    for key, value in required.items():
        props.write_text(complete.replace(f'{key}={value}\n', f'{key}=\n'))
        p = subprocess.run(['bash', 'tools/release.sh', values['NV_VERSAO'], '--ensaio'], env=env, capture_output=True, text=True)
        assert p.returncode == 1 and 'configuracao de release vazia:' in p.stderr, p.stdout+p.stderr
        assert 'sentinel-do-not-print' not in p.stdout+p.stderr
        print(f'ok: release.sh aborta com {key} vazia rc={p.returncode} (env.sh rc=3)')

    with patch.object(r, 'tree_state', return_value=(' M tools/a.sh', 'abc')), patch.dict(os.environ, {}, clear=True):
        try: r.check_tree()
        except RuntimeError: pass
        else: raise AssertionError('arvore suja foi aceita')
        os.environ['NUVIO_RELEASE_TREE']='abc'
        r.check_tree()
        os.environ['NUVIO_RELEASE_TREE']='changed'
        try: r.check_tree()
        except RuntimeError: pass
        else: raise AssertionError('mudanca durante build foi aceita')

    # Um filho falha; os outros terminam e todos os RCs ficam registrados.
    fake = tmp/'work'; (fake/'tools').mkdir(parents=True)
    (fake/'build').mkdir()
    (fake/'tools/arm.sh').write_text('sleep 0.2\necho lg >> finished\n')
    (fake/'tools/release-android.sh').write_text('echo fail\nexit 17\n')
    (fake/'tools/release-samsung.sh').write_text('sleep 0.5\necho samsung >> finished\n')
    out = fake/'build/release-2.0.4'; out.mkdir()
    (out/'SHA256SUMS').write_text('stale')
    (out/'sentinel.apk').write_text('preserve')
    values={'NV_VERSAO':'2.0.4'}
    with patch.object(r,'ROOT',fake), patch.object(r,'tree_state',return_value=('', 'abc')), patch.object(r,'config',return_value=values), patch.object(r,'prepare_ass'), patch.object(sys,'argv',['release.sh','2.0.4']):
        start=time.monotonic()
        try: r.main()
        except RuntimeError as e: assert 'rc=17' in str(e)
        else: raise AssertionError('falha de filho virou sucesso')
        assert 0.48 <= time.monotonic()-start < 2
        assert (fake/'finished').read_text().count('lg')==2
        assert 'samsung' in (fake/'finished').read_text()
        assert not (out/'SHA256SUMS').exists()
        assert (out/'sentinel.apk').read_text()=='preserve'
        assert json.loads((out/'logs/timings.json').read_text())['android']['rc']==17
        assert not (fake/'build/.release-lock').exists()
    os.chdir(ROOT)

    # GNU make conserva objetos e recompila ao mudar um header incluido.
    cache=tmp/'cache'; (cache/'src').mkdir(parents=True); (cache/'tools').mkdir()
    shutil.copy(ROOT/'tools/native-objects.mk',cache/'tools/')
    (cache/'src/value.h').write_text('#define VALUE 1\n')
    (cache/'src/a.c').write_text('#include "value.h"\nint get(void){return VALUE;}\n')
    cmd=['make','-s','-f','tools/native-objects.mk','CC=cc','OBJDIR=obj','SOURCES=src/a.c']
    subprocess.run(cmd,cwd=cache,check=True)
    obj=cache/'obj/src/a.o'; first=obj.read_bytes(); timestamp=obj.stat().st_mtime_ns
    subprocess.run(cmd,cwd=cache,check=True)
    assert obj.stat().st_mtime_ns==timestamp
    time.sleep(1.05)  # make antigo do macOS compara segundos
    (cache/'src/value.h').write_text('#define VALUE 2\n')
    subprocess.run(cmd,cwd=cache,check=True)
    assert obj.read_bytes()!=first
print('ok: chaves obrigatorias, arvore limpa/imutavel, falhas concorrentes, espera, isolamento e cache de headers')
