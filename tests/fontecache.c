// O cache de fontes por canal e o prefetch dos vizinhos do guia.
//
// POR QUE ESTE TESTE EXISTE. As regras de fontecache.c sao do tipo que falha em
// silencio: uma entrada expirada que e servida toca um link morto "com cara de
// fresco"; um prefetch que nao cede atrasa o canal que a pessoa acabou de
// pedir; uma lista pela metade guardada como inteira faz um canal perder
// fontes sem nenhum erro no log. Nenhuma tela mostra isso. Entao cada regra e
// exercitada aqui, com o relogio na mao e a rede fingida.
//
// A rede e um DUBLE de addons_consultar que bloqueia ate o teste soltar — e
// assim que se observa "em curso", "cedeu" e "descartou" sem dormir esperando
// o acaso. addons_ocupado e player_carregando tambem sao dubles: sao ENTRADAS
// do prefetch, e o teste as controla.
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <SDL2/SDL_stdinc.h>

static Uint32 relogio = 100000;
static Uint32 relogioTeste(void) { return relogio; }
#define FC_AGORA() relogioTeste()
#include "../src/fontecache.c"

// --- dubles ------------------------------------------------------------------------

static int buscaOcupada;
static int playerCarregando;
int addons_ocupado(void)       { return buscaOcupada; }
int player_carregando(void)    { return playerCarregando; }
int addons_n(void)             { return 3; }

// A "rede": tres addons, cada um esperando o teste soltar (`solto[k]`) ou o
// cancelamento. Registra o que viu para o teste conferir.
static pthread_mutex_t redeTrava = PTHREAD_MUTEX_INITIALIZER;
static int solto[3];
static int consultas;            // quantas vezes foi chamada
static char ultimoId[64];        // id da ultima chamada
static int dentro;               // 1 enquanto uma chamada esta bloqueada
static int addonsPerguntados;    // ate onde a ultima chamada chegou
static int ignoraCancel;         // devolve lista mesmo cancelada (simula a corrida)
static int fontesPorConsulta = 2;

static void soltarTudo(void) {
  pthread_mutex_lock(&redeTrava);
  solto[0] = solto[1] = solto[2] = 1;
  pthread_mutex_unlock(&redeTrava);
}
static void prenderTudo(void) {
  pthread_mutex_lock(&redeTrava);
  solto[0] = solto[1] = solto[2] = 0;
  pthread_mutex_unlock(&redeTrava);
}
static void soltarAddon(int k) {
  pthread_mutex_lock(&redeTrava); solto[k] = 1; pthread_mutex_unlock(&redeTrava);
}

int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  int k, n = 0, i;
  (void)base;
  Stream *l;
  (void)fios;
  assert(!strcmp(tipo, "tv"));   // o prefetch pede o mesmo tipo que tocarCanal
  *saida = NULL;
  pthread_mutex_lock(&redeTrava);
  consultas++;
  snprintf(ultimoId, sizeof ultimoId, "%s", id);
  dentro = 1;
  addonsPerguntados = 0;
  pthread_mutex_unlock(&redeTrava);
  for (k = 0; k < 3; k++) {
    for (;;) {
      int s, c = cancelado ? cancelado(ctx) : 0;
      pthread_mutex_lock(&redeTrava); s = solto[k]; pthread_mutex_unlock(&redeTrava);
      if (s || c) break;
      usleep(500);
    }
    if (!ignoraCancel && cancelado && cancelado(ctx)) {
      pthread_mutex_lock(&redeTrava); dentro = 0; pthread_mutex_unlock(&redeTrava);
      return -1;
    }
    pthread_mutex_lock(&redeTrava); addonsPerguntados = k + 1; pthread_mutex_unlock(&redeTrava);
  }
  n = fontesPorConsulta;
  l = calloc((size_t)(n > 0 ? n : 1), sizeof(Stream));
  for (i = 0; i < n; i++) snprintf(l[i].url, sizeof l[i].url, "https://x/%s/%d.m3u8", id, i);
  pthread_mutex_lock(&redeTrava); dentro = 0; pthread_mutex_unlock(&redeTrava);
  if (n <= 0) { free(l); return 0; }
  *saida = l;
  return n;
}

// --- apoio -----------------------------------------------------------------------

static int fioVivo(void) {
  int v;
  pthread_mutex_lock(&trava); v = vivo; pthread_mutex_unlock(&trava);
  return v;
}
static int redeDentro(void) {
  int d;
  pthread_mutex_lock(&redeTrava); d = dentro; pthread_mutex_unlock(&redeTrava);
  return d;
}
static int nConsultas(void) {
  int c;
  pthread_mutex_lock(&redeTrava); c = consultas; pthread_mutex_unlock(&redeTrava);
  return c;
}
// Espera com teto: um teste que trava e pior que um que falha.
static void esperarAte(int (*cond)(void), int quero, const char *oque) {
  int i;
  for (i = 0; i < 4000 && cond() != quero; i++) usleep(1000);
  if (cond() != quero) { fprintf(stderr, "TRAVOU esperando %s\n", oque); assert(0); }
}
static void esperarFio(void)   { esperarAte(fioVivo, 0, "o fio de prefetch terminar"); }
static void esperarDentro(void){ esperarAte(redeDentro, 1, "a rede ser chamada"); }

static Stream *listaDe(int n, const char *marca) {
  Stream *l = calloc((size_t)n, sizeof(Stream));
  int i;
  for (i = 0; i < n; i++) snprintf(l[i].rotulo, sizeof l[i].rotulo, "%s-%d", marca, i);
  return l;
}

// Um engatilhar que ja passou o descanso do foco e ja foi avancado.
static void engatilharEAvancar(const char *antes, const char *depois) {
  fontecache_engatilhar(antes, NULL, depois, NULL);
  relogio += FONTECACHE_ESPERA_MS;
  fontecache_avancar();
}

int main(void) {
  Stream *l; int n;

  // 1. ACERTO, E ACERTO CONSOME. O que entra sai igual, uma vez so.
  { Stream *g = listaDe(3, "a");
    fontecache_guardar("canal:a", "tv", g, 3);
    free(g);
    assert(fontecache_n() == 1);
    assert(fontecache_pegar("canal:a", "tv", &l, &n) == FC_ACERTO);
    assert(n == 3 && !strcmp(l[2].rotulo, "a-2"));
    free(l);
    assert(fontecache_pegar("canal:a", "tv", &l, &n) == FC_NADA);
    assert(fontecache_n() == 0); }
  puts("ok  acerto devolve a lista e consome a entrada");

  // 2. A CHAVE E id+tipo. "channel" e "tv" sao canais mas sao chaves distintas
  //    (tocarCanal pede "tv"; o cache nao adivinha).
  { Stream *g = listaDe(1, "b");
    fontecache_guardar("canal:b", "channel", g, 1);
    assert(fontecache_pegar("canal:b", "tv", &l, &n) == FC_NADA);
    assert(fontecache_pegar("canal:b", "channel", &l, &n) == FC_ACERTO);
    free(l); free(g); }
  puts("ok  a chave e id+tipo");

  // 3. O QUE NAO ENTRA: filme, lista vazia, lista acima do teto.
  { Stream *g = listaDe(FONTECACHE_FONTES_MAX + 1, "c");
    fontecache_guardar("tt1", "movie", g, 2);
    fontecache_guardar("canal:c", "tv", g, 0);
    fontecache_guardar("canal:c", "tv", g, FONTECACHE_FONTES_MAX + 1);
    assert(fontecache_n() == 0);
    fontecache_guardar("canal:c", "tv", g, FONTECACHE_FONTES_MAX);
    assert(fontecache_n() == 1);
    assert(fontecache_pegar("canal:c", "tv", &l, &n) == FC_ACERTO && n == FONTECACHE_FONTES_MAX);
    free(l); free(g); }
  puts("ok  filme, vazio e acima do teto ficam fora; no teto entra");

  // 4. EXPIRACAO. No limite ainda vale; um ms depois nao — e a memoria e solta.
  { Stream *g = listaDe(2, "d");
    fontecache_guardar("canal:d", "tv", g, 2);
    relogio += FONTECACHE_VALIDADE_MS;
    assert(fontecache_n() == 1);
    relogio += 1;
    assert(fontecache_n() == 0);
    assert(fontecache_pegar("canal:d", "tv", &l, &n) == FC_NADA);
    { int i, ocupadas = 0;
      pthread_mutex_lock(&trava);
      for (i = 0; i < FONTECACHE_MAX; i++) if (cache[i].lista) ocupadas++;
      pthread_mutex_unlock(&trava);
      assert(ocupadas == 0); }
    free(g); }
  puts("ok  expirada nao e servida e libera a memoria");

  // 5. TETO DE ENTRADAS: a quinta expulsa a MAIS ANTIGA, e so ela.
  { Stream *g = listaDe(1, "e");
    char id[16]; int k;
    for (k = 0; k < FONTECACHE_MAX + 1; k++) {
      snprintf(id, sizeof id, "canal:e%d", k);
      fontecache_guardar(id, "tv", g, 1);
      relogio += 10;
    }
    assert(fontecache_n() == FONTECACHE_MAX);
    assert(fontecache_pegar("canal:e0", "tv", &l, &n) == FC_NADA);
    for (k = 1; k < FONTECACHE_MAX + 1; k++) {
      snprintf(id, sizeof id, "canal:e%d", k);
      assert(fontecache_pegar(id, "tv", &l, &n) == FC_ACERTO);
      free(l);
    }
    // Guardar DE NOVO o mesmo id substitui, nao duplica.
    fontecache_guardar("canal:e9", "tv", g, 1);
    fontecache_guardar("canal:e9", "tv", g, 1);
    assert(fontecache_n() == 1);
    assert(fontecache_pegar("canal:e9", "tv", &l, &n) == FC_ACERTO); free(l);
    free(g); }
  puts("ok  teto de entradas expulsa a mais antiga; mesmo id substitui");

  // 6. O PREFETCH ESPERA O FOCO DESCANSAR, e busca o de BAIXO primeiro.
  soltarTudo();
  fontecache_engatilhar("canal:cima", NULL, "canal:baixo", NULL);
  usleep(20000);
  assert(nConsultas() == 0);                       // 0 ms: segurando a seta
  relogio += FONTECACHE_ESPERA_MS - 1;
  fontecache_avancar();
  usleep(20000);
  assert(nConsultas() == 0);
  relogio += 1;
  fontecache_avancar();
  esperarFio();
  assert(nConsultas() == 1 && !strcmp(ultimoId, "canal:baixo"));
  fontecache_avancar();                            // a vez do segundo vizinho
  esperarFio();
  assert(nConsultas() == 2 && !strcmp(ultimoId, "canal:cima"));
  assert(fontecache_n() == 2);
  fontecache_avancar();                            // nada pendente: nada acontece
  usleep(20000);
  assert(nConsultas() == 2);
  puts("ok  prefetch arranca so com o foco parado, de baixo antes de cima, um por vez");

  // 7. VIZINHO JA NO CACHE NAO E PEDIDO DE NOVO. Ponta da lista (NULL) tambem nao.
  engatilharEAvancar(NULL, "canal:baixo");
  usleep(20000);
  assert(nConsultas() == 2 && !fioVivo());
  assert(fontecache_pegar("canal:baixo", "tv", &l, &n) == FC_ACERTO); free(l);
  assert(fontecache_pegar("canal:cima", "tv", &l, &n) == FC_ACERTO); free(l);
  puts("ok  vizinho ja guardado e ponta da lista nao geram requisicao");

  // 8. NAO ARRANCA com a busca principal ocupada nem com o player carregando.
  buscaOcupada = 1;
  fontecache_engatilhar("canal:f", NULL, NULL, NULL);
  relogio += FONTECACHE_ESPERA_MS;
  fontecache_engatilhar("canal:f", NULL, NULL, NULL);          // de novo, ja com o descanso vencido
  relogio += FONTECACHE_ESPERA_MS;
  usleep(20000);
  assert(nConsultas() == 2);
  buscaOcupada = 0;
  playerCarregando = 1;
  fontecache_avancar();
  usleep(20000);
  assert(nConsultas() == 2);
  playerCarregando = 0;
  fontecache_avancar();                            // agora sim: era o pendente de antes
  esperarFio();
  assert(nConsultas() == 3 && !strcmp(ultimoId, "canal:f"));
  assert(fontecache_pegar("canal:f", "tv", &l, &n) == FC_ACERTO); free(l);
  puts("ok  busca real ocupada ou player carregando seguram o prefetch, que sai depois");

  // 9. EM CURSO E ADOTAVEL; o pedido real do MESMO canal espera em vez de repetir.
  prenderTudo();
  engatilharEAvancar(NULL, "canal:g");
  esperarDentro();
  assert(fontecache_pegar("canal:g", "tv", &l, &n) == FC_EM_CURSO);
  assert(fontecache_pegar("canal:z", "tv", &l, &n) == FC_NADA);
  assert(fontecache_pegar("canal:g", "channel", &l, &n) == FC_NADA);   // tipo diferente
  soltarTudo();
  esperarFio();
  assert(fontecache_pegar("canal:g", "tv", &l, &n) == FC_ACERTO && n == 2); free(l);
  puts("ok  prefetch em curso e visivel como EM_CURSO e vira acerto quando chega");

  // 10. PEDIDO REAL DE OUTRO CANAL: o prefetch CEDE — para de perguntar aos
  //     addons que faltam, deixa de ser adotavel e nao guarda nada.
  prenderTudo();
  engatilharEAvancar(NULL, "canal:h");
  esperarDentro();
  soltarAddon(0);                                  // um addon respondeu
  usleep(20000);
  fontecache_ceder();                              // e o que dispararBusca faz
  assert(fontecache_pegar("canal:h", "tv", &l, &n) == FC_NADA);   // nao adota cancelado
  esperarFio();
  assert(addonsPerguntados == 1);                  // os outros dois nem foram perguntados
  assert(fontecache_n() == 0);
  puts("ok  pedido real faz o prefetch ceder: sem addons a mais, sem lista pela metade");

  // 11. A CORRIDA: cancelado DEPOIS da ultima checagem da consulta, que ainda
  //     devolve lista. O cache tem a propria guarda e descarta mesmo assim.
  ignoraCancel = 1;
  prenderTudo();
  engatilharEAvancar(NULL, "canal:i");
  esperarDentro();
  fontecache_ceder();
  soltarTudo();
  esperarFio();
  assert(fontecache_n() == 0);
  ignoraCancel = 0;
  puts("ok  lista que chega depois de ceder e descartada");

  // 12. O FOCO MUDOU: prefetch em curso que continua vizinho segue; o que nao e
  //     mais vizinho cede. E enquanto ha um no ar, nenhum outro arranca.
  prenderTudo();
  engatilharEAvancar("canal:j", "canal:k");        // arranca k
  esperarDentro();
  assert(!strcmp(ultimoId, "canal:k"));
  { int antes = nConsultas();
    fontecache_engatilhar("canal:k", NULL, "canal:m", NULL);   // k ainda e vizinho: segue
    relogio += FONTECACHE_ESPERA_MS; fontecache_avancar();
    usleep(20000);
    assert(nConsultas() == antes && fioVivo());
    assert(fontecache_pegar("canal:k", "tv", &l, &n) == FC_EM_CURSO);
    fontecache_engatilhar("canal:p", NULL, "canal:q", NULL);   // k saiu da vizinhanca: cede
    relogio += FONTECACHE_ESPERA_MS; fontecache_avancar();
    usleep(20000);
    assert(nConsultas() == antes);                 // nada novo enquanto k nao sai
    assert(fontecache_pegar("canal:k", "tv", &l, &n) == FC_NADA);
    soltarTudo();
    esperarFio();
    assert(fontecache_n() == 0);                   // k cedeu: nada guardado
    fontecache_avancar();                          // agora q, depois p
    esperarFio();
    assert(nConsultas() == antes + 1 && !strcmp(ultimoId, "canal:q")); }
  puts("ok  foco novo mantem o prefetch que ainda e vizinho e cede o que nao e");

  // 13. SEM ADDONS, nada e engatilhado. (addons_n do duble e fixo; o caso vazio
  //     e coberto pela guarda de addons_consultar, que devolve 0 sem addon.)
  //     ENCERRAR com prefetch no ar: cede, junta e esvazia.
  prenderTudo();
  fontecache_avancar();                            // p, pendente do passo 12
  esperarDentro();
  fontecache_encerrar();
  assert(!fioVivo() && fontecache_n() == 0);
  assert(fontecache_pegar("canal:p", "tv", &l, &n) == FC_NADA);
  puts("ok  encerrar com prefetch no ar cede, junta o fio e esvazia");

  puts("fontecache: tudo ok");
  return 0;
}
