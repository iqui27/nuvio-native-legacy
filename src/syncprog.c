#include "syncprog.h"
#include "progresso.h"
#include "catalogo.h"
#include "sessao.h"
#include "perfis.h"
#include "dados.h"
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SP_MAX 240
#define PREFIXO_EPISODIO "__nuvio_episode__:"
// O web nao sincroniza titulo com menos de um minuto (MIN_PROGRESS_SYNC_DURATION_MS).
#define DUR_MINIMA_SEG 60.0

static ProgRegistro caixa[SP_MAX];
static int nCaixa;

static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }


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
    if (v > 0) return v > 1000000000000.0 ? (long long)v : (long long)(v * 1000.0);
  }
  return 0;
}

int syncprog_puxar(void) {
  Jsw w;
  char *r;
  int st = 0, k = 0;
  const char *p;

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfis_ativo());
  jsw_obj_fim(&w);
  r = sessao_rpc("sync_pull_watch_progress", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!ok2xx(r, st)) { free(r); return -1; }

  for (p = js_raiz_array(r); p && k < SP_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    ProgRegistro *d = &caixa[k];
    double pos, dur;
    char id[40];
    if (!js_texto(p, f, "content_id", id, sizeof id) || !id[0]) continue;
    // O web aceita position_ms/duration_ms e position/duration; os primeiros
    // ganham quando existem, porque os segundos ja vem em milissegundos nesta
    // RPC e misturar as duas unidades produz progresso de 100% em tudo.
    pos = js_num(p, f, "position_ms", -1.0);
    dur = js_num(p, f, "duration_ms", -1.0);
    if (pos < 0) pos = js_num(p, f, "position", 0);
    if (dur < 0) dur = js_num(p, f, "duration", 0);
    pos /= 1000.0;
    dur /= 1000.0;
    if (dur <= 1.0) continue;
    memset(d, 0, sizeof *d);
    // content_id pode vir composto de um cliente antigo ("tt123:4:9"): corta,
    // e aproveita temporada/episodio de la se as colunas nao vierem.
    { int tI = 0, eI = 0;
      prog_content_id(d->contentId, sizeof d->contentId, id, &tI, &eI);
      d->temporada = (int)js_num(p, f, "season", -1);
      d->episodio  = (int)js_num(p, f, "episode", -1);
      if (d->episodio <= 0) { d->temporada = tI; d->episodio = eI; } }
    if (d->episodio <= 0) { d->temporada = 0; d->episodio = 0; }
    if (d->temporada < 0) d->temporada = 0;
    snprintf(d->tipo, sizeof d->tipo, "%s", d->episodio > 0 ? "series" : "movie");
    // A chave e SEMPRE recalculada, nunca copiada do servidor: uma linha antiga
    // escrita por este mesmo app trazia "tt123:4:9" em progress_key, e adotar
    // isso perpetuaria a duplicata que estamos consertando.
    prog_chave(d->chave, sizeof d->chave, d->contentId, d->temporada, d->episodio);
    d->posSeg = pos;
    d->durSeg = dur;
    d->lastWatchedMs = lerInstanteMs(p, f);
    d->pendente = 0;
    k++;
  }
  free(r);
  // Vazio nao apaga nada: quem consome so aplica o que veio.
  nCaixa = k;
  return k;
}

int syncprog_empurrar(void) {
  static ProgRegistro pend[PROG_MAX];
  static const char *chaves[PROG_MAX];
  Jsw w;
  char *r;
  int n, i, k = 0, st = 0;

  n = prog_pendentes(pend, PROG_MAX);
  if (n <= 0) return 0;   // vazio nunca vira push; delecao tem RPC propria

  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", perfis_ativo());
  jsw_cs(&w, "p_origin_client_id", dados_cliente_id());
  jsw_chave(&w, "p_entries");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    const ProgRegistro *p = &pend[i];
    char video[64];
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
    chaves[k++] = p->chave;
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  if (k == 0) { jsw_livre(&w); return 0; }
  r = sessao_rpc("sync_push_watch_progress", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  if (!ok2xx(r, st)) {
    printf("[sync] push de progresso falhou (HTTP %d)\n", st);
    free(r);
    return -1;
  }
  free(r);
  // So as chaves que foram: o player pode ter gravado outra durante a viagem.
  prog_marcar_empurrados(chaves, k);
  return k;
}

// APAGA UMA ENTRADA DE PROGRESSO NA CONTA. A outra metade do issue #22: sem
// isto, tirar um item de "Continuar assistindo" apagava o registro local e a
// entrada da conta o trazia de volta no ciclo seguinte, exatamente como a do
// Trakt fazia.
//
// `p_keys` e a chave de progresso (prog_chave), a mesma que o push manda em
// `progress_key` — e nao o content_id. Mandar o id apagaria todos os episodios
// da serie.
int syncprog_remover(const char *chave) {
  Jsw w;
  char *r;
  int st = 0, ok;
  if (!chave || !chave[0]) return 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "p_keys");
  jsw_arr_ini(&w);
  jsw_str(&w, chave);
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  r = sessao_rpc("sync_delete_watch_progress", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  ok = ok2xx(r, st);
  free(r);
  printf("[sync] progresso removido da conta: %s -> %s (HTTP %d)\n",
         chave, ok ? "ok" : "falhou", st);
  fflush(stdout);
  return ok;
}

int syncprog_aplicar(int *casaram) {
  int i, aceitos = 0, noCatalogo = 0;
  for (i = 0; i < nCaixa; i++) {
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
  return aceitos;
}

int  syncprog_puxadas(void) { return nCaixa; }
void syncprog_esquecer(void) { nCaixa = 0; }
