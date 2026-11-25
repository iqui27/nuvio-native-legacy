#!/usr/bin/env python3
"""Icons for the emojis that stream formatters print (source sheet, "From addon").

Reads tools/icones-formatador.tsv and writes:

  deploy/app/art/icones/aj_<lucide>.png           Minimalistic: Lucide, 128 px, white, shape in alpha
  deploy/app/art/icones/lucide/<lucide>.svg       the upstream Lucide SVG, unedited (proof of origin)
  deploy/app/art/icones/formatador/tw_<seq>.png   Standard, colour emoji: Twemoji 72 px, as shipped
  deploy/app/art/icones/formatador/sy_<cp>.png    Standard, monochrome symbol: Noto glyph, 128 px,
                                                  white, shape in alpha (tinted with the text colour)
  deploy/app/art/icones/formatador/fl_<CC>.png    flags (both styles): flagpack-core, 64x48
  deploy/app/art/icones/formatador/LICENSE-*      licence texts (no .txt: tizen-art.sh refuses *.txt)
  src/formatador_icones.inc                       the lookup table compiled into streams.c

Sources are pinned (same reason as tools/icones-lucide.sh: main changes without notice):
  Lucide           lucide-icons/lucide      tag 1.48.0  (ISC)
  Twemoji          jdecked/twemoji          tag v17.0.3 (graphics CC-BY 4.0) -- the maintained fork of
                   twitter/twemoji; twitter's set lacks the Emoji 15.1 family sequence on the list.
  flagpack         Yummygum/flagpack-core   main @ 2026-06-18 (MIT)
  Noto Sans Symbols / Symbols 2 / Math      @fontsource 5.3.0 npm packages (OFL 1.1)

Needs: git, npm, rsvg-convert, ImageMagick (convert), Python Pillow + fontTools.
Usage: python3 tools/icones-formatador.py [--fontes DIR]   (DIR: where to fetch sources, default /tmp)
"""
import argparse, glob, json, os, shutil, subprocess, sys, tarfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONES = os.path.join(RAIZ, 'deploy/app/art/icones')
FMT = os.path.join(ICONES, 'formatador')
INC = os.path.join(RAIZ, 'src/formatador_icones.inc')

LUCIDE = ('https://github.com/lucide-icons/lucide', '1.48.0')
TWEMOJI = ('https://github.com/jdecked/twemoji', 'v17.0.3')
FLAGPACK = ('https://github.com/Yummygum/flagpack-core', 'main')
NPM_FONTES = ['@fontsource/noto-sans-symbols@5.3.0', '@fontsource/noto-sans-symbols-2@5.3.0',
              '@fontsource/noto-sans-math@5.3.0']


def sh(*a, **k):
    return subprocess.run(a, check=True, **k)


def repo(dir_base, url, ref):
    """Blobless clone; files are checked out on demand by `pegar`."""
    d = os.path.join(dir_base, url.rstrip('/').split('/')[-2] + '_' + url.rstrip('/').split('/')[-1])
    if not os.path.isdir(os.path.join(d, '.git')):
        sh('git', 'clone', '-q', '--depth', '1', '--branch', ref, '--filter=blob:none', '--no-checkout', url, d)
    return d


def arvore(d, sub):
    out = subprocess.run(['git', '-C', d, 'ls-tree', '--name-only', 'HEAD', sub + '/'],
                         check=True, capture_output=True, text=True).stdout.split()
    return {os.path.basename(p) for p in out}


def pegar(d, caminhos):
    caminhos = [c for c in caminhos if not os.path.exists(os.path.join(d, c))]
    for i in range(0, len(caminhos), 200):
        sh('git', '-C', d, 'checkout', '-q', 'HEAD', '--', *caminhos[i:i + 200])


def ler_tabela():
    linhas = []
    with open(os.path.join(RAIZ, 'tools/icones-formatador.tsv'), encoding='utf-8') as f:
        for l in f:
            if l.startswith('#') or not l.strip():
                continue
            seq, tipo, luc, glifo, sig = l.rstrip('\n').split('\t')
            linhas.append(dict(seq=seq, tipo=tipo, lucide=None if luc == '-' else luc, glifo=glifo, sig=sig))
    return linhas


def twemoji_nome(seq, disponiveis):
    """Twemoji keeps FE0F in some file names and drops it in others."""
    cps = seq.split('-')
    cands = ['-'.join(cps), '-'.join(cps) + '-fe0f']
    if len(cps) > 1:
        cands.append('-'.join([cps[0], 'fe0f'] + cps[1:]))
        cands.append('-'.join(c + ('-fe0f' if c in ('2642', '2640') else '') for c in cps))
        cands.append('-'.join([cps[0], 'fe0f'] + cps[1:]) .replace('-2642', '-2642-fe0f'))
    for c in cands:
        if c + '.png' in disponiveis:
            return c
    return None


def svg_branco_png(svg, png, lado=128):
    """Same recipe as icones-lucide.sh: currentColor -> white, raster at 512, box-filter to 128."""
    tmp = png + '.512.png'
    txt = open(svg, encoding='utf-8').read().replace('currentColor', '#ffffff')
    with open(png + '.svg', 'w', encoding='utf-8') as f:
        f.write(txt)
    sh('rsvg-convert', '-w', '512', '-h', '512', '-o', tmp, png + '.svg')
    sh('convert', tmp, '-filter', 'box', '-resize', f'{lado}x{lado}', '-strip', 'PNG32:' + png)
    os.remove(tmp); os.remove(png + '.svg')


def fontes_noto(dir_base):
    d = os.path.join(dir_base, 'noto-fontsource')
    os.makedirs(d, exist_ok=True)
    for pk in NPM_FONTES:
        nome = pk.split('/')[1].replace('@', '-')
        if not glob.glob(os.path.join(d, 'fontsource-' + nome + '*')):
            sh('npm', 'pack', '-q', pk, cwd=d, stdout=subprocess.DEVNULL)
    for t in glob.glob(os.path.join(d, '*.tgz')):
        alvo = t[:-4]
        if not os.path.isdir(alvo):
            with tarfile.open(t) as z:
                z.extractall(alvo, filter='data')
    return sorted(glob.glob(os.path.join(d, '*/package/files/*-400-normal.woff'))), d


def glifo_png(cp, fontes, png):
    from PIL import Image, ImageDraw, ImageFont
    from fontTools.ttLib import TTFont
    for f in fontes:
        if cp in TTFont(f).getBestCmap():
            break
    else:
        return False
    # One em size for every symbol (so ▪ stays small and ◈ stays big, as the
    # formatter's author saw them), centred on the font's ascender/descender
    # middle like a text glyph. Shrunk only if the ink would leave the tile.
    lado, tam = 128, 112
    while True:
        fonte = ImageFont.truetype(f, tam)
        im = Image.new('L', (lado, lado), 0)
        dr = ImageDraw.Draw(im)
        x0, y0, x1, y1 = dr.textbbox((lado / 2, lado / 2), chr(cp), font=fonte, anchor='mm')
        if (x1 - x0 <= lado - 8 and y1 - y0 <= lado - 8) or tam < 40:
            break
        tam -= 4
    dx = (lado - (x1 - x0)) / 2 - x0 if (x0 < 4 or x1 > lado - 4) else 0
    dy = (lado - (y1 - y0)) / 2 - y0 if (y0 < 4 or y1 > lado - 4) else 0
    dr.text((lado / 2 + dx, lado / 2 + dy), chr(cp), font=fonte, fill=255, anchor='mm')
    branco = Image.new('RGBA', (lado, lado), (255, 255, 255, 0))
    branco.putalpha(im)
    branco.save(png, optimize=True)
    return True


def utf8_c(seq):
    return ''.join('\\x%02X' % b for b in ''.join(chr(int(c, 16)) for c in seq.split('-')).encode('utf-8'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fontes', default='/tmp/nuvio-icones-formatador')
    a = ap.parse_args()
    os.makedirs(a.fontes, exist_ok=True)
    os.makedirs(FMT, exist_ok=True)
    tab = ler_tabela()

    # --- Lucide (Minimalistic) ---
    dl = repo(a.fontes, *LUCIDE)
    nomes = sorted({t['lucide'] for t in tab if t['lucide']})
    pegar(dl, ['icons/%s.svg' % n for n in nomes] + ['LICENSE'])
    os.makedirs(os.path.join(ICONES, 'lucide'), exist_ok=True)
    novos = 0
    for n in nomes:
        svg = os.path.join(ICONES, 'lucide', n + '.svg')
        if not os.path.exists(svg):
            shutil.copy(os.path.join(dl, 'icons', n + '.svg'), svg)
        png = os.path.join(ICONES, 'aj_%s.png' % n)
        if not os.path.exists(png):   # never redraw an icon another screen already ships
            svg_branco_png(svg, png); novos += 1
    print('lucide: %d icons, %d new' % (len(nomes), novos))

    # --- Twemoji (Standard, colour) ---
    dt = repo(a.fontes, *TWEMOJI)
    disp = arvore(dt, 'assets/72x72')
    tw = {}
    for t in tab:
        if t['tipo'] != 'cor':
            continue
        n = twemoji_nome(t['seq'], disp)
        if not n:
            sys.exit('twemoji: no file for ' + t['seq'])
        tw[t['seq']] = n
    pegar(dt, ['assets/72x72/%s.png' % n for n in tw.values()] + ['LICENSE-GRAPHICS'])
    for seq, n in tw.items():
        sh('convert', os.path.join(dt, 'assets/72x72', n + '.png'), '-strip', 'PNG32:' + os.path.join(FMT, 'tw_%s.png' % seq))
    shutil.copy(os.path.join(dt, 'LICENSE-GRAPHICS'), os.path.join(FMT, 'LICENSE-twemoji'))
    print('twemoji: %d' % len(tw))

    # --- Noto glyphs (Standard, monochrome) ---
    fontes, dn = fontes_noto(a.fontes)
    sy = []
    for t in tab:
        if t['tipo'] != 'mono':
            continue
        cp = int(t['seq'], 16)
        if not glifo_png(cp, fontes, os.path.join(FMT, 'sy_%s.png' % t['seq'])):
            sys.exit('noto: no glyph for U+%04X' % cp)
        sy.append(t['seq'])
    lic = glob.glob(os.path.join(dn, '*/package/LICENSE'))
    if lic:
        shutil.copy(lic[0], os.path.join(FMT, 'LICENSE-noto'))
    print('noto: %d' % len(sy))

    # --- flagpack (flags, both styles) ---
    df = repo(a.fontes, *FLAGPACK)
    todas = arvore(df, 'lib/flags/m')
    cods = sorted(n[:-4] for n in todas if len(n) == 6 and n[:2].isupper() and n[:2].isalpha())
    sub = ['GB-ENG', 'GB-SCT', 'GB-WLS']
    pegar(df, ['lib/flags/m/%s.svg' % c for c in cods + sub + ['GB-UKM']] + ['LICENSE'])
    for c in cods + sub + ['GB-UKM']:
        nome = {'GB-UKM': 'GB'}.get(c, c)
        if c == 'GB':
            continue   # flagpack's own "GB" is absent; GB-UKM is the Union flag
        sh('rsvg-convert', '-w', '64', '-h', '48', '-o', os.path.join(FMT, 'fl_%s.png' % nome),
           os.path.join(df, 'lib/flags/m', c + '.svg'))
    shutil.copy(os.path.join(df, 'LICENSE'), os.path.join(FMT, 'LICENSE-flagpack'))
    print('flagpack: %d' % (len(cods) + len(sub)))

    # --- lookup table ---
    with open(INC, 'w', encoding='utf-8') as f:
        f.write('// GERADO por tools/icones-formatador.py a partir de tools/icones-formatador.tsv.\n')
        f.write('// Nao editar a mao. Ordem: sequencias mais longas primeiro (ZWJ antes da base).\n')
        f.write('// { utf8 sem FE0F, Padrao, Minimalista, cor (1 = desenhar com as cores do PNG) }\n')
        tab_c = [t for t in tab if t['tipo'] != 'texto']
        tab_c.sort(key=lambda t: -len(utf8_c(t['seq'])))
        for t in tab_c:
            if t['tipo'] == 'cor':
                pad, cor = 'formatador/tw_' + t['seq'], 1
            else:
                pad, cor = 'formatador/sy_' + t['seq'], 0
            f.write('{ "%s", "%s", "aj_%s", %d },   // %s %s\n' % (utf8_c(t['seq']), pad, t['lucide'], cor, t['glifo'], t['sig']))
    print('table: %s' % INC)


if __name__ == '__main__':
    main()
