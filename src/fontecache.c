#include "fontecache.h"
#include "addons.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// O relogio e um macro para o teste poder avancar o tempo sem dormir. No app
// e SDL_GetTicks, o mesmo relogio de stream_idade_ms.
#ifndef FC_AGORA
#define FC_AGORA() SDL_GetTicks()
#endif

// O tipo que o prefetch pede. E o que tocarCanal (app.c) pede para um canal do
// guia — e a chave do cache e id+tipo, entao os dois tem de coincidir para o
// acerto acontecer. addons_consultar tenta "channel" sozinho quando "tv" nao
// responde, como a busca real.
#define FC_TIPO "tv"

typedef struct {
  char    id[64];
  char    tipo[16];
  Stream *lista;          // malloc de n * sizeof(Stream); NULL = vaga
  int     n;
  Uint32  quando;         // FC_AGORA() de quando a lista chegou
} Entrada;

// TUDO ABAIXO E COMPARTILHADO entre o fio da UI (engatilhar/pegar/avancar), o
// fio da busca principal (guardar) e o fio do prefetch. Uma trava so, no
// padrao de addTrava/legTrava em addons.c: as secoes sao curtas e nenhuma faz
// rede com a trava tomada.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Entrada cache[FONTECACHE_MAX];
typedef struct {
  Entrada resposta;
  FontecacheEscopo escopo;
  char origem[600];
} EntradaVod;
static EntradaVod vod[FONTECACHE_VOD_MAX];
static unsigned vodGeracao;

// Os pedidos do guia: no maximo dois (um vizinho de cada lado). Substituidos
// inteiros a cada engatilhar — o foco mudou, os vizinhos de antes nao
// interessam mais.
static char   pend[2][64];
// A base do addon que publicou cada vizinho, para o prefetch perguntar SO a
// ele (mesma regra de alvoBase em addons.c; sem ela cada prefetch ia a todos
// os addons de fonte). Vazia = todos.
static char   pendBase[2][600];
static char   emCursoBase[600];
static Uint32 pedidoEm;

// O prefetch em curso. `emCurso` e o id na rede; `cancelar` e lido pelo fio de
// rede entre um addon e outro; `criado` diz se ha um pthread_t para juntar
// (o padrao de fioLegCriado), porque um fio terminado que ninguem junta vaza.
static pthread_t fio;
static int    vivo, criado;
static char   emCurso[64];
static int    cancelar;
static int    emCursoVod;
static FontecacheEscopo preEscopo;

static int expirada(const Entrada *e, Uint32 agora) {
  return (Uint32)(agora - e->quando) > FONTECACHE_VALIDADE_MS;
}

// Com a trava tomada.
static void soltar(Entrada *e) {
  free(e->lista);
  memset(e, 0, sizeof *e);
}

// Com a trava tomada. Indice da entrada valida deste id+tipo, ou -1. Expiradas
// que encontra pelo caminho sao soltas na hora — memoria que nao serve mais
// nao fica ocupada esperando alguem pedir justamente aquele canal.
static int achar(const char *id, const char *tipo, Uint32 agora) {
  int i, r = -1;
  for (i = 0; i < FONTECACHE_MAX; i++) {
    if (!cache[i].lista) continue;
    if (expirada(&cache[i], agora)) { soltar(&cache[i]); continue; }
    if (!strcmp(cache[i].id, id) && !strcmp(cache[i].tipo, tipo)) r = i;
  }
  return r;
}

static int ehCanal(const char *tipo) {
  return tipo && (!strcmp(tipo, "tv") || !strcmp(tipo, "channel"));
}

// Com a trava tomada. Copia a lista para a entrada certa: a do mesmo id (a
// lista nova substitui a velha), senao uma vaga, senao a MAIS ANTIGA — que e
// a mais perto de expirar e a que menos provavelmente o proximo zap quer.
static void guardarSemTrava(const char *id, const char *tipo, const Stream *lista, int n,
                            Uint32 agora) {
  int i, alvo = -1;
  Stream *copia;
  alvo = achar(id, tipo, agora);
  if (alvo < 0)
    for (i = 0; i < FONTECACHE_MAX; i++) if (!cache[i].lista) { alvo = i; break; }
  if (alvo < 0) {
    alvo = 0;
    for (i = 1; i < FONTECACHE_MAX; i++)
      if ((Sint32)(cache[i].quando - cache[alvo].quando) < 0) alvo = i;
  }
  copia = malloc(sizeof(Stream) * (size_t)n);
  if (!copia) { printf("[fontecache] sem memoria para %d fontes de %s\n", n, id); return; }
  memcpy(copia, lista, sizeof(Stream) * (size_t)n);
  soltar(&cache[alvo]);
  snprintf(cache[alvo].id, sizeof cache[alvo].id, "%s", id);
  snprintf(cache[alvo].tipo, sizeof cache[alvo].tipo, "%s", tipo);
  cache[alvo].lista = copia;
  cache[alvo].n = n;
  cache[alvo].quando = agora;
}

void fontecache_guardar(const char *id, const char *tipo, const Stream *lista, int n) {
  if (!id || !*id || !lista || n <= 0 || !ehCanal(tipo)) return;
  if (n > FONTECACHE_FONTES_MAX) {
    // Fora, e nao cortada: ver a conta em fontecache.h.
    printf("[fontecache] %s: %d fontes passam do teto de %d; nao guardo\n",
           id, n, FONTECACHE_FONTES_MAX);
    return;
  }
  pthread_mutex_lock(&trava);
  guardarSemTrava(id, tipo, lista, n, FC_AGORA());
  pthread_mutex_unlock(&trava);
}

int fontecache_pegar(const char *id, const char *tipo, Stream **lista, int *n) {
  int i, r = FC_NADA;
  if (lista) *lista = NULL;
  if (n) *n = 0;
  if (!id || !*id || !tipo) return FC_NADA;
  pthread_mutex_lock(&trava);
  i = achar(id, tipo, FC_AGORA());
  if (i >= 0) {
    // CONSOME: a entrada sai do cache e a propriedade da lista passa ao
    // chamador, sem copiar.
    if (lista) { *lista = cache[i].lista; cache[i].lista = NULL; }
    if (n) *n = cache[i].n;
    soltar(&cache[i]);
    r = FC_ACERTO;
  } else if (vivo && !emCursoVod && !cancelar && !strcmp(emCurso, id) && !strcmp(FC_TIPO, tipo)) {
    r = FC_EM_CURSO;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

int fontecache_n(void) {
  int i, k = 0;
  Uint32 agora = FC_AGORA();
  pthread_mutex_lock(&trava);
  for (i = 0; i < FONTECACHE_MAX; i++)
    if (cache[i].lista && !expirada(&cache[i], agora)) k++;
  pthread_mutex_unlock(&trava);
  return k;
}

static int ehVod(const char *tipo) {
  return tipo && (!strcmp(tipo, "movie") || !strcmp(tipo, "series"));
}

static int mesmoEscopo(const FontecacheEscopo *a, const FontecacheEscopo *b) {
  return a->perfil == b->perfil && a->addons == b->addons &&
         a->geracao == b->geracao && !strcmp(a->conta, b->conta);
}

// Com a trava. Uma troca de conta/perfil/configuracao tambem solta os dados
// anteriores, em vez de deixa-los ocupados ate alguem pedir o mesmo titulo.
static void podarVod(const FontecacheEscopo *escopo, Uint32 agora) {
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++) {
    Entrada *e = &vod[i].resposta;
    if (e->lista && ((Uint32)(agora - e->quando) >= FONTECACHE_VOD_VALIDADE_MS ||
                    !mesmoEscopo(&vod[i].escopo, escopo)))
      soltar(e);
  }
}

unsigned fontecache_vod_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&trava);
  g = vodGeracao;
  pthread_mutex_unlock(&trava);
  return g;
}

void fontecache_vod_limpar(void) {
  pthread_mutex_lock(&trava);
  vodGeracao++;
  if (emCursoVod && vivo) cancelar = 1;
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++) soltar(&vod[i].resposta);
  pthread_mutex_unlock(&trava);
}

static int guardarVodSemTrava(const char *id, const char *tipo, const char *origem,
                            const FontecacheEscopo *escopo,
                            const Stream *lista, int n, Uint32 quando) {
  int alvo = -1;
  Uint32 agora = FC_AGORA();
  Stream *copia;
  size_t bytes;
  origem = origem ? origem : "";
  if (!id || !*id || !ehVod(tipo) || !escopo || !lista || n <= 0 ||
      strlen(id) >= sizeof vod[0].resposta.id ||
      strlen(origem) >= sizeof vod[0].origem ||
      (size_t)n > FONTECACHE_VOD_BYTES / sizeof(Stream) ||
      (Uint32)(agora - quando) >= FONTECACHE_VOD_VALIDADE_MS) return 0;
  bytes = sizeof(Stream) * (size_t)n;
  if (escopo->geracao != vodGeracao) return 0;
  podarVod(escopo, agora);
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++)
    if (vod[i].resposta.lista && !strcmp(vod[i].resposta.id, id) &&
        !strcmp(vod[i].resposta.tipo, tipo) && !strcmp(vod[i].origem, origem)) {
      alvo = i; break;
    }
  if (alvo < 0)
    for (int i = 0; i < FONTECACHE_VOD_MAX; i++)
      if (!vod[i].resposta.lista) { alvo = i; break; }
  if (alvo < 0) {
    alvo = 0;
    for (int i = 1; i < FONTECACHE_VOD_MAX; i++)
      if ((Uint32)(agora - vod[i].resposta.quando) >
          (Uint32)(agora - vod[alvo].resposta.quando)) alvo = i;
  }
  // Solta antes de alocar: mesmo a substituicao respeita o teto do cache.
  soltar(&vod[alvo].resposta);
  copia = malloc(bytes);
  if (copia) {
    Entrada *e = &vod[alvo].resposta;
    memcpy(copia, lista, bytes);
    snprintf(e->id, sizeof e->id, "%s", id);
    snprintf(e->tipo, sizeof e->tipo, "%s", tipo);
    e->lista = copia; e->n = n; e->quando = quando;
    vod[alvo].escopo = *escopo;
    snprintf(vod[alvo].origem, sizeof vod[alvo].origem, "%s", origem);
  }
  return copia != NULL;
}

void fontecache_vod_guardar(const char *id, const char *tipo, const char *origem,
                            const FontecacheEscopo *escopo,
                            const Stream *lista, int n, Uint32 quando) {
  pthread_mutex_lock(&trava);
  guardarVodSemTrava(id, tipo, origem, escopo, lista, n, quando);
  pthread_mutex_unlock(&trava);
}

int fontecache_vod_pegar(const char *id, const char *tipo, const char *origem,
                        const FontecacheEscopo *escopo,
                        Stream **lista, int *n, Uint32 *idade) {
  int r = FC_NADA;
  Uint32 agora = FC_AGORA();
  if (lista) *lista = NULL;
  if (n) *n = 0;
  if (idade) *idade = 0;
  if (!id || !*id || !ehVod(tipo) || !escopo || !lista || !n) return FC_NADA;
  origem = origem ? origem : "";
  pthread_mutex_lock(&trava);
  if (escopo->geracao != vodGeracao) { pthread_mutex_unlock(&trava); return FC_NADA; }
  podarVod(escopo, agora);
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++) {
    Entrada *e = &vod[i].resposta;
    if (!e->lista || strcmp(e->id, id) || strcmp(e->tipo, tipo) ||
        strcmp(vod[i].origem, origem)) continue;
    *lista = malloc(sizeof(Stream) * (size_t)e->n);
    if (*lista) {
      memcpy(*lista, e->lista, sizeof(Stream) * (size_t)e->n);
      *n = e->n;
      if (idade) *idade = agora - e->quando;
      r = FC_ACERTO;
    }
    break;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

void fontecache_vod_apagar(const char *id, const char *tipo, const char *origem,
                          const FontecacheEscopo *escopo) {
  if (!id || !tipo || !escopo) return;
  origem = origem ? origem : "";
  pthread_mutex_lock(&trava);
  for (int i = 0; i < FONTECACHE_VOD_MAX; i++) {
    Entrada *e = &vod[i].resposta;
    if (e->lista && mesmoEscopo(&vod[i].escopo, escopo) &&
        !strcmp(e->id, id) && !strcmp(e->tipo, tipo) &&
        !strcmp(vod[i].origem, origem)) soltar(e);
  }
  pthread_mutex_unlock(&trava);
}

// --- o fio de prefetch ------------------------------------------------------------

static int cancelado(void *u) {
  int c;
  (void)u;
  pthread_mutex_lock(&trava); c = cancelar; pthread_mutex_unlock(&trava);
  return c;
}

static void *prefetch(void *u) {
  Stream *lista = NULL;
  char id[64], base[600];
  int n;
  Uint32 inicio = FC_AGORA();
  (void)u;
  pthread_mutex_lock(&trava);
  snprintf(id, sizeof id, "%s", emCurso);
  snprintf(base, sizeof base, "%s", emCursoBase);
  pthread_mutex_unlock(&trava);

  n = addons_consultar(id, emCursoVod ? "series" : FC_TIPO, base,
                       FONTECACHE_FIOS, cancelado, NULL, &lista);

  pthread_mutex_lock(&trava);
  if (cancelar) {
    // Cedeu a um pedido real, ou o app esta fechando. O que veio pode estar
    // pela metade: fora.
    printf("[fontecache] %s: prefetch cedeu (%d fontes descartadas)\n", id, n > 0 ? n : 0);
  } else if (emCursoVod) {
    if (guardarVodSemTrava(id, "series", "", &preEscopo, lista, n, FC_AGORA()))
      printf("[proximo] fontes do proximo pre-carregadas: %s %d fontes (%u ms)\n",
             id, n, (unsigned)(FC_AGORA() - inicio));
  } else if (n > 0) {
    guardarSemTrava(id, FC_TIPO, lista, n, FC_AGORA());
    printf("[fontecache] %s: %d fontes engatilhadas\n", id, n);
  } else {
    // Nada ou erro: nao guarda. O pedido real vai a rede e ve por si.
    printf("[fontecache] %s: prefetch sem fontes\n", id);
  }
  emCurso[0] = 0;
  vivo = 0;
  pthread_mutex_unlock(&trava);
  free(lista);
  fflush(stdout);
  return NULL;
}

// SEM a trava. Arranca o proximo pedido pendente que ainda nao esta no cache,
// se nao ha fio no ar e o foco ja descansou. Quem chama ja garantiu que a
// busca principal esta ociosa e que o player nao esta carregando.
static void tentar(void) {
  char id[64] = "", base[600] = "";
  int k, juntar;
  Uint32 agora = FC_AGORA();
  pthread_mutex_lock(&trava);
  if (vivo || (Uint32)(agora - pedidoEm) < FONTECACHE_ESPERA_MS) {
    pthread_mutex_unlock(&trava);
    return;
  }
  for (k = 0; k < 2 && !id[0]; k++) {
    if (!pend[k][0]) continue;
    if (achar(pend[k], FC_TIPO, agora) >= 0) { pend[k][0] = 0; continue; }   // ja tem
    snprintf(id, sizeof id, "%s", pend[k]);
    snprintf(base, sizeof base, "%s", pendBase[k]);
    pend[k][0] = 0;
  }
  if (!id[0]) { pthread_mutex_unlock(&trava); return; }
  juntar = criado;
  pthread_mutex_unlock(&trava);

  // O fio anterior ja terminou (vivo == 0), mas ainda precisa ser juntado.
  if (juntar) pthread_join(fio, NULL);

  pthread_mutex_lock(&trava);
  criado = 0;
  emCursoVod = 0;
  cancelar = 0;
  vivo = 1;
  snprintf(emCurso, sizeof emCurso, "%s", id);
  snprintf(emCursoBase, sizeof emCursoBase, "%s", base);
  if (pthread_create(&fio, NULL, prefetch, NULL) != 0) { vivo = 0; emCurso[0] = 0; }
  else criado = 1;
  pthread_mutex_unlock(&trava);
}

// Mesmo fio, limite de concorrencia e cancelamento do guia. Chamado pela UI;
// nunca ocupa o estado/resultado da busca real de addons.c.
int fontecache_precarregar_proximo(const char *id, const FontecacheEscopo *escopo) {
  int juntar, ok;
  if (!id || !*id || strlen(id) >= sizeof emCurso || !escopo || addons_ocupado()) return 0;
  pthread_mutex_lock(&trava);
  if (vivo) { pthread_mutex_unlock(&trava); return 0; }
  juntar = criado;
  pthread_mutex_unlock(&trava);
  if (juntar) pthread_join(fio, NULL);
  pthread_mutex_lock(&trava);
  criado = 0; cancelar = 0; vivo = 1; emCursoVod = 1;
  preEscopo = *escopo;
  snprintf(emCurso, sizeof emCurso, "%s", id);
  emCursoBase[0] = 0;
  // Vizinhos de um guia anterior nao devem arrancar durante o episodio.
  pend[0][0] = pend[1][0] = 0;
  ok = pthread_create(&fio, NULL, prefetch, NULL) == 0;
  if (ok) criado = 1;
  else { vivo = 0; emCurso[0] = 0; }
  pthread_mutex_unlock(&trava);
  return ok;
}

void fontecache_cancelar_proximo(void) {
  pthread_mutex_lock(&trava);
  if (emCursoVod && vivo) cancelar = 1;
  pthread_mutex_unlock(&trava);
}

void fontecache_engatilhar(const char *idAntes, const char *baseAntes,
                           const char *idDepois, const char *baseDepois) {
  int mantem = 0;
  if (!addons_n()) return;
  pthread_mutex_lock(&trava);
  // O de BAIXO primeiro: e para onde a mesma seta que trouxe o foco ate aqui
  // continua indo, e e o CH+ do controle.
  snprintf(pend[0], sizeof pend[0], "%s", idDepois ? idDepois : "");
  snprintf(pend[1], sizeof pend[1], "%s", idAntes  ? idAntes  : "");
  snprintf(pendBase[0], sizeof pendBase[0], "%s", baseDepois ? baseDepois : "");
  snprintf(pendBase[1], sizeof pendBase[1], "%s", baseAntes  ? baseAntes  : "");
  pedidoEm = FC_AGORA();
  // O prefetch que esta na rede continua se ainda e vizinho do foco novo;
  // senao para de pedir aos addons que faltam — era vizinho de um foco que ja
  // nao existe, e cada requisicao a mais e custo por nada.
  if (vivo && !cancelar) {
    mantem = (pend[0][0] && !strcmp(emCurso, pend[0])) ||
             (pend[1][0] && !strcmp(emCurso, pend[1]));
    if (!mantem) cancelar = 1;
  }
  pthread_mutex_unlock(&trava);
  // Um pedido pendente com a busca principal ocupada espera por
  // fontecache_avancar, que addons_estado chama quando ela solta.
  if (addons_ocupado() || player_carregando()) return;
  tentar();
}

void fontecache_avancar(void) {
  if (player_carregando()) return;
  tentar();
}

void fontecache_ceder(void) {
  pthread_mutex_lock(&trava);
  if (vivo) cancelar = 1;
  pthread_mutex_unlock(&trava);
}

void fontecache_encerrar(void) {
  int juntar, i;
  pthread_mutex_lock(&trava);
  cancelar = 1;
  juntar = criado;
  pend[0][0] = pend[1][0] = 0;
  pthread_mutex_unlock(&trava);
  if (juntar) pthread_join(fio, NULL);
  pthread_mutex_lock(&trava);
  criado = vivo = 0;
  emCurso[0] = 0;
  for (i = 0; i < FONTECACHE_MAX; i++) soltar(&cache[i]);
  vodGeracao++;
  for (i = 0; i < FONTECACHE_VOD_MAX; i++) soltar(&vod[i].resposta);
  pthread_mutex_unlock(&trava);
}
