// DIAGNOSTICO DA LIVE TV (livetvdiag.h).
//
// POR QUE EXISTE (#158). Na LG C4 do pasha o .ts do Xtream chegava (36 s de
// buffer), o uMS reservava o decodificador e nunca mostrava imagem, enquanto
// outro app na mesma TV tocava o canal. O registro so contava isso para quem
// soubesse ler; esta tela mede na TV da pessoa o que separa um canal que toca
// de um que nao toca, e transforma em ajuste:
//
//   1. a CONTA Xtream (formatos declarados, telas, validade);
//   2. para cada canal da fileira em foco no guia, a REDE: o .ts e a .m3u8
//      pedidos direto ao provedor (os dois, mesmo com a conta declarando um so
//      — e a hipotese "o provedor serve HLS assim mesmo"), o que chega (e TS?
//      que codecs a PMT declara? 10 bits?) e a que velocidade;
//   3. o PLAYER: o formato que respondeu vai ao pipeline de verdade, num quadro
//      da tela, ate o primeiro quadro (tempo) ou ate desistir (por que);
//   4. a RECOMENDACAO (livetv_regras.h), com "Aplicar".
//
// UMA COISA DE CADA VEZ: a conta do pasha tem 1 tela (telas=0/1), e pedido em
// paralelo ao mesmo provedor e recusado (os 40404 em sequencia do registro
// 14115). O fio de rede e o pipeline nunca se cruzam, e entre um e outro ha
// uma pausa para o provedor soltar a conexao.
//
// Nada aqui grava URL, usuario ou senha no registro: so o nome do canal.
#include "livetvdiag.h"
#include "livetv_regras.h"
#include "ts_sonda.h"
#include "ajustes.h"
#include "addons.h"
#include "avisos.h"
#include "fontecache.h"  /* addons_consultar */
#include "gfx.h"
#include "guia.h"
#include "idioma.h"
#include "layout.h"
#include "player.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefonecartao.h"
#include "proxyts.h"
#include "rede.h"
#include "streams.h"
#include "text.h"
#include "vazao.h"
#include "video.h"
#include "xtream.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
#define LTD_TEM_PLAYER 0   // o Mac nao tem pipeline (video.c: stubs)
#else
#define LTD_TEM_PLAYER 1
#endif

#define LTD_MAX          6
#define LTD_PLAYER_MS    18000u  // acima dos 15 s do Xtream: ve quem abre aos 14
#define LTD_MOSTRA_MS    1500u   // o quadro fica na tela um instante depois de abrir
#define LTD_PAUSA_MS     1000u   // o provedor solta a conexao entre um pedido e outro
#define LTD_TRECHO_B     786431L
#define LTD_LINHA        70.0f   // altura de cada canal na lista
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueLtd;
static float toqueLtdOffset;
static int toqueLtdRolar(const PonteiroRolagem *e) { return toquerol_evento(&toqueLtd, e); }
static void ltdPhoneReiniciar(void);
#endif

enum { F_HLS = 0, F_TS = 1 };
enum { E_PARADO, E_REDE, E_PLAYER, E_PRONTO };

typedef struct {
  int  tentado, servido;  // pedido feito; respondeu 2xx com video
  int  http, curl;
  int  ms;                // pedido -> corpo
  long bytes;
  int  kbps;
  int  playlist;          // HLS: 1 = #EXTM3U com segmento
  int  ehTs;
  char codec[96];
  int  dezBits, embaralhado;
  char url[4096];         // o que vai ao player (nunca ao registro)
  // player
  int  testou, tocou, decoder, quadroMs, largura, altura, falha;
  char erro[64];
  // MODOS DO LOAD (video_definir_modo_live): cada formato passa pelos modos
  // A/B/C ate um tocar; o resultado de cada um fica para o registro e a
  // recomendacao. modoOk = o que tocou, -1 nenhum.
  int  modoFeito, modoOk;
  int  corpoM3u;          // o .ts respondeu uma playlist (registro 14565)
  char resumo[300];       // ts_resumo do trecho lido
  char segUrl[4096];      // HLS: o primeiro segmento (modo D toca so ele)
  int  modoFalha[5], modoQuadroMs[5];
} LtdFormato;

typedef struct {
  char nome[140], id[80], base[600], cabs[512];
  int  xt, alturaNome;
  int  semFonte;          // addon: nenhuma fonte
  LtdFormato f[2];
  int  pronto;
} LtdItem;

static struct {
  int estado, sair, n, atual, botao;
  LtdItem it[LTD_MAX];
  char grupo[64];
  int xtConfig, contaLida;
  XtreamConta conta;
  int kbps, kbpsPior, latenciaMs, redeMedida, kbpsDoSegmento;
  // player em curso
  int pfFormato, pfModo, pfVivo;
  Uint32 pfDesde, pfTocouEm, pausaAte;
  LtdRecomendacao rec;
  int recModo, tocouModo[5];   // modo do load que mais tocou; -1 = nenhum
  int aplicado, enviou;
  _Atomic int fioOcupado;
  volatile int cancelado;
  pthread_t fio;
  int fioCriado;
} L;

// --- rede -------------------------------------------------------------------
static const char *const *vetorCabs(const char *cab, char *copia, size_t n, const char *v[8]) {
  int k = 0;
  char *l, *ctx = NULL;
  if (!cab || !cab[0]) return NULL;
  snprintf(copia, n, "%s", cab);
  for (l = strtok_r(copia, "\n", &ctx); l && k < 7; l = strtok_r(NULL, "\n", &ctx)) v[k++] = l;
  v[k] = NULL;
  return k ? v : NULL;
}

static void juntarUrl(const char *base, const char *rel, char *dst, size_t n) {
  const char *q;
  if (!strncmp(rel, "http://", 7) || !strncmp(rel, "https://", 8)) { snprintf(dst, n, "%s", rel); return; }
  if (rel[0] == '/') {
    const char *h = strstr(base, "://");
    const char *barra = h ? strchr(h + 3, '/') : NULL;
    int len = barra ? (int)(barra - base) : (int)strlen(base);
    snprintf(dst, n, "%.*s%s", len, base, rel);
    return;
  }
  q = strrchr(base, '/');
  { const char *interroga = strchr(base, '?');
    if (interroga && q && q > interroga) q = NULL; }
  snprintf(dst, n, "%.*s/%s", q ? (int)(q - base) : (int)strlen(base), base, rel);
}

static void codecDe(const TsSonda *t, LtdFormato *f) {
  int i, u = 0;
  f->codec[0] = 0;
  f->ehTs = t->deslocamento >= 0;
  f->embaralhado = t->embaralhados > 0;
  for (i = 0; i < t->nEs && u < (int)sizeof f->codec - 24; i++) {
    const char *nome = ts_tipo_nome(t->es[i].tipo, t->es[i].desc);
    if (!strcmp(nome, "?") || !strcmp(nome, "privado") || !strcmp(nome, "legenda DVB") ||
        !strcmp(nome, "teletexto")) continue;
    if (strstr(f->codec, nome)) continue;
    u += snprintf(f->codec + u, sizeof f->codec - (size_t)u, "%s%s", u ? " · " : "", nome);
  }
  f->dezBits = t->videoPid >= 0 && ((!t->hevc && (t->perfil == 110 || t->perfil == 122)) ||
                                    (t->hevc && t->perfil == 2));
}

// Le o comeco de `url` e preenche servido/http/kbps/codec. Devolve 1 com corpo.
static int sondarTrecho(LtdFormato *f, const char *url, const char *const *cabs) {
  long n = 0;
  int st = 0, er = 0;
  Uint32 t0 = SDL_GetTicks();
  char *b;
  (void)cabs;  // rede_baixar_trecho_st nao leva cabecalho; os canais com cabecalho sao HLS de addon
  b = rede_baixar_trecho_st(url, 8, 0, LTD_TRECHO_B, &n, &st, &er, NULL, 0);
  f->ms = (int)(SDL_GetTicks() - t0);
  if (!f->http) f->http = st;
  f->curl = er;
  f->bytes = n;
  if (b && n > 0 && n < 4096) {
    // CORPO PEQUENO DEMAIS PARA SER VIDEO (#158, registro 14520: o .ts direto
    // devolveu 663 B ao curl enquanto o uMS bufferizava). Diz o que e, sem
    // imprimir o corpo (pode trazer URL com usuario e senha).
    const char *tipo = strstr(b, "#EXTM3U") ? "playlist m3u" : (strstr(b, "<html") || strstr(b, "<HTML") ||
                       strstr(b, "<!DOCTYPE")) ? "html" : (b[0] == '{' || b[0] == '[') ? "json" : "outro";
    long imprim = 0, k;
    for (k = 0; k < n; k++) if ((b[k] >= 32 && b[k] < 127) || b[k] == '\n' || b[k] == '\r') imprim++;
    f->corpoM3u = strstr(b, "#EXTM3U") != NULL;
    printf("[livetv-diag] corpo curto: %ld B, %s, %ld%% texto, 0x47 no inicio=%d, tem http=%d\n", n, tipo,
           imprim * 100 / n, (unsigned char)b[0] == 0x47, strstr(b, "http") != NULL);
  }
  if (b && n > 0) {
    TsSonda t;
    ts_sondar((const unsigned char *)b, n, &t);
    codecDe(&t, f);
    ts_resumo(&t, f->resumo, sizeof f->resumo);
    if (f->ms > 0) f->kbps = (int)((double)n * 8.0 / (double)f->ms);
    f->servido = f->ehTs;
  }
  free(b);
  return n > 0;
}

// HLS: a playlist, a variante (pela resolucao principal), e o primeiro
// segmento, que diz codec e velocidade como o .ts direto.
static void sondarHls(LtdFormato *f, const char *url, const char *const *cabs) {
  char atual[4096], prox[4096], fin[4096];
  int volta;
  snprintf(atual, sizeof atual, "%s", url);
  for (volta = 0; volta < 2; volta++) {
    long n = 0;
    int st = 0, er = 0;
    char *b;
    const char *l, *escolhida = NULL;
    int alvo = nv_res_opcao_altura(ajustes_livetv_resolucao()), melhorDif = 1 << 30;
    fin[0] = 0;
    b = rede_baixar_trecho_st(atual, 6, 0, 262143, &n, &st, &er, fin, sizeof fin);
    if (!volta) { f->http = st; f->curl = er; }
    if (!b || n < 7 || strncmp(b, "#EXTM3U", 7)) { free(b); return; }
    if (fin[0]) snprintf(atual, sizeof atual, "%s", fin);
    if (strstr(b, "#EXT-X-STREAM-INF")) {
      // MASTER: a variante mais perto da resolucao principal (sem ela, a
      // primeira, que e a ordem do provedor).
      for (l = b; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) {
        if (strncmp(l, "#EXT-X-STREAM-INF", 17)) continue;
        { const char *r = strstr(l, "RESOLUTION="), *fimLinha = strchr(l, '\n');
          int h = 0, dif;
          if (r && (!fimLinha || r < fimLinha)) { const char *x = strchr(r, 'x'); if (x) h = atoi(x + 1); }
          dif = alvo ? abs(h - alvo) : (escolhida ? 1 << 30 : 0);
          if (!escolhida || dif < melhorDif) { melhorDif = dif; escolhida = fimLinha ? fimLinha + 1 : NULL; } }
      }
    } else {
      for (l = b; l && *l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL)
        if (*l != '#' && *l != '\n' && *l != '\r') { escolhida = l; break; }
      f->playlist = escolhida != NULL;
    }
    if (!escolhida) { free(b); return; }
    { size_t k = strcspn(escolhida, "\r\n");
      char rel[2048];
      snprintf(rel, sizeof rel, "%.*s", (int)(k < sizeof rel ? k : sizeof rel - 1), escolhida);
      juntarUrl(atual, rel, prox, sizeof prox); }
    { int mestre = strstr(b, "#EXT-X-STREAM-INF") != NULL;
      if (!mestre) {
        // A PLAYLIST DE MIDIA, resumida sem URL (#158): o que o motor HLS do
        // uMS le e o hls.js do navegador tambem.
        int segs = 0, disc = 0, chave = 0, mapa = 0, pdt = 0, td = -1, outroHost = 0, extTs = 0;
        double soma = 0.0;
        const char *h0 = strstr(atual, "://"), *l2;
        size_t hn = h0 ? strcspn(h0 + 3, "/") : 0;
        for (l2 = b; l2 && *l2; l2 = strchr(l2, '\n') ? strchr(l2, '\n') + 1 : NULL) {
          if (!strncmp(l2, "#EXTINF:", 8)) { segs++; soma += atof(l2 + 8); }
          else if (!strncmp(l2, "#EXT-X-DISCONTINUITY", 20)) disc++;
          else if (!strncmp(l2, "#EXT-X-KEY", 10) && !strstr(l2, "METHOD=NONE")) chave++;
          else if (!strncmp(l2, "#EXT-X-MAP", 10)) mapa++;
          else if (!strncmp(l2, "#EXT-X-PROGRAM-DATE-TIME", 24)) pdt++;
          else if (!strncmp(l2, "#EXT-X-TARGETDURATION:", 22)) td = atoi(l2 + 22);
          else if (*l2 != '#' && *l2 != '\n' && *l2 != '\r') {
            size_t k = strcspn(l2, "\r\n?");
            if (k > 3 && !strncmp(l2 + k - 3, ".ts", 3)) extTs++;
            if (!strncmp(l2, "http", 4) && h0) {
              const char *h1 = strstr(l2, "://");
              if (h1 && (strcspn(h1 + 3, "/") != hn || strncmp(h1 + 3, h0 + 3, hn))) outroHost++;
            }
          }
        }
        printf("[livetv-diag] playlist: %d segmento(s) somando %.1f s, alvo %d s, .ts=%d, outro host=%d, "
               "discontinuity=%d, chave=%d, map=%d, program-date-time=%d, endlist=%d\n", segs, soma, td,
               extTs, outroHost, disc, chave, mapa, pdt, strstr(b, "#EXT-X-ENDLIST") != NULL);
        snprintf(f->segUrl, sizeof f->segUrl, "%s", prox);
      }
      free(b);
      if (mestre) { snprintf(atual, sizeof atual, "%s", prox); continue; } }
    { int http = f->http;
      sondarTrecho(f, prox, cabs);
      f->http = http; }
    return;
  }
}

static void medirVelocidade(const char *url, const char *const *cabs) {
  int kps[8], i, n = 0, pior = 0;
  RedeVazao r;
  memset(&r, 0, sizeof r);
  n = rede_medir_vazao(url, cabs, 6, 0, 0, &L.cancelado, kps, 8, &r, NULL, 0);
  if (n <= 0 && r.bytes <= 0) return;
  L.latenciaMs = (int)r.esperaMs;
  if (r.ms > 0) L.kbps = (int)((double)r.bytes * 8.0 / (double)r.ms);
  for (i = 0; i < n; i++) if (!pior || (kps[i] > 0 && kps[i] < pior)) pior = kps[i];
  L.kbpsPior = pior;
  L.redeMedida = 1;
  printf("[livetv-diag] rede ate o provedor: %d kbps (pior segundo %d), primeiro byte em %d ms\n",
         L.kbps, L.kbpsPior, L.latenciaMs);
}

// A melhor fonte de um canal de addon: a da resolucao principal, senao a
// primeira (a ordem do addon e o ranking dele).
static void fonteDoAddon(LtdItem *it) {
  Stream *lista = NULL;
  int n = addons_consultar(it->id, "tv", it->base, 2, NULL, NULL, &lista), i, k = 0;
  int alvo = nv_res_opcao_altura(ajustes_livetv_resolucao());
  if (n <= 0 || !lista) { it->semFonte = 1; free(lista); return; }
  for (i = 0; i < n; i++) {
    int a = lista[i].altura ? lista[i].altura : nv_res_do_texto(lista[i].rotulo);
    if (lista[i].url[0] && nv_res_preferida(a, alvo)) { k = i; break; }
  }
  while (k < n && !lista[k].url[0]) k++;
  if (k >= n) { it->semFonte = 1; free(lista); return; }
  { int hls = strstr(lista[k].url, ".m3u8") != NULL;
    LtdFormato *f = &it->f[hls ? F_HLS : F_TS];
    f->tentado = 1;
    snprintf(f->url, sizeof f->url, "%s", lista[k].url);
    snprintf(it->cabs, sizeof it->cabs, "%s", lista[k].cabecalhos);
    it->alturaNome = lista[k].altura ? lista[k].altura : nv_res_do_texto(lista[k].rotulo); }
  free(lista);
}

static void *fioRede(void *u) {
  LtdItem *it = u;
  char copia[512];
  const char *v[8];
  const char *const *cabs;
  int f;
  if (it == &L.it[0] && L.xtConfig) {
    L.contaLida = xtream_conta_ler(&L.conta);
    if (L.conta.valido)
      printf("[livetv-diag] conta: status=%s formatos=%s%s telas=%d/%d vence=%lld\n", L.conta.status,
             L.conta.formatosDeclarados ? (L.conta.temM3u8 ? "m3u8 " : "") : "nao declarados",
             L.conta.formatosDeclarados ? (L.conta.temTs ? "ts" : "") : "",
             L.conta.conexoes, L.conta.maxConexoes, L.conta.expira);
    else printf("[livetv-diag] conta: sem user_info (HTTP %d)\n", L.conta.http);
  }
  if (it->xt) {
    // OS DOIS FORMATOS, sempre: e assim que se sabe se o provedor serve .m3u8
    // com a conta declarando so .ts.
    if (xtream_url_formato(it->id, "m3u8", it->f[F_HLS].url, sizeof it->f[F_HLS].url)) it->f[F_HLS].tentado = 1;
    if (xtream_url_formato(it->id, "ts", it->f[F_TS].url, sizeof it->f[F_TS].url)) it->f[F_TS].tentado = 1;
  } else fonteDoAddon(it);
  cabs = vetorCabs(it->cabs, copia, sizeof copia, v);
  for (f = 0; f < 2 && !L.cancelado; f++) {
    LtdFormato *x = &it->f[f];
    if (!x->tentado) continue;
    if (f == F_HLS) sondarHls(x, x->url, cabs);
    else sondarTrecho(x, x->url, cabs);
    // O .ts QUE E PLAYLIST (registro 14565: 663 B de #EXTM3U no endpoint .ts
    // do provedor): o player recebe HLS nos dois formatos. Resume a playlist.
    if (f == F_TS && x->corpoM3u && !L.cancelado) {
      int http = x->http;
      sondarHls(x, x->url, cabs);
      x->http = http;
    }
    // O MESMO .ts COM USER-AGENT DE PLAYER: o provedor pode entregar outra
    // coisa ao "Nuvio/1.0" do curl e ao player (hipotese 3 do #158).
    if (f == F_TS && x->tentado && !x->servido && !L.cancelado) {
      static const char *const ua[] = { "User-Agent: VLC/3.0.20 LibVLC/3.0.20", NULL };
      RedeControle ctl = { LTD_TRECHO_B + 1, &L.cancelado };
      RedeMedida md;
      long n = 0;
      char *b2;
      memset(&md, 0, sizeof md);
      b2 = rede_baixar_bin_medido_controle(x->url, 8, ua, &ctl, &n, &md);
      { TsSonda t; char r[300] = "-";
        if (b2 && n > 0) { ts_sondar((const unsigned char *)b2, n, &t); ts_resumo(&t, r, sizeof r); }
        printf("[livetv-diag] rede %s TS com UA de player: HTTP %d %ld B em %lu ms | %s\n", it->nome,
               md.status, n, md.ms, r);
        if (b2 && n > 0 && t.deslocamento >= 0) {
          codecDe(&t, x); x->servido = 1; x->bytes = n; x->ms = (int)md.ms;
          if (md.ms) x->kbps = (int)((double)n * 8.0 / (double)md.ms);
        } }
      free(b2);
    }
    printf("[livetv-diag] rede %s %s: HTTP %d curl=%d %ld B em %d ms (%d kbps) %s%s%s%s\n",
           it->nome, f == F_HLS ? "HLS" : "TS", x->http, x->curl, x->bytes, x->ms, x->kbps,
           x->servido ? "video TS" : (f == F_HLS && x->http >= 200 && x->http < 300) ? "playlist sem video" : "sem video",
           x->codec[0] ? " | " : "", x->codec, x->dezBits ? " | 10 bits" : "");
    if (x->resumo[0]) printf("[livetv-diag] ts %s %s: %s\n", it->nome, f == F_HLS ? "HLS" : "TS", x->resumo);
    fflush(stdout);
  }
  // VELOCIDADE. MEDIDO na C9 (01/10): o .ts CONTINUO chega na taxa do proprio
  // canal (2 Mbps num HD) e nao na da rede — a recomendacao saia "SD" numa
  // rede que baixa segmento HLS a 7 Mbps. A vazao boa e a do SEGMENTO HLS, que
  // vem de uma vez: fica a maior entre os canais. Sem HLS, o .ts continuo so
  // da a latencia, e a resolucao nao e recomendada pela rede.
  if (!L.cancelado) {
    if (it->f[F_HLS].servido && it->f[F_HLS].kbps > L.kbps) {
      L.kbps = it->f[F_HLS].kbps; L.redeMedida = 1; L.kbpsDoSegmento = 1;
      printf("[livetv-diag] rede ate o provedor: %d kbps (segmento HLS de %s)\n", L.kbps, it->nome);
    } else if (!L.redeMedida && it->f[F_TS].servido) {
      medirVelocidade(it->f[F_TS].url, cabs);
      L.kbpsDoSegmento = 0;
    }
  }
  fflush(stdout);
  atomic_store(&L.fioOcupado, 0);
  return NULL;
}

static void juntarFio(void) {
  if (L.fioCriado) { pthread_join(L.fio, NULL); L.fioCriado = 0; }
}

static void iniciarRede(int i) {
  juntarFio();
  atomic_store(&L.fioOcupado, 1);
  L.estado = E_REDE;
  L.atual = i;
  if (pthread_create(&L.fio, NULL, fioRede, &L.it[i]) == 0) L.fioCriado = 1;
  else atomic_store(&L.fioOcupado, 0);
}

// --- player -------------------------------------------------------------------
// Qual formato vai ao pipeline primeiro: o que a REDE mostrou servindo video,
// na ordem do Xtream (xtream_formatos: o que a conta declara e o que ja tocou).
static int proximoFormato(const LtdItem *it) {
  const char *ext[2];
  int k = it->xt ? xtream_formatos(ext) : 0, i;
  for (i = 0; i < k; i++) {
    int f = !strcmp(ext[i], "ts") ? F_TS : F_HLS;
    if (it->f[f].servido && !it->f[f].testou) return f;
  }
  for (i = 0; i < 2; i++) if (it->f[i].servido && !it->f[i].testou) return i;
  // Nada pareceu video pela rede: tenta mesmo assim o primeiro pedido que
  // respondeu (um addon pode exigir cabecalho que a sonda nao manda).
  for (i = 0; i < 2; i++) if (it->f[i].tentado && it->f[i].http >= 200 && it->f[i].http < 300 && !it->f[i].testou) return i;
  return -1;
}

static int algumTocou(const LtdItem *it) { return it->f[0].tocou || it->f[1].tocou; }

static GfxRect quadroPlayer;   // onde o video aparece; o desenho atualiza
// Quantos modos cada formato tenta: o HLS (que a rede provou ser video) os
// tres; o TS os tres nos dois primeiros canais e so o padrao nos outros —
// para o teste caber em poucos minutos.
// MODO D (so no diagnostico, so HLS, so os dois primeiros canais): o uMS
// recebe o PRIMEIRO SEGMENTO como arquivo .ts, sem o motor HLS. Tocar ali e
// nao tocar no HLS separa "o TS do provedor nao decodifica nesta TV" de "o
// motor HLS do uMS nao lida com esta playlist".
// MODO P = o PROXY DE TS (proxyts.h): o HLS vai ao uMS como TS continuo. Vem
// primeiro no HLS (e no .ts que o painel responde com playlist), porque e o
// conserto medido na C9; os modos do load ficam depois, para comparacao.
enum { M_A, M_B, M_C, M_D, M_P, M_N };
static int ordemModos(int f, int *m) {
  int n = 0;
  const LtdFormato *x = &L.it[L.atual].f[f];
  if (!LTD_TEM_PLAYER) { m[n++] = M_A; return n; }
  if (f == F_HLS || x->corpoM3u) {
    if (proxyts_disponivel()) m[n++] = M_P;
    m[n++] = M_A;
    if (f == F_HLS && L.atual < 2) { m[n++] = M_B; m[n++] = M_C; if (x->segUrl[0]) m[n++] = M_D; }
    return n;
  }
  m[n++] = M_A;
  if (L.atual < 2) { m[n++] = M_B; m[n++] = M_C; }
  return n;
}
static int nModos(int f) { int m[M_N]; return ordemModos(f, m); }
static int modoDoPasso(int f, int k) { int m[M_N], n = ordemModos(f, m); return k < n ? m[k] : M_A; }
static const char *letraModo(int m) {
  return m == M_B ? "B" : m == M_C ? "C" : m == M_D ? "D" : m == M_P ? "P" : "A";
}

static void iniciarPlayer(int f) {
  LtdItem *it = &L.it[L.atual];
  LtdFormato *x = &it->f[f];
  if (!x->modoFeito) x->modoOk = -1;
  L.pfFormato = f;
  L.pfModo = modoDoPasso(f, x->modoFeito);
  video_definir_modo_live(L.pfModo <= M_C ? L.pfModo : 0);
  L.pfDesde = SDL_GetTicks();
  L.pfTocouEm = 0;
  L.estado = E_PLAYER;
  video_definir_reconexao(0);
  video_definir_cabecalhos(it->cabs);
  proxyts_definir_cabecalhos(it->cabs);
  { char px[96], marcada[4200];
    snprintf(marcada, sizeof marcada, "%s%s", PROXYTS_PREFIXO, x->url);
    L.pfVivo = video_tocar(L.pfModo == M_D ? x->segUrl
                           : proxyts_resolver(L.pfModo == M_P ? marcada : x->url, px, sizeof px)); }
  // O QUADRO do painel "No player agora" desde o load: sem isto o primeiro
  // quadro sairia em tela cheia por cima da tela ate o desenho seguinte.
  if (L.pfVivo && quadroPlayer.w > 1.0f)
    video_janela((int)quadroPlayer.x, (int)quadroPlayer.y, (int)quadroPlayer.w, (int)quadroPlayer.h);
  if (!L.pfVivo) {
    x->falha = LTD_TEM_PLAYER ? LTD_ERRO_PLAYER : LTD_SEM_TESTE;
    snprintf(x->erro, sizeof x->erro, "%s", LTD_TEM_PLAYER ? "load recusado" : "");
  }
}

static void fecharPlayer(void) {
  if (L.pfVivo) video_parar();
  L.pfVivo = 0;
  L.pausaAte = SDL_GetTicks() + LTD_PAUSA_MS;
}

static void logPlayer(const LtdItem *it, const LtdFormato *x, int f) {
  if (x->tocou && x->modoOk == L.pfModo)
    printf("[livetv-diag] player %s %s modo %s: tocou em %d ms, %dx%d\n", it->nome, f == F_HLS ? "HLS" : "TS",
           letraModo(L.pfModo), x->quadroMs, x->largura, x->altura);
  else
    printf("[livetv-diag] player %s %s modo %s: nao tocou (%s%s%s) decoder=%d buffer=%.1fs\n", it->nome,
           f == F_HLS ? "HLS" : "TS", letraModo(L.pfModo),
           x->falha == LTD_SEM_DECODER ? "dado chegou, decoder mudo" :
           x->falha == LTD_SEM_RESPOSTA ? "nada chegou" :
           x->falha == LTD_SEM_TESTE ? "sem pipeline nesta plataforma" : "erro",
           x->erro[0] ? ": " : "", x->erro, x->decoder, video_buffer_fim());
  fflush(stdout);
}

static void recomendar(void);

static void avancar(void) {
  L.it[L.atual].pronto = 1;
  if (L.atual + 1 < L.n && !L.cancelado) { iniciarRede(L.atual + 1); return; }
  L.estado = E_PRONTO;
  recomendar();
}

static void passoPlayer(Uint32 agora) {
  LtdItem *it = &L.it[L.atual];
  LtdFormato *x = &it->f[L.pfFormato];
  Uint32 d = agora - L.pfDesde;
  int acabou = 0;
  if (!L.pfVivo) acabou = 1;
  else if (L.pfTocouEm) {
    if (agora - L.pfTocouEm >= LTD_MOSTRA_MS) acabou = 1;
  } else if (video_falhou()) {
    x->falha = LTD_ERRO_PLAYER;
    snprintf(x->erro, sizeof x->erro, "%s", video_erro_texto());
    acabou = 1;
  } else if (video_pos() > 0.05 || video_tocando()) {
    x->tocou = 1;
    x->modoOk = L.pfModo;
    x->falha = LTD_OK;
    x->quadroMs = (int)d;
    x->decoder = 1;
    x->largura = video_largura();
    x->altura = video_altura();
    L.pfTocouEm = agora;
  } else if (d >= LTD_PLAYER_MS) {
    x->decoder = video_decoder_anunciou();
    x->falha = video_buffer_fim() > 0.5 ? LTD_SEM_DECODER : LTD_SEM_RESPOSTA;
    acabou = 1;
  }
  if (!acabou) return;
  if (L.pfVivo || x->falha != LTD_SEM_TESTE) logPlayer(it, x, L.pfFormato);
  if (L.pfModo < 5) {
    x->modoFalha[L.pfModo] = x->modoOk == L.pfModo ? LTD_OK : x->falha;
    x->modoQuadroMs[L.pfModo] = x->modoOk == L.pfModo ? x->quadroMs : 0;
  }
  x->modoFeito++;
  // O formato acaba quando tocou ou quando os modos dele acabaram; no Mac
  // (sem pipeline) ele nem conta como testado.
  if (LTD_TEM_PLAYER && (x->tocou || x->modoFeito >= nModos(L.pfFormato))) x->testou = 1;
  if (!LTD_TEM_PLAYER) x->modoFeito = 99;
  fecharPlayer();
  // O OUTRO FORMATO so se este nao tocou: ve se o HLS abre onde o TS nao abre.
  if (!x->tocou && LTD_TEM_PLAYER) {
    int g = proximoFormato(it);
    if (g >= 0) { L.estado = E_PLAYER; L.pfFormato = g; L.pfVivo = 0; L.pfDesde = 0; return; }
  }
  avancar();
}

// --- recomendacao ----------------------------------------------------------------
static void recomendar(void) {
  LtdCanal c[LTD_MAX * 2];
  int n = 0, i, f;
  for (i = 0; i < L.n; i++)
    for (f = 0; f < 2; f++) {
      const LtdFormato *x = &L.it[i].f[f];
      if (!x->tentado || (!x->servido && !x->testou)) continue;
      c[n].formato = f;
      c[n].tocou = x->tocou;
      c[n].testouPlayer = x->testou && x->falha != LTD_SEM_TESTE;
      c[n].falha = x->testou ? x->falha : x->servido ? LTD_SEM_TESTE : LTD_NAO_E_VIDEO;
      c[n].quadroMs = x->quadroMs;
      c[n].esperaMs = -1;
      c[n].kbpsFluxo = x->kbps;
      c[n].alturaVista = x->altura;
      c[n].dezBits = x->dezBits;
      n++;
    }
  ltd_recomendar(c, n, &L.rec);
  // MODO DO LOAD: o que mais tocou; empate fica com o menor (A, o de sempre).
  memset(L.tocouModo, 0, sizeof L.tocouModo);
  for (i = 0; i < L.n; i++)
    for (f = 0; f < 2; f++)
      if (L.it[i].f[f].tocou && L.it[i].f[f].modoOk >= 0 && L.it[i].f[f].modoOk < M_N)
        L.tocouModo[L.it[i].f[f].modoOk]++;
  L.recModo = -1;
  for (f = 0; f < 3; f++) if (L.tocouModo[f] && (L.recModo < 0 || L.tocouModo[f] > L.tocouModo[L.recModo])) L.recModo = f;
  if (L.redeMedida && L.kbps > 0 && L.kbpsDoSegmento) {
    L.rec.kbpsMediana = L.kbps;
    L.rec.resolucao = ltd_resolucao_pela_vazao(L.kbps);
  } else if (!L.kbpsDoSegmento) L.rec.resolucao = 0;   // .ts continuo: taxa do canal, nao da rede
  L.rec.latenciaMs = L.latenciaMs;
  printf("[livetv-diag] recomenda: formato=%d resolucao=%d espera=%d | tocaram HLS %d/%d TS %d/%d | "
         "sem decoder %d | 10 bits %d | %d kbps | tocaram por modo A %d B %d C %d D %d proxy %d -> modo %d\n",
         L.rec.formato, L.rec.resolucao, L.rec.espera,
         L.rec.tocaram[0], L.rec.tentados[0], L.rec.tocaram[1], L.rec.tentados[1], L.rec.semDecoder,
         L.rec.dezBits, L.rec.kbpsMediana, L.tocouModo[0], L.tocouModo[1], L.tocouModo[2], L.tocouModo[3], L.tocouModo[4], L.recModo);
  fflush(stdout);
}

// --- ciclo -----------------------------------------------------------------------
static void comecar(void) {
  GuiaVariante v[LTD_MAX];
  static char bases[LTD_MAX][600];
  int i;
#ifdef NV_TOUCH_UI
  ltdPhoneReiniciar();
#endif
  L.cancelado = 0;
  juntarFio();
  memset(L.it, 0, sizeof L.it);
  L.kbps = L.kbpsPior = 0; L.latenciaMs = -1; L.redeMedida = 0; L.kbpsDoSegmento = 0;
  L.aplicado = 0; L.enviou = 0; L.contaLida = 0;
  memset(&L.conta, 0, sizeof L.conta);
  memset(&L.rec, 0, sizeof L.rec);
  L.xtConfig = xtream_configurado();
  L.n = guia_canais_para_teste(v, bases, LTD_MAX, L.grupo, sizeof L.grupo);
  for (i = 0; i < L.n; i++) {
    snprintf(L.it[i].nome, sizeof L.it[i].nome, "%s", v[i].nome);
    snprintf(L.it[i].id, sizeof L.it[i].id, "%s", v[i].id);
    snprintf(L.it[i].base, sizeof L.it[i].base, "%s", bases[i]);
    L.it[i].xt = xtream_e_id(v[i].id);
    L.it[i].alturaNome = v[i].altura;
  }
  printf("[livetv-diag] inicio: %d canal(is) de \"%s\", xtream=%d, resolucao principal=%d, formato=%d, espera=%u ms\n",
         L.n, L.grupo, L.xtConfig, ajustes_livetv_resolucao(), ajustes_livetv_formato(),
         ajustes_livetv_espera_ms());
  fflush(stdout);
  L.botao = 0;
  if (L.n > 0) iniciarRede(0);
  else L.estado = E_PRONTO;
}

void livetvdiag_iniciar(void) {
#ifdef NV_TOUCH_UI
  toqueLtdOffset = 0.0f; toquerol_limpar(&toqueLtd);
#endif
  L.sair = 0;
  // O PREVIEW DO GUIA solta o pipeline: o teste precisa dele, e a conta de
  // 1 tela nao aguenta o preview e o teste juntos.
  if (player_mini_ativo()) player_fechar_mini();
  comecar();
}

int livetvdiag_quer_sair(void) { return L.sair; }

void livetvdiag_encerrar(void) {
  L.cancelado = 1;
  if (L.estado == E_PLAYER) fecharPlayer();
  juntarFio();
  if (L.estado != E_PRONTO) L.estado = E_PARADO;
}

void livetvdiag_atualizar(float dt, Uint32 agora) {
  (void)dt;
  if (L.sair) return;
  if (L.estado == E_REDE && !atomic_load(&L.fioOcupado)) {
    LtdItem *it = &L.it[L.atual];
    int f;
    juntarFio();
    if (L.cancelado) { L.estado = E_PRONTO; return; }
    f = proximoFormato(it);
    if (f < 0) {
      printf("[livetv-diag] player %s: nada para tocar (%s)\n", it->nome,
             it->semFonte ? "addon sem fonte" : "a rede nao trouxe video em nenhum formato");
      L.pausaAte = agora + LTD_PAUSA_MS;
      avancar();
      return;
    }
    L.estado = E_PLAYER; L.pfFormato = f; L.pfVivo = 0; L.pfDesde = 0;
    L.pausaAte = agora + LTD_PAUSA_MS;
    return;
  }
  if (L.estado == E_PLAYER) {
    if (!L.pfDesde) {
      if ((int)(agora - L.pausaAte) < 0) return;
      iniciarPlayer(L.pfFormato);
      return;
    }
    passoPlayer(agora);
  }
}

// --- teclas --------------------------------------------------------------------
enum { B_APLICAR, B_ENVIAR, B_DENOVO, B_N };
static int botoes(int *lista) {
  int n = 0;
  if (L.estado != E_PRONTO) return 0;
  if (L.rec.confianca > 0 || L.redeMedida || L.recModo >= 0 || L.tocouModo[M_P]) lista[n++] = B_APLICAR;
  lista[n++] = B_ENVIAR;
  lista[n++] = B_DENOVO;
  return n;
}

void livetvdiag_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int lista[B_N], n;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    livetvdiag_encerrar();
    L.sair = 1;
    return;
  }
  n = botoes(lista);
  if (!n) return;
  if (L.botao >= n) L.botao = n - 1;
  if (k == SDLK_LEFT && L.botao > 0) L.botao--;
  else if (k == SDLK_RIGHT && L.botao + 1 < n) L.botao++;
  else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    int b = lista[L.botao];
    if (b == B_APLICAR) {
      ajustes_livetv_aplicar(L.rec.resolucao, L.rec.formato, L.rec.espera);
      if (L.recModo >= 0) ajustes_livetv_aplicar_modo(L.recModo);
      if (L.tocouModo[M_P]) ajustes_livetv_aplicar_proxy(1);
      L.aplicado = 1;
      printf("[livetv-diag] aplicado: resolucao=%d formato=%d espera=%d\n", L.rec.resolucao,
             L.rec.formato, L.rec.espera);
      fflush(stdout);
    } else if (b == B_ENVIAR) {
      avisos_enviar_registro_atual();
      L.enviou = 1;
    } else comecar();
  }
}

// PONTEIRO (#99). Os botoes do rodape (so existem com o teste pronto) poem o
// foco em L.botao, a mesma variavel das setas; o OK do clique aplica, envia
// ou testa de novo pelo caminho de sempre.
static void ponteiroBotao(int i, int b) {
  int lista[B_N];
  (void)b;
  if (L.sair || i < 0 || i >= botoes(lista)) return;
  L.botao = i;
}
int livetvdiag_teste_foco(void) { return L.botao; }

// --- desenho -------------------------------------------------------------------
static float areaX(void) { float x; ajustes_area_conteudo(NV_MARGEM_X, NV_MARGEM_X, &x, NULL); return x; }
static float areaW(void) { float w; ajustes_area_conteudo(NV_MARGEM_X, NV_MARGEM_X, NULL, &w); return w; }
static char sepDec(void) { return idioma_ponto_decimal(ajustes_idioma()) ? '.' : ','; }

// GLASS UI (mockup de Ajustes, quadros "livetv-diag"): cada painel e uma
// ilha, o titulo e o kicker, as metricas sao as linhas com fio das ilhas.
static void painel(GfxRect r, float ar, float ag, float ab) {
  (void)ar; (void)ag; (void)ab;
  ajustes_ui_ilha(r, 32, 0);
}
static void titulo(GfxRect r, const char *t, const char *sub) {
  ajustes_ui_kicker(t, r.x + 32.0f, r.y + 28.0f, 1);
  if (sub) txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_APOIO, sub, 243, 242, 239, 255, r.w - 64.0f),
                              r.x + 32.0f, r.y + 52.0f, 0.48f);
}
static void metrica(GfxRect r, float y, const char *rot, const char *val, int cr, int cg, int cb) {
  TxtLinha v = txt_linha_corta(TXT_AJ_ESTADO, val, cr, cg, cb, 255, r.w * 0.55f);
  TxtLinha k = txt_linha_corta(TXT_AJ_ESTADO, i18n(rot), 243, 242, 239, 255, r.w - 80.0f - v.w);
  gfx_cor((GfxRect){ r.x + 32.0f, y, r.w - 64.0f, 1 }, 0, 1, 1, 1, 0.07f);
  txt_desenhar_alpha(k, r.x + 32.0f, y + 11.0f, 0.45f);
  txt_desenhar(v, r.x + r.w - 32.0f - v.w, y + 11.0f);
}
static void segundos(char *d, size_t n, int ms) {
  snprintf(d, n, "%d%c%d s", ms / 1000, sepDec(), (ms % 1000) / 100);
}
static void mbps(char *d, size_t n, int kbps) {
  char b[24];
  vazao_fmt_mbps(b, sizeof b, kbps, sepDec());
  snprintf(d, n, "%s Mbps", b);
}

static const char *nomeFormatoOpcao(int f) {
  return f == 1 ? "HLS (.m3u8)" : f == 2 ? "TS (.ts)" : "Automático";
}
static const char *nomeResOpcao(int r) {
  static const char *const N[] = { "Automática", "4K", "1080p", "720p", "SD" };
  return r >= 0 && r < 5 ? N[r] : "Automática";
}
static const char *nomeEsperaOpcao(int e) {
  return e == 1 ? "25 s" : e == 2 ? "45 s" : "Automática";
}

static void desenharRede(GfxRect r, float ar, float ag, float ab) {
  char a[96], b[64];
  painel(r, ar, ag, ab);
  titulo(r, "Rede até o provedor", NULL);
  if (!L.redeMedida) {
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n(L.estado == E_PRONTO ? "Nenhum canal respondeu para medir."
                                                                         : "Medindo…"),
                                 170, 178, 190, 255, r.w - 56.0f), r.x + 28.0f, r.y + 76.0f);
    return;
  }
  // O NUMERO GRANDE e a barra com o pior segundo por cima, na mesma escala.
  { float x = r.x + 32.0f, w = r.w - 64.0f, y = r.y + 54.0f, esc = 30000.0f;
    TxtLinha n, u;
    vazao_fmt_mbps(b, sizeof b, L.kbps, sepDec());
    n = txt_linha(TXT_AJ_NUM58, b, 243, 242, 239, 255);
    u = txt_linha(TXT_AJ_TEXTO, "Mbps", 243, 242, 239, 255);
    txt_desenhar(n, x, y);
    txt_desenhar_alpha(u, x + n.w + 10, y + n.h * 0.78f - u.h, 0.55f);
    y += n.h * 0.92f + 12;
    while (L.kbps > esc * 0.9f) esc *= 2;
    gfx_cor((GfxRect){ x, y, w, 8 }, 0.5f, 1, 1, 1, 0.07f);
    gfx_cor((GfxRect){ x, y, w * L.kbps / esc, 8 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.55f);
    if (L.kbpsPior > 0) gfx_cor((GfxRect){ x, y, w * L.kbpsPior / esc, 8 }, 0.5f, 0.910f, 0.722f, 0.290f, 1);
    y += 8 + 10;
    { float lx = x;
      TxtLinha l;
      if (L.kbpsPior > 0) {
        char m[32];
        vazao_fmt_mbps(m, sizeof m, L.kbpsPior, sepDec());
        snprintf(a, sizeof a, i18n("Pior segundo %s Mbps"), m);
        l = txt_linha(TXT_ILHA_HORA, a, 243, 242, 239, 255);
        gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, 0.910f, 0.722f, 0.290f, 1);
        txt_desenhar_alpha(l, lx + 17, y, 0.5f); lx += 17 + l.w + 18;
      }
      l = txt_linha(TXT_ILHA_HORA, i18n("Velocidade"), 243, 242, 239, 255);
      gfx_cor((GfxRect){ lx, y + (l.h - 10) * 0.5f, 10, 10 }, 0.3f, 0.953f, 0.949f, 0.937f, 0.55f);
      txt_desenhar_alpha(l, lx + 17, y, 0.5f); }
    y += 18 + 12;
    if (L.latenciaMs >= 0) {
      snprintf(b, sizeof b, i18n("%d ms"), L.latenciaMs);
      metrica(r, y, "Latência (primeiro byte)", b, L.latenciaMs > 800 ? 232 : 243,
              L.latenciaMs > 800 ? 184 : 242, L.latenciaMs > 800 ? 74 : 239);
    } }
}

static void desenharConta(GfxRect r, float ar, float ag, float ab) {
  char a[128];
  painel(r, ar, ag, ab);
  titulo(r, "Conta Xtream", NULL);
  if (!L.xtConfig) {
    txt_bloco(TXT_CAPTION, i18n("Sem conta Xtream neste perfil: os canais testados são dos addons."),
              170, 178, 190, r.x + 28.0f, r.y + 76.0f, r.w - 56.0f, 30.0f, 1, 3);
    return;
  }
  if (!L.contaLida && L.estado != E_PRONTO && L.atual == 0) {
    txt_desenhar(txt_linha_corta(TXT_CAPTION, i18n("Lendo a conta…"), 170, 178, 190, 255, r.w - 56.0f),
                 r.x + 28.0f, r.y + 76.0f);
    return;
  }
  if (!L.conta.valido) {
    snprintf(a, sizeof a, i18n("O servidor não respondeu a conta (HTTP %d)."), L.conta.http);
    txt_bloco(TXT_CAPTION, a, 244, 196, 150, r.x + 28.0f, r.y + 76.0f, r.w - 56.0f, 30.0f, 1, 3);
    return;
  }
  metrica(r, r.y + 62.0f, "Situação", L.conta.status, 243, 242, 239);
  if (L.conta.formatosDeclarados)
    snprintf(a, sizeof a, "%s%s%s", L.conta.temM3u8 ? "HLS" : "", L.conta.temM3u8 && L.conta.temTs ? " · " : "",
             L.conta.temTs ? "TS" : "");
  else snprintf(a, sizeof a, "%s", i18n("não declarados"));
  metrica(r, r.y + 104.5f, "Formatos permitidos", a, 243, 242, 239);
  snprintf(a, sizeof a, i18n("%d de %d"), L.conta.conexoes, L.conta.maxConexoes);
  { int cheio = L.conta.maxConexoes > 0 && L.conta.conexoes >= L.conta.maxConexoes, k;
    TxtLinha v = txt_linha(TXT_AJ_ESTADO, a, 243, 242, 239, 255);
    metrica(r, r.y + 147.0f, "Telas em uso", a, cheio ? 232 : 243, cheio ? 184 : 242, cheio ? 74 : 239);
    for (k = 0; k < L.conta.maxConexoes && k < 10; k++) {
      float px = r.x + r.w - 32.0f - v.w - 10 - (L.conta.maxConexoes - k) * 15.0f;
      gfx_cor((GfxRect){ px, r.y + 147.0f + 11 + (v.h - 10) * 0.5f, 10, 10 }, 0.3f, 1, 1, 1, k < L.conta.conexoes ? 0.8f : 0.12f);
    } }
  if (L.conta.expira > 0) {
    time_t t = (time_t)L.conta.expira;
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(a, sizeof a, "%d/%m/%Y", &tm);
  } else snprintf(a, sizeof a, "%s", i18n("sem vencimento"));
  metrica(r, r.y + 189.5f, "Validade", a, 243, 242, 239);
}

static void textoResultado(const LtdItem *it, char *res, size_t nr, char *det, size_t nd, int *cor) {
  const LtdFormato *ok = it->f[F_HLS].tocou ? &it->f[F_HLS] : it->f[F_TS].tocou ? &it->f[F_TS] : NULL;
  const LtdFormato *ref = ok;
  char t[32], k[32];
  int f;
  *cor = 0;
  det[0] = 0;
  if (!it->pronto && &L.it[L.atual] == it && L.estado != E_PRONTO) {
    snprintf(res, nr, "%s", i18n(L.estado == E_REDE ? "Pedindo ao provedor…" : "Abrindo no player…"));
    *cor = 3;
    return;
  }
  if (!it->pronto) { snprintf(res, nr, "%s", i18n("Na fila")); *cor = 3; return; }
  if (ok) {
    segundos(t, sizeof t, ok->quadroMs);
    { char fm[48];
      if (ok->modoOk > 0) snprintf(fm, sizeof fm, i18n("%s · modo %s"), ok == &it->f[F_HLS] ? "HLS" : "TS",
                                   letraModo(ok->modoOk));
      else snprintf(fm, sizeof fm, "%s", ok == &it->f[F_HLS] ? "HLS" : "TS");
      snprintf(res, nr, i18n("Tocou em %s (%s)"), t, fm); }
    *cor = 1;
  } else {
    // A falha mais informativa: a do player, senao a da rede.
    const LtdFormato *p = it->f[F_TS].testou ? &it->f[F_TS] : it->f[F_HLS].testou ? &it->f[F_HLS] : NULL;
    *cor = 2;
    if (!p && ((it->f[F_TS].servido && it->f[F_TS].falha == LTD_SEM_TESTE) ||
               (it->f[F_HLS].servido && it->f[F_HLS].falha == LTD_SEM_TESTE)))
      p = it->f[F_TS].servido ? &it->f[F_TS] : &it->f[F_HLS];
    if (it->semFonte) snprintf(res, nr, "%s", i18n("O addon não mandou fonte"));
    else if (p && p->falha == LTD_SEM_TESTE) { snprintf(res, nr, "%s", i18n("Responde; o player só é testado na TV")); *cor = 3; }
    else if (p && p->falha == LTD_SEM_DECODER) snprintf(res, nr, "%s", i18n("Os dados chegam, mas a TV não decodifica"));
    else if (p && p->falha == LTD_SEM_RESPOSTA) snprintf(res, nr, "%s", i18n("O player não recebeu nada"));
    else if (p && p->falha == LTD_ERRO_PLAYER) snprintf(res, nr, i18n("Erro do player: %s"), p->erro);
    else {
      int h = it->f[F_TS].http ? it->f[F_TS].http : it->f[F_HLS].http;
      if (h >= 400) snprintf(res, nr, i18n("O provedor recusou (HTTP %d)"), h);
      else if (!h) snprintf(res, nr, "%s", i18n("O provedor não respondeu"));
      else snprintf(res, nr, "%s", i18n("A resposta não é vídeo"));
    }
    ref = p ? p : it->f[F_TS].servido ? &it->f[F_TS] : &it->f[F_HLS];
  }
  // DETALHE: resolucao (do player, senao do nome), codec e vazao do trecho.
  { int u = 0, alt = ref && ref->altura ? ref->altura : it->alturaNome;
    if (alt) u += snprintf(det + u, nd - (size_t)u, "%dp", alt);
    for (f = 0; f < 2 && !(ref && ref->codec[0]); f++) if (it->f[f].codec[0]) ref = &it->f[f];
    if (ref && ref->codec[0]) u += snprintf(det + u, nd - (size_t)u, "%s%s", u ? " · " : "", ref->codec);
    if (ref && ref->dezBits) u += snprintf(det + u, nd - (size_t)u, " · %s", i18n("10 bits"));
    if (ref && ref->kbps > 0) { mbps(k, sizeof k, ref->kbps); snprintf(det + u, nd - (size_t)u, "%s%s", u ? " · " : "", k); } }
}

static void desenharCanais(GfxRect r, float ar, float ag, float ab) {
  char sub[160], res[160], det[160];
  int i;
  painel(r, ar, ag, ab);
  if (L.n) snprintf(sub, sizeof sub, i18n("%d canais de “%s”, um de cada vez"), L.n, L.grupo);
  else snprintf(sub, sizeof sub, "%s", i18n("Abra o guia numa fileira com canais e volte aqui."));
  titulo(r, "Canais testados", sub);
#ifdef NV_TOUCH_UI
  toquerol_vincular(&toqueLtd, (GfxRect){r.x + 32.0f, r.y + 90.0f, r.w - 64.0f, r.h - 106.0f}, gfx_escala(),
                   0.0f, L.n * (LTD_LINHA + 4.0f) - (r.h - 106.0f), 1, &toqueLtdOffset);
  ponteiro_rolagem(toqueLtdRolar);
  gfx_recorte(r.x, r.y + 90.0f, r.w, r.h - 106.0f);
#endif
  for (i = 0; i < L.n; i++) {
    const LtdItem *it = &L.it[i];
    GfxRect ln = { r.x + 32.0f, r.y + 90.0f + (float)i * (LTD_LINHA + 4.0f), r.w - 64.0f, LTD_LINHA };
#ifdef NV_TOUCH_UI
    ln.y -= toqueLtdOffset;
#endif
    int cor, atual = i == L.atual && L.estado != E_PRONTO;
    float x = ln.x + 20, yc = ln.y + ln.h * 0.5f;
    TxtLinha tn, tr;
#ifdef NV_TOUCH_UI
    if (ln.y > r.y + r.h - 16) break;
    if (ln.y + ln.h < r.y + 90.0f) continue;
#else
    if (ln.y + ln.h > r.y + r.h - 16) break;
#endif
    textoResultado(it, res, sizeof res, det, sizeof det, &cor);
    if (atual) ajustes_ui_foco_linha(ln, 22);
    ajustes_ui_neutro((GfxRect){ x, yc - 21, 42, 42 }, 12, 0.07f);
    gfx_icone((GfxRect){ x + 10, yc - 11, 22, 22 }, "aj_tv-minimal-play", 0.953f, 0.949f, 0.937f, 0.7f);
    x += 42 + 16;
    tr = cor == 1 ? txt_linha_corta(TXT_AJ_18, res, 76, 195, 138, 255, ln.w * 0.5f)
       : cor == 2 ? txt_linha_corta(TXT_AJ_18, res, 229, 83, 75, 255, ln.w * 0.5f)
       : txt_linha_corta(TXT_AJ_18, res, 243, 242, 239, 255, ln.w * 0.5f);
    tn = txt_linha_corta(TXT_ILHA_ITEM, it->nome, 243, 242, 239, 255, ln.x + ln.w - 20 - tr.w - 30 - x);
    if (det[0]) {
      TxtLinha td = txt_linha_corta(TXT_ILHA_HORA, det, 243, 242, 239, 255, ln.x + ln.w - 20 - tr.w - 30 - x);
      txt_desenhar(tn, x, yc - (tn.h + 3 + td.h) * 0.5f);
      txt_desenhar_alpha(td, x, yc - (tn.h + 3 + td.h) * 0.5f + tn.h + 3, 0.45f);
    } else txt_desenhar(tn, x, yc - tn.h * 0.5f);
    txt_desenhar_alpha(tr, ln.x + ln.w - 20 - tr.w, yc - tr.h * 0.5f, cor == 3 && !atual ? 0.4f : cor == 3 ? 0.75f : 1.0f);
    if (atual) {
      float px = ln.x + ln.w - 20 - tr.w - 9 - 9;
      gfx_cor((GfxRect){ px - 5, yc - 9.5f, 19, 19 }, 0.5f, ar, ag, ab, 0.22f);
      gfx_cor((GfxRect){ px, yc - 4.5f, 9, 9 }, 0.5f, ar, ag, ab, 1);
    }
  }
#ifdef NV_TOUCH_UI
  gfx_sem_recorte();
#endif
}

static void desenharTeste(GfxRect r, float ar, float ag, float ab, Uint32 agora) {
  GfxRect v = { r.x + 32.0f, r.y + 90.0f, r.w - 64.0f, r.h - 90.0f - 28.0f };
  quadroPlayer = v;
  painel(r, ar, ag, ab);
  if (L.estado == E_PLAYER && L.pfVivo) {
    char a[96], t[32];
    segundos(t, sizeof t, (int)(agora - L.pfDesde));
    snprintf(a, sizeof a, i18n("%s em %s · %s"), L.it[L.atual].nome, L.pfFormato == F_HLS ? "HLS" : "TS", t);
    titulo(r, "No player agora", a);
    video_janela((int)v.x, (int)v.y, (int)v.w, (int)v.h);
    gfx_furo_raio(v, 18.0f / v.h);
    return;
  }
  if (L.estado == E_PRONTO && (L.rec.confianca || L.redeMedida)) {
    // AJUSTES SUGERIDOS: o de agora -> o recomendado, os tres que "Aplicar" muda.
    char a[96];
    int ac = ajustes_livetv_resolucao(), fc = ajustes_livetv_formato(), ec = (int)ajustes_livetv_espera_ms();
    ec = ec == 25000 ? 1 : ec == 45000 ? 2 : 0;
    titulo(r, "Ajustes sugeridos", NULL);
    snprintf(a, sizeof a, "%s  \xe2\x86\x92  %s", i18n(nomeResOpcao(ac)), i18n(nomeResOpcao(L.rec.resolucao)));
    metrica(r, r.y + 62.0f, "Resolução principal", a, ac == L.rec.resolucao ? 190 : 255, ac == L.rec.resolucao ? 189 : 255, ac == L.rec.resolucao ? 187 : 255);
    snprintf(a, sizeof a, "%s  \xe2\x86\x92  %s", i18n(nomeFormatoOpcao(fc)), i18n(nomeFormatoOpcao(L.rec.formato)));
    metrica(r, r.y + 104.5f, "Formato do Xtream", a, fc == L.rec.formato ? 190 : 255, fc == L.rec.formato ? 189 : 255, fc == L.rec.formato ? 187 : 255);
    snprintf(a, sizeof a, "%s  \xe2\x86\x92  %s", i18n(nomeEsperaOpcao(ec)), i18n(nomeEsperaOpcao(L.rec.espera)));
    metrica(r, r.y + 147.0f, "Espera para abrir o canal", a, ec == L.rec.espera ? 190 : 255, ec == L.rec.espera ? 189 : 255, ec == L.rec.espera ? 187 : 255);
    if (L.recModo >= 0) {
      int mc = ajustes_livetv_modo();
      snprintf(a, sizeof a, "%s  \xe2\x86\x92  %s", letraModo(mc), letraModo(L.recModo));
      metrica(r, r.y + 189.5f, "Modo do player da Live TV", a, mc == L.recModo ? 190 : 255, mc == L.recModo ? 189 : 255, mc == L.recModo ? 187 : 255);
    }
    txt_bloco(TXT_ILHA_HORA, i18n("Ficam em Ajustes › TV ao vivo › Se o canal não abre, e dá para mudar à mão depois."),
              243, 242, 239, r.x + 32.0f, r.y + r.h - 28.0f - 44.0f, r.w - 64.0f, 22.0f, 0.45f, 2);
    return;
  }
  titulo(r, "No player agora", NULL);
  txt_bloco(TXT_ILHA_GENERO, i18n(L.estado == E_PRONTO ? "Teste concluído. O vídeo de cada canal aparece aqui enquanto ele é testado."
                                                   : "Cada canal abre aqui por até 18 s em cada modo do player, depois de a rede responder."),
            243, 242, 239, r.x + 32.0f, r.y + 62.0f, r.w - 64.0f, 24.0f, 0.55f, 4);
}

// Uma linha (ou duas) de recomendacao; a que nao cabe acima dos botoes fica de
// fora — por isso a ordem de chamada e a ordem de importancia.
static int linhaRec(GfxRect r, float *y, const char *s, int cr, int cg, int cb) {
  float lim = r.y + r.h - 96.0f, w = r.w - 64.0f;
  int linhas = (float)txt_linha(TXT_ILHA_GENERO, s, cr, cg, cb, 255).w > w - 36.0f ? 2 : 1;
  int alerta = cr > 230 && cg < 200;
  if (*y + 26.0f * (float)linhas > lim) return 0;
  // Icone pela gravidade (alerta ambar, informacao neutra); o texto na tinta
  // da ilha, so o alerta em ambar.
  if (alerta) gfx_icone((GfxRect){ r.x + 32.0f, *y + 2.0f, 18, 18 }, "aj_triangle-alert", 0.910f, 0.722f, 0.290f, 1);
  else gfx_icone((GfxRect){ r.x + 32.0f, *y + 2.0f, 18, 18 }, "aj_info", 0.953f, 0.949f, 0.937f, 0.55f);
  if (!alerta && cr == 214) { cr = 243; cg = 242; cb = 239; }
  txt_bloco(TXT_ILHA_GENERO, s, cr, cg, cb, r.x + 32.0f + 28.0f, *y, w - 36.0f, 25.0f, alerta ? 1.0f : 0.8f, 2);
  *y += 25.0f * (float)linhas + 10.0f;
  return 1;
}

static void desenharRecomendacoes(GfxRect r, float ar, float ag, float ab) {
  char a[240];
  float y = r.y + 72.0f;
  int lista[B_N], n, i, semVideo = 0, hlsServido = 0;
  painel(r, ar, ag, ab);
  titulo(r, "Recomendações", NULL);
  if (L.estado != E_PRONTO) {
    float pct = L.n ? (float)L.atual / (float)L.n : 0.0f;
    snprintf(a, sizeof a, i18n("Testando %d de %d…"), L.atual + 1, L.n);
    txt_desenhar(txt_linha_corta(TXT_ILHA_NOME, a, 243, 242, 239, 255, r.w - 64.0f), r.x + 32.0f, r.y + 62.0f);
    gfx_cor((GfxRect){ r.x + 32.0f, r.y + 106.0f, r.w - 64.0f, 8 }, 0.5f, 1, 1, 1, 0.07f);
    if (pct > 0) gfx_cor((GfxRect){ r.x + 32.0f, r.y + 106.0f, (r.w - 64.0f) * pct, 8 }, 0.5f, ar, ag, ab, 1);
    txt_bloco(TXT_ILHA_APOIO, i18n("Cada canal abre aqui por até 18 s em cada modo do player, depois de a rede responder."),
              243, 242, 239, r.x + 32.0f, r.y + 128.0f, r.w - 64.0f, 22.0f, 0.45f, 2);
    return;
  }
  for (i = 0; i < L.n; i++) {
    if (!algumTocou(&L.it[i])) semVideo++;
    if (L.it[i].xt && L.it[i].f[F_HLS].servido) hlsServido++;
  }
  if (L.aplicado) linhaRec(r, &y, i18n("Aplicado. Os próximos canais já usam estes ajustes."), 150, 222, 170);
  if (L.enviou) {
    int st = avisos_envio_estado();
    linhaRec(r, &y, i18n(st == 2 ? "Resultado enviado no registro." : st == 3 ? "O envio do registro falhou."
                                                                             : "Enviando o registro…"),
             st == 3 ? 244 : 150, st == 3 ? 170 : 222, st == 3 ? 140 : 170);
  }
  if (L.rec.semDecoder) {
    snprintf(a, sizeof a, i18n("%d canal(is): o vídeo chega, mas a TV não começa a decodificar. Esperar mais não resolve; tente o outro formato ou outra resolução do canal."), L.rec.semDecoder);
    linhaRec(r, &y, a, 244, 196, 150);
  }
  if (L.rec.dezBits)
    linhaRec(r, &y, i18n("Há canal em 10 bits: muitas TVs não decodificam esse vídeo. Prefira a versão HD ou SD dele."), 244, 196, 150);
  if (L.xtConfig && L.conta.valido && L.conta.maxConexoes > 0 && L.conta.conexoes >= L.conta.maxConexoes)
    linhaRec(r, &y, i18n("Todas as telas da conta estão em uso: feche o Xtream em outro aparelho."), 244, 196, 150);
  if (L.tocouModo[M_P] && !L.tocouModo[M_A])
    linhaRec(r, &y, i18n("Os canais HLS só abriram pelo proxy de TS: ele fica ligado para a Live TV."), (int)(ar * 255), (int)(ag * 255), (int)(ab * 255));
  if (L.recModo > 0) {
    snprintf(a, sizeof a, i18n("Os canais abriram no modo %s do player e não no padrão: Aplicar passa a usá-lo nos canais."),
             letraModo(L.recModo));
    linhaRec(r, &y, a, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255));
  }
  if (L.rec.formato == 1)
    linhaRec(r, &y, i18n("O HLS abriu mais canais que o TS nesta TV: o formato do Xtream passa a pedir HLS primeiro."), 214, 220, 230);
  else if (L.rec.formato == 2)
    linhaRec(r, &y, i18n("O TS abriu mais canais que o HLS nesta TV: o formato do Xtream passa a pedir TS primeiro."), 214, 220, 230);
  if (L.xtConfig && L.conta.valido && L.conta.formatosDeclarados && !L.conta.temM3u8 && hlsServido)
    linhaRec(r, &y, i18n("O provedor entrega HLS mesmo com a conta declarando só TS."), 214, 220, 230);
  if (L.redeMedida && L.rec.resolucao) {
    char m[32];
    mbps(m, sizeof m, L.rec.kbpsMediana);
    snprintf(a, sizeof a, i18n("Com %s até o provedor, %s é a resolução que toca sem travar."), m,
             i18n(nomeResOpcao(L.rec.resolucao)));
    linhaRec(r, &y, a, 214, 220, 230);
  }
  if (L.rec.espera)
    linhaRec(r, &y, i18n("Um canal levou mais de 11 s para abrir: a espera sobe para não cortar quem ia abrir."), 214, 220, 230);
  if (L.latenciaMs > 800)
    linhaRec(r, &y, i18n("O provedor demora para responder: a troca de canal vai ser lenta mesmo com rede boa."), 244, 196, 150);
  if (semVideo && semVideo < L.n) {
    snprintf(a, sizeof a, i18n("%d de %d canais não abriram: são esses canais, não a TV. Use a versão em outra resolução deles."), semVideo, L.n);
    linhaRec(r, &y, a, 214, 220, 230);
  } else if (L.n && semVideo == L.n)
    linhaRec(r, &y, i18n("Nenhum canal abriu: confira a conta, a rede e envie o resultado no registro."), 244, 196, 150);
  // BOTOES no rodape do painel.
  n = botoes(lista);
  { static const char *const ROT[B_N] = { "Aplicar recomendadas", "Enviar no registro", "Testar de novo" };
    float x = r.x + 32.0f, by = r.y + r.h - 28.0f - 60.0f;
    if (L.botao >= n) L.botao = n ? n - 1 : 0;
    for (i = 0; i < n; i++) {
      float bw = ajustes_ui_botao(i18n(ROT[lista[i]]), NULL, x, by, i == L.botao);
      ponteiro_alvo(x, by, bw, 60.0f, ponteiroBotao, NULL, i, 0);
      x += bw + 12.0f;
    } }
}

#ifdef NV_TOUCH_UI
static TelefoneCartao ltdPhone;
static void ltdPhoneReiniciar(void) { telefonecartao_limpar(&ltdPhone); }
static int ltdPhoneRolar(const PonteiroRolagem *e) {
  return L.sair ? 0 : toquerol_evento(&ltdPhone.rolagem, e);
}
static void ltdPhoneAcao(int i, int escolha) {
  SDL_Event e = {0};
  int lista[B_N], n = botoes(lista), k;
  (void)i;
  if (L.sair) return;
  e.type = SDL_KEYDOWN;
  if (!escolha) e.key.keysym.sym = SDLK_AC_BACK;
  else {
    for (k = 0; k < n && lista[k] != escolha - 1; k++) {}
    if (k == n) return;
    L.botao = k;
    e.key.keysym.sym = SDLK_RETURN;
  }
  livetvdiag_evento(&e);
}
static float ltdPhoneTexto(TxtEstilo estilo, const char *s, float x, float y, float w, float a) {
  return txt_bloco_corta(estilo, s, 243, 242, 239, x, y, w, 30, a, 0);
}
static float ltdPhoneSecao(const char *s, float x, float y, float w, float a) {
  if (a > 0) gfx_cor((GfxRect){x, y, w, 1}, 0, 1, 1, 1, .12f * a);
  return 20 + ltdPhoneTexto(TXT_ILHA_ITEM, i18n(s), x, y + 20, w, a) + 18;
}
static float ltdPhoneCampo(const char *s, const char *val, float x, float y, float w, float a) {
  float h = ltdPhoneTexto(TXT_CAPTION, i18n(s), x, y, w, .55f * a);
  h += 6 + ltdPhoneTexto(TXT_ILHA_GENERO, val, x, y + h + 6, w, a);
  return h + 18;
}
static float ltdPhoneRec(float x, float y, float w, float a) {
  float inicio = y;
  int semVideo = 0, hlsServido = 0;
  char b[300];
#define LTD_PHONE_REC(s) do { y += ltdPhoneTexto(TXT_ILHA_GENERO, (s), x, y, w, a) + 18; } while (0)
  for (int i = 0; i < L.n; i++) {
    if (!algumTocou(&L.it[i])) semVideo++;
    if (L.it[i].xt && L.it[i].f[F_HLS].servido) hlsServido++;
  }
  if (L.aplicado) LTD_PHONE_REC(i18n("Aplicado. Os próximos canais já usam estes ajustes."));
  if (L.enviou) {
    int st = avisos_envio_estado();
    LTD_PHONE_REC(i18n(st == 2 ? "Resultado enviado no registro." : st == 3 ? "O envio do registro falhou." : "Enviando o registro…"));
  }
  if (L.rec.semDecoder) {
    snprintf(b, sizeof b, i18n("%d canal(is): o vídeo chega, mas a TV não começa a decodificar. Esperar mais não resolve; tente o outro formato ou outra resolução do canal."), L.rec.semDecoder);
    LTD_PHONE_REC(b);
  }
  if (L.rec.dezBits) LTD_PHONE_REC(i18n("Há canal em 10 bits: muitas TVs não decodificam esse vídeo. Prefira a versão HD ou SD dele."));
  if (L.xtConfig && L.conta.valido && L.conta.maxConexoes > 0 && L.conta.conexoes >= L.conta.maxConexoes)
    LTD_PHONE_REC(i18n("Todas as telas da conta estão em uso: feche o Xtream em outro aparelho."));
  if (L.tocouModo[M_P] && !L.tocouModo[M_A])
    LTD_PHONE_REC(i18n("Os canais HLS só abriram pelo proxy de TS: ele fica ligado para a Live TV."));
  if (L.recModo > 0) {
    snprintf(b, sizeof b, i18n("Os canais abriram no modo %s do player e não no padrão: Aplicar passa a usá-lo nos canais."), letraModo(L.recModo));
    LTD_PHONE_REC(b);
  }
  if (L.rec.formato == 1) LTD_PHONE_REC(i18n("O HLS abriu mais canais que o TS nesta TV: o formato do Xtream passa a pedir HLS primeiro."));
  else if (L.rec.formato == 2) LTD_PHONE_REC(i18n("O TS abriu mais canais que o HLS nesta TV: o formato do Xtream passa a pedir TS primeiro."));
  if (L.xtConfig && L.conta.valido && L.conta.formatosDeclarados && !L.conta.temM3u8 && hlsServido)
    LTD_PHONE_REC(i18n("O provedor entrega HLS mesmo com a conta declarando só TS."));
  if (L.redeMedida && L.rec.resolucao) {
    char m[32]; mbps(m, sizeof m, L.rec.kbpsMediana);
    snprintf(b, sizeof b, i18n("Com %s até o provedor, %s é a resolução que toca sem travar."), m, i18n(nomeResOpcao(L.rec.resolucao)));
    LTD_PHONE_REC(b);
  }
  if (L.rec.espera) LTD_PHONE_REC(i18n("Um canal levou mais de 11 s para abrir: a espera sobe para não cortar quem ia abrir."));
  if (L.latenciaMs > 800) LTD_PHONE_REC(i18n("O provedor demora para responder: a troca de canal vai ser lenta mesmo com rede boa."));
  if (semVideo && semVideo < L.n) {
    snprintf(b, sizeof b, i18n("%d de %d canais não abriram: são esses canais, não a TV. Use a versão em outra resolução deles."), semVideo, L.n);
    LTD_PHONE_REC(b);
  } else if (L.n && semVideo == L.n)
    LTD_PHONE_REC(i18n("Nenhum canal abriu: confira a conta, a rede e envie o resultado no registro."));
#undef LTD_PHONE_REC
  return y - inicio;
}
static float ltdPhoneCorpo(float x, float y, float w, float a) {
  float inicio = y;
  char b[300], val[128];
  y += ltdPhoneSecao("Rede até o provedor", x, y, w, a);
  if (L.redeMedida) {
    mbps(val, sizeof val, L.kbps); y += ltdPhoneCampo("Velocidade", val, x, y, w, a);
    if (L.kbpsPior > 0) { mbps(val, sizeof val, L.kbpsPior); y += ltdPhoneCampo("Pior segundo", val, x, y, w, a); }
    if (L.latenciaMs >= 0) { snprintf(val, sizeof val, i18n("%d ms"), L.latenciaMs); y += ltdPhoneCampo("Latência (primeiro byte)", val, x, y, w, a); }
  } else y += ltdPhoneTexto(TXT_CAPTION, i18n(L.estado == E_PRONTO ? "Nenhum canal respondeu para medir." : "Medindo…"), x, y, w, a) + 18;
  y += ltdPhoneSecao("Conta Xtream", x, y, w, a);
  if (!L.xtConfig)
    y += ltdPhoneTexto(TXT_CAPTION, i18n("Sem conta Xtream neste perfil: os canais testados são dos addons."), x, y, w, a) + 18;
  else if (!L.contaLida && L.estado != E_PRONTO && L.atual == 0)
    y += ltdPhoneTexto(TXT_CAPTION, i18n("Lendo a conta…"), x, y, w, a) + 18;
  else if (!L.conta.valido) {
    snprintf(b, sizeof b, i18n("O servidor não respondeu a conta (HTTP %d)."), L.conta.http);
    y += ltdPhoneTexto(TXT_CAPTION, b, x, y, w, a) + 18;
  } else {
    y += ltdPhoneCampo("Situação", L.conta.status, x, y, w, a);
    if (L.conta.formatosDeclarados) snprintf(val, sizeof val, "%s%s%s", L.conta.temM3u8 ? "HLS" : "", L.conta.temM3u8 && L.conta.temTs ? " · " : "", L.conta.temTs ? "TS" : "");
    else snprintf(val, sizeof val, "%s", i18n("não declarados"));
    y += ltdPhoneCampo("Formatos permitidos", val, x, y, w, a);
    snprintf(val, sizeof val, i18n("%d de %d"), L.conta.conexoes, L.conta.maxConexoes);
    y += ltdPhoneCampo("Telas em uso", val, x, y, w, a);
    if (L.conta.expira > 0) {
      time_t t = (time_t)L.conta.expira; struct tm tm; localtime_r(&t, &tm); strftime(val, sizeof val, "%d/%m/%Y", &tm);
    } else snprintf(val, sizeof val, "%s", i18n("sem vencimento"));
    y += ltdPhoneCampo("Validade", val, x, y, w, a);
  }
  y += ltdPhoneSecao("Canais testados", x, y, w, a);
  if (L.n) snprintf(b, sizeof b, i18n("%d canais de “%s”, um de cada vez"), L.n, L.grupo);
  else snprintf(b, sizeof b, "%s", i18n("Abra o guia numa fileira com canais e volte aqui."));
  y += ltdPhoneTexto(TXT_CAPTION, b, x, y, w, a) + 18;
  for (int i = 0; i < L.n; i++) {
    char res[160], det[160]; int cor;
    textoResultado(&L.it[i], res, sizeof res, det, sizeof det, &cor);
    y += ltdPhoneTexto(TXT_ILHA_ITEM, L.it[i].nome, x, y, w, a) + 8;
    y += ltdPhoneTexto(TXT_ILHA_GENERO, res, x, y, w, a) + 8;
    if (det[0]) y += ltdPhoneTexto(TXT_CAPTION, det, x, y, w, .55f * a) + 8;
    y += 20;
  }
  if (L.estado != E_PRONTO) {
    y += ltdPhoneSecao("Recomendações", x, y, w, a);
    snprintf(b, sizeof b, i18n("Testando %d de %d…"), L.n ? L.atual + 1 : 0, L.n);
    y += ltdPhoneTexto(TXT_ILHA_ITEM, b, x, y, w, a) + 14;
    float pct = L.n ? (float)L.atual / L.n : 0;
    if (a > 0) {
      gfx_cor((GfxRect){x, y, w, 8}, .5f, 1, 1, 1, .07f * a);
      if (pct > 0) gfx_cor((GfxRect){x, y, w * fminf(1, pct), 8}, .5f, 1, 1, 1, a);
    }
    y += 26 + ltdPhoneTexto(TXT_CAPTION, i18n("Cada canal abre aqui por até 18 s em cada modo do player, depois de a rede responder."), x, y + 26, w, a) + 18;
  } else {
    if (L.rec.confianca || L.redeMedida) {
      int ec = (int)ajustes_livetv_espera_ms(); ec = ec == 25000 ? 1 : ec == 45000 ? 2 : 0;
      y += ltdPhoneSecao("Ajustes sugeridos", x, y, w, a);
      snprintf(val, sizeof val, "%s → %s", i18n(nomeResOpcao(ajustes_livetv_resolucao())), i18n(nomeResOpcao(L.rec.resolucao)));
      y += ltdPhoneCampo("Resolução principal", val, x, y, w, a);
      snprintf(val, sizeof val, "%s → %s", i18n(nomeFormatoOpcao(ajustes_livetv_formato())), i18n(nomeFormatoOpcao(L.rec.formato)));
      y += ltdPhoneCampo("Formato do Xtream", val, x, y, w, a);
      snprintf(val, sizeof val, "%s → %s", i18n(nomeEsperaOpcao(ec)), i18n(nomeEsperaOpcao(L.rec.espera)));
      y += ltdPhoneCampo("Espera para abrir o canal", val, x, y, w, a);
      if (L.recModo >= 0) {
        snprintf(val, sizeof val, "%s → %s", letraModo(ajustes_livetv_modo()), letraModo(L.recModo));
        y += ltdPhoneCampo("Modo do player da Live TV", val, x, y, w, a);
      }
      y += ltdPhoneTexto(TXT_CAPTION, i18n("Ficam em Ajustes › TV ao vivo › Se o canal não abre, e dá para mudar à mão depois."), x, y, w, a) + 18;
    }
    y += ltdPhoneSecao("Recomendações", x, y, w, a);
    y += ltdPhoneRec(x, y, w, a);
  }
  return y - inicio;
}
static void ltdPhoneDesenhar(Uint32 agora) {
  static const char *const rot[B_N] = { "Aplicar recomendadas", "Enviar no registro", "Testar de novo" };
  int lista[B_N], n = botoes(lista);
  float anterior = gfx_escala(), escala = gfx_escala_ui();
  gfx_escala_sair(escala);
  float paginaW = nv_layout_w / escala, paginaH = nv_layout_h / escala;
  telefonecartao_medir(&ltdPhone, paginaW, paginaH, n ? n : 1);
  float x = ltdPhone.corpo.x, w = ltdPhone.corpo.w, y = ltdPhone.corpo.y;
  float cab = telefonecartao_titulo(i18n("Diagnóstico da Live TV"), x, y, w, 0);
  cab += ltdPhoneTexto(TXT_CAPTION, i18n("Mede a rede até o provedor e testa canais de verdade nesta TV."), x, y + cab, w, 0) + 18;
  float videoTopo = y + cab, videoH = fminf(w * 9 / 16, fminf(paginaH * .24f, fmaxf(60, ltdPhone.corpo.h - cab - 144)));
  GfxRect v = {x + (w - videoH * 16 / 9) * .5f, videoTopo, videoH * 16 / 9, videoH};
  if (L.estado == E_PLAYER) {
    char sub[240], t[32];
    segundos(t, sizeof t, (int)(agora - L.pfDesde));
    snprintf(sub, sizeof sub, i18n("%s em %s · %s"), L.it[L.atual].nome, L.pfFormato == F_HLS ? "HLS" : "TS", t);
    cab += ltdPhoneTexto(TXT_ILHA_ITEM, i18n("No player agora"), x, y + cab, w, 0) + 8;
    cab += ltdPhoneTexto(TXT_CAPTION, sub, x, y + cab, w, 0) + 18;
    videoH = fminf(videoH, fmaxf(60, ltdPhone.corpo.h - cab - 144));
    v = (GfxRect){x + (w - videoH * 16 / 9) * .5f, y + cab, videoH * 16 / 9, videoH};
    cab += videoH + 20;
  }
  // The native plane uses base canvas units. It stays fixed outside the
  // scrolling body, and the hole always matches its full destination.
  float vw = video_largura(), vh = video_altura();
  if (L.estado == E_PLAYER && L.pfVivo && vw > 1 && vh > 1) {
    float q = vw / vh, caixa = v.w / v.h;
    if (q > caixa) { float h = v.w / q; v.y += (v.h - h) * .5f; v.h = h; }
    else { float width = v.h * q; v.x += (v.w - width) * .5f; v.w = width; }
  }
  quadroPlayer = (GfxRect){v.x * escala, v.y * escala, v.w * escala, v.h * escala};
  ltdPhone.corpo.y += cab; ltdPhone.corpo.h -= cab;
  float total = ltdPhoneCorpo(x, 0, w, 0);
  telefonecartao_comecar(&ltdPhone, total, L.estado, !L.sair, ltdPhoneRolar, 1);
  ltdPhoneCorpo(x, ltdPhone.corpo.y - ltdPhone.offset, w, 1);
  gfx_sem_recorte();
  float cy = y + telefonecartao_titulo(i18n("Diagnóstico da Live TV"), x, y, w, 1);
  cy += ltdPhoneTexto(TXT_CAPTION, i18n("Mede a rede até o provedor e testa canais de verdade nesta TV."), x, cy, w, .65f) + 18;
  if (L.estado == E_PLAYER) {
    char sub[240], t[32]; segundos(t, sizeof t, (int)(agora - L.pfDesde));
    snprintf(sub, sizeof sub, i18n("%s em %s · %s"), L.it[L.atual].nome, L.pfFormato == F_HLS ? "HLS" : "TS", t);
    cy += ltdPhoneTexto(TXT_ILHA_ITEM, i18n("No player agora"), x, cy, w, 1) + 8;
    ltdPhoneTexto(TXT_CAPTION, sub, x, cy, w, .65f);
    gfx_cor(v, 0, 0, 0, 0, 1);
    if (L.pfVivo) {
      video_janela((int)(quadroPlayer.x + .5f), (int)(quadroPlayer.y + .5f),
                   (int)(quadroPlayer.w + .5f), (int)(quadroPlayer.h + .5f));
      gfx_furo_raio(v, 18 / v.h);
    }
  }
  if (L.botao >= n) L.botao = n ? n - 1 : 0;
  if (n) for (int i = 0; i < n; i++)
    telefonecartao_botao(&ltdPhone, i, i18n(rot[lista[i]]), i == L.botao, NULL, ltdPhoneAcao, lista[i] + 1, 1);
  else telefonecartao_botao(&ltdPhone, 0, i18n("Cancelar"), 1, NULL, ltdPhoneAcao, 0, 1);
  gfx_escala_sair(anterior);
}
#endif
void livetvdiag_desenhar(Uint32 agora) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { ltdPhoneDesenhar(agora); return; }
#endif
  float ar, ag, ab, x0 = 48.0f + ajustes_rail_largura_fixa(), colE = 620.0f, vao = 24.0f;
  ajustes_acento(&ar, &ag, &ab);
  ajustes_ui_fundo();
  // Titulo ao lado da ilha do relogio, como o diagnostico.
  { float x = ajustes_ilha_x() + 202.0f;
    TxtLinha tit = txt_linha(TXT_AJ_TIT28, i18n("Diagnóstico da Live TV"), 243, 242, 239, 255);
    TxtLinha sub = txt_linha_corta(TXT_AJ_ESTADO, i18n("Mede a rede até o provedor e testa canais de verdade nesta TV."),
                                   243, 242, 239, 255, NV_TELA_W - 60 - x - tit.w - 18);
    txt_desenhar(tit, x, 64 - tit.h * 0.5f);
    txt_desenhar_alpha(sub, x + tit.w + 18, 64 - sub.h * 0.5f, 0.62f); }
  { float y0 = 112.0f, yb = NV_TELA_H - 40.0f, xd = x0 + colE + vao, wd = NV_TELA_W - 40.0f - xd;
    float hRede = 238.0f, hConta = 246.0f, hCanais = 610.0f;
    desenharRede((GfxRect){ x0, y0, colE, hRede }, ar, ag, ab);
    desenharConta((GfxRect){ x0, y0 + hRede + 18.0f, colE, hConta }, ar, ag, ab);
    desenharTeste((GfxRect){ x0, y0 + hRede + hConta + 36.0f, colE, yb - (y0 + hRede + hConta + 36.0f) },
                  ar, ag, ab, agora);
    desenharCanais((GfxRect){ xd, y0, wd, hCanais }, ar, ag, ab);
    desenharRecomendacoes((GfxRect){ xd, y0 + hCanais + 20.0f, wd, yb - (y0 + hCanais + 20.0f) }, ar, ag, ab); }
}
