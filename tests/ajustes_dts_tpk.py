#!/usr/bin/env python3
"""The Samsung .tpk DTS/TrueHD row: one setting, Off / 5.1 / Stereo.

Compiles the real dtsTpkMigrar and ajustes_dts_tpk_modo out of src/ajustes.c
(as tests/ajustes_dts.py does) and checks the values saved by the two rows it
replaced ("DTS convertido em" = dtsSaidaLocal, and the delivery test =
dtsTpkModoLocal), plus the table entries that make the row on the .tpk.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
src = (ROOT / 'src/ajustes.c').read_text()
falhas = 0


def confere(ok, msg):
    global falhas
    print(('ok   ' if ok else 'FAIL ') + msg)
    falhas += 0 if ok else 1


def funcao(nome):
    m = re.search(r'^(?:static )?int ' + nome + r'\([^)]*\) \{', src, re.M)
    assert m, nome
    return src[m.start():src.index('\n}\n', m.end()) + 3]


# 1. Migration and the delivery the server gets, on the real code.
c = r'''
#include <stdio.h>
#define NV_TPK 1
enum { DTSTPK_HLS = 0, DTSTPK_PROG_TS, DTSTPK_DESLIGADO };
enum { AJ_DTS_AC3 = 0, AJ_N };
static int valor[AJ_N];
''' + funcao('dtsTpkMigrar') + funcao('ajustes_dts_tpk_modo') + r'''
int main(void) {
  /* modo antigo (-1 = nao gravado; 0..4 = algum modo ligado; 5 = Desligado), saida antiga (-1, 0 = Estereo, 1 = 5.1) */
  static const int caso[][3] = {
    { 5, 1, 0 }, { 5, 0, 0 }, { 5, -1, 0 },      /* teste Desligado: continua desligado   */
    { 0, 0, 2 }, { 1, 0, 2 }, { -1, 0, 2 },      /* Estereo de antes: Estereo              */
    { 0, 1, 1 }, { 4, 1, 1 }, { -1, 1, 1 },      /* 5.1 de antes: 5.1                      */
    { 0, -1, 1 }, { 3, -1, 1 },                  /* so o modo ligado: o padrao, 5.1        */
  };
  int f = 0;
  for (unsigned i = 0; i < sizeof caso / sizeof *caso; i++) {
    int v = dtsTpkMigrar(caso[i][0], caso[i][1]);
    printf("%s dtsTpkModoLocal %d + dtsSaidaLocal %d -> dtsTpkLocal %d (expected %d)\n",
           v == caso[i][2] ? "ok  " : "FAIL", caso[i][0], caso[i][1], v, caso[i][2]);
    f += v != caso[i][2];
  }
  static const int modo[3] = { DTSTPK_DESLIGADO, DTSTPK_HLS, DTSTPK_HLS };
  for (int v = 0; v < 3; v++) {
    valor[AJ_DTS_AC3] = v;
    printf("%s row value %d -> delivery %d (expected %d)\n", ajustes_dts_tpk_modo() == modo[v] ? "ok  " : "FAIL",
           v, ajustes_dts_tpk_modo(), modo[v]);
    f += ajustes_dts_tpk_modo() != modo[v];
  }
  return f;
}
'''
with tempfile.TemporaryDirectory() as d:
    (Path(d) / 't.c').write_text(c)
    subprocess.run(['cc', '-Wall', '-Werror', '-o', f'{d}/t', f'{d}/t.c'], check=True)
    r = subprocess.run([f'{d}/t'], capture_output=True, text=True)
    print(r.stdout, end='')
    falhas += r.returncode

# 2. The row on the .tpk: three values, its own saved key, both old keys read.
tpk = re.search(r'#ifdef NV_TPK\nstatic const char \*V_DTS_SAIDA\[\] = \{([^}]*)\};\n#define N_DTS_SAIDA 3\n#else\n'
                r'static const char \*V_DTS_SAIDA\[\] = \{([^}]*)\};\n#define N_DTS_SAIDA 2', src)
confere(tpk is not None and tpk.group(1).strip() == '"Desligado", "5.1 (Dolby Digital)", "Estéreo (AAC)"',
        '.tpk values: Desligado / 5.1 (Dolby Digital) / Estereo (AAC)')
confere(tpk is not None and tpk.group(2).strip() == '"Estéreo (AAC)", "5.1 (Dolby Digital)"', 'LG values unchanged')
confere(re.search(r'#ifdef NV_TPK\n  "dtsTpkLocal",[^\n]*\n#else\n  "dtsSaidaLocal",\n#endif', src) is not None,
        '.tpk saves the row as dtsTpkLocal, LG keeps dtsSaidaLocal')
confere('if (!strcmp(chave, "dtsSaidaLocal")) { dtsSaidaAntiga = v; continue; }' in src and
        'if (!strcmp(chave, "dtsTpkModoLocal")) { dtsModoAntigo = v; continue; }' in src and
        'valor[AJ_DTS_AC3] = dtsTpkMigrar(dtsModoAntigo, dtsSaidaAntiga);' in src,
        'the load reads both old keys and migrates when dtsTpkLocal is absent')
padrao = (ROOT / 'src/ajustes_ux_padrao.inc').read_text()
confere(re.search(r'#ifdef NV_TPK\n  1, /\* DTS/TrueHD on the Samsung \.tpk: 5\.1', padrao) is not None, '.tpk default: 5.1')
for velho in ('AJ_DTS_TPK_MODO', 'AJ_DTS_TPK_CORPO', 'dtsTpkCorpoLocal', 'ajustes_dts_tpk_corpo'):
    usos = [str(p.relative_to(ROOT)) for p in (ROOT / 'src').rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.inc')
            and velho in p.read_text(errors='ignore')]
    confere(not usos, f'{velho} is gone ({", ".join(usos) or "nowhere"})')

print(f'ajustes_dts_tpk: {falhas} failure(s)')
raise SystemExit(1 if falhas else 0)
