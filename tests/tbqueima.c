// TorBox burn-in on LG/Android (src/tbqueima.c): the controller against a
// fake player and a fake TorBox. bash tests/tbqueima.sh
#include "tbqueima.h"
#include "tbburn.h"
#include "tbrelogio.h"
#include "dts/tbaudio_tpk.h"
#include "rede.h"
#include "streams.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern unsigned tbq_teste_ms;
static int falhas;
#define OK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

// ---- the fake player ------------------------------------------------------------
static VideoFaixa la[4], ll[4];
static int nA, nL, aAtual, sondado = 1;
static char hdrTxt[16] = "none";
static int reaberturas, reabAudio;
static double reabAlvo;
static char reabUrl[256];
int video_n_audio(void) { return tbq_ativo() ? tbq_n_audio() : nA; }
int video_n_legenda(void) { return tbq_ativo() ? tbq_n_legenda() : nL; }
const VideoFaixa *video_audio(int i) { if (tbq_ativo()) return tbq_audio(i); return i >= 0 && i < nA ? &la[i] : NULL; }
const VideoFaixa *video_legenda(int i) { if (tbq_ativo()) return tbq_legenda(i); return i >= 0 && i < nL ? &ll[i] : NULL; }
int video_audio_atual(void) { return tbq_ativo() ? tbq_audio_atual() : aAtual; }
const char *video_url_atual(void) { return "https://addon.example/file.mkv"; }
const char *video_hdr(void) { return hdrTxt; }
int video_mkv_sondado(void) { return sondado; }
void tbq_host_reabrir(double alvo, int audio) {
  reaberturas++; reabAlvo = alvo; reabAudio = audio;
  snprintf(reabUrl, sizeof reabUrl, "%s", tbq_url_player() ? tbq_url_player() : "");
  nA = 1; nL = 0;   // whichever opens, the player's own lists are TorBox-like now
}

// ---- the fake TorBox (tbburn.h) ---------------------------------------------------
static int pedidos, pedSub, pedAud, cancelados, readds, itemConta, resultado = TBB_BUSCANDO, pode = 1, temAssVisto;
static int memoria, hdrPermitido;
void tbburn_pedir(const char *u, int s, int a) { (void)u; pedidos++; pedSub = s; pedAud = a; resultado = TBB_BUSCANDO; }
int tbburn_resultado(char *m, unsigned n, char *mo, unsigned mn) {
  snprintf(m, n, "%s", resultado == TBB_PRONTO ? "https://tb.example/x/playlist.m3u8?token=s" : "");
  if (mo && mn) snprintf(mo, mn, "%s", resultado == TBB_FALHOU ? "TorBox said no" : "");
  return resultado;
}
void tbburn_cancelar(void) { cancelados++; }
int tbburn_pode(void) { return pode; }
int tbburn_item_na_conta(const char *u) { (void)u; return itemConta; }
void tbburn_readicionar(const char *u, int s, int a) { (void)u; (void)s; (void)a; readds++; resultado = TBB_BUSCANDO; }
void tbburn_original_tocando(const char *u) { (void)u; }
void tbburn_original_parou(void) {}
void tbburn_cedo_bombear(void) {}
void tbburn_procurar_cedo(const char *u, const char *h, const char *a, uint64_t b, const char *p) { (void)u; (void)h; (void)a; (void)b; (void)p; }
void tbburn_arquivo_tem_ass(int s) { temAssVisto = s; }
void tbburn_definir_escolha(int v) { (void)v; }
void tbburn_lembrar(const TbFaixa *f) { memoria = f ? 2 : 1; }
int tbburn_memoria(TbFaixa *m) { (void)m; return 0; }
int tbburn_escolher_auto(const TbFaixa *f, int n, int mem, const TbFaixa *m, const char *p) {
  (void)m; (void)p;
  if (mem == 1 || !n) return -1;
  for (int i = 0; i < n; i++) if (f[i].letreiro) return i;   // Signs & Songs first
  return 0;
}
int tbburn_hdr_da_fonte(void) { return 0; }
int tbburn_hdr_permitido(void) { return hdrPermitido; }
void tbburn_hdr_lembrar(int s) { (void)s; }
int ajustes_leg_queimar(void) { return 1; }
const char *ling_legenda(void) { return ""; }
void mkvass_teto_conexoes(int n) { (void)n; }
int legref_em_rede(void) { return 0; }
void (*stream_foco_demorado)(const Stream *s);
// The head reader never reaches the network here.
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); r->status = 404; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
void rede_fio_fechar(void) {}
int tba_pl_ler(const char *t, const char *b, TbaPl *pl) { (void)t; (void)b; (void)pl; return 0; }
void tba_pl_liberar(TbaPl *pl) { (void)pl; }
int tba_ts_primeiro_pts(const unsigned char *b, size_t n, double *s) { (void)b; (void)n; (void)s; return 0; }

// ---- helpers ------------------------------------------------------------------
static TbqVista vis;
static void arquivo(void) {
  memset(la, 0, sizeof la); memset(ll, 0, sizeof ll);
  nA = 2; aAtual = 0; nL = 3;
  snprintf(la[0].rotulo, sizeof la[0].rotulo, "Japanese"); snprintf(la[1].rotulo, sizeof la[1].rotulo, "English");
  snprintf(ll[0].codec, sizeof ll[0].codec, "S_TEXT/UTF8"); ll[0].ordinalMkv = 0; snprintf(ll[0].rotulo, sizeof ll[0].rotulo, "English SRT");
  snprintf(ll[1].codec, sizeof ll[1].codec, "S_TEXT/ASS"); ll[1].ordinalMkv = 1; snprintf(ll[1].rotulo, sizeof ll[1].rotulo, "Full");
  snprintf(ll[2].codec, sizeof ll[2].codec, "S_TEXT/ASS"); ll[2].ordinalMkv = 2; ll[2].letreiro = 1; snprintf(ll[2].rotulo, sizeof ll[2].rotulo, "Signs");
}
static void novo(void) {
  tbq_tocar();
  arquivo();
  pedidos = cancelados = readds = reaberturas = 0; reabUrl[0] = 0; itemConta = 0; pode = 1;
  resultado = TBB_BUSCANDO; snprintf(hdrTxt, sizeof hdrTxt, "none"); sondado = 1;
  memset(&vis, 0, sizeof vis);
  vis.ativo = 1; vis.permitido = 1; vis.dur = 1400; vis.pos = 5;
  tbq_teste_ms = 1000;
}
static void quadro(unsigned ms) { tbq_teste_ms += ms; tbq_bombear(&vis); }
static void queimando(void) {
  novo();
  vis.pronto = vis.tocando = 1;
  quadro(16);
  OK(pedidos == 1 && pedSub == 2 && pedAud == 0);   // Signs & Songs, audio 0
  resultado = TBB_PRONTO; vis.pos = 7.5;
  quadro(16);
}

int main(void) {
  OK(tbq_codec_ass("S_TEXT/ASS") && tbq_codec_ass("S_SSA") && !tbq_codec_ass("S_TEXT/UTF8") && !tbq_codec_ass(NULL));
  OK(tbq_hdr_texto("HDR10") && tbq_hdr_texto("DolbyVision") && !tbq_hdr_texto("none") && !tbq_hdr_texto("SDR") && !tbq_hdr_texto(""));

  // 1. The original plays, the header is applied: Signs & Songs is asked for,
  //    then TorBox's stream opens at the point with the file's menus kept.
  queimando();
  OK(tbq_ativo() && reaberturas == 1 && reabAlvo == 7.5 && reabAudio == -1);
  OK(!strcmp(reabUrl, "https://tb.example/x/playlist.m3u8?token=s"));
  OK(video_n_legenda() == 3 && video_n_audio() == 2 && temAssVisto);
  OK(video_tpk_leg_oculta(1) && video_tpk_leg_oculta(2) && !video_tpk_leg_oculta(0));
  OK(video_tpk_queimada() == 2 && video_tpk_queimar_abrindo());
  OK(video_tpk_queimar_tirar_main() == 1 && video_tpk_queimar_tirar_main() == 0);
  vis.pronto = 1; quadro(500);
  OK(!video_tpk_queimar_abrindo());
  OK(video_tpk_tba_linhas() == 0);   // no Passthrough on LG/Android

  // 2. A player error after it played: back to the original, Nuvio draws.
  OK(tbq_erro_player(2001) == 1);
  OK(!tbq_ativo() && tbq_url_player() == NULL && reaberturas == 2 && reabAudio == 0);
  OK(video_tpk_queimar_voltou() == 1);
  arquivo(); quadro(16);
  OK(pedidos == 1);   // gave up on this source: no new request
  OK(tbq_erro_player(2001) == 0);   // not burned: the usual path

  // 3. An error before it played, item still in the account: re-added.
  novo(); vis.pronto = vis.tocando = 1; quadro(16); resultado = TBB_PRONTO; quadro(16);
  vis.pronto = vis.tocando = 0; itemConta = 77;
  OK(tbq_erro_player(-1) == 1 && readds == 1 && !tbq_ativo() && video_tpk_queimar_voltou() == 2);
  OK(video_tpk_queimada() == 2);   // the re-add is pending: the ASS tracks stay out of Main

  // 4. Slow start: the notice at 10 s, OK goes back to the original.
  novo(); vis.pronto = vis.tocando = 1; quadro(16); resultado = TBB_PRONTO; quadro(16);
  vis.pronto = vis.tocando = 0;
  quadro(9000); OK(video_tpk_queimar_aviso() == 0);
  quadro(1100); OK(video_tpk_queimar_aviso() == 3);
  OK(video_tpk_queimar_tardio_ok() == 1 && !tbq_ativo() && reaberturas == 2);

  // 5. No picture in 30 s: a player error.
  novo(); vis.pronto = vis.tocando = 1; quadro(16); resultado = TBB_PRONTO; quadro(16);
  vis.pronto = vis.tocando = 0;
  quadro(15000); quadro(15100);
  OK(!tbq_ativo() && reaberturas == 2);

  // 6. Burn in Off from the list: back to the original, the pick runs again.
  queimando(); vis.pronto = 1; quadro(16);
  video_tpk_queimar_faixa(-1);
  OK(!tbq_ativo() && reaberturas == 2 && video_tpk_queimar_voltou() == 2 && memoria == 1);

  // 7. Another audio while burned: another TorBox stream, nothing to the player.
  queimando(); vis.pronto = vis.tocando = 1; quadro(16);
  OK(tbq_escolher_audio(1) == 1 && pedidos == 2 && pedAud == 1 && video_audio_atual() == 1);
  resultado = TBB_PRONTO; quadro(16);
  OK(tbq_ativo() && reaberturas == 2 && video_n_audio() == 2);   // menus still the file's
  OK(tbq_escolher_audio(1) == 1 && pedidos == 2);

  // 8. TorBox says no: Nuvio draws, no reopen.
  novo(); vis.pronto = vis.tocando = 1; quadro(16);
  resultado = TBB_FALHOU; quadro(16);
  OK(!tbq_ativo() && reaberturas == 0 && video_tpk_queimar_voltou() == 1);

  // 9. LG Home while burned: the original reopens later; TorBox asked again after.
  queimando(); vis.pronto = vis.tocando = 1; quadro(16);
  { double alvo = 100.0; tbq_fundo(&alvo); OK(alvo >= 100.0 && !tbq_ativo()); }
  arquivo(); resultado = TBB_BUSCANDO; quadro(16);
  OK(pedidos == 2);

  // 10. An HDR file: nothing burns by itself.
  novo(); snprintf(hdrTxt, sizeof hdrTxt, "HDR10"); vis.pronto = vis.tocando = 1; quadro(16); quadro(16);
  OK(pedidos == 0);

  // 11. Header not applied yet: no decision; a new source drops the request.
  novo(); sondado = 0; vis.pronto = vis.tocando = 1; quadro(16); OK(pedidos == 0);
  sondado = 1; quadro(16); OK(pedidos == 1);
  tbq_tocar(); OK(cancelados == 1 && video_tpk_queimada() == -1);

  // 12. Audio pick while the request is in flight: asks again, the player switches too.
  novo(); vis.pronto = vis.tocando = 1; quadro(16);
  OK(tbq_escolher_audio(1) == 0 && pedidos == 2 && pedAud == 1);

  if (falhas) { printf("tbqueima: %d failure(s)\n", falhas); return 1; }
  printf("tbqueima ok\n");
  return 0;
}
