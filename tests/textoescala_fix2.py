#!/usr/bin/env python3
"""Focused review regressions; execute the production C measurement helpers."""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def source(name):
    return (ROOT / 'src' / name).read_text()


def function(text, name):
    start = re.search(r'(?:static )?(?:float|void) ' + name + r'\([^)]*\)\s*\{', text).start()
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def ccheck(code):
    with tempfile.TemporaryDirectory() as tmp:
        p = pathlib.Path(tmp)
        (p / 'test.c').write_text('#include <assert.h>\n#include <math.h>\n' + code)
        subprocess.run(['cc', str(p / 'test.c'), '-o', str(p / 'test')], check=True)
        subprocess.run([str(p / 'test')], check=True)


def settings():
    text = source('ajustes_ux_tela.inc')
    appearance = text.split('SEC("Aparência"', 1)[1].split('SEC(', 1)[0]
    assert 'OPC(AJ_ESCALA_TEXTO)' in appearance, 'AJ_ESCALA_TEXTO missing from Appearance navigation'
    assert appearance.index('ROT("Texto e tamanho")') < appearance.index('OPC(AJ_ESCALA_TEXTO)') < appearance.index('ROT("Movimento e relógio")')


def artwork():
    home, detail = source('home.c'), source('detail.c')
    code = '''
static float scale = 1;
float gfx_poster_k(void) { return 1 / scale; }
int cartazPadrao(void) { return 0; }
float escalaCartazPadrao(void) { return 1; }
float escalaDoAjuste(void) { return 1; }
float escalaPosterTelefone(void) { return 1; }
int telefoneui_ativo(void) { return 0; }
#define NV_PAD_CARTAZ_W 260
#define NV_PAD_CARTAZ_H 390
#define NV_CARD_W 212
#define NV_CARD_H 318
#define NV_CARD_LAND_W 360
#define NV_CARD_LAND_H 203
#define COL_FORMA_QUADRADO 1
#define COL_FORMA_POSTER 2
#define NV_DETP_EPM_THUMB_H 200
#define NV_DETP_EP_THUMB_H 190
#define NV_DETP_EP_W 360
#define NV_DETP_EP_GAP 24
#define NV_DETP_EP_PASSO 384
static int layout;
static float episode = 1;
int epApple(void) { return layout == 1; }
int epModerna(void) { return layout == 2; }
float epEscala(void) { return epModerna() ? episode : 1; }
'''
    code += function(home, 'medidaColecao') + '\n'
    code += '\n'.join(function(detail, name) for name in ['epThumbH', 'epCardW', 'epCardPasso'])
    code += '''
int main(void) {
  for (layout = 0; layout < 3; layout++) for (int e = 0; e < 3; e++) {
    episode = e == 0 ? .8f : e == 1 ? 1.f : 1.25f;
    scale = 1; float w = epCardW(), h = epThumbH();
    for (int i = 0; i < 6; i++) {
      float scales[] = {.85f,.9f,1,1.1f,1.2f,1.3f}; scale = scales[i];
      float cw, ch; medidaColecao(0, &cw, &ch);
      assert(fabsf(cw * scale - 360) < .001f);
      assert(fabsf(ch * scale - 203) < .001f);
      assert(fabsf(epCardW() * scale - w) < .001f);
      assert(fabsf(epThumbH() * scale - h) < .001f);
      assert(epCardPasso() > epCardW());
    }
  }
}
'''
    ccheck(code)


def rows():
    text = source('biblioteca.c')
    defines = '\n'.join(line for line in text.splitlines() if re.match(r'#define BIB_(POSTER_H|TIT_GAP|LINHA_PASSO|FOCO_ESCALA)\s', line))
    ccheck('''
static float scale = 1;
float gfx_poster_k(void) { return 1 / scale; }
''' + defines + '''
int main(void) {
  assert(fabsf(BIB_LINHA_PASSO - 422) < .001f);
  float scales[] = {.85f,.9f,1,1.1f,1.2f,1.3f};
  for (int i = 0; i < 6; i++) {
    scale = scales[i];
    float captionBottom = BIB_POSTER_H + BIB_TIT_GAP + 14 + 52;
    float nextTop = BIB_LINHA_PASSO - BIB_POSTER_H * BIB_FOCO_ESCALA / 2;
    assert(nextTop >= captionBottom);
  }
}
''')


def coverage():
    search = source('busca.c')
    assert '#define NV_TXT_TELA' in search
    assert '330.0f * gfx_poster_k()' in search and '186.0f * gfx_poster_k()' in search, 'Search best-result artwork scales'
    draw = function(search, 'busca_desenhar')
    assert 'TXT_INI()' in draw and 'TXT_FIM()' in draw, 'Search missing text layer'
    vt = source('vertudo.c')
    assert 'gfx_escala_txt()' in function(vt, 'vtEscala'), 'TV View all ignores scale'
    draw = function(vt, 'vertudo_desenhar')
    assert 'if (vtTelefone()) gfx_escala_sair' not in draw, 'TV View all skips layer'
    keyboard = source('teclado.c')
    draw = function(keyboard, 'teclado_desenhar')
    assert 'TXT_INI()' in draw and 'ESCALA_REAL_INI()' not in draw, 'TV keyboard ignores scale'
    assert '#define NV_TXT_TELA' in keyboard
    assert 'TXT_INI()' in function(keyboard, 'teclado_evento'), 'Keyboard event geometry skips text layer'
    assert 'TXT_INI()' in function(keyboard, 'teclado_atualizar'), 'Keyboard update geometry skips text layer'
    ccheck('''
static float scale = 1;
float gfx_poster_k(void) { return 1 / scale; }
#define BU_CARTAZ_W (180.0f * gfx_poster_k())
float gfx_escala_txt(void) { return scale; }
float gfx_escala(void) { return scale; }
void gfx_escala_sair(float value) { (void)value; }
int buTelefone(void) { return 0; }
int buTecladoNativo(void) { return 0; }
float buPasso(void) { return scale == 1 ? 202 : 171; }
''' + function(search, 'buCartazW') + '''
int main(void) {
  float scales[] = {.85f,1,1.2f,1.3f};
  for (int i = 0; i < 4; i++) {
    scale = scales[i];
    assert(fabsf(buCartazW() * scale - 180) < .001f);
  }
}
''')


def proof():
    with tempfile.TemporaryDirectory() as tmp:
        p = pathlib.Path(tmp)
        base, target = p / 'base', p / 'target'
        base.write_text('F 0\nT 20\nR 100 50\nC 7 180 270 20 20\nC 8 360 203 220 20\n')
        target.write_text('F 0\nT 24\nR 120 60\nC 7 180 270 20 20\nC 8 432 243.6 220 20\n')
        cmd = [sys.executable, str(ROOT / 'tests/textoescala_confere.py'), 'fixture', str(base), str(target), '1.2']
        assert subprocess.run(cmd, capture_output=True).returncode != 0, 'resized landscape accepted'
        target.write_text('F 0\nT 24\nR 120 60\nC 7 180 270 20 20\n')
        assert subprocess.run(cmd, capture_output=True).returncode != 0, 'missing card accepted'
        target.write_text('F 0\nT 24\nR 120 60\nC 7 180 270 20 20\nC 7 180 270 220 20\n')
        assert subprocess.run(cmd, capture_output=True).returncode != 0, 'replaced card size accepted'
        target.write_text('F 0\nT 24\nR 120 60\nC 7 180 270 20 20\nC 8 360 203 220 20\n')
        assert subprocess.run(cmd, capture_output=True).returncode == 0, 'preserved cards rejected'
        # Run the actual hash checker used by the shell proof.
        cmd = [sys.executable, str(ROOT / 'tests/textoescala_hashes.py'), str(base), str(target)]
        for left, right, rc in [('a f\na f\na f\n', 'a f\na f\na f\n', 0),
                                ('a f\nb f\nc f\n', 'd f\ne f\nf f\n', 1),
                                ('a f\na f\na f\n', 'b f\nb f\nb f\n', 1),
                                ('a f\na f\na f\n', '', 1)]:
            base.write_text(left)
            target.write_text(right)
            assert subprocess.run(cmd, capture_output=True).returncode == rc
    shell = (ROOT / 'tests/textoescala_prova.sh').read_text()
    assert 'textoescala_hashes.py' in shell
    assert 'paisagem' not in shell


checks = {'settings': settings, 'artwork': artwork, 'rows': rows, 'coverage': coverage, 'proof': proof}
for name in sys.argv[1:] or checks:
    checks[name]()
    print('PASS', name)
