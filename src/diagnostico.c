// Diagnostico sob demanda dos addons e do caminho de artes.
//
// Este modulo mede o que esta sob controle do Nuvio e deixa claro quando um
// defeito esta no servidor externo. Nenhuma URL completa, token ou titulo
// pessoal entra no relatorio enviado.
#include "diagnostico.h"
#include "addons.h"
#include "catalogo.h"
#include "avisos.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "ajustes.h"
#include "layout.h"
#include "rede.h"
#include "tex_cache.h"
#include "text.h"
#include "js.h"
#include <stdatomic.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#ifndef NV_DIAG_AUTO_OPT
/* A aplicação automática só entra em builds depois da matriz física de cada
 * plataforma. O diagnóstico e o relatório seguem disponíveis sem inventar
 * um ganho que ainda não foi comparado no aparelho. */
#define NV_DIAG_AUTO_OPT 0
#endif

#define DIAG_MAX_ADDONS 16
#define DIAG_MAX_ASSETS 12
#define DIAG_TIMEOUT_S  6
#define DIAG_SESSAO_MS (8u * 60u * 1000u)
#define DIAG_MANIFEST_MAX (512L * 1024L)
#define DIAG_STREAM_MAX   (1024L * 1024L)
#define DIAG_ASSET_MAX    (12L * 1024L * 1024L)

typedef enum {
  DR_UNUSED = 0, DR_OK, DR_DESATIVADO, DR_OFFLINE, DR_AUTH,
  DR_NOT_FOUND, DR_RATE_LIMIT, DR_INVALIDO, DR_VAZIO, DR_SERVIDOR,
  DR_INCOMPATIVEL
} DiagResultado;

typedef struct {
  char nome[64];
  char host[160];
  char resultado[32];
  int ativo;
  int http;
  int bytes;
  int manifest_ms;
  int catalog_http;
  int catalog_ms;
  int catalog_ok;
  int stream_ms;
  int asset_ms;
  int asset_bytes;
  int catalogo;
  int stream;
  int legenda;
} DiagAddon;

typedef struct {
  _Atomic int estado; // 0 parado, 1 rodando, 2 pronto, 3 cancelado, 4 falhou
  _Atomic int fase;
  _Atomic int total;
  _Atomic int feitos;
  _Atomic int cancelado;
  SDL_Thread *fio;
  DiagnosticoModo modo;
  char id[40];
  DiagAddon addon[DIAG_MAX_ADDONS];
  int nAddon;
  int nAssets;
  int imagensOk;
  int imagensFalhas;
  int assetsMs;
  int assetsBytes;
  int manifestMs;
  int catalogMs;
  int streamMs;
  int manifestOk;
  int manifestFalhas;
  int catalogOk;
  int catalogFalhas;
  int streamOk;
  int streamFalhas;
  int aplicado;
  int orcamentoAntes;
  int orcamentoAntesFixo;
  int restaurado;
  int enviado;
  int envioFalhou;
  char registroId[96];
  int intro;
  Uint32 inicioMs;
  int coberturaParcial;
  char erro[128];
  char relatorio[18000];
} Diagnostico;

static Diagnostico d;
static int focoModo;
static int sairTela;
static int introGlobal;
static int introDecidido;

static const char *gargaloPrincipal(void);

static int apresentacaoVista(void) {
  char *marca = dados_ler("diagnostico-otimizacao-intro.cfg");
  int vista = marca != NULL;
  free(marca);
  return vista;
}

static int sessaoExpirada(void) {
  return d.inicioMs && SDL_GetTicks() - d.inicioMs >= DIAG_SESSAO_MS;
}

static RedeControle controleDiagnostico(long maxBytes) {
  RedeControle c;
  c.max_bytes = maxBytes;
  c.cancelado = (volatile int *)&d.cancelado;
  return c;
}

static void marcarApresentacaoVista(void) {
  dados_fs_travar();
  dados_gravar("diagnostico-otimizacao-intro.cfg", "versao=1\n");
  dados_fs_liberar();
}

static void campoSeguro(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  if (!dst || !cap) return;
  if (!src) src = "";
  while (*src && n + 1 < cap) {
    unsigned char c = (unsigned char)*src++;
    dst[n++] = (c < 0x20 || c == '|' || c == '=' || c == '\n' || c == '\r') ? '_' : (char)c;
  }
  dst[n] = 0;
}

static const char *resultadoNome(DiagResultado r) {
  switch (r) {
    case DR_OK: return "ok";
    case DR_DESATIVADO: return "desativado";
    case DR_OFFLINE: return "sem_resposta";
    case DR_AUTH: return "autenticacao";
    case DR_NOT_FOUND: return "nao_encontrado";
    case DR_RATE_LIMIT: return "limite";
    case DR_INVALIDO: return "invalido";
    case DR_VAZIO: return "vazio";
    case DR_SERVIDOR: return "erro_servidor";
    case DR_INCOMPATIVEL: return "incompativel";
    default: return "desconhecido";
  }
}

static int jsonValido(const char *corpo) {
  const char *fim;
  if (!corpo || (corpo[0] != '{' && corpo[0] != '[')) return 0;
  fim = js_fim(corpo);
  if (!fim || fim == corpo) return 0;
  while (*fim && (unsigned char)*fim <= ' ') fim++;
  return *fim == 0;
}

static DiagResultado classificar(int status, const char *corpo) {
  if (status == 401 || status == 403) return DR_AUTH;
  if (status == 404) return DR_NOT_FOUND;
  if (status == 429) return DR_RATE_LIMIT;
  if (status >= 500) return DR_SERVIDOR;
  if (!corpo) return DR_OFFLINE;
  if (!corpo[0]) return DR_VAZIO;
  if (!jsonValido(corpo)) return DR_INVALIDO;
  return DR_OK;
}

static int tem(const char *s, const char *key) {
  return s && strstr(s, key) != NULL;
}

static void urlJoin(char *dst, size_t cap, const char *base, const char *path) {
  size_t n;
  if (!dst || !cap) return;
  snprintf(dst, cap, "%s", base ? base : "");
  n = strlen(dst);
  while (n && dst[n - 1] == '/') dst[--n] = 0;
  snprintf(dst + n, cap - n, "/%s", path ? path : "");
}

static const char *amostraTitulo(int ordem) {
  int i, vistos = 0;
  for (i = 0; i < cat_n(); i++) {
    const CatItem *c = cat_item(i);
    if (!c || strncmp(c->imdb, "tt", 2)) continue;
    if (vistos++ == ordem % 3) return c->imdb;
  }
  return "tt0111161";
}

static int extrairUrl(const char *json, const char *campo, char *dst, size_t cap) {
  const char *p, *q;
  size_t n;
  if (!json || !campo || !dst || cap < 2) return 0;
  dst[0] = 0;
  p = strstr(json, campo);
  if (!p) return 0;
  p = strchr(p, ':');
  if (!p) return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  if (*p != '"') return 0;
  p++;
  q = p;
  while (*q && *q != '"') {
    if (*q == '\\' && q[1]) q += 2; else q++;
  }
  n = (size_t)(q - p);
  if (!n || n >= cap) return 0;
  memcpy(dst, p, n);
  dst[n] = 0;
  return !strncmp(dst, "http://", 7) || !strncmp(dst, "https://", 8);
}

static void medirAsset(const char *url) {
  Uint32 inicio;
  long n = 0;
  char *corpo;
  RedeMedida medida;
  RedeControle controle = controleDiagnostico(DIAG_ASSET_MAX);
  if (!url || !*url || d.nAssets >= DIAG_MAX_ASSETS || atomic_load(&d.cancelado)) return;
  if (sessaoExpirada()) { d.coberturaParcial = 1; return; }
  inicio = SDL_GetTicks();
  corpo = rede_baixar_bin_medido_controle(url, DIAG_TIMEOUT_S, NULL, &controle, &n, &medida);
  d.assetsMs += (int)(SDL_GetTicks() - inicio);
  if (corpo && n > 0) {
    d.imagensOk++;
    d.assetsBytes += (int)(n > 2147483647L ? 2147483647L : n);
  } else if (!medida.cancelado) d.imagensFalhas++;
  d.nAssets++;
  if (medida.cancelado) atomic_store(&d.cancelado, 1);
  free(corpo);
}

static int aplicarPerfil(void) {
  // O cache ja possui limite dinamico e aplica o teto permitido pela RAM da
  // TV. O otimizador escolhe apenas dentro desse contrato.
  int mb = d.modo == DIAG_DESEMPENHO ? 160 : 300;
#if !NV_DIAG_AUTO_OPT
  (void)mb;
  printf("[diagnostico] perfil mantido: comparação de candidatos ainda não habilitada neste build\n");
  return 0;
#else
  tex_orcamento_info(&d.orcamentoAntes, NULL, &d.orcamentoAntesFixo, NULL);
  { char checkpoint[256];
    int ok;
    snprintf(checkpoint, sizeof checkpoint,
             "versao=1\nestado=experiment_pending\nantes_mb=%d\nantes_fixo=%d\n",
             d.orcamentoAntes, d.orcamentoAntesFixo);
    dados_fs_travar();
    ok = dados_gravar("diagnostico-otimizacao.checkpoint", checkpoint);
    dados_fs_liberar();
    if (!ok) {
      snprintf(d.erro, sizeof d.erro, "%s", i18n("Não foi possível salvar o checkpoint"));
      return 0;
    }
  }
  tex_definir_orcamento_mb(mb);
  d.aplicado = 1;
  { char cfg[256];
    snprintf(cfg, sizeof cfg,
             "versao=1\nmodo=%s\nantes_mb=%d\naddons=%d\nassets=%d\n",
             d.modo == DIAG_DESEMPENHO ? "desempenho" : "qualidade",
             d.orcamentoAntes, d.nAddon, d.nAssets);
    dados_fs_travar();
    dados_gravar("diagnostico-otimizacao.cfg", cfg);
    dados_apagar("diagnostico-otimizacao.checkpoint"); }
  dados_fs_liberar();
  printf("[diagnostico] perfil aplicado: %s, texturas=%dMB\n",
         d.modo == DIAG_DESEMPENHO ? "desempenho" : "qualidade", mb);
  return 1;
#endif
}

static int diagnosticoWorker(void *arg) {
  int i;
  int catalogTestados = 0, streamTestados = 0;
  int texItens = 0, texPend = 0, texQuentes = 0, texSlots = 0;
  int texMb = 0, fios = 0, fiosMax = 0;
  long texBytes = 0, texLimite = 0, memTotal = 0;
  RedeControle controle;
  (void)arg;
  d.nAddon = addons_n();
  if (d.nAddon > DIAG_MAX_ADDONS) d.nAddon = DIAG_MAX_ADDONS;
  atomic_store(&d.total, d.nAddon);
  atomic_store(&d.fase, 1);
  controle = controleDiagnostico(DIAG_MANIFEST_MAX);
  for (i = 0; i < d.nAddon; i++) {
    DiagAddon *a = &d.addon[i];
    const char *base = addons_base(i);
    char url[760], safe[200], *corpo;
    int status = 0;
    RedeMedida medida;
    memset(a, 0, sizeof *a);
    campoSeguro(a->nome, sizeof a->nome, addons_nome(i));
    rede_url_publica(base, safe, sizeof safe);
    campoSeguro(a->host, sizeof a->host, safe);
    a->ativo = addons_ativo(i);
    if (sessaoExpirada()) { d.coberturaParcial = 1; break; }
    if (!a->ativo) {
      snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(DR_DESATIVADO));
      atomic_fetch_add(&d.feitos, 1);
      continue;
    }
    urlJoin(url, sizeof url, base, "manifest.json");
    corpo = rede_baixar_medido_controle(url, DIAG_TIMEOUT_S, NULL, &controle, &medida);
    a->manifest_ms = (int)medida.ms;
    d.manifestMs += a->manifest_ms;
    a->http = medida.status;
    status = medida.status;
    if (corpo) a->bytes = (int)medida.bytes;
    { DiagResultado r = classificar(status, corpo);
      if (r == DR_OK) d.manifestOk++;
      else if (r != DR_DESATIVADO) d.manifestFalhas++;
      snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(r));
      if (corpo && jsonValido(corpo) && corpo[0] == '{') {
        addons_manifesto_lido(i, corpo);
        a->catalogo = tem(corpo, "catalog");
        a->stream = tem(corpo, "stream");
        a->legenda = tem(corpo, "subtitle") || tem(corpo, "subtitles");
        a->catalogo = addons_fornece(i, ADD_CATALOGO);
        a->stream = addons_fornece(i, ADD_STREAM);
        a->legenda = addons_fornece(i, ADD_LEGENDA);
        if (r == DR_OK && !a->catalogo && !a->stream && !a->legenda)
          snprintf(a->resultado, sizeof a->resultado, "%s", resultadoNome(DR_INCOMPATIVEL));
      }
    }
    if (medida.cancelado) {
      atomic_store(&d.cancelado, 1);
      free(corpo);
      break;
    }
    if (corpo && jsonValido(corpo) && corpo[0] == '{') {
      if (a->catalogo && catalogTestados < 6 && !sessaoExpirada()) {
        char catalogUrl[800];
        char *catalogo;
        snprintf(catalogUrl, sizeof catalogUrl, "%s/catalog/movie/%s.json", base,
                 amostraTitulo(catalogTestados));
        controle = controleDiagnostico(2L * 1024L * 1024L);
        catalogo = rede_baixar_medido_controle(catalogUrl, DIAG_TIMEOUT_S, NULL, &controle, &medida);
        a->catalog_http = medida.status;
        a->catalog_ms = (int)medida.ms;
        a->catalog_ok = catalogo && !medida.limitado && jsonValido(catalogo);
        d.catalogMs += a->catalog_ms;
        if (a->catalog_ok) d.catalogOk++; else d.catalogFalhas++;
        catalogTestados++;
        if (medida.cancelado) atomic_store(&d.cancelado, 1);
        free(catalogo);
      }
      if (a->stream && streamTestados < 6 && !sessaoExpirada()) {
        char streamUrl[800];
        char *streams;
        snprintf(streamUrl, sizeof streamUrl, "%s/stream/movie/%s.json", base,
                 amostraTitulo(streamTestados));
        controle = controleDiagnostico(DIAG_STREAM_MAX);
        streams = rede_baixar_medido_controle(streamUrl, DIAG_TIMEOUT_S, NULL, &controle, &medida);
        status = medida.status;
        a->stream_ms = (int)medida.ms;
        d.streamMs += a->stream_ms;
        if (streams && status >= 200 && status < 300 && jsonValido(streams) && !medida.limitado) d.streamOk++;
        else d.streamFalhas++;
        streamTestados++;
        if (medida.cancelado) atomic_store(&d.cancelado, 1);
        free(streams);
      }
      if (d.nAssets < DIAG_MAX_ASSETS) {
        char asset[800];
        if (extrairUrl(corpo, "logo", asset, sizeof asset)) medirAsset(asset);
        if (extrairUrl(corpo, "poster", asset, sizeof asset)) medirAsset(asset);
        if (extrairUrl(corpo, "background", asset, sizeof asset)) medirAsset(asset);
      }
    }
    free(corpo);
    atomic_fetch_add(&d.feitos, 1);
    if (atomic_load(&d.cancelado)) break;
    if (sessaoExpirada()) { d.coberturaParcial = 1; break; }
  }
  if (!atomic_load(&d.cancelado)) {
    atomic_store(&d.fase, 2);
    aplicarPerfil();
    atomic_store(&d.fase, 3);
    tex_estatisticas(&texItens, &texPend, &texBytes, &texQuentes, NULL);
    tex_orcamento_info(&texMb, &memTotal, NULL, &texSlots);
    texLimite = tex_orcamento_bytes();
    tex_threads_info(&fios, &fiosMax);
    { char *p = d.relatorio;
      size_t left = sizeof d.relatorio;
      int wrote;
      d.relatorio[0] = 0;
      wrote = snprintf(p, left,
        "diagnostico=v1\nid=%s\nversao=%s\nmodo=%s\naddons=%d\nmanifest_ok=%d\nmanifest_falhas=%d\nmanifest_ms=%d\ncatalog_ok=%d\ncatalog_falhas=%d\ncatalog_ms=%d\nassets=%d\nassets_ok=%d\nassets_falhas=%d\nassets_ms=%d\nassets_bytes=%d\nstreams_ok=%d\nstreams_falhas=%d\nstreams_ms=%d\ntex_itens=%d\ntex_pendentes=%d\ntex_quentes=%d\ntex_bytes=%ld\ntex_limite=%ld\ntex_orcamento_mb=%d\nmem_total_mb=%ld\nthreads_usadas=%d\nthreads_disponiveis=%d\ngargalo=%s\naplicacao=%s\ncobertura=%s\n",
        d.id, NV_VERSAO, d.modo == DIAG_DESEMPENHO ? "desempenho" : "qualidade",
        d.nAddon, d.manifestOk, d.manifestFalhas, d.manifestMs,
        d.catalogOk, d.catalogFalhas, d.catalogMs, d.nAssets, d.imagensOk,
        d.imagensFalhas, d.assetsMs, d.assetsBytes, d.streamOk, d.streamFalhas,
        d.streamMs, texItens, texPend, texQuentes, texBytes, texLimite, texMb,
        memTotal, fios, fiosMax, gargaloPrincipal(),
        d.aplicado ? "aplicada" : "padrao_mantido",
        d.coberturaParcial ? "parcial" : "completa");
      if (wrote > 0 && (size_t)wrote < left) { p += wrote; left -= (size_t)wrote; }
      for (i = 0; i < d.nAddon && left > 40; i++) {
        DiagAddon *a = &d.addon[i];
        wrote = snprintf(p, left, "addon=%s|host=%s|active=%d|result=%s|http=%d|bytes=%d|manifest_ms=%d|catalog_http=%d|catalog_ms=%d|catalog_ok=%d|stream_ms=%d|asset_ms=%d|catalog=%d|stream=%d|subtitle=%d\n",
                         a->nome, a->host, a->ativo, a->resultado, a->http,
                         a->bytes, a->manifest_ms, a->catalog_http, a->catalog_ms,
                         a->catalog_ok, a->stream_ms, a->asset_ms,
                         a->catalogo, a->stream, a->legenda);
        if (wrote <= 0 || (size_t)wrote >= left) break;
        p += wrote; left -= (size_t)wrote;
      }
    }
    dados_fs_travar();
    dados_gravar("diagnostico-otimizacao.txt", d.relatorio);
    dados_fs_liberar();
    printf("[diagnostico] relatorio inicio id=%s addons=%d assets=%d streams_ok=%d streams_falhas=%d\n",
           d.id, d.nAddon, d.nAssets, d.streamOk, d.streamFalhas);
    printf("[diagnostico] relatorio fim\n");
    fflush(stdout);
    // O relatorio segue por um envio proprio, com a execucao correlacionada.
    // O log geral da sessao nao entra neste caminho.
    d.enviado = avisos_enviar_diagnostico(d.id, d.relatorio,
                                          d.registroId, sizeof d.registroId);
    d.envioFalhou = !d.enviado;
  }
  if (atomic_load(&d.cancelado)) atomic_store(&d.estado, 3);
  else atomic_store(&d.estado, 2);
  return 0;
}

void diagnostico_iniciar(void) {
  memset(&d, 0, sizeof d);
  atomic_store(&d.estado, 0);
  atomic_store(&d.fase, 0);
  focoModo = 0;
  sairTela = 0;
  d.intro = !apresentacaoVista();
  dados_uuid(d.id, sizeof d.id);
}

void diagnostico_recuperar_checkpoint(void) {
  char *cfg = dados_ler("diagnostico-otimizacao.checkpoint");
  int mb = 0, fixo = 0;
  if (!cfg) return;
  if (strstr(cfg, "estado=experiment_pending")) {
    const char *p = strstr(cfg, "antes_mb=");
    const char *f = strstr(cfg, "antes_fixo=");
    if (p) mb = atoi(p + 9);
    if (f) fixo = atoi(f + 11);
    tex_definir_orcamento_mb(fixo == 3 ? mb : 0);
    dados_fs_travar();
    dados_apagar("diagnostico-otimizacao.checkpoint");
    dados_fs_liberar();
    printf("[diagnostico] checkpoint restaurado: %s (%d MB)\n",
           fixo == 3 ? "fixo" : "automatico", fixo == 3 ? mb : 0);
  }
  free(cfg);
}

static void iniciarTeste(void) {
  int intro = d.intro;
  if (atomic_load(&d.estado) == 1) return;
  memset(&d.addon, 0, sizeof d.addon);
  d.nAddon = 0;
  d.nAssets = 0;
  d.imagensOk = 0;
  d.imagensFalhas = 0;
  d.assetsMs = 0;
  d.assetsBytes = 0;
  d.manifestMs = 0;
  d.catalogMs = 0;
  d.streamMs = 0;
  d.manifestOk = 0;
  d.manifestFalhas = 0;
  d.catalogOk = 0;
  d.catalogFalhas = 0;
  d.streamOk = 0;
  d.streamFalhas = 0;
  d.aplicado = 0;
  d.restaurado = 0;
  d.enviado = 0;
  d.envioFalhou = 0;
  d.registroId[0] = 0;
  d.relatorio[0] = 0;
  d.erro[0] = 0;
  d.coberturaParcial = 0;
  d.inicioMs = SDL_GetTicks();
  dados_uuid(d.id, sizeof d.id);
  d.intro = intro;
  atomic_store(&d.cancelado, 0);
  atomic_store(&d.feitos, 0);
  atomic_store(&d.estado, 1);
  d.modo = focoModo ? DIAG_DESEMPENHO : DIAG_QUALIDADE;
  d.fio = SDL_CreateThread(diagnosticoWorker, "nuvio-diagnostico", NULL);
  if (!d.fio) {
    snprintf(d.erro, sizeof d.erro, "%s", "não foi possível iniciar o teste");
    atomic_store(&d.estado, 4);
  }
}

void diagnostico_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (d.intro) {
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) {
      sairTela = 1;
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      marcarApresentacaoVista();
      d.intro = 0;
    }
    return;
  }
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE) {
    if (atomic_load(&d.estado) == 1) atomic_store(&d.cancelado, 1);
    else sairTela = 1;
    return;
  }
  if (k == SDLK_r && atomic_load(&d.estado) == 2 && d.aplicado) {
    tex_definir_orcamento_mb(d.orcamentoAntesFixo == 3 ? d.orcamentoAntes : 0);
    dados_fs_travar();
    dados_apagar("diagnostico-otimizacao.cfg");
    dados_fs_liberar();
    d.restaurado = 1;
    d.aplicado = 0;
    return;
  }
  if (k == SDLK_e && atomic_load(&d.estado) == 2 && d.envioFalhou && d.relatorio[0]) {
    d.enviado = avisos_enviar_diagnostico(d.id, d.relatorio,
                                          d.registroId, sizeof d.registroId);
    d.envioFalhou = !d.enviado;
    return;
  }
  if (atomic_load(&d.estado) == 1) return;
  if (k == SDLK_LEFT || k == SDLK_RIGHT) { focoModo = !focoModo; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (atomic_load(&d.estado) == 0 || atomic_load(&d.estado) == 3 || atomic_load(&d.estado) == 4)
      iniciarTeste();
    else if (atomic_load(&d.estado) == 2) iniciarTeste();
  }
}

void diagnostico_atualizar(float dt, Uint32 agora) {
  (void)dt; (void)agora;
  if (d.fio && atomic_load(&d.estado) != 1) {
    SDL_WaitThread(d.fio, NULL);
    d.fio = NULL;
  }
}

static void miniCartao(GfxRect r, int tipo, const char *rotulo) {
  TxtLinha t;
  gfx_cor(r, 18.0f / r.h, 0.11f, 0.12f, 0.15f, 1.0f);
  if (tipo == 0) {
    gfx_cor((GfxRect){ r.x + 18, r.y + 18, r.w * 0.34f, r.h - 36 }, 8.0f / r.h, 0.34f, 0.46f, 0.70f, 1.0f);
    gfx_cor((GfxRect){ r.x + r.w * 0.42f, r.y + 28, r.w * 0.48f, 10 }, 0.5f, 0.86f, 0.88f, 0.92f, 0.8f);
    gfx_cor((GfxRect){ r.x + r.w * 0.42f, r.y + 54, r.w * 0.34f, 8 }, 0.5f, 0.45f, 0.55f, 0.68f, 0.8f);
  } else if (tipo == 1) {
    int i;
    for (i = 0; i < 5; i++) {
      float bh = 24.0f + (float)((i * 17) % 54);
      gfx_cor((GfxRect){ r.x + 24.0f + i * 34.0f, r.y + r.h - 34.0f - bh, 20.0f, bh },
              0.25f, 0.30f + i * 0.06f, 0.68f, 0.70f + i * 0.04f, 1.0f);
    }
    gfx_cor((GfxRect){ r.x + 20, r.y + r.h - 30, r.w - 40, 2 }, 0.5f, 0.54f, 0.58f, 0.66f, 0.9f);
  } else {
    int i;
    for (i = 0; i < 7; i++)
      gfx_cor((GfxRect){ r.x + 18.0f + i * (r.w - 36.0f) / 7.0f, r.y + 28,
                         (r.w - 54.0f) / 7.0f, r.h - 56 },
              5.0f / r.h, i == 3 ? 0.75f : 0.28f, i == 3 ? 0.58f : 0.34f,
              i == 3 ? 0.38f : 0.42f, 1.0f);
  }
  t = txt_linha(TXT_CAPTION, i18n(rotulo), 206, 210, 220, 255);
  txt_desenhar(t, r.x, r.y + r.h + 16.0f);
}

/* A tela usa as mesmas primitivas vetoriais do restante do app. Elas sao
 * equivalentes aos SVGs da versao web, mas nao dependem de imagem ou fonte
 * externa para aparecer no primeiro quadro da TV. */
static void painel(GfxRect r, float ar, float ag, float ab) {
  gfx_cor(r, 22.0f / r.h, 0.055f, 0.065f, 0.085f, 0.97f);
  gfx_luz_canto(r, 26.0f / r.h, 150.0f, -70.0f, 500.0f, ar, ag, ab, 0.11f);
  gfx_cor((GfxRect){ r.x, r.y, r.w, 2.0f }, 1.0f, ar, ag, ab, 0.48f);
}

static void painelTitulo(GfxRect r, const char *titulo, const char *subtitulo) {
  txt_desenhar(txt_linha(TXT_BODY, i18n(titulo), 238, 242, 248, 255),
               r.x + 28.0f, r.y + 22.0f);
  if (subtitulo)
    txt_desenhar(txt_linha(TXT_CAPTION, i18n(subtitulo), 154, 164, 178, 255),
                 r.x + 28.0f, r.y + 56.0f);
}

static float limitePct(float n) {
  if (n < 0.0f) return 0.0f;
  if (n > 100.0f) return 100.0f;
  return n;
}

static void barraProgresso(GfxRect r, float pct, float ar, float ag, float ab,
                           int animado, Uint32 agora) {
  float w = r.w * limitePct(pct) / 100.0f;
  gfx_cor(r, 9.0f / r.h, 0.10f, 0.12f, 0.15f, 1.0f);
  if (w > 1.0f)
    gfx_cor((GfxRect){ r.x, r.y, w, r.h }, 9.0f / r.h, ar, ag, ab, 0.96f);
  if (animado && w > 24.0f) {
    float x = r.x + fmodf((float)agora * 0.24f, w + 160.0f) - 80.0f;
    gfx_cor((GfxRect){ x, r.y + 2.0f, 78.0f, r.h - 4.0f },
            7.0f / r.h, 1.0f, 1.0f, 1.0f, 0.18f);
  }
}

static void graficoBarras(GfxRect r, const int *valores, int n, int maior,
                          float ar, float ag, float ab) {
  int i;
  float largura;
  if (n < 1) return;
  if (maior < 1) maior = 1;
  largura = (r.w - (float)(n - 1) * 14.0f) / (float)n;
  gfx_cor((GfxRect){ r.x, r.y + r.h - 2.0f, r.w, 2.0f }, 1.0f,
          0.25f, 0.29f, 0.36f, 0.85f);
  for (i = 0; i < n; i++) {
    float h = (r.h - 12.0f) * (float)valores[i] / (float)maior;
    if (h < 3.0f && valores[i] > 0) h = 3.0f;
    gfx_cor((GfxRect){ r.x + i * (largura + 14.0f), r.y + r.h - h,
                       largura, h }, 7.0f / r.h, ar, ag, ab,
            i == n - 1 ? 0.98f : 0.58f);
  }
}

static void metrica(GfxRect r, float y, const char *rotulo, const char *valor,
                    float cr, float cg, float cb) {
  txt_desenhar(txt_linha(TXT_CAPTION, i18n(rotulo), 154, 164, 178, 255),
               r.x + 28.0f, y);
  txt_desenhar(txt_linha(TXT_BODY, valor, cr, cg, cb, 255), r.x + r.w - 290.0f, y - 3.0f);
}

static const char *gargaloPrincipal(void) {
  int maior = d.assetsMs;
  const char *nome = "Artes, logos e fundos";
  if (d.streamMs > maior) { maior = d.streamMs; nome = "Fontes de vídeo"; }
  if (d.catalogMs > maior) { maior = d.catalogMs; nome = "Catálogos"; }
  if (d.manifestMs > maior) { maior = d.manifestMs; nome = "Manifestos"; }
  return maior > 0 ? nome : "Sem gargalo identificado";
}

static int coberturaPercentual(void) {
  int total = atomic_load(&d.total);
  int feito = atomic_load(&d.feitos);
  if (d.coberturaParcial && total > 0) return (feito * 100) / total;
  return total > 0 ? 100 : 0;
}

static void desenharApresentacao(int primeiraAbertura) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  GfxRect cartao = { 150.0f, 136.0f, 1620.0f, 808.0f };
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor(tela, 0.0f, 0, 0, 0, 0.78f);
  gfx_cor(cartao, 30.0f / cartao.h, 0.055f, 0.058f, 0.068f, 0.98f);
  gfx_luz_canto(cartao, 30.0f / cartao.h, 160.0f, -80.0f, 620.0f, ar, ag, ab, 0.20f);
  txt_desenhar(txt_linha(TXT_TITULO2, i18n("Antes de começar"), 255, 255, 255, 255),
               cartao.x + 64.0f, cartao.y + 42.0f);
  txt_desenhar(txt_linha(TXT_BODY, i18n("O que será medido"), 178, 182, 190, 255),
               cartao.x + 64.0f, cartao.y + 124.0f);
  miniCartao((GfxRect){ cartao.x + 64.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 0, "Artes, logos e fundos");
  miniCartao((GfxRect){ cartao.x + 412.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 1, "Tempos, bytes e falhas");
  miniCartao((GfxRect){ cartao.x + 760.0f, cartao.y + 180.0f, 320.0f, 142.0f }, 2, "Fontes de vídeo");
  txt_bloco(TXT_BODY, i18n("O app lê manifestos e catálogos, testa artes e fontes de vídeo e compara o caminho de carregamento desta TV."),
            192, 196, 206, cartao.x + 64.0f, cartao.y + 388.0f, cartao.w - 128.0f, 34.0f, 1, 3);
  txt_bloco(TXT_BODY, i18n("Haverá testes de rede e consumo de dados. O relatório técnico será enviado automaticamente ao suporte."),
            214, 190, 170, cartao.x + 64.0f, cartao.y + 500.0f, cartao.w - 128.0f, 34.0f, 1, 3);
  txt_desenhar(txt_linha(TXT_CAPTION, i18n("Não altera assistidos, histórico, progresso ou scrobbling."), 164, 168, 178, 255),
               cartao.x + 64.0f, cartao.y + 650.0f);
  txt_desenhar(txt_linha(TXT_CAPTION,
                         i18n(primeiraAbertura ? "OK continua · Voltar fecha" : "OK começa · Voltar cancela"),
                         172, 176, 184, 255),
               cartao.x + 64.0f, cartao.y + cartao.h - 48.0f);
}

void diagnostico_intro_primeira_vez(void) {
  if (introDecidido) return;
  introDecidido = 1;
  introGlobal = !apresentacaoVista();
}

int diagnostico_intro_aberto(void) { return introGlobal; }

void diagnostico_intro_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!introGlobal || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    marcarApresentacaoVista();
    introGlobal = 0;
  } else if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
             k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    introGlobal = 0;
  }
}

void diagnostico_intro_atualizar(float dt, Uint32 agora) {
  (void)dt;
  (void)agora;
}

void diagnostico_intro_desenhar(Uint32 agora) {
  (void)agora;
  if (introGlobal) desenharApresentacao(1);
}

void diagnostico_desenhar(Uint32 agora) {
  int estado = atomic_load(&d.estado);
  int feito = atomic_load(&d.feitos), total = atomic_load(&d.total);
  int itens = 0, pend = 0, quentes = 0, slots = 0, fios = 0, fiosMax = 0;
  long bytes = 0, bytesQuentes = 0, memTotal = 0, limite = 0;
  int orcMb = 0;
  float ar, ag, ab;
  char v[96];
  GfxRect esquerda = { 80.0f, 188.0f, 840.0f, 344.0f };
  GfxRect direita  = { 1000.0f, 188.0f, 840.0f, 344.0f };
  GfxRect baixoEsq = { 80.0f, 562.0f, 840.0f, 342.0f };
  GfxRect baixoDir = { 1000.0f, 562.0f, 840.0f, 342.0f };
  ajustes_acento(&ar, &ag, &ab);
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQuentes);
  tex_orcamento_info(&orcMb, &memTotal, NULL, &slots);
  limite = tex_orcamento_bytes();
  tex_threads_info(&fios, &fiosMax);
  txt_desenhar(txt_linha(TXT_TITULO1, i18n("Diagnóstico e otimização"), 255,255,255,255), NV_MARGEM_X, NV_MARGEM_Y);
  txt_desenhar(txt_linha(TXT_BODY, i18n("Teste os addons, artes e fontes desta TV; o relatório é enviado ao suporte."), 178,182,190,255), NV_MARGEM_X, NV_MARGEM_Y + 52.0f);

  if (estado == 0) {
    painel(esquerda, ar, ag, ab);
    painel(direita, ar, ag, ab);
    painelTitulo(esquerda, "Modo", "Esquerda/direita escolhe o objetivo");
    gfx_cor((GfxRect){ esquerda.x + 28.0f, esquerda.y + 94.0f, 360.0f, 72.0f }, 18.0f / 72.0f,
            focoModo ? 0.10f : ar, focoModo ? 0.12f : ag, focoModo ? 0.15f : ab, 0.98f);
    gfx_cor((GfxRect){ esquerda.x + 420.0f, esquerda.y + 94.0f, 360.0f, 72.0f }, 18.0f / 72.0f,
            focoModo ? ar : 0.10f, focoModo ? ag : 0.12f, focoModo ? ab : 0.15f, 0.98f);
    txt_desenhar(txt_linha(TXT_BODY, i18n("Qualidade"), 250,250,250,255), esquerda.x + 58.0f, esquerda.y + 116.0f);
    txt_desenhar(txt_linha(TXT_BODY, i18n("Desempenho"), 250,250,250,255), esquerda.x + 450.0f, esquerda.y + 116.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n(focoModo ? "Fontes mais leves, menos antecipação e menor pressão de memória" : "Nitidez na resolução exibida e preferências atuais preservadas"), 178,184,194,255), esquerda.x + 30.0f, esquerda.y + 198.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("A ferramenta escolhe os limites da plataforma; não é preciso configurar threads ou buffers."), 178,184,194,255), esquerda.x + 30.0f, esquerda.y + 238.0f);
    barraProgresso((GfxRect){ esquerda.x + 30.0f, esquerda.y + 286.0f, esquerda.w - 60.0f, 12.0f }, 100.0f, ar, ag, ab, 0, agora);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Até 3 títulos, 6 fontes e 12 imagens; limite de 8 minutos"), 160,170,182,255), esquerda.x + 30.0f, esquerda.y + 308.0f);
    painelTitulo(direita, "Painel de resultado", "O que você verá ao terminar");
    miniCartao((GfxRect){ direita.x + 28.0f, direita.y + 92.0f, 240.0f, 128.0f }, 1, "Tempos medidos");
    miniCartao((GfxRect){ direita.x + 296.0f, direita.y + 92.0f, 240.0f, 128.0f }, 2, "Gargalo principal");
    miniCartao((GfxRect){ direita.x + 564.0f, direita.y + 92.0f, 240.0f, 128.0f }, 0, "Memória disponível");
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("O relatório compacto é guardado localmente e só fica como enviado após confirmação do servidor."), 170,178,190,255), direita.x + 30.0f, direita.y + 258.0f);
    txt_desenhar(txt_linha(TXT_BODY, i18n("OK inicia o diagnóstico"), ar * 255.0f, ag * 255.0f, ab * 255.0f, 255), NV_MARGEM_X, 966.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Voltar cancela · a análise não altera assistidos, histórico ou scrobbling"), 172,176,184,255), NV_MARGEM_X + 270.0f, 970.0f);
  } else if (estado == 1) {
    GfxRect andamento = { 180.0f, 248.0f, 1560.0f, 500.0f };
    float pct = total > 0 ? 100.0f * (float)feito / (float)total : 4.0f;
    painel(andamento, ar, ag, ab);
    painelTitulo(andamento, "Diagnóstico em andamento", d.fase == 1 ? "Sondando manifestos, catálogos e fontes" : "Testando artes, logos e fundos");
    snprintf(v, sizeof v, "%d%%", (int)pct);
    txt_desenhar(txt_linha(TXT_TITULO2, v, 246,249,255,255), andamento.x + 64.0f, andamento.y + 124.0f);
    snprintf(v, sizeof v, "%d/%d addons", feito, total);
    txt_desenhar(txt_linha(TXT_BODY, v, 178,188,202,255), andamento.x + 230.0f, andamento.y + 140.0f);
    barraProgresso((GfxRect){ andamento.x + 64.0f, andamento.y + 218.0f, andamento.w - 128.0f, 20.0f }, pct, ar, ag, ab, 1, agora);
    metrica(andamento, andamento.y + 286.0f, "Memória usada por imagens", limite > 0 ? (snprintf(v, sizeof v, "%ld / %ld MB", bytes / (1024L*1024L), limite / (1024L*1024L)), v) : "indisponível", 214,220,230);
    snprintf(v, sizeof v, "%d / %d", fios, fiosMax);
    metrica(andamento, andamento.y + 326.0f, "Threads em uso", v, 214,220,230);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Voltar cancela e interrompe as requisições da sessão."), 176,184,194,255), andamento.x + 64.0f, andamento.y + 416.0f);
  } else if (estado == 2) {
    int tempos[4] = { d.manifestMs, d.catalogMs, d.assetsMs, d.streamMs };
    int maior = d.manifestMs;
    int cobertura = coberturaPercentual();
    if (d.catalogMs > maior) maior = d.catalogMs;
    if (d.assetsMs > maior) maior = d.assetsMs;
    if (d.streamMs > maior) maior = d.streamMs;
    painel(esquerda, ar, ag, ab);
    painel(direita, ar, ag, ab);
    painel(baixoEsq, ar, ag, ab);
    painel(baixoDir, ar, ag, ab);
    painelTitulo(esquerda, "Resultado geral", d.coberturaParcial ? "Cobertura parcial da amostra" : "Amostra concluída");
    snprintf(v, sizeof v, "%d%%", cobertura);
    txt_desenhar(txt_linha(TXT_TITULO2, v, ar * 255.0f, ag * 255.0f, ab * 255.0f, 255), esquerda.x + 32.0f, esquerda.y + 96.0f);
    barraProgresso((GfxRect){ esquerda.x + 32.0f, esquerda.y + 172.0f, esquerda.w - 64.0f, 16.0f }, cobertura, ar, ag, ab, 0, agora);
    snprintf(v, sizeof v, "%d / %d / %d", d.nAddon, d.imagensOk + d.imagensFalhas, d.streamOk);
    metrica(esquerda, esquerda.y + 220.0f, "Cobertura", v, 210,218,230);
    snprintf(v, sizeof v, "%d ok · %d falhas", d.imagensOk, d.imagensFalhas);
    metrica(esquerda, esquerda.y + 260.0f, "Artes", v, 210,218,230);
    snprintf(v, sizeof v, "%s", d.envioFalhou ? "guardado; envio falhou" : d.enviado ? "enviado ao suporte" : "guardado localmente");
    metrica(esquerda, esquerda.y + 300.0f, "Relatório", v, d.envioFalhou ? 240 : 170, d.envioFalhou ? 170 : 220, d.envioFalhou ? 150 : 190);
    painelTitulo(direita, "Gargalo principal", gargaloPrincipal());
    txt_desenhar(txt_linha(TXT_BODY, i18n(gargaloPrincipal()), 244,224,172,255), direita.x + 30.0f, direita.y + 100.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Maior tempo agregado da amostra; indisponibilidade isolada não quebra o addon inteiro."), 174,182,194,255), direita.x + 30.0f, direita.y + 146.0f);
    graficoBarras((GfxRect){ direita.x + 32.0f, direita.y + 212.0f, direita.w - 64.0f, 92.0f }, tempos, 4, maior, ar, ag, ab);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Manifestos   Catálogos   Artes   Fontes"), 150,160,174,255), direita.x + 34.0f, direita.y + 320.0f);
    painelTitulo(baixoEsq, "Recursos desta TV", "Medições do cache no momento do resultado");
    snprintf(v, sizeof v, "%ld / %ld MB", bytes / (1024L*1024L), limite > 0 ? limite / (1024L*1024L) : 0L);
    metrica(baixoEsq, baixoEsq.y + 100.0f, "Memória usada por imagens", v, 220,226,236);
    snprintf(v, sizeof v, "%ld MB", memTotal);
    metrica(baixoEsq, baixoEsq.y + 140.0f, "Memória disponível", memTotal > 0 ? v : "indisponível", 220,226,236);
    snprintf(v, sizeof v, "%d / %d", fios, fiosMax);
    metrica(baixoEsq, baixoEsq.y + 180.0f, "Threads em uso", v, 220,226,236);
    snprintf(v, sizeof v, "%d MB · %d pendentes · %d quentes · %d slots", orcMb, pend, quentes, slots);
    metrica(baixoEsq, baixoEsq.y + 220.0f, "Memória para imagens", v, 176,188,202);
    barraProgresso((GfxRect){ baixoEsq.x + 30.0f, baixoEsq.y + 272.0f, baixoEsq.w - 60.0f, 10.0f }, limite > 0 ? 100.0f * (float)bytes / (float)limite : 0.0f, ar, ag, ab, 0, agora);
    painelTitulo(baixoDir, "O que será aplicado", d.aplicado ? "Comparação aprovada e aplicada" : "Padrão mantido com segurança");
    txt_desenhar(txt_linha(TXT_BODY, i18n(d.modo == DIAG_DESEMPENHO ? "Desempenho" : "Qualidade"), 240,244,250,255), baixoDir.x + 30.0f, baixoDir.y + 100.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n(d.modo == DIAG_DESEMPENHO ? "Menos antecipação, fontes mais leves e resolução menor quando houver ganho comprovado." : "Nitidez na resolução exibida, prioridade do conteúdo visível e transparência preservada."), 176,184,196,255), baixoDir.x + 30.0f, baixoDir.y + 140.0f);
#if NV_DIAG_AUTO_OPT
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Aplicação automática aguardando validação"), 244,218,152,255), baixoDir.x + 30.0f, baixoDir.y + 218.0f);
#else
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("Aplicação automática aguardando validação"), 244,218,152,255), baixoDir.x + 30.0f, baixoDir.y + 218.0f);
#endif
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("O reteste usa a mesma amostra e compara cache frio e quente."), 176,184,196,255), baixoDir.x + 30.0f, baixoDir.y + 258.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n(d.restaurado ? "Configuração anterior restaurada" : d.envioFalhou ? "E tenta enviar novamente" : "Reteste da mesma amostra"), 172,176,184,255), NV_MARGEM_X, 952.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("OK retesta · R restaura · Voltar sai"), 172,176,184,255), NV_MARGEM_X + 270.0f, 952.0f);
  } else {
    painel((GfxRect){ 260.0f, 260.0f, 1400.0f, 360.0f }, ar, ag, ab);
    txt_desenhar(txt_linha(TXT_TITULO2, i18n(d.erro[0] ? d.erro : "O teste foi cancelado."), 240,190,180,255), 320.0f, 340.0f);
    txt_desenhar(txt_linha(TXT_BODY, i18n("Nenhuma configuração foi alterada."), 182,190,202,255), 320.0f, 420.0f);
    txt_desenhar(txt_linha(TXT_CAPTION, i18n("OK tenta de novo · Voltar sai"), 172,176,184,255), 320.0f, 540.0f);
  }
  if (d.intro) desenharApresentacao(0);
}

int diagnostico_quer_sair(void) { return sairTela; }

void diagnostico_encerrar(void) {
  if (d.fio) {
    atomic_store(&d.cancelado, 1);
    SDL_WaitThread(d.fio, NULL);
    d.fio = NULL;
  }
}
