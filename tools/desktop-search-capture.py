#!/usr/bin/env python3
"""Build/capture Search with disposable data; background launch, SDL input, GL readback."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = Path('/Volumes/ExternalSSD/tmp/rev/desktop')
OUT = BASE / 'codigo'
TMP = Path('/Volumes/ExternalSSD/tmp/jobtmp')
BOARD = BASE / 'boards/aprovadas/Mac · busca.png'
NATIVE = '--native' in sys.argv
OUT.mkdir(parents=True, exist_ok=True)
TMP.mkdir(parents=True, exist_ok=True)
os.environ['TMPDIR'] = str(TMP)

def run(*args, **kw):
    return subprocess.run(args, cwd=ROOT, check=True, **kw)

def wait(predicate, message):
    deadline = time.monotonic() + 90
    while not predicate():
        if globals().get('pid'):
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                raise RuntimeError('Capture process exited: '+message)
        if time.monotonic() > deadline:
            raise RuntimeError(message)
        time.sleep(.1)

with tempfile.TemporaryDirectory(prefix='nuvio-search-', dir='/private/tmp') as temp:
    runtime = Path(temp)
    app, data, ipc, home = [runtime / name for name in ('Nuvio.app', 'data', 'ipc', 'home')]
    for folder in (data, ipc, home, app/'Contents/MacOS'):
        folder.mkdir(parents=True)
    assert data.resolve().is_relative_to(runtime.resolve())
    (data/'.isolation-check').write_text('isolated\n')
    assert (data/'.isolation-check').read_text() == 'isolated\n'
    art = runtime/'resources/art'
    run('ditto', str(BASE/'scene-resources'), str(art.parent))
    with zipfile.ZipFile('/Volumes/ExternalSSD/nuvio-ios27.sketch') as archive:
        page = next(json.loads(archive.read(n)) for n in archive.namelist()
                    if n.startswith('pages/') and n.endswith('.json')
                    and json.loads(archive.read(n)).get('name') == 'Mac · busca · v2')
        def images(layer):
            if layer.get('image'):
                yield layer
            for child in layer.get('layers', []):
                yield from images(child)
        layers = list(images(page))
        names = ['Duna: Parte Dois · pôster', 'Duna · pôster', 'Duna · série', 'Filhos de Duna · série', 'Duna: Parte Dois · arte real']
        for name, dest in zip(names, ['search-0.png','search-1.png','search-2.png','search-3.png','search-best.png']):
            layer = next(layer for layer in layers if layer['name'] == name)
            (art/dest).write_bytes(archive.read(layer['image']['_ref']))
        background = next(layer for layer in layers if layer['name'] == 'Arte real · Duna · desfocada')
        (art/'search-background.png').write_bytes(archive.read(background['image']['_ref']))
    for source in [*ROOT.glob('src/novidades*.*'), *ROOT.glob('src/novidades/*.inc')]:
        for name in re.findall(r'"([^"/]*\.txt)"', source.read_text()):
            (data/name).write_text('1\n')
    for name in ['salvos-intro.txt','aviso-log.txt','recintro-social.txt']:
        (data/name).write_text('1\n')
    (data/'dicas-controle-205.txt').write_text('2.1\n')
    (data/'diagnostico-otimizacao-intro.cfg').write_text('versao=1\n')
    (data/'atualizacao-vista.txt').write_text('2.1.0\n')
    (data/'desktop-sidebar.txt').write_text('0\n')
    settings = 'idioma 0\nidiomaAutoLocal 0\nselected_theme 6\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n'
    (data/'ajustes.txt').write_text(settings)
    (data/'ajustes-p1.txt').write_text(settings)
    run('cc','-Isrc','tools/desktop-scene.c','-o',str(runtime/'scene'))
    run(str(runtime/'scene'),str(data/'catalogo-rede.bin'),str(art),str(runtime),'search')
    binary = app/'Contents/MacOS/Nuvio-proof-bin'
    import plistlib
    (app/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(CFBundleExecutable='Nuvio',CFBundleIdentifier='app.nuvio.search-proof',CFBundleName='Nuvio Search Proof',CFBundlePackageType='APPL',NSHighResolutionCapable=True)))
    log = OUT/('busca-native-runtime.log' if NATIVE else 'busca-runtime.log')
    executable = binary.with_name('Nuvio')
    executable.write_text(f'#!/bin/bash\nexec "{binary}" "$@" > "{log}" 2>&1\n')
    executable.chmod(0o755)
    cached = OUT/'busca-proof-bin'
    sources = [*ROOT.glob('src/*.c'), *ROOT.glob('src/dts/*.c'), ROOT/'src/desktop_mac.m']
    dependencies = [*sources, *ROOT.glob('src/*.h'), *ROOT.glob('src/*.inc')]
    if not cached.exists() or any(p.stat().st_mtime > cached.stat().st_mtime for p in dependencies):
        flags = ['-O1','-DNV_DESKTOP_RELEASE','-DNV_DESKTOP_UI','-DNV_TOUCH_UI','-I/opt/homebrew/include','-I/opt/homebrew/include/SDL2','-L/opt/homebrew/lib','-lSDL2','-lSDL2_image','-lSDL2_ttf','-lz','-framework','OpenGL','-framework','Cocoa','-framework','QuartzCore','-Wno-deprecated-declarations']
        for key in ('SUPABASE_URL','SUPABASE_ANON_KEY','TV_LOGIN_BASE','TRAKT_CLIENT_ID','TRAKT_CLIENT_SECRET','SIMKL_CLIENT_ID','SIMKL_APP','TMDB_API_KEY'):
            flags.append('-DNV_'+key+'=""')
        run('cc',*[str(p.relative_to(ROOT)) for p in sources],*flags,'-o',str(cached))
    run('cp', str(cached), str(binary))
    # Use the release's pinned SDL2 frameworks, not Homebrew's SDL2-to-SDL3 shim.
    frameworks = BASE/'cache/frameworks'
    for name, old in [('SDL2','sdl2-compat'),('SDL2_image','sdl2_image'),('SDL2_ttf','sdl2_ttf')]:
        target = frameworks/(name+'.framework')/'Versions/A'/name
        assert target.is_file(), 'Pinned SDL2 framework unavailable'
        run('ditto',str(frameworks/(name+'.framework')),str(app/'Contents/Frameworks'/(name+'.framework')))
        run('install_name_tool','-change',f'/opt/homebrew/opt/{old}/lib/lib{name}-2.0.0.dylib',f'@rpath/{name}.framework/Versions/A/{name}',str(binary))
    run('install_name_tool','-add_rpath','@executable_path/../Frameworks',str(binary))
    run('codesign','--force','--sign','-',str(app),capture_output=True)
    helper = runtime/'front.m'
    helper.write_text('#import <Cocoa/Cocoa.h>\nint main(){@autoreleasepool{printf("%d\\n",NSWorkspace.sharedWorkspace.frontmostApplication.processIdentifier);}}\n')
    run('cc',str(helper),'-framework','Cocoa','-o',str(runtime/'front'))
    def front():
        return int(subprocess.check_output([str(runtime/'front')]))
    foreground = front()
    env = dict(NUVIO_DADOS=str(data),HOME=str(home),NUVIO_UI_PREVIEW='1',NV_CAPTURA='1',NV_CAPTURA_GL='1',NV_CAPTURA_DIR=str(ipc),SDL_MAC_BACKGROUND_APP='1',SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN='1',TMPDIR=str(TMP))
    if NATIVE:
        del env['NV_CAPTURA_GL']
    launch = ['open','-n','-g','-j']
    for key,value in env.items():
        launch += ['--env',key+'='+value]
    log.write_text('')
    pid = None
    def command(value):
        pending = ipc/'nuvio-key'
        (ipc/'pending').write_text(value+'\n'); (ipc/'pending').replace(pending)
        wait(lambda: pending.stat().st_size==0,'Input timeout: '+value)
        time.sleep(.5)
    def shot(name):
        source = ipc/'nuvio-shot.bmp'
        source.unlink(missing_ok=True)
        (ipc/'nuvio-shot-req').write_text('1\n')
        wait(source.exists,'GL readback timeout')
        dest = OUT/(name.replace('busca-','busca-native-') if NATIVE else name)
        run('magick',str(source),'-resize','1440x900!',str(dest))
        return dest
    try:
        run(*launch,'-a',str(app),'--args',str(art))
        wait(lambda:'primeiro quadro na tela' in log.read_text(errors='replace'),'Background launch timeout')
        pid = int(subprocess.check_output(['pgrep','-f','^'+re.escape(str(binary))],text=True).splitlines()[-1])
        assert front()!=pid
        assert '[dados] gravando em '+str(data) in log.read_text(errors='replace')
        command('appearance:DarkAqua'); command('back'); command('janela:1440,900'); command('preview:search')
        time.sleep(2)
        shot('busca-empty.png')
        # A shared stem matches both the localized Duna titles and Dune people.
        command('input:dun')
        time.sleep(4)
        command('mover:1390,850')
        png = shot('busca-app.png')
        command('wheel:1000,700,0,-300,1'); command('wheel:1000,700,0,0,8')
        shot('busca-scroll.png')
        assert front()!=pid
        assert 'capture requires' not in log.read_text(errors='replace')
        if NATIVE:
            assert '[desktop-search] control=NSSearchField' in log.read_text(errors='replace')
        else:
            # Reject a stale/empty results capture: the first poster has real art.
            colors = int(subprocess.check_output(['magick',str(png),'-crop','144x216+312+350','-format','%k','info:']))
            red = float(subprocess.check_output(['magick',str(png),'-crop','144x216+312+350','-format','%[fx:mean.r]','info:']))
            assert colors > 1000 and red > .25, 'Missing result artwork'
            run('magick',str(BOARD),str(png),'+append','-quality','85',str(OUT/'busca-lado.jpg'))
            run('magick',str(OUT/'busca-lado.jpg'),'-resize','1000x1000>','-quality','80',str(OUT/'busca-lado-small.jpg'))
        (OUT/('busca-native-focus.json' if NATIVE else 'busca-focus.json')).write_text(json.dumps(dict(before=foreground,after=front(),app=pid)))
    finally:
        if pid is None:
            result = subprocess.run(['pgrep','-f','^'+re.escape(str(binary))],capture_output=True,text=True)
            if result.returncode==0:
                pid = int(result.stdout.splitlines()[-1])
        if pid:
            for sig in (signal.SIGTERM,signal.SIGKILL):
                try:
                    os.kill(pid,sig)
                except ProcessLookupError:
                    break
                time.sleep(.3)
print('PASS: Search 1440x900; isolated data; app stayed behind;', 'native NSSearchField created' if NATIVE else str(OUT/'busca-lado.jpg'))
