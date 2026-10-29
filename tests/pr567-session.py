"""Exercise playing-source lifecycle and the actual app playback recorder."""
from pathlib import Path
import subprocess
import sys
import tempfile

app = Path('src/app.c').read_text()
recorder = app.split('static void lembrarTocando(void) {', 1)[1].split('\nstatic void guardarPosicaoFonteFalha', 1)[0]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fontepref.h"
#include "dados.h"
static Stream source;
static Stream voltaFonte;
static int season, episode, recorded, aguardandoFonte;
static double position = 3.0;
int stream_n(void) { return 1; }
const Stream *stream_item(int i) { return i == 0 ? &source : NULL; }
int stream_grupo_sem_perda(int i, int forte) { (void)i; (void)forte; return 1; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
static const char *player_id_canal(void) { return ""; }
static int player_aberto(void) { return 1; }
static int player_quer_sair(void) { return 0; }
static int video_pronto(void) { return 1; }
static double video_pos(void) { return position; }
static int video_falhou(void) { return 0; }
static int player_fonte_falhou(void) { return 0; }
int stream_atual(void) { return 0; }
unsigned stream_lista_geracao(void) { return 1; }
static void alvoPlayer(char *d, unsigned n) { snprintf(d, n, "tt123:%d:%d", season, episode); }
static void player_episodio_atual(int *t, int *e) { *t = season; *e = episode; }
static void idBaseDoTitulo(char *d, unsigned n) { snprintf(d, n, "tt123"); }
static void record(const char *id, const Stream *s) { recorded++; fontepref_tocando(id, s); }
#define fontepref_tocando record
static void lembrarTocando(void) {
''' + recorder + r'''
#undef fontepref_tocando
static void seed(void) { fontepref_tocando("tt123:1:1", &source); }
static int playing(void) { return fontepref_tocando_de("tt123:1:2", NULL, 0, NULL, 0); }
int main(void) {
  strcpy(source.bingeGroup, "release-B");
  strcpy(source.provedor, "addon");
  fontepref_definir_perfil(1);
  seed(); assert(playing());
  fontepref_definir_perfil(2);
  assert(!playing());
  assert(fontepref_binge_tocando("tt123:1:2") == -1);
  assert(fontepref_escolher_lista_next("tt123:1:2", &source, 1, NULL, NULL) == -1);
  seed(); fontepref_definir_perfil(2); assert(playing());
  fontepref_esquecer(); assert(!playing());
  puts("PASS profile change/logout clears playing source; same profile preserves it");
  season = 0; episode = 1; position = 2.9; lembrarTocando(); assert(!recorded);
  position = 3.0; lembrarTocando(); assert(recorded == 1 && playing());
  episode = 2; lembrarTocando(); assert(recorded == 2);
  assert(fontepref_binge_tocando("tt123:0:3") == 0);
  season = -1; lembrarTocando(); assert(recorded == 2);
  season = 0; episode = 0; lembrarTocando(); assert(recorded == 2);
  puts("PASS S00 playback recorded after three seconds; invalid episodes rejected");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / 'session.c'
    src.write_text(code)
    includes = ['-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2'] if sys.platform == 'darwin' else subprocess.check_output(['pkg-config', '--cflags', 'sdl2'], text=True).split()
    subprocess.run(['cc', '-Wall', '-Wextra', '-Isrc', *includes, str(src), 'src/fontepref.c', '-lpthread', '-o', tmp + '/session'], check=True)
    subprocess.run([tmp + '/session'], check=True)
