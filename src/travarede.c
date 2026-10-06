// Trava de serie: servidor, conta, Trakt dos outros perfis e ilha. Ver travarede.h.
#include "travarede.h"
#include "trava.h"
#include "visto.h"
#include "contapend.h"
#include "traktauth.h"
#include "trakt.h"
#include "perfis.h"
#include "sessao.h"
#include "rede.h"
#include "js.h"
#include "jsw.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "idioma.h"
#include "player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// O DIARIO (feat/sp-diario) entra sozinho quando o header existir no mesmo
// build: o modo Juntos grava a vez vista tambem no diario dos outros membros,
// com DIARIO_F_JUNTOS e os bits de quem estava junto (API-diario.md).
#if defined(__has_include)
#if __has_include("diario.h")
#include "diario.h"
#define TRAVA_COM_DIARIO 1
#endif
#endif

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define PRAZO_S        15
#define JOB_OPS         8
#define RECONCILIA_MS  (3600u * 1000u)   // conta dos membros: de hora em hora
#define RELEITURA_MS   (15u * 60u * 1000u) // GET de seguranca sem travaRev
#define ESPERA_FALHA_MS 60000u
#define PERFIS_MS       2000u

typedef struct {
  TravaOp op;
  char url[200];
  char corpo[3072];
  char perfilCab[40];
  int  status;
  char idNovo[TRAVA_ID_MAX];
} JobOp;

typedef struct {
  int perfil;
  int n;
  char serie[TRAVA_MAX][24];
  int t[TRAVA_MAX], e[TRAVA_MAX];
  int ok;
} JobRec;

typedef struct {
  unsigned ger;
  char aut[1200];
  int  nOps;
  JobOp ops[JOB_OPS];
  int  get, getPerfil, getStatus;
  long long revPedida;          // travaRev quando o GET saiu
  char getPerfilCab[40];
  char etag[96], getEtag[96];
  char *getCorpo;
  int  nRec;
  JobRec rec[TRAVA_PERFIS_MAX];
} Job;

static SDL_mutex *mtx;
static Job *emVoo;            // dono: o fio, ate `pronto`
static int pronto;
static volatile long long revServidor = -1;
static long long revAplicada = -2;
static unsigned ger;
static int pedirJa, reconciliarJa;
static Uint32 proxGet, proxRec, proxEnvio, proxPerfis, esperaGet, agoraQ;
static int perfilGet;         // perfil da ultima leitura (o ETag e por pessoa)
static char etagGet[96];

static int ativa(void) { return NV_REC_URL[0] != 0; }

void travarede_rev(long long rev) { revServidor = rev; }
void travarede_atualizar_ja(void) { pedirJa = 1; reconciliarJa = 1; }

static void serieDe(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return;
  while (s[k] && s[k] != ':' && k + 1 < tam) { d[k] = s[k]; k++; }
  d[k] = 0;
}

// ---------------------------------------------------------------- perfis

static void acompanharPerfis(void) {
  TravaPerfil v[TRAVA_PERFIS_MAX];
  int i, n = 0;
  const char *sub = sessao_logada() ? sessao_usuario() : "";
  if (strcmp(trava_conta(), sub ? sub : "")) {
    trava_definir_conta(sub);
    ger++;
    pedirJa = 1; reconciliarJa = 1; etagGet[0] = 0;
  }
  memset(v, 0, sizeof v);
  for (i = 0; i < perfis_n() && n < TRAVA_PERFIS_MAX; i++) {
    const ContaPerfil *p = perfis_item(i);
    if (!p || p->indice < 1) continue;
    v[n].indice = p->indice;
    v[n].primario = p->primario;
    v[n].temPin = p->temPin;
    snprintf(v[n].nome, sizeof v[n].nome, "%s", p->nome);
    snprintf(v[n].avatar, sizeof v[n].avatar, "%s", p->avatarUrl);
    snprintf(v[n].cor, sizeof v[n].cor, "%s", p->corHex);
    n++;
  }
  if (perfis_ativo() != trava_perfil_ativo()) { pedirJa = 1; etagGet[0] = 0; }
  trava_definir_perfis(v, n, perfis_ativo());
}

// ---------------------------------------------------------------- o fio

static void cabPerfil(int perfil, char *dst, size_t tam) {
  const TravaPerfil *p = trava_perfil_indice(perfil);
  dst[0] = 0;
  // O PRINCIPAL nao manda o cabecalho (comPerfil do servidor).
  if (perfil < 1 || (p && p->primario) || (!p && perfil == 1)) return;
  snprintf(dst, tam, "X-Nuvio-Perfil: %d", perfil);
}

static void cabecalhos(const Job *j, const char *perfilCab, const char **cab) {
  cab[0] = j->aut;
  cab[1] = "X-Nuvio-Auth: nuvio";
  cab[2] = perfilCab[0] ? perfilCab : NULL;
  cab[3] = NULL;
  cab[4] = NULL;
}

static int falhaDeRede(int st) { return st == 0 || st == 429 || st >= 500; }

// Vistos da conta de um perfil, filtrados pelas series da trava. Paginas de
// 900 (o mesmo de contalib), ate 3.
static void reconciliar(JobRec *r) {
  int pagina, k;
  for (pagina = 1; pagina <= 3; pagina++) {
    char corpo[96], *resp;
    const char *p;
    int st = 0, linhas = 0;
    snprintf(corpo, sizeof corpo, "{\"p_profile_id\":%d,\"p_page\":%d,\"p_page_size\":900}",
             r->perfil, pagina);
    resp = sessao_rpc("sync_pull_watched_items", corpo, &st);
    if (!resp || st < 200 || st >= 300) { free(resp); return; }
    for (p = js_raiz_array(resp); p && *p == '{'; ) {
      const char *f = js_fim(p);
      char id[48] = "", base[24];
      int t, e;
      if (!f) break;
      linhas++;
      js_texto(p, f, "content_id", id, sizeof id);
      serieDe(id, base, sizeof base);
      t = (int)js_num(p, f, "season", 0);
      e = (int)js_num(p, f, "episode", 0);
      if (t > 0 && e > 0)
        for (k = 0; k < r->n; k++)
          if (!strcmp(r->serie[k], base) && trava_ep_cmp(t, e, r->t[k], r->e[k]) > 0) {
            r->t[k] = t; r->e[k] = e;
          }
      p = js_prox(f);
    }
    free(resp);
    r->ok = 1;
    if (linhas < 900) break;
  }
}

static int fio(void *u) {
  Job *j = (Job *)u;
  int i;
  const char *cab[5];
  for (i = 0; i < j->nOps; i++) {
    JobOp *o = &j->ops[i];
    char *r;
    cabecalhos(j, o->perfilCab, cab);
    r = rede_postar_st(o->url, PRAZO_S, cab, o->corpo, &o->status);
    if (r && o->status >= 200 && o->status < 300 && !strcmp(o->op.op, "criar"))
      js_texto_raiz(r, "id", o->idNovo, sizeof o->idNovo);
    printf("[trava] %s %s -> HTTP %d\n", o->op.op, o->op.id, o->status);
    free(r);
    if (falhaDeRede(o->status)) {
      for (i = i + 1; i < j->nOps; i++) j->ops[i].status = -1;
      break;
    }
  }
  if (j->get) {
    char url[200], cabEtag[140];
    int k = 0;
    cabecalhos(j, j->getPerfilCab, cab);
    while (cab[k]) k++;
    if (j->etag[0]) { snprintf(cabEtag, sizeof cabEtag, "If-None-Match: %s", j->etag); cab[k] = cabEtag; cab[k + 1] = NULL; }
    snprintf(url, sizeof url, "%s/v1/travas", NV_REC_URL);
    j->getCorpo = rede_baixar_etag(url, PRAZO_S, cab, &j->getStatus, j->getEtag, sizeof j->getEtag);
  }
  for (i = 0; i < j->nRec; i++) reconciliar(&j->rec[i]);
  SDL_LockMutex(mtx);
  pronto = 1;
  SDL_UnlockMutex(mtx);
  return 0;
}

// ---------------------------------------------------------------- montar

static void corpoOp(const TravaOp *op, JobOp *o) {
  const Trava *t = trava_por_id(op->id);
  Jsw w;
  int k;
  snprintf(o->url, sizeof o->url, "%s/v1/trava%s%s", NV_REC_URL,
           strcmp(op->op, "criar") ? "/" : "", strcmp(op->op, "criar") ? op->op : "");
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  if (!strcmp(op->op, "criar") && t) {
    jsw_cs(&w, "serie", t->serie);
    jsw_cs(&w, "titulo", t->titulo);
    jsw_cs(&w, "poster", t->poster);
    jsw_ci(&w, "modo", t->modo);
    jsw_ci(&w, "pin", t->pin);
  } else jsw_cs(&w, "id", op->id);
  if ((!strcmp(op->op, "criar") || !strcmp(op->op, "membros")) && t) {
    jsw_chave(&w, "membros");
    jsw_arr_ini(&w);
    for (k = 0; k < t->n; k++)
      if (t->m[k].pessoa[0] && t->m[k].estado == TRAVA_DENTRO) jsw_str(&w, t->m[k].pessoa);
    jsw_arr_fim(&w);
  }
  if (!strcmp(op->op, "modo")) jsw_ci(&w, "modo", op->a);
  if (!strcmp(op->op, "pin")) jsw_ci(&w, "pin", op->a);
  if (!strcmp(op->op, "pausar")) jsw_ci(&w, "pausada", op->a);
  if (!strcmp(op->op, "passo")) {
    jsw_ci(&w, "t", op->a);
    jsw_ci(&w, "e", op->b);
    if (op->pessoa[0]) jsw_cs(&w, "pessoa", op->pessoa);
    if (op->c) jsw_ci(&w, "desfazer", 1);
  }
  jsw_obj_fim(&w);
  snprintf(o->corpo, sizeof o->corpo, "%s", jsw_texto_final(&w));
  jsw_livre(&w);
  cabPerfil(op->perfil, o->perfilCab, sizeof o->perfilCab);
}

static int lancar(Uint32 agora) {
  Job *j;
  int i, n = trava_fila_n(), querGet, querRec;
  SDL_Thread *th;
  if (emVoo || !ativa() || !sessao_logada() || !sessao_token()[0] || !trava_conta()[0]) return 0;
  querGet = (int)(agora - esperaGet) >= 0 &&
            (pedirJa || revServidor != revAplicada || perfilGet != trava_perfil_ativo() ||
             (int)(agora - proxGet) >= 0);
  querRec = reconciliarJa || (trava_n() > 0 && (int)(agora - proxRec) >= 0);
  if (!n && !querGet && !querRec) return 0;
  if (n && (int)(agora - proxEnvio) < 0 && !querGet && !querRec) return 0;
  j = (Job *)calloc(1, sizeof *j);
  if (!j) return 0;
  j->ger = ger;
  snprintf(j->aut, sizeof j->aut, "Authorization: Bearer %s", sessao_token());
  if ((int)(agora - proxEnvio) >= 0)
    for (i = 0; i < n && j->nOps < JOB_OPS; i++) {
      TravaOp op;
      if (!trava_fila_ver(i, &op)) break;
      // Operacao de trava ainda local espera o "criar" dela ser confirmado.
      if (strcmp(op.op, "criar") && !strncmp(op.id, "l-", 2)) break;
      if (!strcmp(op.op, "criar") && !trava_por_id(op.id)) { trava_fila_tirar(&op); i--; n--; continue; }
      j->ops[j->nOps].op = op;
      corpoOp(&op, &j->ops[j->nOps]);
      j->nOps++;
    }
  if (querGet) {
    j->get = 1;
    j->revPedida = revServidor;
    j->getPerfil = trava_perfil_ativo();
    cabPerfil(j->getPerfil, j->getPerfilCab, sizeof j->getPerfilCab);
    if (perfilGet == j->getPerfil) snprintf(j->etag, sizeof j->etag, "%s", etagGet);
    pedirJa = 0;
    proxGet = agora + RELEITURA_MS;
  }
  if (querRec) {
    // Os membros DA CONTA de todas as travas (o ativo tambem: ele pode ter
    // assistido em outro app Nuvio), com as series de cada um.
    for (i = 0; i < trava_n(); i++) {
      const Trava *t = trava_i(i);
      int k;
      for (k = 0; k < t->n; k++) {
        int r, s, p = t->m[k].perfil;
        JobRec *jr = NULL;
        if (p < 1 || t->m[k].estado != TRAVA_DENTRO) continue;
        for (r = 0; r < j->nRec; r++) if (j->rec[r].perfil == p) jr = &j->rec[r];
        if (!jr && j->nRec < TRAVA_PERFIS_MAX) { jr = &j->rec[j->nRec++]; jr->perfil = p; }
        if (!jr) continue;
        for (s = 0; s < jr->n; s++) if (!strcmp(jr->serie[s], t->serie)) break;
        if (s == jr->n && jr->n < TRAVA_MAX) {
          snprintf(jr->serie[jr->n], sizeof jr->serie[0], "%s", t->serie);
          jr->n++;
        }
      }
    }
    reconciliarJa = 0;
    proxRec = agora + RECONCILIA_MS;
  }
  if (!j->nOps && !j->get && !j->nRec) { free(j); return 0; }
  if (!mtx) mtx = SDL_CreateMutex();
  emVoo = j;
  pronto = 0;
  th = SDL_CreateThread(fio, "nv-trava", j);
  if (!th) { emVoo = NULL; free(j); return 0; }
  SDL_DetachThread(th);
  return 1;
}

static void aplicar(Job *j, Uint32 agora) {
  int i, falhou = 0;
  if (j->ger != ger) goto fim;   // conta trocou no meio: nada daqui vale
  for (i = 0; i < j->nOps; i++) {
    JobOp *o = &j->ops[i];
    if (o->status >= 200 && o->status < 300) {
      trava_fila_tirar(&o->op);
      if (!strcmp(o->op.op, "criar") && o->idNovo[0]) trava_confirmar(o->op.id, o->idNovo);
    } else if (o->status >= 400 && o->status < 500 && o->status != 429) {
      // Recusa de verdade (trava que nao existe mais, membro que saiu, conta
      // sem perfis): repetir nao muda nada. A trava continua LOCAL.
      printf("[trava] %s %s recusado (HTTP %d): sai da fila\n", o->op.op, o->op.id, o->status);
      trava_fila_tirar(&o->op);
    } else if (o->status != -1 || falhou) falhou = 1;
  }
  if (falhou) proxEnvio = agora + ESPERA_FALHA_MS;
  if (j->get) {
    if (j->getStatus == 0) {
      // Sem rede: tenta de novo em um minuto, nao a cada quadro.
      esperaGet = agora + ESPERA_FALHA_MS;
    } else {
      // Respondeu (200, 304, ou recusa: sem a migracao 012, conta so Trakt).
      // Em todos, a revisao que pediu a leitura esta atendida.
      revAplicada = j->revPedida;
      perfilGet = j->getPerfil;
      if (j->getStatus >= 200 && j->getStatus < 300 && j->getCorpo &&
          j->getPerfil == trava_perfil_ativo() &&
          trava_aplicar_servidor(j->getCorpo, j->getPerfil)) {
        snprintf(etagGet, sizeof etagGet, "%s", j->getEtag);
        trava_definir_rev((long long)js_num(j->getCorpo, NULL, "rev", 0));
      } else if (j->getStatus >= 400 && j->getStatus != 429) {
        esperaGet = agora + RELEITURA_MS;
      }
    }
  }
  for (i = 0; i < j->nRec; i++) {
    const JobRec *r = &j->rec[i];
    int s, k;
    if (!r->ok) continue;
    for (s = 0; s < r->n; s++) {
      if (r->t[s] <= 0) continue;
      for (k = 0; k < trava_n(); k++) {
        const Trava *t = trava_i(k);
        if (!strcmp(t->serie, r->serie[s]))
          trava_posicao(t->id, r->perfil, r->t[s], r->e[s], (long long)time(NULL));
      }
    }
  }
fim:
  free(j->getCorpo);
  free(j);
}

// ---------------------------------------------------------------- Juntos

typedef struct {
  char imdb[24], serie[24];
  VistoPar pares[16];
  int n, visto, np;
  int perfis[TRAVA_PERFIS_MAX];
  char toks[TRAVA_PERFIS_MAX][200];
} TraktLote;

static int fioTrakt(void *u) {
  TraktLote *l = (TraktLote *)u;
  int i;
  for (i = 0; i < l->np; i++)
    if (l->toks[i][0])
      printf("[trava] trakt do perfil %d: %s\n", l->perfis[i],
             trakt_episodios_marcar_token(l->toks[i], l->imdb, l->pares, l->n, l->visto)
             ? "ok" : "falhou");
  free(l);
  return 0;
}

static void traktOutros(const char *imdb, const VistoPar *pares, int n, int visto,
                        const int *perfis, int np) {
  TraktLote *l = (TraktLote *)calloc(1, sizeof *l);
  int i, algum = 0;
  SDL_Thread *th;
  if (!l) return;
  snprintf(l->imdb, sizeof l->imdb, "%s", imdb);
  l->n = n > 16 ? 16 : n;
  memcpy(l->pares, pares, sizeof(VistoPar) * (size_t)l->n);
  l->visto = visto;
  // SO o Trakt de quem e MEMBRO desta trava Juntos e tem o token NESTA TV
  // (decisao do dono). Nunca o de quem nao e membro.
  for (i = 0; i < np && l->np < TRAVA_PERFIS_MAX; i++) {
    l->perfis[l->np] = perfis[i];
    if (traktauth_token_de(perfis[i], l->toks[l->np], sizeof l->toks[0])) algum = 1;
    l->np++;
  }
  if (!algum) { free(l); return; }
  th = SDL_CreateThread(fioTrakt, "nv-trava-trakt", l);
  if (th) SDL_DetachThread(th); else free(l);
}

typedef struct {
  char imdb[24], serie[24];
  VistoPar pares[16];
  int n, np;
  int perfis[TRAVA_PERFIS_MAX], antT[TRAVA_PERFIS_MAX], antE[TRAVA_PERFIS_MAX];
} Desfazer;

static void desfazerJuntos(const void *ctx) {
  const Desfazer *d = (const Desfazer *)ctx;
  int i;
  if (!d) return;
  for (i = 0; i < d->np; i++) {
#if TRAVA_JUNTOS_NA_CONTA
    contapend_episodios_perfil(d->perfis[i], d->imdb, "series", d->pares, d->n, 0);
#endif
    trava_passo(d->serie, d->perfis[i], d->antT[i], d->antE[i], 1);
  }
  contapend_chutar();
  traktOutros(d->imdb, d->pares, d->n, 0, d->perfis, d->np);
  printf("[trava] juntos desfeito para %d perfis em %s\n", d->np, d->serie);
}

static const char *nomePerfil(int indice) {
  const TravaPerfil *p = trava_perfil_indice(indice);
  return p && p->nome[0] ? p->nome : "?";
}

void travarede_visto(const char *imdb, const VistoPar *pares, int n, int visto) {
  char serie[24];
  int idx[TRAVA_MAX], ni, i, k, mt = 0, me = 0, ativo = trava_perfil_ativo();
  Desfazer d;
  const Trava *ultima = NULL;
  serieDe(imdb, serie, sizeof serie);
  // DESMARCAR NAO RECUA A TRAVA: a posicao e "o mais adiantado visto", e
  // desmarcar um episodio no meio nao desfaz o resto. So o Desfazer da ilha
  // volta (ele sabe o ponto de antes).
  if (!serie[0] || !visto || !pares || n < 1) return;
  for (i = 0; i < n; i++)
    if (pares[i].temporada > 0 && trava_ep_cmp(pares[i].temporada, pares[i].episodio, mt, me) > 0) {
      mt = pares[i].temporada; me = pares[i].episodio;
    }
  if (mt <= 0) return;
  ni = trava_da_serie(serie, ativo, idx, TRAVA_MAX);
  if (!ni) return;

  // Os OUTROS membros das travas JUNTOS desta serie (sem repetir), com o ponto
  // de antes para o Desfazer.
  memset(&d, 0, sizeof d);
  snprintf(d.imdb, sizeof d.imdb, "%s", serie);
  snprintf(d.serie, sizeof d.serie, "%s", serie);
  d.n = n > 16 ? 16 : n;
  memcpy(d.pares, pares, sizeof(VistoPar) * (size_t)d.n);
  for (i = 0; i < ni; i++) {
    const Trava *t = trava_i(idx[i]);
    if (!t || t->modo != TRAVA_JUNTOS || t->pausada) continue;
    ultima = t;
    for (k = 0; k < t->n; k++) {
      int p = t->m[k].perfil, j, dup = 0;
      if (p < 1 || p == ativo || t->m[k].estado != TRAVA_DENTRO) continue;
      for (j = 0; j < d.np; j++) if (d.perfis[j] == p) dup = 1;
      if (dup || d.np >= TRAVA_PERFIS_MAX) continue;
      d.perfis[d.np] = p;
      d.antT[d.np] = t->m[k].t;
      d.antE[d.np] = t->m[k].e;
      d.np++;
    }
  }
  trava_passo(serie, ativo, mt, me, 0);
  if (!d.np) return;

  for (i = 0; i < d.np; i++) {
    trava_passo(serie, d.perfis[i], mt, me, 0);
#if TRAVA_JUNTOS_NA_CONTA
    if (sessao_logada())
      contapend_episodios_perfil(d.perfis[i], imdb, "series", d.pares, d.n, 1);
#endif
#ifdef TRAVA_COM_DIARIO
    { DiarioEntrada e;
      int q, j;
      unsigned char bits = (unsigned char)(ativo >= 1 && ativo <= 8 ? 1u << (ativo - 1) : 0);
      for (j = 0; j < d.np; j++)
        if (d.perfis[j] >= 1 && d.perfis[j] <= 8) bits |= (unsigned char)(1u << (d.perfis[j] - 1));
      for (q = 0; q < d.n; q++) {
        memset(&e, 0, sizeof e);
        snprintf(e.id, sizeof e.id, "%s", serie);
        e.tipo = DIARIO_SERIE;
        e.origem = DIARIO_ORIG_MANUAL;
        e.temporada = d.pares[q].temporada;
        e.episodio = d.pares[q].episodio;
        e.quando = (long long)time(NULL);
        e.hora = -1;
        e.comPerfis = bits;
        e.flags = DIARIO_F_JUNTOS;
        diario_adicionar(d.perfis[i], &e);
      } }
#endif
  }
  if (sessao_logada()) contapend_chutar();
  traktOutros(imdb, d.pares, d.n, 1, d.perfis, d.np);

  { char nomes[160], frase[220];
    IlhaAcao a;
    if (d.np == 1) snprintf(nomes, sizeof nomes, "%s", nomePerfil(d.perfis[0]));
    else if (d.np == 2) snprintf(nomes, sizeof nomes, i18n("%s e %s"),
                                 nomePerfil(d.perfis[0]), nomePerfil(d.perfis[1]));
    else snprintf(nomes, sizeof nomes, i18n("%s e mais %d"), nomePerfil(d.perfis[0]), d.np - 1);
    snprintf(frase, sizeof frase, i18n("Marcado para %s"), nomes);
    memset(&a, 0, sizeof a);
    a.icone = "aj_users";
    a.frase = frase;
    a.titulo = ultima ? ultima->titulo : "";
    a.onde = i18n("Trava de série · Juntos");
    a.thumb = ultima && ultima->poster[0] ? ultima->poster : NULL;
    a.tipo = ILHA_ACENTO;
    a.desfazer = desfazerJuntos;
    a.ctx = &d;
    a.ctxN = sizeof d;
    ilhaacao_feita(&a);
    printf("[trava] juntos: T%dE%d de %s marcado para %d perfis\n", mt, me, serie, d.np);
  }
}

static void ouvinte(const char *imdb, const VistoPar *pares, int n, int visto) {
  travarede_visto(imdb, pares, n, visto);
}

void travarede_posicao_local(const char *serie) {
  VistoPar v[256];
  int i, n, mt = 0, me = 0;
  char s[24];
  serieDe(serie, s, sizeof s);
  if (!s[0] || !trava_da_serie(s, trava_perfil_ativo(), NULL, 0)) return;
  n = vistoep_ate_aqui(s, 999, 9999, v, 256);
  for (i = 0; i < n; i++)
    if (v[i].temporada > 0 && vistoep_estado(s, v[i].temporada, v[i].episodio) == 1 &&
        trava_ep_cmp(v[i].temporada, v[i].episodio, mt, me) > 0) {
      mt = v[i].temporada; me = v[i].episodio;
    }
  if (mt > 0) trava_passo(s, trava_perfil_ativo(), mt, me, 0);
}

// ---------------------------------------------------------------- ilha

// "Sandra alcancou voce em Severance. Proximo: T1E5". Fora do player, no
// maximo 1 por trava por dia (o plano, T4).
static struct { char id[TRAVA_ID_MAX]; long dia; } ditos[16];
static TravaAlcance pendente;
static int temPendente;

static int jaDisseHoje(const char *id) {
  long dia = (long)(time(NULL) / 86400);
  int i, livre = -1;
  for (i = 0; i < 16; i++) {
    if (!strcmp(ditos[i].id, id)) {
      if (ditos[i].dia == dia) return 1;
      ditos[i].dia = dia;
      return 0;
    }
    if (livre < 0 && (!ditos[i].id[0] || ditos[i].dia != dia)) livre = i;
  }
  if (livre < 0) livre = 0;
  snprintf(ditos[livre].id, sizeof ditos[livre].id, "%s", id);
  ditos[livre].dia = dia;
  return 0;
}

static void avisarAlcance(const TravaAlcance *ev) {
  const Trava *t = trava_por_id(ev->id);
  char frase[260], ep[32], forte[96], chave[64];
  IlhaAvisoEx a;
  int eu, lt = 0, le = 0;
  if (!t || ev->membro < 0 || ev->membro >= t->n || jaDisseHoje(t->id)) return;
  eu = trava_membro_do_perfil(t, trava_perfil_ativo());
  if (eu < 0) return;
  // O proximo que AGORA esta liberado para mim.
  if (!trava_limite(t, eu, NULL, NULL, &lt, &le)) { lt = t->m[eu].t; le = t->m[eu].e + 1; }
  trava_ep_texto(lt, le, ep, sizeof ep);
  snprintf(frase, sizeof frase, i18n("%s alcançou você em %s. Próximo: %s"),
           ilha_forte(forte, sizeof forte, nomePerfil(t->m[ev->membro].perfil)), t->titulo, ep);
  snprintf(chave, sizeof chave, "trava:alc:%s", t->id);
  memset(&a, 0, sizeof a);
  a.chave = chave;
  a.tipo = ILHA_ACENTO;
  a.icone = "aj_users";
  a.texto = frase;
  a.ms = 9000u;
  a.capa = t->poster[0] ? t->poster : NULL;
  ilha_avisar_ex(&a);
  printf("[trava] alcance: membro %d em %s (%s)\n", ev->membro, t->serie, ep);
}

// ---------------------------------------------------------------- passo

void travarede_iniciar(void) {
  if (!mtx) mtx = SDL_CreateMutex();
  visto_ao_marcar(ouvinte);
  acompanharPerfis();
  pedirJa = 1; reconciliarJa = 1;
}

void travarede_passo(Uint32 agora) {
  int feito = 0;
  agoraQ = agora;
  if ((int)(agora - proxPerfis) >= 0) { proxPerfis = agora + PERFIS_MS; acompanharPerfis(); }
  if (emVoo && mtx) {
    SDL_LockMutex(mtx); feito = pronto; SDL_UnlockMutex(mtx);
    if (feito) { Job *j = emVoo; emVoo = NULL; aplicar(j, agora); }
  }
  if (!emVoo) lancar(agora);
  // "Alcancou voce": nunca no player; espera a volta.
  if (!temPendente && trava_alcance_proximo(&pendente)) temPendente = 1;
  if (temPendente && !player_aberto()) { temPendente = 0; avisarAlcance(&pendente); }
}
