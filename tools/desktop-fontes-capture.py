#!/usr/bin/env python3
"""Approved Sources proof: fresh data only, background launch, SDL input, GL readback."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'codigo'
BUILD = OUT / 'fontes-build/Nuvio.app'
BOARD = BASE / 'boards/aprovadas/Mac · fontes.png'
TMP = Path('/Volumes/ExternalSSD/tmp/jobtmp')
parser = argparse.ArgumentParser()
parser.add_argument('--reuse-build', action='store_true')
args = parser.parse_args()
signal.signal(signal.SIGTERM, lambda signum, frame: sys.exit(128+signum))
os.environ['TMPDIR'] = str(TMP)
OUT.mkdir(parents=True, exist_ok=True)

def run(*command, **kwargs):
    return subprocess.run(command, cwd=ROOT, check=True, **kwargs)

def wait(check, message):
    deadline = time.monotonic()+90
    while not check():
        if time.monotonic()>deadline:
            raise RuntimeError(message)
        time.sleep(.05)

if not args.reuse_build:
    (BUILD/'Contents/MacOS').mkdir(parents=True, exist_ok=True)
    sources = [str(p) for p in sorted((ROOT/'src').glob('*.c'))]
    sources += [str(p) for p in sorted((ROOT/'src/dts').glob('*.c'))]
    with (OUT/'fontes-build.log').open('w') as log:
        run('cc', *sources, 'src/desktop_mac.m', '-o', str(BUILD/'Contents/MacOS/Nuvio'), '-O1',
            '-DNV_DESKTOP_RELEASE', '-DNV_DESKTOP_UI', '-DNV_TOUCH_UI',
            '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2', '-L/opt/homebrew/lib',
            '-lSDL2', '-lSDL2_image', '-lSDL2_ttf', '-lz', '-framework', 'OpenGL',
            '-framework', 'Cocoa', '-framework', 'QuartzCore', '-Wno-deprecated-declarations',
            '-Wno-macro-redefined', stdout=log, stderr=subprocess.STDOUT)
# Refuse stale binaries which still permit capture to fall back to real data.
if b'capture isolation unavailable; refusing default data' not in (BUILD/'Contents/MacOS/Nuvio').read_bytes():
    raise RuntimeError('Capture requires the fail-closed desktop data guard')
# mkdtemp failure aborts before launch. No copy of any existing data/profile.
with tempfile.TemporaryDirectory(prefix='nuvio-fontes-', dir='/private/tmp') as temp:
    runtime = Path(temp).resolve()
    data, ipc, app = runtime/'data', runtime/'ipc', runtime/'Nuvio.app'
    data.mkdir(mode=0o700); ipc.mkdir(mode=0o700)
    assert data.is_dir() and not data.is_symlink() and data.parent==runtime
    run('ditto', str(BUILD), str(app))
    art = runtime/'resources/art'
    run('ditto', str(BASE/'scene-resources'), str(art.parent))
    run('ditto', str(ROOT/'deploy/app/fonts'), str(art.parent/'fonts'))
    # The same Fallout backdrop and logo used by the approved detail board.
    hero = runtime/'design/glass-ilha/arte.jpg'
    hero.parent.mkdir(parents=True)
    shutil.copyfile(art/'00.jpg', hero)
    for source in [*ROOT.glob('src/novidades*.*'), *ROOT.glob('src/novidades/*.inc')]:
        for name in re.findall(r'"([^"/]*\.txt)"', source.read_text()):
            (data/name).write_text('1\n')
    for name in ('salvos-intro.txt', 'aviso-log.txt', 'recintro-social.txt'):
        (data/name).write_text('1\n')
    (data/'dicas-controle-205.txt').write_text('2.1\n')
    (data/'diagnostico-otimizacao-intro.cfg').write_text('versao=1\n')
    (data/'atualizacao-vista.txt').write_text('2.1.0\n')
    (data/'desktop-sidebar.txt').write_text('0\n')
    settings = 'idioma 0\nidiomaAutoLocal 0\nhomeLayoutLocal 2\nselected_theme 11\nfonteInterface 0\nprecarregarFonte 0\n'
    for name in ('ajustes.txt','ajustes-p1.txt'):
        (data/name).write_text(settings)
    fixture = runtime/'scene'
    scene = runtime/'scene.c'
    scene.write_text((ROOT/'tools/desktop-scene.c').read_text()
                     .replace('"", 1, 1, "en-US"','"", 1, 0, "pt-BR"')
                     .replace('"2024  ·  2 seasons"','"2024  ·  2 temporadas"')
                     .replace('"Series  ·  Sci-Fi"','"Ação e aventura · Ficção científica"')
                     .replace('item->nota = 84;', 'snprintf(item->nomeEpisodio,sizeof item->nomeEpisodio,"A cabeça"); item->nota = 84;'))
    run('cc','-Isrc',str(scene),'-o',str(fixture))
    run(str(fixture),str(data/'catalogo-rede.bin'),str(art),str(runtime))
    # Seed the existing metadata cache; no public metadata lookup is needed.
    names=['O fim','O alvo','A cabeça','Os carniçais','O passado','A armadilha','O rádio','O começo']
    metadata=json.dumps({'meta':{'id':'tt12637874','type':'series','name':'Fallout',
        'description':'Em um mundo onde quase tudo foi destruído, os habitantes dos abrigos precisam voltar à superfície.',
        'poster':str(art/'poster/00.jpg'),'background':str(hero),'logo':str(art/'logo/00.png'),
        'genres':['Ação e aventura','Ficção científica'],'year':'2024','runtime':'60 min',
        'videos':[{'id':f'tt12637874:1:{i+1}','season':1,'episode':i+1,'title':name,
                   'thumbnail':str(art/f'{i%8:02}.jpg'),'released':'2024-04-10T00:00:00.000Z'}
                  for i,name in enumerate(names)]}},ensure_ascii=False)
    for profile in (0,1):
        key=f'/{profile}/pt-BR/nuvio:pt-BR/series/tt12637874'
        code=2166136261
        for byte in key.encode(): code=((code^byte)*16777619)&0xffffffff
        (data/f'meta-{code%64:02}.txt').write_text(f'{int(time.time())}\n{key}\n{metadata}')
    (data/'perfis.txt').write_text('1\t0\t1\t0\t#777B83\tDesktop Viewer\t\t\t0\n')
    # Build a launcher which refuses missing/non-fresh isolation before exec.
    executable = app/'Contents/MacOS/Nuvio'
    binary = executable.with_name('Nuvio-proof-bin')
    executable.replace(binary)
    marker = data/'capture-isolated'
    marker.write_text(str(runtime))
    log = OUT/'fontes-runtime.log'
    executable.write_text(f'''#!/bin/bash
set -eu
[ "${{NUVIO_DADOS:-}}" = "{data}" ] || exit 78
[ -d "{data}" ] && [ ! -L "{data}" ] && [ -f "{marker}" ] || exit 78
[ "${{NV_CAPTURA_DIR:-}}" = "{ipc}" ] || exit 78
exec "{binary}" "$@" > "{log}" 2>&1
''')
    executable.chmod(0o755)
    with (app/'Contents/Info.plist').open('wb') as plist:
        plistlib.dump(dict(CFBundleExecutable='Nuvio', CFBundleIdentifier='app.nuvio.fontes-proof',
                           CFBundleName='Nuvio Sources Proof', CFBundlePackageType='APPL',
                           NSHighResolutionCapable=True),plist)
    run('codesign','--force','--sign','-',str(app),capture_output=True)
    helper = runtime/'front.m'
    helper.write_text('#import <Cocoa/Cocoa.h>\nint main(){@autoreleasepool{printf("%d\\n",NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier);}}\n')
    run('cc',str(helper),'-framework','Cocoa','-o',str(runtime/'front'))
    def front():
        return int(subprocess.check_output([str(runtime/'front')]))
    foreground = front()
    log.write_text('')
    def command(value):
        pending = ipc/'nuvio-key'
        (ipc/'pending').write_text(value+'\n'); (ipc/'pending').replace(pending)
        wait(lambda: pending.stat().st_size==0,'Input timeout: '+value)
        time.sleep(.4)
    def photo(name):
        source = ipc/'nuvio-shot.bmp'
        source.unlink(missing_ok=True)
        (ipc/'nuvio-shot-req').write_text('1\n')
        def complete():
            if not source.exists(): return False
            b=source.read_bytes()
            return len(b)>=54 and len(b)==struct.unpack_from('<I',b,2)[0]
        wait(complete,'GL readback timeout')
        bitmap=source.read_bytes(); w,h=struct.unpack_from('<ii',bitmap,18)
        assert abs(w/h-1440/900)<.001
        png=OUT/(name+'.png')
        run('magick','-size',f'{w}x{h}','-depth','8','bgra:-','-flip','-resize','1440x900!',str(png),input=bitmap[54:])
        assert struct.unpack('>II',png.read_bytes()[16:24])==(1440,900)
        return png
    env=dict(NUVIO_DADOS=str(data),NUVIO_UI_PREVIEW='1',NV_CAPTURA='1',NV_CAPTURA_GL='1',
             NV_CAPTURA_DIR=str(ipc),SDL_MAC_BACKGROUND_APP='1',SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1')
    launch=['open','-n','-g','-j']
    for key,value in env.items(): launch+=['--env',key+'='+value]
    pid=None
    try:
        run(*launch,'-a',str(app),'--args',str(art))
        wait(lambda:'primeiro quadro na tela' in log.read_text(errors='replace'),'Background launch timeout')
        pid=int(subprocess.check_output(['pgrep','-f','^'+re.escape(str(binary))],text=True).splitlines()[-1])
        assert front()!=pid
        assert '[dados] gravando em '+str(data) in log.read_text(errors='replace')
        command('appearance:DarkAqua'); command('back'); command('janela:1440,900')
        command('preview:sources'); time.sleep(3)
        assert '[fonte] folha: 20 de 20' in log.read_text(errors='replace')
        png=photo('fontes-app')
        shutil.copyfile(ipc/'source-controls.png',OUT/'fontes-native-controls.png')
        coverage=float(subprocess.check_output(['magick',str(OUT/'fontes-native-controls.png'),
                                               '-format','%[fx:mean.a]','info:']))
        assert .002<coverage<.15, 'Native controls missing from GL proof'
        command('mover:1050,600');command('wheel:1050,600,0,-700,1');command('wheel:1050,600,0,0,8')
        time.sleep(1);photo('fontes-scroll')
        command('back');time.sleep(1);photo('fontes-closed')
        command('preview:sources');time.sleep(1)
        # Native control and SDL pointer share the exact same filter action.
        command('clicar:530,213');time.sleep(1);photo('fontes-filter')
        assert front()!=pid
        run('magick',str(BOARD),'-resize','1440x900!',str(png),'+append','-quality','85',str(OUT/'fontes-lado.jpg'))
        run('magick',str(OUT/'fontes-lado.jpg'),'-resize','1000x1000>','-quality','80',str(OUT/'fontes-lado-small.jpg'))
        (OUT/'fontes-focus.json').write_text(json.dumps(dict(before=foreground,after=front(),app=pid,isolation=True)))
    finally:
        if pid is None:
            found=subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True,text=True)
            if found.returncode==0: pid=int(found.stdout.splitlines()[-1])
        if pid:
            try:
                os.kill(pid,signal.SIGTERM)
                time.sleep(.3)
                try: os.kill(pid,signal.SIGKILL)
                except ProcessLookupError: pass
            except ProcessLookupError: pass
    assert subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True).returncode==1
print('PASS: Sources 1440x900, isolated profile, background SDL input + GL readback; process stopped')
