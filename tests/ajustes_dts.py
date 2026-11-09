#!/usr/bin/env python3
"""Exercise real settings persistence/account functions without SDL/GL.

The harness extracts production functions and tables rather than reproducing
account merge logic. Only unrelated settings metadata and language/image side
effects are stubbed; JSON parsing is linked from src/js.c.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/ajustes.c').read_text()


def function(name):
    match = re.search(r'^(?:static )?(?:int|void|const char \*const \*)\s*' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    # Functions end at a closing brace in column zero; nested blocks are indented.
    line_end = source.index('\n', match.end())
    end = line_end if source[match.end():line_end].rstrip().endswith('}') else source.index('\n}', match.end()) + 2
    return source[match.start():end]


def block(pattern):
    match = re.search(pattern, source, re.S)
    assert match, pattern
    return match.group()


# Preprocess the real presentation catalog: hidden settings must also stay out
# of search results and the differences list, which bypass visivel().
for macro in [None, 'NV_WEBOS', 'NV_TPK', 'NV_ANDROID']:
    command = [os.environ.get('CC', 'cc'), '-E', '-P', '-x', 'c']
    if macro:
        command.append('-D' + macro)
    command.append(str(ROOT / 'src/ajustes_ux_tela.inc'))
    catalog = subprocess.check_output(command, text=True)
    assert 'DTS' not in catalog, macro

harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "js.h"
'''
harness += block(r'typedef enum \{.*?\n\} OpcaoId;')
harness += block(r'static const char \*CHAVE\[\] = \{.*?\n\};')
harness += '\n#define LING_OPC_ORIGINAL 30\n#define FIL_LIMITE_PADRAO 7\n#define TXT_FAMILIA_INTER 0\n'
harness += block(r'#define AJ_N_TEMAS 12.*?#define AJ_N_TEMAS_OPC      24') + '\n'
harness += '\nstatic int valor[] = {\n#include "ajustes_ux_padrao.inc"\n};\n'
harness += r'''
_Static_assert(sizeof valor / sizeof *valor == AJ_N, "default count");
_Static_assert(sizeof CHAVE / sizeof *CHAVE == AJ_N, "key count");
enum { OP_ESCOLHA, OP_NUMERO, OP_LEITURA, OP_ACAO };
typedef struct { int tipo, min, max; } Opcao;
static Opcao OPCOES[AJ_N];
static char dirAjustes[512];
static int idiomaFonteGravada;
static int ajustes_idioma(void) { return 1; }
static void dados_marcar_sujo(int x) { (void)x; }
static void pstAplicar(void) {}
static int nValores(int op) { (void)op; return 2; }
static int temaLocal(void) { return 0; }
static void selospacote_conta_do_blob(const char *j) { (void)j; }
static void idiomasDoBlob(const char *j, const char *f) { (void)j; (void)f; }
static void idiomaContaDoBlob(const char *j, const char *f) { (void)j; (void)f; }
static void idiomaResolver(int n) { (void)n; }
static const char *const *literaisDe(int op) { (void)op; return NULL; }
'''
for name in ['limita', 'gravar', 'camelParaSnake',
             'igualSemCaixa', 'somenteDesteAparelho', 'ajustes_aplicar_blob',
             'dePerfil', 'acharValor', 'textoDoValor']:
    harness += '\n' + function(name) + '\n'
harness += '\ntypedef struct { const char *vi, *vf; char texto[160]; } Troca;\n'
harness += function('ajustes_mesclar_blob')
harness += r'''
int main(void) {
  char tmp[] = "/tmp/nuvio-dts-settings-XXXXXX", path[700], buf[32768];
  char *out = NULL;
  FILE *f;
  size_t n;
  int i;
  assert(mkdtemp(tmp));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", tmp);
  for (i = 0; i < AJ_N; ++i) OPCOES[i].tipo = OP_ACAO;
  OPCOES[AJ_HERO].tipo = OP_ESCOLHA;
  assert(dePerfil(AJ_HERO));
  assert(gravar());
  snprintf(path, sizeof path, "%s/ajustes.txt", tmp);
  f = fopen(path, "r"); assert(f);
  n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
  assert(!strstr(buf, "dtsSaidaLocal") && !strstr(buf, "dtsLgLocal"));
  // Legacy DTS keys are ignored, regardless of spelling,
  // wrapped value or flat value.
  assert(!ajustes_aplicar_blob("{\"dts_saida_local\":{\"type\":\"number\",\"value\":0}}"));
  assert(!ajustes_aplicar_blob("{\"dtsSaidaLocal\":0}"));
  assert(!ajustes_aplicar_blob("{\"dts_saida_local\":1}"));
  // Outbound merge excludes DTS even if a future account already has the key.
  assert(!ajustes_mesclar_blob("{\"dts_saida_local\":1}", &out) && !out);
  valor[AJ_HERO] = 1;
  assert(ajustes_mesclar_blob("{\"dts_saida_local\":0,\"hero_section_enabled\":true}", &out) == 1);
  assert(out && strstr(out, "\"dts_saida_local\":0") && strstr(out, "\"hero_section_enabled\":false"));
  free(out);
  // Verify the account reader actually runs: an ordinary account key changes.
  assert(ajustes_aplicar_blob("{\"hero_section_enabled\":true}") == 1);
  assert(valor[AJ_HERO] == 0);
  unlink(path); rmdir(tmp);
  puts("ajustes_dts: removed preference, legacy-key isolation and defaults passed");
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='nuvio-dts-settings-build-') as directory:
    path = Path(directory)
    (path / 'harness.c').write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-Wall', '-Wextra',
                    '-Werror', '-Wno-format-truncation', '-I' + str(ROOT / 'src'), str(path / 'harness.c'),
                    str(ROOT / 'src/js.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
