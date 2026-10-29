// Legenda automatica: busca nos addons em PARALELO, lista publicada a cada resposta, cancelamento.
#include "stub_perfisessao.h"
#include "../src/addons.c"
#include <assert.h>
#include <unistd.h>
static int modo;                 // 0 = tempos, 1 = cancelar no meio, 3 = fila sem barreira
static _Atomic int vistoParcial;
static unsigned long vistoRapido; // modo 3: quando o ultimo provedor foi consultado
// #201: sem extras neste teste; o hash nunca e medido aqui.
char *rede_baixar_trecho_st(const char *u, int s, long long a, long long b, long *t, int *st, int *e, char *f, unsigned tf) {
  (void)u; (void)s; (void)a; (void)b; (void)t; (void)st; (void)e; (void)f; (void)tf;
  assert(!"Range inesperado"); return NULL;
}
static unsigned long agoraMs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
const char *rede_ultimo_erro(void) { return ""; }
char *rede_baixar_medido_controle(const char *url, int seconds, const char *const *h, const RedeControle *c, RedeMedida *m) {
  int idx = atoi(url + strlen("https://fixture.invalid/provider"));
  (void)h; (void)c;
  assert(seconds == LEG_TETO_S);
  if (modo == 2) {   // cancelar (fechou o player) enquanto os addons respondem
    if (idx == 0) { pthread_mutex_lock(&legTrava); legParar = 1; legGeracao++; pthread_mutex_unlock(&legTrava); }
    else usleep(300000);
  }
  else if (modo == 3) {   // Todos menos o ultimo ocupam os trabalhadores.
    if (idx < nAddon - 1) usleep(2000000);
    else vistoRapido = agoraMs();         // o valido precisa entrar antes do prazo
  }
  else if (modo == 1 && idx == 0) { static int uma; if (!uma++) { usleep(150000); pthread_mutex_lock(&legTrava); legGeracao++; pthread_mutex_unlock(&legTrava); } }
  else if (idx == 0) { usleep(500000); if (nLegs >= 1) vistoParcial = 1; }   // o rapido (2) ja publicou
  else if (idx == 1) usleep(500000);
  *m = (RedeMedida){ .status = 200, .bytes = 10, .ms = 1 };
  if (modo == 3 ? idx == nAddon - 1 : idx == 2) return strdup("{\"subtitles\":[{\"lang\":\"PORTUGUESE\",\"url\":\"https://fixture.invalid/pt.srt\"}]}");
  return strdup("{\"subtitles\":[{\"lang\":\"eng\",\"url\":\"https://fixture.invalid/en.srt\"}]}");
}
const char *i18n(const char *t) { return t; }
int main(void) {
  const char *nomes[] = { "A", "B", "C" };
  unsigned long t0, ms;
  memset(addon, 0, sizeof addon); nAddon = 3;
  for (int i = 0; i < 3; i++) {
    addon[i].ativo = addon[i].legenda = 1;
    snprintf(addon[i].base, sizeof addon[i].base, "https://fixture.invalid/provider%d", i);
    snprintf(addon[i].nome, sizeof addon[i].nome, "%s", nomes[i]);
  }
  ling_conta_legenda("pt"); ling_conta_legenda2(""); ling_local_legenda("");
  snprintf(legId, sizeof legId, "tt1"); snprintf(legTipo, sizeof legTipo, "movie");
  legParar = 0; fioLegVivo = 1; t0 = agoraMs(); buscarLegendas(NULL); ms = agoraMs() - t0;
  // 3 addons de 0,5 s / 0,5 s / ~0: serial seria >= 1000 ms.
  assert(ms < 900);
  assert(vistoParcial);                                   // a lista do rapido ja estava publicada
  assert(nLegs >= 1 && !strcmp(legs[0].provedor, "C") && ling_casa(legs[0].idioma, "pt"));  // "PORTUGUESE" casa
  // Titulo novo no meio da busca: a rodada velha e descartada, a nova conclui.
  modo = 1; nLegs = 0; fioLegVivo = 1; buscarLegendas(NULL);
  assert(!fioLegVivo && nLegs >= 1 && !strcmp(legs[0].provedor, "C"));
  // Parar (fechou o player) enquanto respondem: nada entra na lista.
  modo = 2; nLegs = 0; legParar = 0; fioLegVivo = 1; buscarLegendas(NULL);
  assert(nLegs == 0 && !fioLegVivo);
  // SATURACAO: 32 e 127 lentos antes do valido. A espera de 2 s representa
  // o teto real de 30 s: o ultimo precisa iniciar sem esperar a primeira leva.
  for (int total = 33; total <= ADD_MAX; total += ADD_MAX - 33) {
    modo = 3; nLegs = 0; legParar = 0; fioLegVivo = 1; vistoRapido = 0;
    memset(addon, 0, sizeof addon); nAddon = total;
    for (int i = 0; i < total; i++) {
      addon[i].ativo = addon[i].legenda = 1;
      snprintf(addon[i].base, sizeof addon[i].base, "https://fixture.invalid/provider%d", i);
      snprintf(addon[i].nome, sizeof addon[i].nome, "%d", i);
    }
    t0 = agoraMs(); buscarLegendas(NULL);
    assert(vistoRapido && vistoRapido - t0 < 1200);
    assert(nLegs >= 1 && atoi(legs[0].provedor) == total - 1 && ling_casa(legs[0].idioma, "pt"));
  }
  puts("addons paralelo: publicacao ao vivo, idioma, cancelamento e saturacao ate 128 addons ok");
  return 0;
}
