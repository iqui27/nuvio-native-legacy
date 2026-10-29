#include "proximopreparo.h"
#include "addons.h"
#include "ajustes.h"
#include "fonteantecipa.h"
#include "player.h"
#include "perfisessao.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum { PP_METADADOS, PP_META_PRONTA, PP_RESOLVENDO, PP_RESOLVIDO, PP_RENOVANDO };
typedef struct {
  _Atomic unsigned refs;
  _Atomic int estado, cancelado;
  unsigned geracao;
  NvPerfilSessao sessaoPerfil;
  uint64_t configuracao;
  FontecacheEscopo escopo;
  AddonsConsulta *addons;
  StreamPreparo *fontes;
  Stream *metadados;
  int n, indice, resultado, renovou, bufferTentou, continuar;
  int catalogo, temporada, episodio;
  char atual[64], alvo[64];
  Stream escolhida;
  FonteFalhaSessao *politica;
  Uint32 metadadosEm, resolvidaEm;
  unsigned token;
} ProximoJob;

// O ponteiro pertence somente a UI; os workers guardam sua propria referencia.
// Cancelar solta imediatamente a referencia da UI. HTTP ja em voo termina com
// a copia antiga e nao pode voltar a publicar nem executar outra candidata.
static ProximoJob *proximo;
static _Atomic unsigned geracao = 1;
static pthread_mutex_t fiosTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fiosFim = PTHREAD_COND_INITIALIZER;
static unsigned fios;

static void soltar(ProximoJob *j) {
  if (!j || atomic_fetch_sub_explicit(&j->refs, 1, memory_order_acq_rel) != 1) return;
  addons_consulta_liberar(j->addons);
  stream_preparo_liberar(j->fontes);
  fontefalha_sessao_liberar(j->politica);
  free(j->metadados);
  free(j);
}
static int cancelado(void *u) {
  ProximoJob *j = u;
  return atomic_load_explicit(&j->cancelado, memory_order_acquire) ||
         j->geracao != atomic_load_explicit(&geracao, memory_order_acquire) ||
         !nv_perfil_sessao_atual(j->sessaoPerfil);
}
static int mesmoEscopo(const FontecacheEscopo *a, const FontecacheEscopo *b) {
  return a->perfil == b->perfil && a->addons == b->addons && a->geracao == b->geracao &&
         a->sessao == b->sessao && !strcmp(a->conta, b->conta);
}
static int contextoAtual(const ProximoJob *j) {
  FontecacheEscopo atual;
  if (!j || cancelado((void *)j) || !ajustes_fonte_precarregar() || ajustes_fonte_manual()) return 0;
  addons_capturar_escopo(&atual);
  return mesmoEscopo(&atual, &j->escopo) && addons_consulta_atual(j->addons) &&
         j->configuracao == stream_preparo_config_atual();
}
static void cancelarToken(ProximoJob *j) {
#ifdef NV_ANDROID
  if (j && j->token) player_precarregar_cancelar(j->token);
#endif
  if (j) j->token = 0;
}
static void liberarProximo(int invalidarIntento) {
  ProximoJob *j = proximo;
  proximo = NULL;
  if (invalidarIntento) atomic_fetch_add_explicit(&geracao, 1, memory_order_acq_rel);
  if (!j) return;
  atomic_store_explicit(&j->cancelado, 1, memory_order_release);
  cancelarToken(j);
  soltar(j);
}
void proximopreparo_cancelar(void) { liberarProximo(1); }
unsigned proximopreparo_geracao(void) {
  return atomic_load_explicit(&geracao, memory_order_acquire);
}

static void *trabalhar(void *u) {
  ProximoJob *j = u;
  int estado = atomic_load_explicit(&j->estado, memory_order_acquire);
  if (estado == PP_METADADOS) {
    j->n = cancelado(j) ? -1 : addons_consulta_consultar(j->addons, j->alvo, "series",
                                                       FONTECACHE_FIOS, cancelado, j, &j->metadados);
    j->metadadosEm = SDL_GetTicks();
    atomic_store_explicit(&j->estado, PP_META_PRONTA, memory_order_release);
  } else {
    if (estado == PP_RENOVANDO) {
      int ok = stream_preparo_renovar(j->fontes, j->indice, cancelado, j, &j->escolhida);
      if (!ok) j->indice = -1;
      if (!ok && !cancelado(j)) j->continuar = 1;
      j->resultado = ok;
    } else {
      j->resultado = stream_preparo_resolver(j->fontes, cancelado, j, &j->escolhida,
                                            &j->indice, &j->politica);
    }
    j->resolvidaEm = SDL_GetTicks();
    atomic_store_explicit(&j->estado, PP_RESOLVIDO, memory_order_release);
  }
  soltar(j);
  pthread_mutex_lock(&fiosTrava);
  if (--fios == 0) pthread_cond_broadcast(&fiosFim);
  pthread_mutex_unlock(&fiosTrava);
  return NULL;
}
static int despachar(ProximoJob *j, int estado) {
  pthread_t fio;
  pthread_attr_t a;
  atomic_store_explicit(&j->estado, estado, memory_order_release);
  atomic_fetch_add_explicit(&j->refs, 1, memory_order_relaxed);
  pthread_mutex_lock(&fiosTrava);
  fios++;
  pthread_mutex_unlock(&fiosTrava);
  pthread_attr_init(&a);
  pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&a, 512u * 1024u);
  int ok = pthread_create(&fio, &a, trabalhar, j) == 0;
  pthread_attr_destroy(&a);
  if (!ok) {
    soltar(j);
    pthread_mutex_lock(&fiosTrava);
    if (--fios == 0) pthread_cond_broadcast(&fiosFim);
    pthread_mutex_unlock(&fiosTrava);
  }
  return ok;
}

int proximopreparo_agendar(int catalogo, int t, int e, const char *atual,
                          const char *alvo, const FontecacheEscopo *escopo) {
  if (!atual || !*atual || !alvo || !*alvo || !strcmp(atual, alvo) ||
      strlen(atual) >= sizeof ((ProximoJob *)0)->atual ||
      strlen(alvo) >= sizeof ((ProximoJob *)0)->alvo || !escopo || t < 0 || e < 1 ||
      !ajustes_fonte_precarregar() || ajustes_fonte_manual()) return 0;
  if (proximo && !strcmp(proximo->atual, atual) && !strcmp(proximo->alvo, alvo) &&
      contextoAtual(proximo)) return 1;
  proximopreparo_cancelar();
  ProximoJob *j = calloc(1, sizeof *j);
  if (!j) return 0;
  atomic_init(&j->refs, 1);
  atomic_init(&j->estado, PP_METADADOS);
  atomic_init(&j->cancelado, 0);
  j->geracao = atomic_load_explicit(&geracao, memory_order_acquire);
  j->sessaoPerfil = nv_perfil_sessao_ler();
  j->escopo = *escopo;
  j->configuracao = stream_preparo_config_atual();
  j->addons = addons_consulta_capturar();
  j->catalogo = catalogo; j->temporada = t; j->episodio = e; j->indice = -1;
  snprintf(j->atual, sizeof j->atual, "%s", atual);
  snprintf(j->alvo, sizeof j->alvo, "%s", alvo);
  if (!j->addons || !contextoAtual(j)) { soltar(j); return 0; }
  proximo = j;
  Uint32 idade = 0;
  int cache = fontecache_vod_pegar(alvo, "series", "", escopo, &j->metadados, &j->n, &idade);
  if (cache == FC_ACERTO && j->metadados && j->n > 0) {
    j->metadadosEm = SDL_GetTicks() - idade;
    atomic_store_explicit(&j->estado, PP_META_PRONTA, memory_order_release);
  } else {
    free(j->metadados); j->metadados = NULL; j->n = 0;
    if (!despachar(j, PP_METADADOS)) { proximopreparo_cancelar(); return 0; }
  }
  return 1;
}

static void prepararVideo(ProximoJob *j) {
#ifdef NV_ANDROID
  if (!j->bufferTentou && j->resultado == 1 && j->indice >= 0 && j->escolhida.url[0]) {
    j->bufferTentou = 1;
    unsigned token = player_precarregar_novo_token();
    if (token && player_precarregar_proxima_fonte(j->catalogo, j->temporada, j->episodio,
                                                 &j->escolhida, token)) j->token = token;
  }
#else
  (void)j;
#endif
}
static int fonteVale(const ProximoJob *j, Uint32 agora, int bufferPronto) {
  Uint32 idade = agora - j->resolvidaEm;
  return bufferPronto ? idade < FA_BUFFER_VALE_MS : idade < FA_PREPARADA_VALE_MS &&
         (j->escolhida.infoHash[0] || agora - j->metadadosEm < FA_LINK_VALE_MS);
}
static int continuarEscolha(ProximoJob *j) {
  cancelarToken(j);
  j->bufferTentou = 0; j->indice = -1; j->resultado = 0; j->continuar = 0;
  if (stream_preparo_continuar(j->fontes, &j->politica)) {
    if (despachar(j, PP_RESOLVENDO)) return 1;
    j->politica = stream_preparo_tomar_politica(j->fontes);
    // O proximo pedido explicito pode tentar despachar pelo roteador. Nao
    // repetir pthread_create em cada quadro nem perder o orcamento usado.
    j->indice = j->politica ? -2 : -1;
    atomic_store_explicit(&j->estado, PP_RESOLVIDO, memory_order_release);
  }
  return 0;
}
void proximopreparo_passo(const char *atual) {
  ProximoJob *j = proximo;
  if (!j) return;
  if (!atual || strcmp(j->atual, atual) || !contextoAtual(j)) {
    proximopreparo_cancelar(); return;
  }
  int estado = atomic_load_explicit(&j->estado, memory_order_acquire);
  if (estado == PP_META_PRONTA) {
    // A metadata miss/failure has spent no resolver budget. Keep a pending
    // explicit Next intent valid so the router can run its cold fallback once.
    // User/profile/config cancellations still revoke the intent above.
    if (j->n <= 0 || !j->metadados) { liberarProximo(0); return; }
    // A resposta completa e opcionalmente reaproveitada pela busca real. O
    // cache pode recusá-la pelo teto; o snapshot abaixo conserva TODA a lista.
    fontecache_vod_guardar(j->alvo, "series", "", &j->escopo,
                           j->metadados, j->n, j->metadadosEm);
    j->fontes = stream_preparo_criar_alvo(j->metadados, j->n, -1, j->alvo);
    free(j->metadados); j->metadados = NULL;
    // A fronteira de conta pode ocorrer durante a captura do resolvedor.
    // A metadata pertence ao ator original, mesmo se o novo snapshot for atual.
    if (cancelado(j)) { proximopreparo_cancelar(); return; }
    if (!j->fontes) { liberarProximo(0); return; }
    stream_preparo_definir_episodio(j->fontes, j->temporada, j->episodio);
    if (!despachar(j, PP_RESOLVENDO)) { liberarProximo(0); return; }
  } else if (estado == PP_RESOLVIDO) {
    if (!stream_preparo_contexto_atual(j->fontes) || j->resultado < 0) {
      proximopreparo_cancelar(); return;
    }
    if (j->continuar && continuarEscolha(j)) return;
#ifdef NV_ANDROID
    if (j->token) {
      int nativo = player_precarregar_estado(j->token);
      if (nativo < 0) {
        // Falha REAL de rede/parser: a candidata ja custou uma vaga. Continuar
        // devolve a MESMA politica ao resolvedor, sem repetir esta URL nem
        // recomecar o primeiro provedor. Cancelamento (0) nao entra aqui.
        if (continuarEscolha(j)) return;
      } else if (!nativo) {
        cancelarToken(j);
      }
    }
#endif
    prepararVideo(j);
  }
}
int proximopreparo_em_curso(const char *alvo) {
  ProximoJob *j = proximo;
  if (!j || !alvo || strcmp(j->alvo, alvo) || !contextoAtual(j)) return 0;
  return atomic_load_explicit(&j->estado, memory_order_acquire) != PP_RESOLVIDO;
}

// Uma renovacao do MESMO link, nao uma nova escolha nem outro orcamento. O
// posplay a pede perto da contagem final; trabalho ainda utilizavel fica intacto.
int proximopreparo_renovar(const char *atual, const char *alvo) {
  ProximoJob *j = proximo;
  if (!j || !atual || !alvo || strcmp(j->atual, atual) || strcmp(j->alvo, alvo) ||
      !contextoAtual(j)) return 0;
  // Recolher uma falha nativa antes de renovar: ela continua a politica gasta,
  // em vez de repetir a mesma candidata. passo pode soltar ou despachar o job.
  proximopreparo_passo(atual);
  j = proximo;
  if (!j || atomic_load_explicit(&j->estado, memory_order_acquire) != PP_RESOLVIDO ||
      j->renovou || j->resultado != 1 || j->indice < 0) return 0;
  Uint32 agora = SDL_GetTicks();
#ifdef NV_ANDROID
  int nativo = j->token ? player_precarregar_estado(j->token) : 0;
  if (nativo < 0) { continuarEscolha(j); return 0; }
  // READY e carregamento em curso conservam o mesmo token. Uma busca que
  // terminou tarde ou um salto aos creditos nao desfaz bytes ja preparados.
  if (nativo > 0 && fonteVale(j, agora, 1)) return 0;
  if (!nativo && fonteVale(j, agora, 0)) {
    // O backend cancelou ou nao estava disponivel, mas a URL ja conferida
    // ainda serve. Uma tentativa nativa basta; nao repetir HTTP do resolvedor.
    j->renovou = 1;
    cancelarToken(j); j->bufferTentou = 0;
    prepararVideo(j);
    return 1;
  }
#else
  if (fonteVale(j, agora, 0)) return 0;
#endif
  j->renovou = 1;
  cancelarToken(j);
  j->bufferTentou = 0;
  if (!despachar(j, PP_RENOVANDO)) {
    // The winner already spent resolver budget. Preserve that exact ledger
    // for the router's existing -2 continuation, without a new attempt here.
    if (!j->politica) j->politica = stream_preparo_tomar_politica(j->fontes);
    j->indice = j->politica ? -2 : -1; j->resultado = 0;
    atomic_store_explicit(&j->estado, PP_RESOLVIDO, memory_order_release);
    return 0;
  }
  return 1;
}
int proximopreparo_consumir(const char *alvo, ProximoPreparoResultado *saida) {
  ProximoJob *j = proximo;
  if (!saida) return 0;
  memset(saida, 0, sizeof *saida); saida->indiceFonte = -1;
  if (!j) return 0;
  if (!alvo || strcmp(j->alvo, alvo) || !contextoAtual(j)) {
    proximopreparo_cancelar(); return 0;
  }
  // A falha pode chegar no mesmo quadro do OK. Recolher o estado nativo antes
  // de consumir impede entregar novamente a URL cujo aquecimento falhou.
  proximopreparo_passo(j->atual);
  j = proximo;
  if (!j) return 0;
  if (atomic_load_explicit(&j->estado, memory_order_acquire) != PP_RESOLVIDO) return 0;
  if (!stream_preparo_contexto_atual(j->fontes) || j->resultado < 0) {
    proximopreparo_cancelar(); return 0;
  }
  Uint32 agora = SDL_GetTicks();
  int bufferPronto = 0;
#ifdef NV_ANDROID
  bufferPronto = j->token && player_precarregar_estado(j->token) == 2;
#endif
  // Um buffer real tem idade limitada propria. Fora dele, a URL assinada do
  // addon conserva sua idade original; native hash resolvido tem idade nova.
  int vale = fonteVale(j, agora, bufferPronto);
  if (!vale) {
    cancelarToken(j);
    if (j->resultado == 1) {
      if (!j->politica) j->politica = stream_preparo_tomar_politica(j->fontes);
      j->indice = j->politica ? -2 : -1;
    }
  }
  saida->fontes = stream_preparo_tomar_lista(j->fontes, &saida->n);
  if (!saida->fontes || saida->n <= 0) {
    free(saida->fontes); saida->fontes = NULL;
    proximopreparo_cancelar(); return 0;
  }
  saida->indiceFonte = j->indice;
  saida->sessaoPerfil = j->sessaoPerfil;
  saida->politica = j->politica; j->politica = NULL;
  saida->token = j->token; j->token = 0;
  saida->catalogo = j->catalogo; saida->temporada = j->temporada; saida->episodio = j->episodio;
  saida->idadeMs = agora - j->metadadosEm;
  proximo = NULL;
  soltar(j);
  return 1;
}
void proximopreparo_liberar_resultado(ProximoPreparoResultado *r) {
  if (!r) return;
#ifdef NV_ANDROID
  if (r->token) player_precarregar_cancelar(r->token);
#endif
  free(r->fontes);
  fontefalha_sessao_liberar(r->politica);
  memset(r, 0, sizeof *r); r->indiceFonte = -1;
}
void proximopreparo_encerrar(void) {
  proximopreparo_cancelar();
  pthread_mutex_lock(&fiosTrava);
  while (fios) pthread_cond_wait(&fiosFim, &fiosTrava);
  pthread_mutex_unlock(&fiosTrava);
}
