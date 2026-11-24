// Cliente assincrono da busca por frase do Farol — ver farolbusca.h.
// Sem SDL, sem _Thread_local (compativel com NV_TPK40), sem log de frase/token/URL.
#include "farolbusca.h"
#include "farolacha.h"
#include "faroltexto.h"
#include "idbase.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef FAROL_BUSCA_WORKER
#define FB_FALLBACK_URL FAROL_BUSCA_WORKER
#elif defined(NV_REC_URL)
#define FB_FALLBACK_URL NV_REC_URL
#else
#define FB_FALLBACK_URL ""
#endif

// Ganchos fracos dos modulos do app: na build completa as definicoes fortes
// de nvbackend.c, recomenda.c, perfilinfantil.c, ajustes.c, redesaude.c e
// trakt.c vencem; nos testes isolados os padroes seguros permitem linkar sem
// puxar a arvore inteira nem SDL.
__attribute__((weak)) size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  snprintf(dst, tam, "%s", FB_FALLBACK_URL);
  return strlen(dst);
}
__attribute__((weak)) int recomenda_cabecalhos(const char **cab, char *aut,
                                               size_t na, char *via, size_t nv,
                                               char *perfil, size_t np) {
  (void)cab; (void)aut; (void)na; (void)via; (void)nv; (void)perfil; (void)np;
  return 0;
}
__attribute__((weak)) int perfil_infantil_ativo(void) { return 0; }
__attribute__((weak)) const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
__attribute__((weak)) int rede_saude_offline(void) { return 0; }
__attribute__((weak)) void trakt_http_recusada(void) {}

typedef struct {
  uint64_t  gen;
  long long fim;
  unsigned  prazoMs;
  int       cancelar;
  int       usouTrakt;
  char      url[512];
  char      aut[3200];
  char      via[64];
  char      perfil[64];
  char     *corpo;
  size_t    nCorpo;
} BuscaJob;

typedef struct {
  int  tem;
  long tmdb;
  char imdb[64];
  char tipo[8];
  char poster[NV_ARTE_URL_MAX];
} PedidoAbrir;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condFim = PTHREAD_COND_INITIALIZER;
static uint64_t        geracaoAtual;
static int             ativos;
static BuscaJob       *jobAtual;

static FarolBuscaInfo  estadoAtual;
static FarolBuscaResp  respAtual;
static unsigned        sementeAtual;
static int             temSementeFixa;
static unsigned        sementeFixa;

static int             privadoCfg;
static int             consentiuCfg;
static int             temUrlOverride;
static char            urlOverride[384];
static int             temAuthOverride;
static char            authOverrideAut[512];
static char            authOverrideVia[64];

static PedidoAbrir     abrirPendente;

// ---------------------------------------------------------------- relogio --

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static unsigned hashFrase(const char *s) {
  uint32_t h = 2166136261u;
  if (!s) return h;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 16777619u;
  }
  return (unsigned)h;
}

static void limparJob(BuscaJob *j) {
  if (!j) return;
  if (j->corpo) {
    memset(j->corpo, 0, j->nCorpo);
    free(j->corpo);
  }
  memset(j->aut, 0, sizeof j->aut);
  memset(j->via, 0, sizeof j->via);
  memset(j->perfil, 0, sizeof j->perfil);
  free(j);
}

static int jobCancelado(BuscaJob *j) {
  int sim;
  pthread_mutex_lock(&trava);
  sim = j->cancelar || j->gen != geracaoAtual;
  pthread_mutex_unlock(&trava);
  return sim;
}

static int buscaParar(void *u) {
  BuscaJob *j = (BuscaJob *)u;
  return jobCancelado(j) || agoraMs() >= j->fim;
}

// ------------------------------------------------------------ fio de rede --

static void *executarBusca(void *u) {
  BuscaJob *j = (BuscaJob *)u;
  const char *cab[5];
  int nc = 0;
  int http = 0;
  int cancelado = 0;
  int prazoEstourou = 0;
  int transporteFalhou = 0;
  char *respCorpo = NULL;
  size_t respLen = 0;
  FarolBuscaResp parsed;
  int parseOk = 0;

  if (j->aut[0]) cab[nc++] = j->aut;
  if (j->via[0]) cab[nc++] = j->via;
  if (j->perfil[0]) cab[nc++] = j->perfil;
#ifndef __EMSCRIPTEN__
  cab[nc++] = "Content-Type: application/json";
#endif
  cab[nc] = NULL;

#ifdef __EMSCRIPTEN__
  {
    RedePararEstado prev = rede_parar_trocar(buscaParar, j);
    int seg = (int)((j->prazoMs + 999u) / 1000u);
    if (seg < 1) seg = 1;
    respCorpo = rede_postar_st(j->url, seg, cab, j->corpo, &http);
    rede_parar_trocar(prev.parar, prev.usuario);
    if (jobCancelado(j)) {
      cancelado = 1;
    } else if (agoraMs() >= j->fim && http == 0) {
      prazoEstourou = 1;
    } else if (!respCorpo || http == 0) {
      transporteFalhou = 1;
    } else {
      respLen = strlen(respCorpo);
    }
  }
#else
  {
    RedePedido req;
    RedeResposta res;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.metodo = "POST";
    req.url = j->url;
    req.cabecalhos = cab;
    req.corpo = j->corpo;
    req.n_corpo = j->nCorpo;
    req.prazo_ms = j->prazoMs;
    req.max_bytes = 65536;
    req.max_cabecalhos = 8192;
    req.seguir = 0;
    req.parar = buscaParar;
    req.parar_usuario = j;

    rede_pedir(&req, &res);
    http = res.status;
    if (jobCancelado(j)) {
      cancelado = 1;
    } else if (res.erro == REDE_PRAZO ||
               (res.erro == REDE_CANCELADO && agoraMs() >= j->fim)) {
      prazoEstourou = 1;
    } else if (res.erro != REDE_OK || http == 0) {
      transporteFalhou = 1;
    } else if (res.corpo && res.n_corpo > 0) {
      respCorpo = res.corpo;
      respLen = res.n_corpo;
      res.corpo = NULL;
    }
    rede_resposta_limpar(&res);
  }
#endif

  if (!cancelado && !prazoEstourou && !transporteFalhou && http == 200 && respCorpo) {
    parseOk = farolbusca_parsear_resposta(respCorpo, respLen, &parsed);
  }
  free(respCorpo);

  if (!cancelado && http == 401 && j->usouTrakt)
    trakt_http_recusada();

  pthread_mutex_lock(&trava);
  if (jobAtual == j) jobAtual = NULL;
  if (j->gen == geracaoAtual) {
    if (cancelado || j->cancelar) {
      estadoAtual.estado = FB_CANCELADO;
      estadoAtual.motivo = FB_MOTIVO_CANCELADO;
      estadoAtual.origem = FB_ORIG_NENHUMA;
      estadoAtual.http = 0;
      estadoAtual.llm_status = 0;
      estadoAtual.usouLocal = 0;
      estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_CANCELADO);
      memset(&estadoAtual.filtro, 0, sizeof estadoAtual.filtro);
      memset(&respAtual, 0, sizeof respAtual);
    } else if (parseOk && parsed.nIds > 0) {
      respAtual = parsed;
      estadoAtual.estado = FB_PRONTO;
      estadoAtual.motivo = FB_MOTIVO_OK;
      estadoAtual.origem = parsed.origem != FB_ORIG_NENHUMA ? parsed.origem : FB_ORIG_LLM;
      estadoAtual.http = http;
      estadoAtual.llm_status = parsed.llm_status;
      estadoAtual.usouLocal = 0;
      estadoAtual.filtro = parsed.filtro;
      estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_OK);
    } else {
      FarolBuscaMotivo mot;
      if (prazoEstourou) mot = FB_MOTIVO_PRAZO;
      else if (transporteFalhou) mot = FB_MOTIVO_OFFLINE;
      else if (http == 429) mot = FB_MOTIVO_LIMITE;
      else if (parseOk && parsed.nIds == 0) mot = FB_MOTIVO_VAZIO;
      else mot = FB_MOTIVO_ERRO;

      memset(&respAtual, 0, sizeof respAtual);
      memset(&estadoAtual.filtro, 0, sizeof estadoAtual.filtro);
      estadoAtual.estado = FB_PRONTO;
      estadoAtual.motivo = mot;
      estadoAtual.origem = FB_ORIG_LOCAL;
      estadoAtual.http = http;
      estadoAtual.llm_status = parseOk ? parsed.llm_status : 0;
      estadoAtual.usouLocal = 1;
      estadoAtual.fraseEstado = farolbusca_frase_estado(mot);
    }
  }
  ativos--;
  if (ativos == 0) pthread_cond_broadcast(&condFim);
  pthread_mutex_unlock(&trava);

  limparJob(j);
  return NULL;
}

// ------------------------------------------------------ inicio / controle --

static int obterBaseUrl(char *dst, size_t cap) {
  dst[0] = '\0';
  pthread_mutex_lock(&trava);
  if (temUrlOverride) {
    snprintf(dst, cap, "%s", urlOverride);
    pthread_mutex_unlock(&trava);
    return dst[0] != '\0';
  }
  pthread_mutex_unlock(&trava);

#ifdef FAROL_BUSCA_WORKER
  if (FAROL_BUSCA_WORKER[0]) {
    snprintf(dst, cap, "%s", FAROL_BUSCA_WORKER);
    return 1;
  }
#endif
  nv_rec_url_copiar(dst, cap);
  return dst[0] != '\0';
}

static int obterAuth(char *aut, size_t na, char *via, size_t nv,
                     char *perfil, size_t np, int *usouTrakt) {
  const char *cab[4];
  aut[0] = '\0'; via[0] = '\0'; perfil[0] = '\0';
  *usouTrakt = 0;

  pthread_mutex_lock(&trava);
  if (temAuthOverride) {
    snprintf(aut, na, "%s", authOverrideAut);
    snprintf(via, nv, "%s", authOverrideVia);
    *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
    pthread_mutex_unlock(&trava);
    return aut[0] != '\0';
  }
  pthread_mutex_unlock(&trava);

  if (!recomenda_cabecalhos(cab, aut, na, via, nv, perfil, np)) return 0;
  *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
  return 1;
}

int farolbusca_pedir_com(const char *frase, const FarolBuscaCtx *ctx) {
  char norm[FAROL_BUSCA_FRASE_BYTES];
  char corpoBuf[FAROL_BUSCA_CORPO_MAX + 1];
  char baseUrl[384];
  char aut[3200], via[64], perfil[64];
  size_t nCorpo;
  FarolBuscaLocal local;
  unsigned semente;
  int usouTrakt = 0, temRede;

  if (!farolbusca_normalizar_frase(frase, norm, sizeof norm)) return 0;
  nCorpo = farolbusca_montar_corpo(norm, ctx, corpoBuf, sizeof corpoBuf);
  if (nCorpo == 0) return 0;

  farolbusca_intencao_local(norm, &local);
  pthread_mutex_lock(&trava);
  semente = temSementeFixa ? sementeFixa : hashFrase(norm);
  pthread_mutex_unlock(&trava);

  temRede = !rede_saude_offline() &&
            obterBaseUrl(baseUrl, sizeof baseUrl) &&
            obterAuth(aut, sizeof aut, via, sizeof via, perfil, sizeof perfil, &usouTrakt);

  if (!temRede) {
    memset(aut, 0, sizeof aut);
    pthread_mutex_lock(&trava);
    if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
    geracaoAtual++;
    memset(&respAtual, 0, sizeof respAtual);
    memset(&estadoAtual, 0, sizeof estadoAtual);
    estadoAtual.estado = FB_PRONTO;
    estadoAtual.motivo = FB_MOTIVO_OFFLINE;
    estadoAtual.origem = FB_ORIG_LOCAL;
    estadoAtual.usouLocal = 1;
    estadoAtual.local = local;
    estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_OFFLINE);
    estadoAtual.geracao = geracaoAtual;
    sementeAtual = semente;
    pthread_mutex_unlock(&trava);
    return 1;
  }

  {
    BuscaJob *j = (BuscaJob *)calloc(1, sizeof *j);
    size_t nBase;
    pthread_attr_t attr;
    int attrOk, erro;
    pthread_t fio;

    if (!j) { memset(aut, 0, sizeof aut); return 0; }
    j->corpo = (char *)malloc(nCorpo + 1);
    if (!j->corpo) { memset(aut, 0, sizeof aut); limparJob(j); return 0; }
    memcpy(j->corpo, corpoBuf, nCorpo + 1);
    j->nCorpo = nCorpo;

    nBase = strlen(baseUrl);
    while (nBase > 0 && baseUrl[nBase - 1] == '/') baseUrl[--nBase] = '\0';
    snprintf(j->url, sizeof j->url, "%s/v1/farol/busca", baseUrl);
    snprintf(j->aut, sizeof j->aut, "%s", aut);
    snprintf(j->via, sizeof j->via, "%s", via);
    snprintf(j->perfil, sizeof j->perfil, "%s", perfil);
    j->usouTrakt = usouTrakt;
    j->prazoMs = FAROL_BUSCA_PRAZO_MS;
    j->fim = agoraMs() + (long long)j->prazoMs;
    memset(aut, 0, sizeof aut);

    attrOk = pthread_attr_init(&attr) == 0;
    if (!attrOk) { limparJob(j); return 0; }
    erro = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&trava);
    if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
    geracaoAtual++;
    j->gen = geracaoAtual;
    memset(&respAtual, 0, sizeof respAtual);
    memset(&estadoAtual, 0, sizeof estadoAtual);
    estadoAtual.local = local;
    estadoAtual.geracao = geracaoAtual;
    sementeAtual = semente;
    if (!erro) {
      ativos++;
      jobAtual = j;
      estadoAtual.estado = FB_BUSCANDO;
      estadoAtual.motivo = FB_MOTIVO_BUSCANDO;
      estadoAtual.origem = FB_ORIG_NENHUMA;
      estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_BUSCANDO);
      erro = pthread_create(&fio, &attr, executarBusca, j);
      if (erro) {
        if (jobAtual == j) jobAtual = NULL;
        ativos--;
        if (ativos == 0) pthread_cond_broadcast(&condFim);
      }
    }
    if (erro) {
      estadoAtual.estado = FB_PRONTO;
      estadoAtual.motivo = FB_MOTIVO_OFFLINE;
      estadoAtual.origem = FB_ORIG_LOCAL;
      estadoAtual.usouLocal = 1;
      estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_OFFLINE);
    }
    pthread_mutex_unlock(&trava);
    pthread_attr_destroy(&attr);
    if (erro) { limparJob(j); return 1; }
    return 1;
  }
}

int farolbusca_pedir(const char *frase) {
  FarolBuscaCtx ctx;
  memset(&ctx, 0, sizeof ctx);
  pthread_mutex_lock(&trava);
  ctx.privado = privadoCfg;
  ctx.consentiu = consentiuCfg;
  pthread_mutex_unlock(&trava);
  if (!ctx.privado) {
    ctx.infantil = perfil_infantil_ativo();
    ctx.idioma = ajustes_tmdb_idioma();
  }
  return farolbusca_pedir_com(frase, &ctx);
}

void farolbusca_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) {
    jobAtual->cancelar = 1;
  } else if (estadoAtual.estado == FB_BUSCANDO) {
    estadoAtual.estado = FB_CANCELADO;
    estadoAtual.motivo = FB_MOTIVO_CANCELADO;
    estadoAtual.fraseEstado = farolbusca_frase_estado(FB_MOTIVO_CANCELADO);
  }
  pthread_mutex_unlock(&trava);
}

void farolbusca_encerrar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) jobAtual->cancelar = 1;
  geracaoAtual++;
  while (ativos > 0) pthread_cond_wait(&condFim, &trava);
  jobAtual = NULL;
  memset(&estadoAtual, 0, sizeof estadoAtual);
  memset(&respAtual, 0, sizeof respAtual);
  memset(&abrirPendente, 0, sizeof abrirPendente);
  pthread_mutex_unlock(&trava);
}

FarolBuscaEstado farolbusca_estado(FarolBuscaInfo *info) {
  FarolBuscaEstado st;
  pthread_mutex_lock(&trava);
  if (info) *info = estadoAtual;
  st = estadoAtual.estado;
  pthread_mutex_unlock(&trava);
  return st;
}

// ----------------------------------------- resolucao viva no catalogo ----

static int ehSerieTipo(const char *t) {
  return t && (!strcmp(t, "series") || !strcmp(t, "show") ||
               !strcmp(t, "serie") || !strcmp(t, "tv"));
}

static int ehCanalItem(const CatItem *c) {
  if (!c) return 1;
  if (!strcmp(c->tipo, "channel") || !strcmp(c->tipo, "channels") ||
      !strcmp(c->tipo, "tv") || !strcmp(c->tipo, "events"))
    return 1;
  if (!strncmp(c->imdb, "cs:channel:", 11)) return 1;
  return 0;
}

// Procura o item no catalogo atual (sem guardar indice entre quadros).
// Prefere copia que bate tipo e tem poster.
static int procurarNoCatalogo(const char *imdb, long tmdb, const char *tipo,
                              int nCat) {
  int i, melhor = -1, melhorScore = -1;
  int querSerie = ehSerieTipo(tipo);
  int temTipo = (tipo && tipo[0]);
  for (i = 0; i < nCat; i++) {
    const CatItem *c = cat_item(i);
    int casa = 0, score = 0;
    if (!c || ehCanalItem(c)) continue;
    if (imdb && imdb[0] && c->imdb[0] && idbase_mesmo(c->imdb, imdb)) {
      if (!temTipo || !c->tipo[0] || ehSerieTipo(c->tipo) == querSerie) {
        casa = 1;
        score = 4;
      }
    }
    if (!casa && tmdb > 0 && c->tmdb == tmdb) {
      if (!temTipo || !c->tipo[0] || ehSerieTipo(c->tipo) == querSerie) {
        casa = 1;
        score = 2;
      }
    }
    if (!casa) continue;
    if (c->poster[0]) score += 1;
    if (score > melhorScore) {
      melhor = i;
      melhorScore = score;
    }
  }
  return melhor;
}

static void preencherPosterId(char *poster, size_t cap, const char *imdb) {
  char base[24];
  if (!poster || !cap) return;
  poster[0] = '\0';
  if (!imdb || imdb[0] != 't' || imdb[1] != 't' || !imdb[2]) return;
  idbase_copiar(imdb, base, sizeof base);
  if (base[0] != 't' || base[1] != 't' || !base[2]) return;
  snprintf(poster, cap, "https://images.metahub.space/poster/medium/%s/img", base);
}

static void preencherArteCatPorId(CatItem *d) {
  char base[24];
  if (!d || d->imdb[0] != 't' || d->imdb[1] != 't' || !d->imdb[2]) return;
  idbase_copiar(d->imdb, base, sizeof base);
  if (base[0] != 't' || base[1] != 't' || !base[2]) return;
  if (!d->poster[0])
    snprintf(d->poster, sizeof d->poster,
             "https://images.metahub.space/poster/medium/%s/img", base);
  if (!d->backdrop[0])
    snprintf(d->backdrop, sizeof d->backdrop,
             "https://images.metahub.space/background/medium/%s/img", base);
  if (!d->logo[0])
    snprintf(d->logo, sizeof d->logo,
             "https://images.metahub.space/logo/medium/%s/img", base);
}

static void preencherItemDeCat(FarolBuscaItem *dst, int idx, const CatItem *c,
                               const FarolBuscaId *rem, int minutos, int mot) {
  memset(dst, 0, sizeof *dst);
  dst->noCatalogo = 1;
  dst->indice = idx;
  dst->minutos = minutos;
  dst->motivoAcha = mot;
  dst->tmdb = c->tmdb > 0 ? c->tmdb : (rem ? rem->tmdb : 0);
  if (c->imdb[0]) idbase_copiar(c->imdb, dst->imdb, sizeof dst->imdb);
  else if (rem && rem->imdb[0]) snprintf(dst->imdb, sizeof dst->imdb, "%s", rem->imdb);
  else if (dst->tmdb > 0) snprintf(dst->imdb, sizeof dst->imdb, "tmdb:%ld", dst->tmdb);
  if (c->tipo[0]) snprintf(dst->tipo, sizeof dst->tipo, "%s", ehSerieTipo(c->tipo) ? "series" : "movie");
  else if (rem && rem->tipo[0]) snprintf(dst->tipo, sizeof dst->tipo, "%s", rem->tipo);
  else strcpy(dst->tipo, "movie");
  if (c->poster[0]) snprintf(dst->poster, sizeof dst->poster, "%s", c->poster);
  else preencherPosterId(dst->poster, sizeof dst->poster, dst->imdb);
}

static void preencherItemForaCat(FarolBuscaItem *dst, const FarolBuscaId *rem) {
  memset(dst, 0, sizeof *dst);
  dst->noCatalogo = 0;
  dst->indice = -1;
  dst->tmdb = rem->tmdb;
  if (rem->imdb[0]) snprintf(dst->imdb, sizeof dst->imdb, "%s", rem->imdb);
  else if (rem->tmdb > 0) snprintf(dst->imdb, sizeof dst->imdb, "tmdb:%ld", rem->tmdb);
  snprintf(dst->tipo, sizeof dst->tipo, "%s",
           ehSerieTipo(rem->tipo) ? "series" : "movie");
  preencherPosterId(dst->poster, sizeof dst->poster, dst->imdb);
}

int farolbusca_resolver(FarolBuscaItem *out, int max, unsigned *revisaoOut) {
  FarolBuscaInfo info;
  FarolBuscaResp resp;
  unsigned semente;
  int tentativa;

  if (revisaoOut) *revisaoOut = cat_revisao();
  if (!out || max < 1) return 0;

  pthread_mutex_lock(&trava);
  info = estadoAtual;
  resp = respAtual;
  semente = sementeAtual;
  pthread_mutex_unlock(&trava);

  if (info.estado != FB_PRONTO) return 0;

  if (!info.usouLocal) {
    for (tentativa = 0; tentativa < 3; tentativa++) {
      unsigned rev = cat_revisao();
      int nCat = cat_n();
      int i, k = 0;
      for (i = 0; i < resp.nIds && k < max; i++) {
        int idx = procurarNoCatalogo(resp.ids[i].imdb, resp.ids[i].tmdb,
                                     resp.ids[i].tipo, nCat);
        const CatItem *c = idx >= 0 ? cat_item(idx) : NULL;
        if (c) preencherItemDeCat(&out[k++], idx, c, &resp.ids[i], 0, 0);
        else preencherItemForaCat(&out[k++], &resp.ids[i]);
      }
      if (cat_revisao() == rev) {
        if (revisaoOut) *revisaoOut = rev;
        return k;
      }
      if (k > 0) memset(out, 0, sizeof(*out) * (size_t)k);
    }
    if (revisaoOut) *revisaoOut = cat_revisao();
    return 0;
  }

  // Fallback local: delega ao farolacha com a intencao mais proxima, sem
  // guardar indices entre quadros (toda chamada revalida cat_revisao()).
  for (tentativa = 0; tentativa < 3; tentativa++) {
    FarolAchado ach[FAROL_BUSCA_IDS_MAX];
    int lim = max < FAROL_BUSCA_IDS_MAX ? max : FAROL_BUSCA_IDS_MAX;
    unsigned rev = 0;
    int nAch, i, k = 0;
    const char *gen = info.local.genero[0] ? info.local.genero : NULL;

    nAch = farolacha_listar_genero(info.local.intencao, info.local.maxMin,
                                   gen, ach, lim, semente, &rev);
    if (nAch == 0 && gen) {
      nAch = farolacha_listar_rev(info.local.intencao, info.local.maxMin,
                                  ach, lim, semente, &rev);
    }
    if (nAch == 0 && info.local.intencao != FA_SURPRESA) {
      nAch = farolacha_listar_rev(FA_SURPRESA, 0, ach, lim, semente, &rev);
    }
    for (i = 0; i < nAch && k < max; i++) {
      const CatItem *c = farolacha_item(&ach[i], rev);
      if (!c) { k = -1; break; }
      preencherItemDeCat(&out[k++], ach[i].indice, c, NULL,
                         ach[i].minutos, ach[i].motivo);
    }
    if (k >= 0 && cat_revisao() == rev) {
      if (revisaoOut) *revisaoOut = rev;
      return k;
    }
    if (k > 0) memset(out, 0, sizeof(*out) * (size_t)k);
  }
  if (revisaoOut) *revisaoOut = cat_revisao();
  return 0;
}

const CatItem *farolbusca_item(const FarolBuscaItem *it, unsigned revisao) {
  int n;
  const CatItem *c;
  if (!it || !it->noCatalogo || it->indice < 0) return NULL;
  if (cat_revisao() != revisao) return NULL;
  n = cat_n();
  if (it->indice >= n) return NULL;
  c = cat_item(it->indice);
  if (!c || cat_revisao() != revisao) return NULL;
  return c;
}

int farolbusca_abrir_item(const FarolBuscaItem *it) {
  int idx = -1, tentativa;
  CatItem novo;
  if (!it || (!it->imdb[0] && it->tmdb <= 0)) return -1;

  // Sempre re-resolve pela identidade contra o catalogo vivo sob cat_revisao()
  // (nunca confia num indice de quadro anterior).
  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned rev = cat_revisao();
    int nCat = cat_n();
    idx = procurarNoCatalogo(it->imdb, it->tmdb, it->tipo, nCat);
    if (cat_revisao() == rev) break;
    idx = -1;
  }
  if (idx >= 0) return idx;

  // Fora do catalogo: cartaz minimo pelo id (contrato de busca_pediu_abrir).
  memset(&novo, 0, sizeof novo);
  if (it->imdb[0]) snprintf(novo.imdb, sizeof novo.imdb, "%s", it->imdb);
  else snprintf(novo.imdb, sizeof novo.imdb, "tmdb:%ld", it->tmdb);
  novo.tmdb = it->tmdb;
  snprintf(novo.tipo, sizeof novo.tipo, "%s",
           ehSerieTipo(it->tipo) ? "series" : "movie");
  if (it->poster[0]) snprintf(novo.poster, sizeof novo.poster, "%s", it->poster);
  preencherArteCatPorId(&novo);
  return cat_acrescentar(&novo);
}

int farolbusca_selecionar(const FarolBuscaItem *it) {
  if (!it || (!it->imdb[0] && it->tmdb <= 0)) return 0;
  pthread_mutex_lock(&trava);
  memset(&abrirPendente, 0, sizeof abrirPendente);
  abrirPendente.tem = 1;
  abrirPendente.tmdb = it->tmdb;
  snprintf(abrirPendente.imdb, sizeof abrirPendente.imdb, "%s", it->imdb);
  snprintf(abrirPendente.tipo, sizeof abrirPendente.tipo, "%s", it->tipo);
  snprintf(abrirPendente.poster, sizeof abrirPendente.poster, "%s", it->poster);
  pthread_mutex_unlock(&trava);
  return 1;
}

int farolbusca_pediu_abrir(int *indiceCatalogo) {
  PedidoAbrir p;
  FarolBuscaItem it;
  int idx;

  pthread_mutex_lock(&trava);
  if (!abrirPendente.tem) {
    pthread_mutex_unlock(&trava);
    return 0;
  }
  p = abrirPendente;
  memset(&abrirPendente, 0, sizeof abrirPendente);
  pthread_mutex_unlock(&trava);

  memset(&it, 0, sizeof it);
  it.tmdb = p.tmdb;
  snprintf(it.imdb, sizeof it.imdb, "%s", p.imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", p.tipo);
  snprintf(it.poster, sizeof it.poster, "%s", p.poster);
  idx = farolbusca_abrir_item(&it);
  if (idx < 0) return 0;
  if (indiceCatalogo) *indiceCatalogo = idx;
  return 1;
}

// -------------------------------------------------- configuracao / testes --

void farolbusca_definir_privado(int privado) {
  pthread_mutex_lock(&trava);
  privadoCfg = privado ? 1 : 0;
  pthread_mutex_unlock(&trava);
}

int farolbusca_privado(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = privadoCfg;
  pthread_mutex_unlock(&trava);
  return v;
}

void farolbusca_definir_consentimento(int consentiu) {
  pthread_mutex_lock(&trava);
  consentiuCfg = consentiu ? 1 : 0;
  pthread_mutex_unlock(&trava);
}

int farolbusca_consentimento(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = consentiuCfg;
  pthread_mutex_unlock(&trava);
  return v;
}

void farolbusca_definir_url(const char *baseUrl) {
  pthread_mutex_lock(&trava);
  if (baseUrl) {
    temUrlOverride = 1;
    snprintf(urlOverride, sizeof urlOverride, "%s", baseUrl);
  } else {
    temUrlOverride = 0;
    urlOverride[0] = '\0';
  }
  pthread_mutex_unlock(&trava);
}

void farolbusca_definir_auth(const char *autorizacao, const char *via) {
  pthread_mutex_lock(&trava);
  if (autorizacao && autorizacao[0]) {
    temAuthOverride = 1;
    snprintf(authOverrideAut, sizeof authOverrideAut, "%s", autorizacao);
    snprintf(authOverrideVia, sizeof authOverrideVia, "%s",
             (via && via[0]) ? via : "X-Nuvio-Auth: nuvio");
  } else {
    temAuthOverride = 0;
    memset(authOverrideAut, 0, sizeof authOverrideAut);
    memset(authOverrideVia, 0, sizeof authOverrideVia);
  }
  pthread_mutex_unlock(&trava);
}

void farolbusca_definir_semente(unsigned semente) {
  pthread_mutex_lock(&trava);
  temSementeFixa = 1;
  sementeFixa = semente;
  pthread_mutex_unlock(&trava);
}

int farolbusca_em_voo(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = ativos;
  pthread_mutex_unlock(&trava);
  return n;
}
