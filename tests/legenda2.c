// SECOND SUBTITLE SESSION (src/legenda2.c): bounded download, cancellation,
// session/selection generation guards, independent offset, explicit states,
// and the promise that the primary overlay is never touched.
//
//   bash tests/legenda2.sh                 (plain)
//   SANITIZE=1 bash tests/legenda2.sh      (ASan/UBSan)
//   SANITIZE=thread bash tests/legenda2.sh (TSan)
#include "legenda2.h"
#include "legenda.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The default downloader must never run in this test (no network).
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); abort(); }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; abort(); }
const char *i18n(const char *s) { return s; }

static const char *SRT_A =
  "1\n00:00:10,000 --> 00:00:12,000\nsegunda A\n\n"
  "2\n00:00:20,000 --> 00:00:22,000\nsegunda A2\n\n";
static const char *SRT_B =
  "1\n00:00:10,000 --> 00:00:12,000\nsegunda B\n\n";
static const char *ASS =
  "[Script Info]\nScriptType: v4.00+\n\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:10.00,0:00:12.00,Default,,0,0,0,,ass\n";

// Fake downloader: body chosen by URL; "bloqueia" waits for the gate and
// honours parar() like the real one (rede_pedir polls it during transfer).
static pthread_mutex_t mt = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int portaoAberto, emCurso, vistoParar, chamadas;
static int baixar(const char *url, long maxBytes, unsigned prazoMs,
                  int (*parar)(void *), void *u, char **corpo, long *n) {
  const char *b = NULL;
  (void)prazoMs;
  pthread_mutex_lock(&mt); chamadas++; emCurso = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&mt);
  if (strstr(url, "bloqueia")) {
    for (;;) {
      int ok;
      if (parar(u)) {
        pthread_mutex_lock(&mt); vistoParar = 1; emCurso = 0; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&mt);
        return -1;
      }
      pthread_mutex_lock(&mt); ok = portaoAberto; pthread_mutex_unlock(&mt);
      if (ok) break;
      usleep(1000);
    }
  }
  pthread_mutex_lock(&mt); emCurso = 0; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&mt);
  if (strstr(url, "/ass")) b = ASS;
  else if (strstr(url, "/a")) b = SRT_A;
  else if (strstr(url, "/b")) b = SRT_B;
  else if (strstr(url, "/vazio")) b = "nada aqui\n";
  else if (strstr(url, "/grande")) {
    *n = maxBytes + 1; *corpo = calloc(1, (size_t)*n + 1); return 0;
  } else return -1;
  *n = (long)strlen(b);
  *corpo = malloc((size_t)*n + 1);
  memcpy(*corpo, b, (size_t)*n + 1);
  return 0;
}

static Leg2Estado esperarFim(void) {
  int i;
  for (i = 0; i < 4000; i++) {
    Leg2Estado e = legenda2_estado();
    if (e != LEG2_CARREGANDO) return e;
    usleep(1000);
  }
  return legenda2_estado();
}
static void esperarEmCurso(void) {
  pthread_mutex_lock(&mt);
  while (!emCurso) pthread_cond_wait(&cv, &mt);
  pthread_mutex_unlock(&mt);
}
static void abrirPortao(int v) { pthread_mutex_lock(&mt); portaoAberto = v; pthread_mutex_unlock(&mt); }

static int textoEm(double t, char *dst, size_t n) {
  LegendaCue c[LEGENDA_SIMULTANEAS];
  int k = legenda2_cues(t, c, LEGENDA_SIMULTANEAS);
  if (k > 0) snprintf(dst, n, "%s", c[0].texto); else dst[0] = 0;
  return k;
}

static void *leitor(void *u) {
  int i;
  (void)u;
  for (i = 0; i < 20000; i++) {
    LegendaCue c[LEGENDA_SIMULTANEAS];
    LegendaDocumento *d;
    legenda2_cues(10.5 + (i % 3), c, LEGENDA_SIMULTANEAS);
    legenda2_estado();
    d = legenda2_documento();
    legenda_documento_liberar(d);
  }
  return NULL;
}

int main(void) {
  char t[128];
  unsigned gPrim;
  legenda2_definir_baixador(baixar);

  // The PRIMARY overlay, as if an external SRT were playing.
  legenda_definir_corpo("1\n00:00:10,000 --> 00:00:12,000\nprincipal\n\n");
  gPrim = legenda_geracao();

  // 1. Not ACTIVE before a document exists; then active with real cues.
  legenda2_reiniciar();
  abrirPortao(0);
  legenda2_escolher("a0000000000000001", "https://x.invalid/a?bloqueia", "en", "OpenSubtitles");
  esperarEmCurso();
  assert(legenda2_estado() == LEG2_CARREGANDO);
  assert(textoEm(10.5, t, sizeof t) == 0);          // nothing drawn while loading
  assert(legenda2_documento() == NULL);
  abrirPortao(1);
  assert(esperarFim() == LEG2_ATIVA);
  assert(textoEm(10.5, t, sizeof t) == 1 && !strcmp(t, "segunda A"));
  assert(!strcmp(legenda2_identidade(), "a0000000000000001"));
  { LegendaDocumento *d = legenda2_documento();
    const LegendaDocumentoInfo *in = legenda_documento_info(d);
    assert(d && !strcmp(in->identidade, "a0000000000000001") && !strcmp(in->idioma, "en"));
    assert(!strstr(in->identidade, "http"));
    legenda_documento_liberar(d); }

  // 2. Independent offset, applied once: +1000 ms of USER DELAY shows the cue
  //    1 s LATER (visible at 11.0, gone at 12.0); the primary keeps its own.
  legenda2_definir_offset_manual(1000);
  assert(textoEm(10.5, t, sizeof t) == 0);          // delayed: not yet
  assert(textoEm(11.5, t, sizeof t) == 1 && !strcmp(t, "segunda A"));
  legenda2_definir_offset_auto(500);
  assert(legenda2_offset_total() == 1500);
  // lookup = offAuto - manual = 500 - 1000 = -500: cue reappears when
  // pos - 0.5 >= 10 -> pos 10.5.
  assert(textoEm(10.4, t, sizeof t) == 0);
  assert(textoEm(10.6, t, sizeof t) == 1 && !strcmp(t, "segunda A"));
  { LegendaCue c[2];
    assert(legenda_cues(10.5, 0, c, 2) == 1 && !strcmp(c[0].texto, "principal"));
    assert(legenda_cues(9.5, 0, c, 2) == 0); }      // primary not shifted by the secondary
  legenda2_definir_offset_auto(0);
  legenda2_definir_offset_manual(60000); assert(legenda2_offset_total() == 60000);
  legenda2_definir_offset_manual(-60000); assert(legenda2_offset_total() == -60000);
  legenda2_definir_offset_manual(90000); assert(legenda2_offset_manual() == 60000);
  legenda2_definir_offset_auto(90000); assert(legenda2_offset_total() == 60000);
  legenda2_definir_offset_manual(0); assert(legenda2_offset_total() == 30000); // AutoSync bound unchanged
  legenda2_definir_offset_manual(0); legenda2_definir_offset_auto(0);

  // 3. Stale download after a NEW SELECTION: the first file never shows.
  abrirPortao(0);
  pthread_mutex_lock(&mt); emCurso = 0; pthread_mutex_unlock(&mt);
  legenda2_escolher("a0000000000000002", "https://x.invalid/a?bloqueia", "en", "P");
  esperarEmCurso();
  legenda2_escolher("a0000000000000003", "https://x.invalid/b", "en", "P");
  abrirPortao(1);
  assert(esperarFim() == LEG2_ATIVA);
  usleep(20000);
  assert(textoEm(10.5, t, sizeof t) == 1 && !strcmp(t, "segunda B"));
  assert(textoEm(20.5, t, sizeof t) == 0);          // A2 exists only in the stale file
  assert(!strcmp(legenda2_identidade(), "a0000000000000003"));

  // 4. Stale download after a MEDIA CHANGE: cancelled (parar seen), nothing
  //    published, state and offset reset.
  abrirPortao(0);
  pthread_mutex_lock(&mt); emCurso = 0; vistoParar = 0; pthread_mutex_unlock(&mt);
  legenda2_definir_offset_manual(750);
  legenda2_escolher("a0000000000000004", "https://x.invalid/a?bloqueia", "en", "P");
  esperarEmCurso();
  legenda2_reiniciar();
  pthread_mutex_lock(&mt); while (!vistoParar) pthread_cond_wait(&cv, &mt); pthread_mutex_unlock(&mt);
  usleep(20000);
  assert(legenda2_estado() == LEG2_NENHUMA);
  assert(legenda2_offset_manual() == 0);
  assert(textoEm(10.5, t, sizeof t) == 0 && !legenda2_identidade()[0]);

  // 5. Turn off cancels too.
  pthread_mutex_lock(&mt); emCurso = 0; vistoParar = 0; pthread_mutex_unlock(&mt);
  legenda2_escolher("a0000000000000005", "https://x.invalid/a?bloqueia", "en", "P");
  esperarEmCurso();
  legenda2_desligar();
  pthread_mutex_lock(&mt); while (!vistoParar) pthread_cond_wait(&cv, &mt); pthread_mutex_unlock(&mt);
  assert(legenda2_estado() == LEG2_NENHUMA);
  abrirPortao(1);

  // 6. Explicit failure/unsupported states, never ACTIVE, no document.
  legenda2_escolher("a0000000000000006", "https://x.invalid/ass", "en", "P");
  assert(esperarFim() == LEG2_NAO_SUPORTADA && !legenda2_documento());
  legenda2_escolher("a0000000000000007", "https://x.invalid/vazio", "en", "P");
  assert(esperarFim() == LEG2_FALHOU && !legenda2_documento());
  legenda2_escolher("a0000000000000008", "https://x.invalid/falha", "en", "P");
  assert(esperarFim() == LEG2_FALHOU);
  legenda2_escolher("a0000000000000009", "https://x.invalid/grande", "en", "P");
  assert(esperarFim() == LEG2_FALHOU);              // over the byte cap
  assert(textoEm(10.5, t, sizeof t) == 0);

  // 7. Concurrency: selections and media changes while the draw thread reads.
  { pthread_t r;
    int i;
    assert(pthread_create(&r, NULL, leitor, NULL) == 0);
    for (i = 0; i < 300; i++) {
      char id[24];
      snprintf(id, sizeof id, "a%016x", i);
      legenda2_escolher(id, i % 2 ? "https://x.invalid/a" : "https://x.invalid/b", "en", "P");
      if (i % 7 == 0) legenda2_reiniciar();
      if (i % 11 == 0) legenda2_desligar();
      legenda2_definir_offset_manual(i * 10 % 3000);
    }
    pthread_join(r, NULL); }
  legenda2_escolher("a00000000000000ff", "https://x.invalid/b", "en", "P");
  assert(esperarFim() == LEG2_ATIVA);

  // 8. The primary overlay was never touched by any of the above: same
  //    generation (no legenda_carregar/definir/desligar), same text.
  assert(legenda_geracao() == gPrim && legenda_ligada_em(gPrim));
  { LegendaCue c[2]; assert(legenda_cues(10.5, 0, c, 2) == 1 && !strcmp(c[0].texto, "principal")); }

  legenda2_encerrar();
  legenda2_encerrar();
  assert(legenda2_estado() == LEG2_NENHUMA);
  printf("legenda2: states, caps, cancel, stale session/selection, independent offset, primary untouched ok (%d downloads)\n", chamadas);
  return 0;
}
