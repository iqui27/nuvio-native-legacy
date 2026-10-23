#include "syncprog.h"
#include "vistoep.h"
#include "progresso.h"
#include "catalogo.h"
#include "sessao.h"
#include "perfis.h"
#include "perfisessao.h"
#include <pthread.h>
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SP_MAX 240
#define PREFIXO_EPISODIO "__nuvio_episode__:"
// O web nao sincroniza titulo com menos de um minuto (MIN_PROGRESS_SYNC_DURATION_MS).
#define DUR_MINIMA_SEG 60.0

static ProgRegistro caixa[SP_MAX];
static int nCaixa;
static NvPerfilSessao caixaSessao;
static pthread_mutex_t caixaTrava = PTHREAD_MUTEX_INITIALIZER;

static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }
static char *contaRpc(const SyncProgRemocao *, const char *, const char *, int *);
static int pedidoContaAtual(const SyncProgRemocao *);


// updated_at ganha de last_watched, como em rowFreshness do web. Numero pode
// vir em segundos ou em ms (mapProgressRow trata os dois); texto e ISO.
static long long lerInstanteMs(const char *p, const char *f) {
  static const char *chaves[] = { "updated_at", "last_watched", "last_watched_at" };
  unsigned i;
  for (i = 0; i < sizeof chaves / sizeof *chaves; i++) {
    char txt[64];
    double v;
    // Texto primeiro: js_num aceita valor entre aspas e leria "2026-09-04T..."
    // como 2026. Uma data ISO tem '-' ou 'T'; numero entre aspas nao.
    if (js_texto(p, f, chaves[i], txt, sizeof txt) && txt[0]) {
      if (strchr(txt, '-') || strchr(txt, 'T')) { long long ms = js_ms_iso(txt); if (ms > 0) return ms; continue; }
      v = strtod(txt, NULL);
    } else {
      v = js_num(p, f, chaves[i], -1.0);
    }
    if (v > 0) {
      double ms = v > 1000000000000.0 ? v : v * 1000.0;
      if (isfinite(ms) && ms < (double)LLONG_MAX) return (long long)ms;
    }
  }
  return 0;
}

// Ausencia/null permitem o fallback do contrato; um valor presente invalido
// nao pode virar "ausente" e sobrescrever uma posicao valida com zero.
static int lerNumero(const char *p, const char *f, const char *chave,
                     double padrao, double *saida) {
  char cru[8];
  if (!js_tem(p, f, chave) ||
      (js_bruto(p, f, chave, cru, sizeof cru) && !strcmp(cru, "null"))) {
    *saida = padrao;
    return 1;
  }
  *saida = js_num(p, f, chave, NAN);
  return isfinite(*saida);
}

static int lerCoordenada(const char *p, const char *f, const char *chave, int *saida) {
  double v;
  if (!lerNumero(p, f, chave, -1.0, &v)) return 0;
  if (!isfinite(v) || v < INT_MIN || v > INT_MAX) return 0;
  *saida = (int)v;
  return v == *saida;
}

int syncprog_puxar_sessao(int perfil, uint64_t sessao) {
  Jsw w;
  char *r;
  int st = 0, k = 0;
  ProgRegistro *novo;
  SyncProgRemocao dono;
  if (!syncprog_remocao_capturar(&dono) || dono.perfil != perfil || dono.sessao != sessao) return -1;
  const char *p;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfil);
  jsw_obj_fim(&w);
  r = contaRpc(&dono, "sync_pull_watch_progress", jsw_texto_final(&w), &st);
  memset(dono.jwt, 0, sizeof dono.jwt);
  jsw_livre(&w);
  if (!ok2xx(r, st)) { free(r); return -1; }

  novo = calloc(SP_MAX, sizeof *novo);
  if (!novo) { free(r); return -1; }
  for (p = js_raiz_array(r); p && k < SP_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    ProgRegistro *d = &novo[k];
    double pos, dur;
    char id[40];
    if (!js_texto(p, f, "content_id", id, sizeof id) || !id[0]) continue;
    // O web aceita position_ms/duration_ms e position/duration; os primeiros
    // ganham quando existem, porque os segundos ja vem em milissegundos nesta
    // RPC e misturar as duas unidades produz progresso de 100% em tudo.
    if (!lerNumero(p, f, "position_ms", -1.0, &pos) ||
        !lerNumero(p, f, "duration_ms", -1.0, &dur)) continue;
    if (pos < 0 && !lerNumero(p, f, "position", 0, &pos)) continue;
    if (dur < 0 && !lerNumero(p, f, "duration", 0, &dur)) continue;
    pos /= 1000.0;
    dur /= 1000.0;
    if (!isfinite(pos) || !isfinite(dur) || dur <= 1.0 ||
        pos >= (double)LLONG_MAX / 1000.0 || dur >= (double)LLONG_MAX / 1000.0) continue;
    memset(d, 0, sizeof *d);
    d->perfil = perfil;
    // content_id pode vir composto de um cliente antigo ("tt123:4:9"): corta,
    // e aproveita temporada/episodio de la se as colunas nao vierem.
    { int tI = 0, eI = 0;
      prog_content_id(d->contentId, sizeof d->contentId, id, &tI, &eI);
      if (!lerCoordenada(p, f, "season", &d->temporada) ||
          !lerCoordenada(p, f, "episode", &d->episodio)) continue;
      if (d->episodio <= 0) { d->temporada = tI; d->episodio = eI; } }
    if (d->episodio <= 0) { d->temporada = 0; d->episodio = 0; }
    if (d->temporada < 0) d->temporada = 0;
    snprintf(d->tipo, sizeof d->tipo, "%s", d->episodio > 0 ? "series" : "movie");
    // A chave e SEMPRE recalculada, nunca copiada do servidor: uma linha antiga
    // escrita por este mesmo app trazia "tt123:4:9" em progress_key, e adotar
    // isso perpetuaria a duplicata que estamos consertando.
    prog_chave(d->chave, sizeof d->chave, d->contentId, d->temporada, d->episodio);
    d->posSeg = pos < 0 ? 0 : pos;
    d->durSeg = dur;
    d->lastWatchedMs = lerInstanteMs(p, f);
    d->pendente = 0;
    k++;
  }
  free(r);
  // Vazio nao apaga nada: quem consome so aplica o que veio.
  if (!nv_perfil_sessao_travar(sessao)) { free(novo); return -1; }
  if (!pedidoContaAtual(&dono)) { nv_perfil_sessao_destravar(); free(novo); return -1; }
  pthread_mutex_lock(&caixaTrava);
  memcpy(caixa, novo, sizeof *novo * (size_t)k);
  nCaixa = k;
  caixaSessao = sessao;
  pthread_mutex_unlock(&caixaTrava);
  nv_perfil_sessao_destravar();
  free(novo);
  return k;
}

int syncprog_empurrar_sessao(int perfil, uint64_t sessao) {
  ProgRegistro *pend = malloc(sizeof *pend * PROG_MAX);
  if (!pend) return -1;
  SyncProgRemocao dono;
  Jsw w;
  char *r;
  int n, i, k = 0, st = 0;

  if (!nv_perfil_sessao_travar(sessao)) { free(pend); return -1; }
  if (!syncprog_remocao_capturar(&dono) || dono.perfil != perfil || dono.sessao != sessao) {
    nv_perfil_sessao_destravar(); free(pend); return -1;
  }
  n = perfil == perfis_ativo() ? prog_pendentes(pend, PROG_MAX) : 0;
  nv_perfil_sessao_destravar();
  if (n <= 0) { memset(&dono, 0, sizeof dono); free(pend); return 0; }   // vazio nunca vira push; delecao tem RPC propria

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", dono.perfil);
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, "p_entries");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    const ProgRegistro *p = &pend[i];
    char video[64];
    if (p->perfil != dono.perfil) continue;
    if (p->durSeg < DUR_MINIMA_SEG) continue;   // ruido: o web tambem nao manda
    if (p->episodio > 0) snprintf(video, sizeof video, PREFIXO_EPISODIO "%d:%d", p->temporada, p->episodio);
    else                 snprintf(video, sizeof video, "%s", p->contentId);
    jsw_obj_ini(&w);
    jsw_cs(&w, "content_id", p->contentId);
    jsw_cs(&w, "content_type", p->tipo);
    jsw_cs(&w, "video_id", video);
    if (p->episodio > 0) { jsw_ci(&w, "season", p->temporada); jsw_ci(&w, "episode", p->episodio); }
    else                 { jsw_chave(&w, "season"); jsw_nulo(&w);
                           jsw_chave(&w, "episode"); jsw_nulo(&w); }
    jsw_ci(&w, "position", (long long)(p->posSeg * 1000.0));
    jsw_ci(&w, "duration", (long long)(p->durSeg * 1000.0));
    jsw_ci(&w, "last_watched", p->lastWatchedMs > 0 ? p->lastWatchedMs : prog_agora_ms());
    jsw_cs(&w, "progress_key", p->chave);
    jsw_obj_fim(&w);
    // Compactar so o que foi enviado preserva a copia usada para confirmar o
    // push. k <= i, entao nao pisa em uma entrada ainda nao processada.
    if (k != i) pend[k] = *p;
    k++;
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  if (k == 0) { jsw_livre(&w); memset(&dono, 0, sizeof dono); free(pend); return 0; }
  r = contaRpc(&dono, "sync_push_watch_progress", jsw_texto_final(&w), &st);
  memset(dono.jwt, 0, sizeof dono.jwt);
  jsw_livre(&w);
  if (!ok2xx(r, st)) {
    printf("[sync] push de progresso falhou (HTTP %d)\n", st);
    free(r); free(pend);
    return -1;
  }
  free(r);
  // A confirmacao de uma copia antiga nao quita uma escrita mais nova feita
  // pelo player na mesma chave durante a viagem.
  NvPerfilSessao agora = nv_perfil_sessao_ler();
  if (nv_perfil_sessao_travar(agora)) {
    if (pedidoContaAtual(&dono)) prog_confirmar_empurrados(pend, k);
    nv_perfil_sessao_destravar();
  }
  free(pend); return k;
}

// APAGA UMA ENTRADA DE PROGRESSO NA CONTA. A outra metade do issue #22: sem
// isto, tirar um item de "Continuar assistindo" apagava o registro local e a
// entrada da conta o trazia de volta no ciclo seguinte, exatamente como a do
// Trakt fazia.
//
// `p_keys` e a chave de progresso (prog_chave), a mesma que o push manda em
// `progress_key` — e nao o content_id. Mandar o id apagaria todos os episodios
// da serie.
// The public backend contract and the official Android TV client both pass
// p_profile_id here. Omitting it invokes the SQL default (profile 1).
extern int sessao_identidade_local_copiar(char *, size_t, char *, size_t) NV_PERFIL_OPCIONAL;
extern const char *sessao_usuario(void) NV_PERFIL_OPCIONAL;
extern int sessao_token_copiar(char *, size_t) NV_PERFIL_OPCIONAL;
extern void sessao_token_recusado(const char *) NV_PERFIL_OPCIONAL;
extern char *nuvem_rpc_com(const char *, const char *, const char *, int *) NV_PERFIL_OPCIONAL;
int syncprog_remocao_capturar(SyncProgRemocao *p) {
  if (!p) return 0;
  memset(p, 0, sizeof *p); p->sessao = nv_perfil_sessao_ler();
  if (!nv_perfil_sessao_travar(p->sessao)) return 0;
  p->perfil = perfis_ativo();
  if (sessao_identidade_local_copiar)
    p->autenticada = sessao_identidade_local_copiar(p->usuario, sizeof p->usuario, p->jwt, sizeof p->jwt);
  else snprintf(p->usuario, sizeof p->usuario, "%s", sessao_usuario ? sessao_usuario() : ""); // account-free fixture
  nv_perfil_sessao_destravar();
  return 1;
}
static int pedidoContaAtual(const SyncProgRemocao *p) {
  if (!sessao_identidade_local_copiar) return 1; // isolated fixtures
  char usuario[80], jwt[3000];
  int atual = sessao_identidade_local_copiar(usuario, sizeof usuario, jwt, sizeof jwt) && !strcmp(usuario, p->usuario);
  memset(jwt, 0, sizeof jwt); return atual;
}
static char *contaRpc(const SyncProgRemocao *p, const char *funcao, const char *corpo, int *status) {
  char *r = NULL; *status = 0;
  if (p->autenticada && p->jwt[0] && nuvem_rpc_com) {
    const char *jwt = p->jwt;
    char atualUsuario[80], novo[3000], confirmado[3000];
    // Refresh is worker-only and only useful while the original account is
    // still current. A profile switch alone keeps the immutable destination;
    // a different login never supplies authentication for this old gesture.
    if (sessao_identidade_local_copiar && sessao_token_copiar &&
        sessao_identidade_local_copiar(atualUsuario, sizeof atualUsuario, novo, sizeof novo) &&
        !strcmp(atualUsuario, p->usuario) && sessao_token_copiar(novo, sizeof novo) &&
        sessao_identidade_local_copiar(atualUsuario, sizeof atualUsuario, confirmado, sizeof confirmado) &&
        !strcmp(atualUsuario, p->usuario) && !strcmp(novo, confirmado)) jwt = confirmado;
    r = nuvem_rpc_com(funcao, corpo, jwt, status);
    if (*status == 401 && sessao_token_recusado && sessao_identidade_local_copiar &&
        sessao_identidade_local_copiar(atualUsuario, sizeof atualUsuario, novo, sizeof novo) &&
        !strcmp(atualUsuario, p->usuario) && !strcmp(novo, jwt)) {
      sessao_token_recusado(jwt);
      if (sessao_identidade_local_copiar(atualUsuario, sizeof atualUsuario, novo, sizeof novo) &&
          !strcmp(atualUsuario, p->usuario) && strcmp(novo, jwt)) {
        free(r); r = nuvem_rpc_com(funcao, corpo, novo, status);
      }
    }
    memset(novo, 0, sizeof novo); memset(confirmado, 0, sizeof confirmado);
  }
  // Existing isolated fixtures provide only sessao_rpc. A complete app always
  // has local identity copy and never resolves a destination account here.
  else if (!sessao_identidade_local_copiar && nv_perfil_sessao_atual(p->sessao) && p->perfil == perfis_ativo())
    r = sessao_rpc(funcao, corpo, status);
  return r;
}
int syncprog_remover_pedido(const SyncProgRemocao *p, const char *chave) {
  if (!p || p->perfil < 1 || !chave || !*chave) return 0;
  Jsw w; int st = 0; char *r = NULL;
  jsw_iniciar(&w); jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", p->perfil);
  jsw_chave(&w, "p_keys"); jsw_arr_ini(&w); jsw_str(&w, chave); jsw_arr_fim(&w); jsw_obj_fim(&w);
  r = contaRpc(p, "sync_delete_watch_progress", jsw_texto_final(&w), &st);
  jsw_livre(&w); int ok = ok2xx(r, st); free(r);
  printf("[sync] progresso removido da conta no perfil %d: %s -> %s (HTTP %d)\n", p->perfil, chave, ok ? "ok" : "falhou", st);
  fflush(stdout); return ok;
}
int syncprog_remover_sessao(const char *chave, int perfil, uint64_t sessao, const char *usuario) {
  SyncProgRemocao p;
  if (!syncprog_remocao_capturar(&p)) return 0;
  int correto = p.perfil == perfil && p.sessao == sessao && !strcmp(p.usuario, usuario ? usuario : "");
  int ok = correto ? syncprog_remover_pedido(&p, chave) : 0;
  memset(&p, 0, sizeof p); return ok;
}
int syncprog_remover(const char *chave) {
  SyncProgRemocao p;
  if (!syncprog_remocao_capturar(&p)) return 0;
  int ok = syncprog_remover_pedido(&p, chave); memset(&p, 0, sizeof p); return ok;
}

// Marcar/desmarcar visto na conta mora em contapend.c (jornal persistido).

int syncprog_puxar(void) {
  return syncprog_puxar_sessao(perfis_ativo(), nv_perfil_sessao_ler());
}
int syncprog_empurrar(void) {
  return syncprog_empurrar_sessao(perfis_ativo(), nv_perfil_sessao_ler());
}

int syncprog_aplicar(int *casaram) {
  int i, aceitos = 0, noCatalogo = 0, perfil = perfis_ativo();
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (casaram) *casaram = 0;
  if (!nv_perfil_sessao_travar(sessao)) return 0;
  pthread_mutex_lock(&caixaTrava);
  if (caixaSessao != sessao) nCaixa = 0;
  for (i = 0; i < nCaixa; i++) {
    // A resposta pertence ao perfil que fez o pedido. A escolha na TV pode
    // mudar enquanto a RPC esta em voo ou antes deste consumo no fio principal.
    if (caixa[i].perfil != perfil) continue;
    if (!prog_aplicar_remoto(&caixa[i])) continue;
    aceitos++;
    { int idx = cat_indice_por_imdb(caixa[i].contentId);
      if (idx >= 0) {
        cat_aplicar_progresso(idx, caixa[i].posSeg, caixa[i].durSeg,
                              caixa[i].temporada, caixa[i].episodio);
        noCatalogo++;
      } }
  }
  if (nCaixa)
    printf("[sync] progresso: %d linhas, %d aceitas, %d no catalogo\n", nCaixa, aceitos, noCatalogo);
  nCaixa = 0;
  if (casaram) *casaram = noCatalogo;
  pthread_mutex_unlock(&caixaTrava);
  nv_perfil_sessao_destravar();
  return aceitos;
}

int syncprog_puxadas(void) {
  pthread_mutex_lock(&caixaTrava);
  int n = nv_perfil_sessao_atual(caixaSessao) ? nCaixa : 0;
  pthread_mutex_unlock(&caixaTrava);
  return n;
}
void syncprog_esquecer(void) {
  pthread_mutex_lock(&caixaTrava); nCaixa = 0; pthread_mutex_unlock(&caixaTrava);
}
