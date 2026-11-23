// FALLBACK DO MOTOR ANTIGO (revisao PR734CD-r1, bloqueio 3): o "libass=0" que
// o painel NOVO (Legendas > Estilo) grava em assperf.txt nao pode atravessar
// para o motor antigo — o painel antigo nao oferece religar, e no texto
// simples os letreiros posicionados (\pos, \an7...) somem sem caminho de
// volta. Com a chave dos Ajustes no motor antigo a preferencia nao se
// aplica; no motor novo ela continua valendo.
//
//   bash tests/assmotor_fallback.sh
#include "legenda.h"
#include "assperf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dados_ler(const char *nome) {
  return strcmp(nome, "assperf.txt") ? NULL : strdup("libass=0\n");
}
int dados_apagar(const char *nome) { (void)nome; return 1; }

static int motor = 1;
static int ler_motor(void) { return __atomic_load_n(&motor, __ATOMIC_RELAXED); }

static const char *DOC =
  "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 480\n"
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\an7\\pos(64,48)}PLACA\n"
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Fala\n";

static int extrai(void) {
  LegendaCue *cues = NULL;
  int n = legenda_extrair_ass(DOC, &cues);
  free(cues);
  return n;
}

int main(void) {
  ass_novo_registrar(ler_motor);
  assperf_iniciar();
  printf("Libass do painel novo=%d\n", assperf_get(AP_LIBASS));
  if (assperf_get(AP_LIBASS) != 0) return 2;

  /* Motor NOVO + libass=0: texto simples, so dialogo (a preferencia vale). */
  __atomic_store_n(&motor, 1, __ATOMIC_RELAXED);
  int novo = extrai();
  printf("motor novo: eventos=%d\n", novo);
  if (novo != 1) return 2;

  /* Motor ANTIGO + libass=0: a preferencia nao atravessa — PLACA e Fala. */
  __atomic_store_n(&motor, 0, __ATOMIC_RELAXED);
  int antigo = extrai();
  printf("motor antigo: eventos=%d\n", antigo);
  if (antigo != 2) { printf("assmotor_fallback: FALHOU\n"); return 1; }

  printf("assmotor_fallback: ok\n");
  return 0;
}
