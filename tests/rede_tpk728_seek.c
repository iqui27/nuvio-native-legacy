// #728: seek do usuario DURANTE a reconexao do TPK sem convertido.
// A queda 0xFE6C0026 reabre a fonte aos 12 s com cv.reabrindo segurando a
// posicao mantida ate o host chegar perto do alvo (reabrindoVigiar). Um seek
// nesse meio tempo tem de apontar o "reabrindo" para o ALVO NOVO na hora:
// video_pos() segue o seek e video_busca_em_voo() solta quando o host pousa —
// antes, o vigia comparava com o alvo da queda e segurava ate 30 s (o app via
// video_pos preso, busca_em_voo=1, toast "Seek failed" e a capa por cima).
//
// Roda src/video_tpk.c contra o host falso do tpk-cdn (mesmo esquema de
// tests/tpk-cdn.c): queda aos 12 s, reabertura e seek da reconexao; depois
// seek do usuario (a) logo apos o reopen, (b) ainda com o seek da reconexao
// pendente, (c) com o host PAUSADO — o ack do seek do usuario solta o
// reabrindo mesmo sem tocar ("Assistir do comeco" preserva a pausa).
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"
#include "dts/dts_hls_tpk.h"
#include "audioescolha.h"
#include "cdnlink.h"

int ajustes_audio_offset_ms(void) { return 0; }
int ajustes_audio_normalizar(void) { return 0; }
int ajustes_hdr10p_strip(void) { return 0; }
void dts_engine_set_normalize(int on) { (void)on; }
void dts_remux_set_hdr10p_strip(int on) { (void)on; }
int ling_prefere_original(void) { return 0; }
int ling_prefere_regional(const char *pref) { (void)pref; return 0; }
int ling_afinidade(const char *cod, const char *pref) { (void)cod; (void)pref; return 0; }
const char *ling_audio_preferido(void) { return ""; }
int video_taxa_efetiva(void) { return 100; }
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return c && p && !strcmp(c, p); }
static const char *pref = "";
const char *ling_audio(void) { return pref; }
static const char *pref2 = "", *orig = "";
const char *ling_audio2(void) { return pref2; }
const char *ling_original(void) { return orig; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }
void intro_definir_capitulos(const IntroTrecho *v, int n) { (void)v; (void)n; }
void intro_definir_capitulos_palpite(const IntroTrecho *v, int n) { (void)v; (void)n; }
void intro_definir_capitulos_marcas(const double *v, int n) { (void)v; (void)n; }
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)tamFinal;
  if (tam) *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 1;
  return 0;
}
char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim, long *tam) {
  (void)url; (void)segundos; (void)ini; (void)fim;
  if (tam) *tam = 0;
  return 0;
}

// Sem pre-busca de MKV: a sonda nunca recebe cabecalho (caminho "sem
// convertido", igual ao cenario da revisao F8-r2b).
static unsigned char cab[8];
static long cabN;
static int cabPronto;
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url;
  if (!cabPronto) return 0;
  if (buf) { *buf = malloc((size_t)cabN); memcpy(*buf, cab, (size_t)cabN); }
  if (n) *n = cabN;
  return 1;
}

// --- the converter: always there, continuous TS ------------------------------
static int abriuConversor, urlEmN;
static double urlEmUlt = -1, inicioStream = -1;
int proxyts_e_url(const char *u) { (void)u; return 0; }
static int modoFake = DTSTPK_PROG_TS;
int ajustes_dts_tpk_modo(void) { return modoFake; }
int ajustes_dts_ac3(void) { return 1; }
int dts_tpk_disponivel(void) { return 0; }
const char *dts_tpk_modo_nome(int m) { (void)m; return "TS"; }
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n) {
  (void)url; (void)headers; (void)audio_ordinal; (void)stereo_aac; (void)modo;
  abriuConversor++;
  snprintf(out, n, "http://127.0.0.1:1/tok/v.ts?t=%.0f", inicio);
  return 1;
}
int dts_tpk_url_em(double s, char *out, size_t n) {
  urlEmN++; urlEmUlt = s; inicioStream = -1;   // the new stream's keyframe is not known yet
  snprintf(out, n, "http://127.0.0.1:1/tok/v.ts?t=%.0f", s);
  return 1;
}
void dts_tpk_fechar(void) {}
int dts_tpk_estado(void) { return DTSTPK_PRONTO; }
const char *dts_tpk_erro(void) { return ""; }
double dts_tpk_duracao(void) { return 7200; }
int dts_tpk_modo(void) { return modoFake; }
double dts_tpk_inicio_stream(void) { return inicioStream; }
const char *dts_tpk_caminho(void) { return "DTS core only"; }
int dts_tpk_prebuffer_ms(void) { return 1000; }
int dts_tpk_sem_indice(void) { return 0; }
int dts_tpk_e_url(const char *u) { return u && strstr(u, "127.0.0.1") != NULL; }
int video_faixa_dts(const VideoFaixa *f) { return f && !strncmp(f->codec, "A_DTS", 5); }

// --- the host ------------------------------------------------------------------
void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_geracao(void (*)(const char *, const char *, unsigned));
void nv_tpk_video_registrar_busca_geracao(void (*)(int, unsigned));
void nv_tpk_video_erro_geracao(unsigned, int, int);
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);
static char ultUrl[256];
static int nAbriu, nBuscas, posMs;
static char ultCab[512];
static void hAbrir(const char *u, const char *c) { nAbriu++; snprintf(ultUrl, sizeof ultUrl, "%s", u); snprintf(ultCab, sizeof ultCab, "%s", c ? c : ""); }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static int ultBusca = -1;
static void hBuscarP(int ms) { nBuscas++; ultBusca = ms; }
// Pedidos na ORDEM em que o host os recebe, com a geracao de cada um (#728
// r4). O host devolve no ack a geracao da operacao que executou — ackPostada
// reproduz isso pelo indice do pedido (negativo conta do fim: -1 = ultimo).
static int postMs[16];
static unsigned postGer[16];
static int nPost;
void hBuscarGerP(int ms, unsigned ger) {
  nBuscas++; ultBusca = ms;
  if (nPost < 16) { postMs[nPost] = ms; postGer[nPost] = ger; }
  nPost++;
}
void ackPostada(int i) {
  int k = i < 0 ? nPost + i : i;
  if (k < 0 || k >= nPost || k >= 16) abort();
  nv_tpk_video_evento(11, postMs[k], (int)postGer[k]);
}
void falhaPostada(int i) {
  int k = i < 0 ? nPost + i : i;
  if (k < 0 || k >= nPost || k >= 16) abort();
  nv_tpk_video_evento(12, postMs[k], (int)postGer[k]);
}
static int pausas, retomadas, pausaRecusada;
// Like Video.cs: a pause that takes reports EV_PAUSADO, a play EV_TOCANDO; a
// pause during a seek is refused ("Failed to pause") and reports nothing.
static void hPausar(int p) {
  if (p) { pausas++; if (!pausaRecusada) nv_tpk_video_evento(3, 0, 0); }
  else { retomadas++; nv_tpk_video_evento(2, 0, 0); }
}
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return posMs; }
static void hEscolher(int tipo, int idx) { (void)tipo; (void)idx; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(5); } }
static const char *tvLang = "eng";   // the language the TV gives its one playable track
static void playerPronto(void) {
  nv_tpk_video_evento(1, 0, 0);
  nv_tpk_video_faixa(0, 0, tvLang);
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(2, 0, 0);
}

// --- the fake cdnlink ---------------------------------------------------------
static int resolveOk = 1, nResolve, recenteSim, validadeMs = 150 * 60 * 1000;
static const char *cachedUrl = "https://cdn.test/checked";
static int fOrigem(const char *u) { return u && !strncmp(u, "http", 4) && !strstr(u, "127.0.0.1") && !strstr(u, ".m3u8") && !strstr(u, "tb-cdn.st"); }
static int fRecente(const char *o, const char *cab, unsigned maxMs, char *d, unsigned t, unsigned *idade) {
  (void)o; (void)cab; (void)maxMs;
  if (!recenteSim) return 0;
  snprintf(d, t, "%s", cachedUrl); if (idade) *idade = 5000; return 1;
}
static int fResolver(const char *o, const char *c, int s, char *d, unsigned t, unsigned *ms) {
  (void)o; (void)c; (void)s; (void)ms;
  nResolve++;
  SDL_Delay(20);
  if (!resolveOk) { d[0] = 0; return 0; }
  snprintf(d, t, "https://cdn.test/fresh%d", nResolve);
  return 1;
}
static int fileRequests;
int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  ok(usenet == 1 && item == 42 && file == 7, "direct refresh keeps item/file identity");
  fileRequests++;
  SDL_Delay(20);
  snprintf(dst, n, "https://nexus.tb-cdn.st/fresh%d", fileRequests);
  return 1;
}
#include "tpk-cdn-mkv.inc"
static unsigned fValidade(void) { return (unsigned)validadeMs; }
void video_tpk_cdn_arquivo(const char *url, int usenet, int item, int file);
int video_tpk_capa(void);
static void esperarAbrir(int n) { Uint32 t0 = SDL_GetTicks(); while (nAbriu < n && SDL_GetTicks() - t0 < 8000) bombear(1); }

// Bombeia ate `ms` wall ou a condicao; devolve quantos ms levou (ou -1).
static long ateQue(int (*cond)(void), unsigned ms) {
  Uint32 t0 = SDL_GetTicks();
  while (SDL_GetTicks() - t0 < ms) {
    bombear(1);
    if (posMs > 0) posMs += 5;
    if (cond()) return (long)(SDL_GetTicks() - t0);
  }
  return -1;
}
static int vooCaiu(void) { return video_busca_em_voo() == 0; }

// Sobe a fonte, derruba aos 12 s com o erro de rede do TPK e espera a
// reabertura (nAbriu=2, cv.reabrindo=1, seek da reconexao pendente).
static void quedaEReabre(void) {
  resolveOk = 0; nAbriu = 0; nBuscas = 0; ultBusca = -1; posMs = 0; nPost = 0;
  video_definir_reconexao(1);
  // Fonte MP4: a sonda de cabecalho MKV nao dispara (sondaMkvCabe), entao o
  // teste nao cria o fioMkv — a corrida pre-existente dele com video_tocar
  // (mkvEstado, nao sincronizado) e ortogonal ao #728 e sujaria o TSan.
  video_definir_mp4(1);
  video_tocar("http://addon.test/a.mp4");
  esperarAbrir(1);
  posMs = 1000; playerPronto(); bombear(3); posMs = 12000; bombear(20);
  ok(fabs(video_pos() - 12.0) < 0.5, "antes da queda: pos ~12 s");
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);
  bombear(2);
  ok(video_reconectando() && !video_falhou(), "queda de rede: reconectando, sem falhar");
  esperarAbrir(2);
}

// A: retomada normal segura ~12 s durante o reopen (comportamento do #728).
static void cenarioRetomada(void) {
  printf("== A: retomada normal\n");
  quedaEReabre();
  posMs = 0; playerPronto(); bombear(3);
  ok(ultBusca == 12000, "seek da reconexao enviado para 12 s");
  ok(video_pos() >= 11.0, "video_pos segura ~12 s durante o reopen (nao recua a 0)");
  nv_tpk_video_evento(11, 12000, 0); posMs = 12000;   // o host pousa no alvo
  long ms = ateQue(vooCaiu, 3000);
  ok(ms >= 0 && ms < 3000, "reabrindo solta logo que o host pousa em 12 s");
  ok(fabs(video_pos() - 12.0) < 1.5, "retomada confirma ~12 s");
  video_parar();
}

// B: seek do usuario DEPOIS do reopen (seek da reconexao ja enviado).
static void cenarioSeekAposReabrir(double alvo, double teto) {
  printf("== B: seek do usuario para %.0f s depois do reopen\n", alvo);
  quedaEReabre();
  posMs = 0; playerPronto(); bombear(3);
  ok(ultBusca == 12000, "seek da reconexao enviado para 12 s");
  video_buscar(alvo);
  ok(ultBusca == (int)(alvo * 1000), "seek do usuario chega ao host");
  nv_tpk_video_evento(11, 12000, 0);                  // ack velho: CAS nao casa
  nv_tpk_video_evento(11, (int)(alvo * 1000), 0);
  posMs = (int)(alvo * 1000) + 1;                     // o host pousa no alvo novo
  bombear(2);
  ok(fabs(video_pos() - alvo) < teto, "video_pos segue o alvo NOVO na hora (sem ficar em 12 s)");
  long ms = ateQue(vooCaiu, 5000);
  ok(ms >= 0, "busca_em_voo solta em menos de 5 s (sem os 25-30 s do alvo velho)");
  ok(fabs(video_pos() - alvo) < teto + 1.0, "posicao final no alvo novo");
  video_parar();
}

// C: seek do usuario ANTES do ack, com o seek da reconexao ainda pendente
// (ramo reconBuscarMs do video_buscar).
static void cenarioSeekPendente(double alvo) {
  printf("== C: seek do usuario para %.0f s com o seek da reconexao pendente\n", alvo);
  quedaEReabre();
  // Reopen feito (nAbriu=2), mas o player novo ainda nao deu pronto/tocando:
  // reconBuscarMs continua enfileirado.
  video_buscar(alvo);
  posMs = 0; playerPronto(); bombear(3);
  ok(ultBusca == (int)(alvo * 1000), "o seek enfileirado sai para o alvo NOVO");
  nv_tpk_video_evento(11, (int)(alvo * 1000), 0);
  posMs = (int)(alvo * 1000) + 1;
  bombear(2);
  ok(fabs(video_pos() - alvo) < 1.0, "video_pos segue o alvo NOVO na hora (sem ficar em 12 s)");
  long ms = ateQue(vooCaiu, 5000);
  ok(ms >= 0, "busca_em_voo solta em menos de 5 s");
  video_parar();
}

// D: seek do usuario com o HOST PAUSADO durante a reabertura (a pausa chega
// depois do ack da retomada; o "Assistir do comeco" preserva a pausa e chama
// buscarUsuario, src/player.c). O ack do seek do usuario tem de soltar o
// reabrindo mesmo com tocando=0 — antes, o vigia esperava o tocar e a capa e
// busca_em_voo ficavam presas ate o prazo (toast "Seek failed" aos 20 s).
static void cenarioSeekPausado(double alvo) {
  printf("== D: seek do usuario para %.0f s com o host pausado\n", alvo);
  quedaEReabre();
  posMs = 0; playerPronto();
  nv_tpk_video_evento(1, 720000, 0);
  bombear(3);
  ok(ultBusca == 12000, "seek da reconexao enviado para 12 s");
  posMs = 12000;
  nv_tpk_video_evento(11, 12000, 0);                  // ack do seek da retomada
  video_pausar(1);
  ok(!video_tocando(), "host confirma a pausa durante a reabertura");
  video_buscar(alvo);
  ok(ultBusca == (int)(alvo * 1000), "host recebe o seek pausado");
  posMs = (int)(alvo * 1000);
  nv_tpk_video_evento(11, (int)(alvo * 1000), 0);     // ack do seek do usuario
  nv_tpk_video_evento(3, 0, 0);                        // segue pausado
  bombear(3);
  ok(fabs(video_pos() - alvo) < 0.5, "video_pos no alvo do seek pausado");
  ok(!video_busca_em_voo() && !video_tpk_capa(),
     "ack do seek libera reabrindo e capa mesmo com o host pausado");
  video_parar();
}

int main(void) {
  static const CdnlinkGanchos g = { fOrigem, fRecente, fResolver, fValidade };
  SDL_Init(0);
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscarP, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_cdn_ganchos(&g);
  cenarioRetomada();
  cenarioSeekAposReabrir(0.0, 1.0);
  cenarioSeekAposReabrir(300.0, 2.0);
  cenarioSeekPendente(42.0);
  cenarioSeekPausado(300.0);
  cenarioSeekPausado(0.0);
  printf("\n%s\n", falhas ? "FALHOU" : "rede_tpk728_seek: ok");
  return falhas != 0;
}
