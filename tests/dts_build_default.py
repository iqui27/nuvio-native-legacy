#!/usr/bin/env python3
"""Check default ARM staging and SSH payload with isolated SDK/TV substitutes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = ('lib/dts-starfish-webos3.so', 'lib/dts-starfish-webos4.so',
           'licenses/dts/COPYING.LGPLv2.1', 'licenses/dts/SOURCE.txt')


def executable(path, text):
    path.write_text(text)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix='nuvio-dts-default-build-') as temporary:
    base = Path(temporary)
    for mode in ('default', 'disabled', 'tampered'):
        repository = base / mode
        tools = repository / 'tools'
        tools.mkdir(parents=True)
        for name in ('arm.sh', 'ctr.sh', 'env.sh', 'tizen-config.xml'):
            shutil.copy2(ROOT / 'tools' / name, tools / name)
        (tools / 'p2p-motor').mkdir()
        shutil.copy2(ROOT / 'tools/p2p-motor/pasta.sh', tools / 'p2p-motor/pasta.sh')
        app = repository / 'deploy/app'
        (app / 'art/marcas').mkdir(parents=True)
        (app / 'fonts').mkdir()
        for name in ('trakt.png', 'logo-novo-marca.png', 'abertura.jpg', 'login-fundo.jpg'):
            (app / 'art/marcas' / name).write_bytes(b'fixture art')
        for name in ('icon.png', 'icon-large.png', 'splash.png'):
            (app / name).write_bytes(b'fixture image')
        (app / 'appinfo.json').write_text((ROOT / 'deploy/app/appinfo.json').read_text())
        (repository / 'local.properties').write_text(
            'NUVIO_SUPABASE_URL=https://fixture.invalid\n'
            'NUVIO_SUPABASE_ANON_KEY=fixture-public\n'
            'TV_LOGIN_WEB_BASE_URL=https://fixture.invalid/login\n')
        remote = repository / 'remote'
        remote.mkdir()
        commands = repository / 'commands'
        commands.mkdir()
        runtime = commands / 'sdk'
        executable(runtime, '''#!/usr/bin/env python3
import os,pathlib,sys
args=sys.argv[1:]
assert 'nuvio-webos-sdk' in args
assert 'src/*.c src/dts/*.c' in args[-1]
enabled='NUVIO_DTS_FFMPEG=1' in args
assert enabled == (os.environ['FIXTURE_MODE'] != 'disabled')
work=pathlib.Path(args[args.index('-v')+1].split(':/work')[0])
env=pathlib.Path(args[args.index('--env-file')+1]).read_text()
(work/'nuvio-proto.arm').write_text(env+'\\nfixture ARM binary\\n')
if enabled:
    for relative in ('lib/dts-starfish-webos3.so','lib/dts-starfish-webos4.so',
                     'licenses/dts/COPYING.LGPLv2.1','licenses/dts/SOURCE.txt'):
        target=work/'deploy/app'/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text('fixture '+relative)
''')
        executable(commands / 'sshpass', '''#!/usr/bin/env python3
import os,pathlib,shutil,subprocess,sys
args=sys.argv[1:]
remote=os.environ['FIXTURE_REMOTE']
prefix='/media/developer/apps/usr/palm/applications/space.nuvio.native.legacy'
if 'scp' in args:
    source,destination=args[-2:]
    target=pathlib.Path(destination.split(':',1)[1].replace(prefix,remote))
    shutil.copy2(source,target)
    if os.environ['FIXTURE_MODE']=='tampered' and target.name=='dts-starfish-webos4.so.novo':
        target.write_bytes(b'corrupted transfer')
else:
    command=args[-1].replace(prefix,remote)
    if command.startswith('pidof '): sys.exit(0)
    # Cache ownership belongs to the real TV runtime, outside this payload test.
    if 'chown -R' in command: command=command.split(' && chown -R')[0]
    sys.exit(subprocess.run(['sh','-c',command]).returncode)
''')
        executable(commands / 'sleep', '#!/bin/sh\nexit 0\n')
        executable(commands / 'nc', '#!/bin/sh\ncat >/dev/null\nprintf \'{"returnValue":true}\\n\'\n')
        env = os.environ.copy()
        for name in ('NUVIO_DTS_FFMPEG', 'NUVIO_SDK_IMAGE', 'NUVIO_DTS_ROOT', 'NUVIO_PROPERTIES'):
            env.pop(name, None)
        env.update(NUVIO_CONTAINER_RUNTIME=str(runtime), NUVIO_TV_IP='fixture.invalid',
                   NUVIO_TV_PASS='fixture', FIXTURE_MODE=mode, FIXTURE_REMOTE=str(remote),
                   NUVIO_P2P_MOTOR='none', NUVIO_RUST_MKVCORE='0', PATH=str(commands) + os.pathsep + env['PATH'])
        if mode == 'disabled':
            env['NUVIO_DTS_FFMPEG'] = '0'
        result = subprocess.run(['bash', 'tools/arm.sh'], cwd=repository, env=env,
                                capture_output=True, text=True)
        if mode == 'tampered':
            assert result.returncode != 0, result.stdout
            assert 'FALHOU: lib/dts-starfish-webos4.so' in result.stdout, result.stdout + result.stderr
            continue
        assert result.returncode == 0, result.stdout + result.stderr
        assert (remote / 'nuvio-proto').read_bytes() == (repository / 'nuvio-proto.arm').read_bytes()
        for relative in PAYLOAD:
            if mode == 'default':
                assert (remote / relative).read_bytes() == (app / relative).read_bytes()
                assert 'ok (' + relative + ')' in result.stdout
            else:
                assert not (remote / relative).exists()

print('dts_build_default: default decoder, nested sources, adapters/notices deployment, opt-out and checksum rejection passed')
