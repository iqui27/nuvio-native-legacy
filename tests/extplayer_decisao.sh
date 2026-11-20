#!/bin/bash
# Player setting (Internal / External / Ask) + headers -> decision. Ver o .c.
#   bash tests/extplayer_decisao.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-extplayer.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc src/extplayer.c tests/extplayer_decisao.c -o "$work/t"
"$work/t"

# Exercise production package persistence with legacy playback preferences.
python3 - "$work/prefs.c" <<'PY'
from pathlib import Path
import sys

source = Path('src/ajustes.c').read_text()
functions = source[source.index('static void ppGravar(void)'):source.index('int ajustes_player_externo(int anime)')]
Path(sys.argv[1]).write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char ppPkg[2][96];
enum { AJ_PLAYER_EXTERNO, AJ_PLAYER_ANIME };
static int valor[2];
static int ppN(void) { return 0; }
static int ppBase(int op) { return op == AJ_PLAYER_ANIME; }
static void dados_gravar(const char *path, const char *text) {
  FILE *f = fopen(path, "w");
  assert(f);
  assert(fputs(text, f) >= 0);
  assert(fclose(f) == 0);
}
static char *dados_ler(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) return NULL;
  char *text = calloc(1024, 1);
  assert(text);
  assert(fread(text, 1, 1023, f) < 1023);
  assert(!ferror(f));
  assert(fclose(f) == 0);
  return text;
}
''' + functions + r'''
int main(void) {
  const char *legacy = "aspecto 2\nleg_tamanho 160\nleg_cor 3\n";
  dados_gravar("player.txt", legacy);
  ppCarregar();
  assert(!ppPkg[0][0] && !ppPkg[1][0]);
  ppReconciliar();
  char *text = dados_ler("player.txt");
  assert(text && !strcmp(text, legacy));
  free(text);

  strcpy(ppPkg[0], "org.videolan.vlc");
  strcpy(ppPkg[1], "com.mxtech.videoplayer.ad");
  ppGravar();
  text = dados_ler("player.txt");
  assert(text && !strcmp(text, legacy));
  free(text);

  dados_gravar("player.txt", "aspecto 1\nleg_tamanho 200\nleg_cor 4\n");
  ppCarregar();
  assert(!strcmp(ppPkg[0], "org.videolan.vlc"));
  assert(!strcmp(ppPkg[1], "com.mxtech.videoplayer.ad"));
  ppReconciliar(); /* Neither saved app is installed. */
  ppCarregar();
  assert(!ppPkg[0][0] && !ppPkg[1][0]);
  text = dados_ler("player.txt");
  assert(text && !strcmp(text, "aspecto 1\nleg_tamanho 200\nleg_cor 4\n"));
  free(text);
  puts("external-player preferences: ok");
}
''')
PY
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror "$work/prefs.c" -o "$work/prefs"
(cd "$work" && ./prefs)
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -DNV_ANDROID -Isrc -I/opt/homebrew/include src/extplayer.c tests/extplayer_decisao.c -o "$work/ta"
"$work/ta"
