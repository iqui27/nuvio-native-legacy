#include "watchstate_envio.h"
#include "addonurl.h"
#include "addons.h"
#include "dados.h"
#include "perfis.h"
#include "rede.h"
#include "sessao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARQ_FILA "watchstate_fila.txt"

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  acorda = PTHREAD_COND_INITIALIZER;
static WsFila *fila;
static int     iniciado;

// Com a trava. Diario pequeno (<= WS_FILA_MAX linhas): regravar inteiro basta.
static void gravar(void) {
  char *s = fila->n ? ws_fila_serializar(fila) : NULL;
  if (s) { dados_gravar(ARQ_FILA, s); free(s); }
  else if (!fila->n) dados_apagar(ARQ_FILA);
}

typedef struct {
  int sondado;
  char base[NV_ADDON_URL_MAX], destino[20];
  WsDecl decl;
} Destino;

// Com a trava da lista, copia tudo: nenhum ponteiro para um slot sobrevive.
static int copiarDestino(int i, Destino *s) {
  if (perfis_ativo() != perfis_ativo_addons() || !addons_ativo(i)) return 0;
  memset(s, 0, sizeof *s);
  snprintf(s->base, sizeof s->base, "%s", addons_base(i));
  s->sondado = addons_sondado(i);
  addons_watchstate(i, &s->decl);
  ws_destino(s->base, sessao_usuario(), perfis_ativo(), s->destino);
  return 1;
}

// O manifesto pode ainda estar por sondar no arranque.
static int destinoAtual(const char *addon, Destino *s) {
  int i, achou = 0;
  addons_lista_travar();
  for (i = 0; i < addons_n(); i++) {
    if (copiarDestino(i, s) && !strcmp(s->destino, addon)) { achou = 1; break; }
  }
  addons_lista_destravar();
  return achou;
}

static int autorizado(const char *addon, void *u) {
  Destino s;
  (void)u;
  return destinoAtual(addon, &s);
}

// Chamado por ws_fila_passo COM a trava: solta durante o pedido para o desenho
// poder enfileirar sem esperar os 15 s de um addon fora do ar.
static int enviaReal(const char *addon, const char *caminho, const char *corpo,
                     int *retryAfterS, void *u) {
  static const char *const cab[] = {"Content-Type: application/json", NULL};
  RedePedido p;
  RedeResposta r;
  char url[NV_ADDON_PEDIDO_MAX];
  Destino s, atual;
  int st = 0;
  (void)u;
  // FORA DA LISTA DE AGORA NAO E ERRO DO ADDON nem ordem de descarte: no
  // arranque o diario e lido com a lista do PACOTE (art/addons.txt, a de
  // exemplo), que nao e a lista da conta — o pendente do addon da conta fica
  // segurado (WS_AUSENTE) ate a lista certa voltar, e nada e POSTado nem
  // perdido antes disso. Quem o usuario tirou de verdade ja foi podado na
  // hora, por quem tirou (addons_definir_lista/addons_esquecer/addons_alternar).
  if (!destinoAtual(addon, &s)) return WS_AUSENTE;
  if (!s.decl.push) return s.sondado ? 410 : WS_AUSENTE;
  if (s.decl.viewers || nv_addon_url(url, sizeof url, s.base, "%s", caminho) <= 0 ||
      !url[0]) return 410;
  memset(&p, 0, sizeof p);
  p.metodo = "POST"; p.url = url; p.cabecalhos = cab;
  p.corpo = corpo; p.n_corpo = strlen(corpo);
  p.prazo_ms = 15000; p.seguir = 0;
  // A UI pode reordenar ou substituir a lista antes da poda. Revalida a
  // identidade em toda a lista; o pedido usa somente a copia de A.
  if (!destinoAtual(s.destino, &atual) || strcmp(atual.base, s.base)) return 410;
  pthread_mutex_unlock(&trava);
  rede_pedir(&p, &r);
  if (r.erro == REDE_OK) st = r.status;   // 4xx/5xx e resposta; 0 = rede ou prazo
  *retryAfterS = r.retry_after_s > 0 ? r.retry_after_s : 0;
  rede_resposta_limpar(&r);
  pthread_mutex_lock(&trava);
  return st;
}

static void *fio(void *arg) {
  (void)arg;
  pthread_mutex_lock(&trava);
  for (;;) {
    long long agora = (long long)time(NULL), prox;
    // Expiracao e WS_AUSENTE tambem alteram a fila, mesmo sem pedido.
    if (fila->n) { ws_fila_passo(fila, agora, enviaReal, NULL); gravar(); }
    prox = ws_fila_proxima(fila);
    if (prox < 0) pthread_cond_wait(&acorda, &trava);
    else {
      struct timespec ate;
      agora = (long long)time(NULL);
      if (prox <= agora) continue;
      ate.tv_sec = (time_t)prox; ate.tv_nsec = 0;
      pthread_cond_timedwait(&acorda, &trava, &ate);
    }
  }
  return NULL;
}

void wsenvio_iniciar(void) {
  pthread_t t;
  char *txt;
  pthread_mutex_lock(&trava);
  if (!iniciado) {
    iniciado = 1;
    fila = calloc(1, sizeof *fila);
    if (fila && (rede_pedido_capacidades() & REDE_CAP_CORPO) &&
        pthread_create(&t, NULL, fio, NULL) == 0) {
      pthread_detach(t);
      // O diario entra INTEIRO. Nenhuma poda aqui: no arranque a lista de
      // agora e a do PACOTE (o addons_carregar do art/addons.txt roda antes
      // desta linha), e podar contra ela apagava o pendente dos addons da
      // CONTA em todo boot — o destino (base+conta+perfil) nunca casava.
      // Quem a lista de agora nao conhece fica segurado no envio (WS_AUSENTE)
      // ate a lista da conta voltar; quem o usuario tirou e podado na hora,
      // por quem tirou.
      if ((txt = dados_ler(ARQ_FILA))) { ws_fila_carregar(fila, txt); free(txt); }
      pthread_cond_signal(&acorda);
    } else { free(fila); fila = NULL; }
  }
  pthread_mutex_unlock(&trava);
}

// Poda o diario EM DISCO, com o fio ainda nao nascido.
static void podarDisco(int (*ainda)(const char *addon, void *u), void *u) {
  char *txt = dados_ler(ARQ_FILA);
  WsFila f = {0};
  if (txt) { ws_fila_carregar(&f, txt); free(txt); }
  ws_fila_podar(&f, ainda, u);
  { char *s = f.n ? ws_fila_serializar(&f) : NULL;
    if (s) { dados_gravar(ARQ_FILA, s); free(s); }
    else dados_apagar(ARQ_FILA); }
  ws_fila_limpar(&f);
}

void wsenvio_podar(void) {
  pthread_mutex_lock(&trava);
  if (fila) { ws_fila_podar(fila, autorizado, NULL); gravar(); }
  else podarDisco(autorizado, NULL);
  pthread_mutex_unlock(&trava);
}

// 0 = e ESTE destino e sai do diario; o resto fica.
static int naoDestino(const char *addon, void *u) { return strcmp(addon, (const char *)u); }

void wsenvio_podar_addon(const char *base) {
  char destino[20];
  if (!base || !base[0]) return;
  ws_destino(base, sessao_usuario(), perfis_ativo(), destino);
  pthread_mutex_lock(&trava);
  if (fila) { ws_fila_podar(fila, naoDestino, destino); gravar(); }
  else podarDisco(naoDestino, destino);
  pthread_mutex_unlock(&trava);
}

void wsenvio_emitir(const WsEvento *e) {
  int i, n, novo = 0;
  if (!e || !(rede_pedido_capacidades() & REDE_CAP_CORPO)) return;
  // viewers:false manda o evento para o dono da configuracao do addon. Perfil
  // que usa os addons de outro nao empurra o que ele assiste para esse dono.
  if (perfis_ativo() != perfis_ativo_addons()) return;
  wsenvio_iniciar();
  addons_lista_travar();
  n = addons_n();
  addons_lista_destravar();
  for (i = 0; i < n; i++) {
    Destino s;
    int tem;
    char url[NV_ADDON_PEDIDO_MAX], id[WS_ID_MAX + 64], corpo[1024];
    const char *base = s.base;
    addons_lista_travar();
    tem = copiarDestino(i, &s);
    addons_lista_destravar();
    if (!tem || !base[0]) continue;
    if (!ws_decl_aceita(&s.decl, e->ev, e->tipo, e->metaId, e->videoId)) continue;
    if (!ws_url(base, e, url, sizeof url) || !ws_id(e, id, sizeof id) ||
        !ws_corpo(e, corpo, sizeof corpo)) continue;
    // Tira a base e a query; nenhuma credencial entra no diario.
    { size_t lb = strcspn(base, "?");
      char *q;
      while (lb && base[lb - 1] == '/') lb--;
      memmove(url, url + lb, strlen(url + lb) + 1);
      if ((q = strchr(url, '?'))) *q = 0;
    }
    pthread_mutex_lock(&trava);
    if (fila && autorizado(s.destino, NULL) &&
        ws_fila_empurrar(fila, s.destino, url, id, corpo, (long long)time(NULL))) novo = 1;
    pthread_mutex_unlock(&trava);
  }
  if (novo) {
    pthread_mutex_lock(&trava);
    gravar();
    pthread_cond_signal(&acorda);
    pthread_mutex_unlock(&trava);
  }
}
