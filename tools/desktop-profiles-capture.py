#!/usr/bin/env python3
"""Approved profiles, fresh offline data, background SDL input and GL readback."""
import json
import os
from pathlib import Path
import plistlib
import re
import shlex
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'codigo'
APP = OUT / 'perfis-proof/Nuvio.app'
BOARD = BASE / 'boards/aprovadas/Mac · perfis.png'
TMP = Path('/Volumes/ExternalSSD/tmp/jobtmp')
os.environ['TMPDIR'] = str(TMP)
OUT.mkdir(parents=True, exist_ok=True)


def run(*args, **kwargs):
    return subprocess.run(args, cwd=ROOT, check=True, **kwargs)


def wait(predicate, message):
    deadline = time.monotonic() + 60
    while not predicate():
        if time.monotonic() > deadline:
            raise RuntimeError(message)
        time.sleep(.1)


with tempfile.TemporaryDirectory(prefix='nuvio-perfis-', dir='/private/tmp') as temporary:
    runtime = Path(temporary)
    data, ipc = runtime / 'data', runtime / 'ipc'
    data.mkdir(); ipc.mkdir()
    # A fresh directory is mandatory. C's capture-only startup also fails closed.
    assert data.is_dir() and not list(data.iterdir())
    (data / '.isolation').write_text('isolated profile proof\n')
    assert (data / '.isolation').read_text() == 'isolated profile proof\n'
    if not APP.exists():
        APP.parent.mkdir(parents=True, exist_ok=True)
        run('ditto', str(BASE / 'Nuvio.app'), str(APP))
    binary = APP / 'Contents/MacOS/Nuvio-proof-bin'
    executable = binary.with_name('Nuvio')
    if '--no-build' not in os.sys.argv:
        sources = [str(p) for p in ROOT.glob('src/*.c')]
        sources += [str(p) for p in ROOT.glob('src/dts/*.c')]
        run('cc', *sources, 'src/desktop_mac.m', '-o', str(binary), '-O1',
            '-DNV_DESKTOP_RELEASE', '-DNV_DESKTOP_UI', '-DNV_TOUCH_UI', '-DNV_VERSAO="2.1.0"',
            '-DNV_SUPABASE_URL=""', '-DNV_SUPABASE_ANON_KEY=""', '-DNV_TV_LOGIN_BASE=""',
            '-DNV_TRAKT_CLIENT_ID=""', '-DNV_TRAKT_CLIENT_SECRET=""',
            '-DNV_SIMKL_CLIENT_ID=""', '-DNV_SIMKL_APP=""', '-DNV_TMDB_API_KEY=""',
            '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
            '-L/opt/homebrew/lib', '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz',
            '-framework', 'Cocoa', '-framework', 'QuartzCore', '-framework', 'OpenGL',
            '-Wno-deprecated-declarations')
    assert binary.is_file(), 'No proof binary; refusing launch'
    built_app = APP
    APP = runtime / 'Nuvio.app'
    run('ditto', str(built_app), str(APP))
    binary = APP / 'Contents/MacOS/Nuvio-proof-bin'
    executable = binary.with_name('Nuvio')
    art = runtime / 'resources/art'
    run('ditto', str(BASE / 'scene-resources'), str(art.parent))
    backdrop = art / 'profiles-backdrop.png'
    run('ditto', str(BASE / 'boards/perfis-backdrop.png'), str(backdrop))
    profiles = []
    for i, name in enumerate(('Gustavo', 'Bruno', 'Pedro', 'Marcela'), 1):
        profiles.append(f'{i}\t0\t{int(i == 1)}\t1\t#7D56FF\t{name}\t\t{backdrop}\t1\n')
    (data / 'perfis.txt').write_text(''.join(profiles))
    (data / 'perfil.txt').write_text('1\n')
    for source in [*ROOT.glob('src/novidades*.*'), *ROOT.glob('src/novidades/*.inc')]:
        for name in re.findall(r'"([^"/]*\.txt)"', source.read_text()):
            (data / name).write_text('1\n')
    for name in ('salvos-intro.txt', 'aviso-log.txt', 'recintro-social.txt', 'perfilfundo-20.txt', 'aparencia-20.txt'):
        (data / name).write_text('1\n')
    (data / 'dicas-controle-205.txt').write_text('2.1\n')
    (data / 'diagnostico-otimizacao-intro.cfg').write_text('versao=1\n')
    (data / 'atualizacao-vista.txt').write_text('2.0.8\n')
    settings = 'idioma 0\nidiomaAutoLocal 0\nperfilFundoLocal 2\npsContinuarLocal 0\npsTamanhoLocal 1\n'
    for name in ('ajustes.txt', 'ajustes-p1.txt'):
        (data / name).write_text(settings)
    helper = runtime / 'front.m'
    helper.write_text('#import <Cocoa/Cocoa.h>\nint main(){@autoreleasepool{printf("%d\\n",NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier);}}\n')
    run('cc', str(helper), '-framework', 'Cocoa', '-o', str(runtime / 'front'))

    def front():
        return int(subprocess.check_output([str(runtime / 'front')]))

    foreground = front()
    log = OUT / 'perfis-runtime.log'
    env = dict(NUVIO_DADOS=str(data), NUVIO_UI_PREVIEW='1', NV_CAPTURA='1',
               NV_CAPTURA_GL='1', NV_CAPTURA_DIR=str(ipc), SDL_MAC_BACKGROUND_APP='1',
               SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
    info_path = APP / 'Contents/Info.plist'
    info = plistlib.loads(info_path.read_bytes())
    info['CFBundleIdentifier'] = 'app.nuvio.proof.' + runtime.name
    info_path.write_bytes(plistlib.dumps(info))
    exports = ''.join('export ' + key + '=' + shlex.quote(value) + '\n' for key, value in env.items())
    executable.write_text('#!/bin/bash\nset -eu\n' + exports +
                          'echo isolated-launch > ' + shlex.quote(str(runtime / 'launched')) + '\n' +
                          'test -d "$NUVIO_DADOS"\n' +
                          'exec ' + shlex.quote(str(binary)) + ' "$@" > ' + shlex.quote(str(log)) + ' 2>&1\n')
    executable.chmod(0o755)
    run('codesign', '--force', '--sign', '-', str(APP), capture_output=True)
    # Exercise fail-closed startup before any SDL window can be created.
    for isolated in (None, str(runtime / 'missing/data')):
        rejected_env = dict(os.environ, **env)
        if isolated is None:
            rejected_env.pop('NUVIO_DADOS', None)
        else:
            rejected_env['NUVIO_DADOS'] = isolated
        rejected = subprocess.run([str(binary), str(art)], env=rejected_env,
                                  capture_output=True, text=True, timeout=10)
        assert rejected.returncode == 78 and 'capture requires writable isolated NUVIO_DADOS' in rejected.stderr
    launch = ['open', '-n', '-g', '-j']
    for key, value in env.items():
        launch += ['--env', key + '=' + value]
    pid = None
    log.write_text('')

    def command(value):
        pending = ipc / 'nuvio-key'
        (ipc / 'pending').write_text(value + '\n')
        (ipc / 'pending').replace(pending)
        wait(lambda: pending.stat().st_size == 0, 'SDL input timeout: ' + value)
        time.sleep(.4)

    def shot(name):
        source = ipc / 'nuvio-shot.bmp'
        source.unlink(missing_ok=True)
        (ipc / 'nuvio-shot-req').write_text('1\n')
        wait(source.exists, 'GL readback timeout')
        dest = OUT / name
        run('magick', str(source), '-resize', '1440x900!', str(dest))
        return dest

    try:
        run(*launch, '-a', str(APP), '--args', str(art))
        wait(lambda: 'primeiro quadro na tela' in log.read_text(errors='replace'), 'Background launch timeout')
        pid = int(subprocess.check_output(['pgrep', '-f', '^' + re.escape(str(binary))], text=True).splitlines()[-1])
        assert front() != pid
        assert '[dados] gravando em ' + str(data) in log.read_text(errors='replace')
        command('appearance:DarkAqua'); command('back'); command('janela:1440,900'); command('preview:profiles')
        command('mover:1300,800')
        time.sleep(3)
        command('back')
        command('preview:profiles')
        png = shot('perfis-app.png')
        run('magick', str(BOARD), str(png), '+append', '-quality', '85', str(OUT / 'perfis-lado.jpg'))
        run('magick', str(OUT / 'perfis-lado.jpg'), '-resize', '1000x1000>', '-quality', '80', str(OUT / 'perfis-review.jpg'))
        # Existing footer navigation remains reachable through real SDL keys.
        command('down'); command('right'); command('ok')
        shot('perfis-manage.png')
        assert front() != pid
        (OUT / 'perfis-focus.json').write_text(json.dumps(dict(before=foreground, after=front(), app=pid)))
    finally:
        if pid is None:
            result = subprocess.run(['pgrep', '-f', '^' + re.escape(str(binary))], capture_output=True, text=True)
            if result.returncode == 0:
                pid = int(result.stdout.splitlines()[-1])
        if pid:
            try:
                os.kill(pid, signal.SIGTERM)
                time.sleep(.3)
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
print('PASS: isolated profiles 1440x900 GL readback; background app stopped:', OUT / 'perfis-lado.jpg')
