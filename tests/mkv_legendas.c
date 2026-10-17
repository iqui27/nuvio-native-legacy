// #92: casamento legenda da TV <-> TrackEntry num MKV de varias legendas, e
// colheita do mkvass pela faixa certa. O MKV vem de tests/mkv_legendas.sh:
//   TrackNumber 1 video, 2 audio, 3 ASS "Faixa A" (eng), 4 SRT "Faixa B" (por),
//   5 ASS "Faixa C" (spa).
// A LG lista essas legendas com trackNum 0, 1, 2 (MEDIDO na C9 num Erai-raws
// de 8 legendas: 0..7 contra TrackNumber 3..10).
#include "../src/mkv.h"
#include "../src/mkvass.h"
#include "../src/legenda.h"
#include "../src/rede.h"
#include "../src/dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const char *i18n(const char *s) { return s; }

static int falhas;
static void ok(int c, const char *o) {
  printf("  %-60s %s\n", o, c ? "ok" : "FALHOU");
  if (!c) falhas++;
}
static long agoraMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
static int colher(double ateSeg) {
  long t0 = agoraMs();
  for (;;) {
    int e = mkvass_estado();
    mkvass_passo((agoraMs() - t0) / 1000.0 * 6.0);
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return e;
    if (agoraMs() - t0 > (long)(ateSeg * 1000)) return e;
    usleep(20 * 1000);
  }
}
static void esperarFio(void) {
  long t0 = agoraMs();
  while (mkvass_ocupado() && agoraMs() - t0 < 20000) usleep(10 * 1000);
}
// Texto do cue vivo no instante do i-esimo evento (1 + i*2,5 s + 0,7 s).
static int cueCom(int i, const char *pref) {
  LegendaCue v[LEGENDA_SIMULTANEAS]; int k, m;
  m = legenda_cues(1.7 + i * 2.5, 0, v, LEGENDA_SIMULTANEAS);
  for (k = 0; k < m; k++) if (strstr(v[k].texto, pref)) return 1;
  return 0;
}

int main(int argc, char **argv) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  int n, idx[8], modo;
  if (argc < 2) { fprintf(stderr, "uso: %s url\n", argv[0]); return 2; }
  dados_iniciar(NULL);

  printf("[1] cabecalho do MKV de varias legendas\n");
  n = mkv_faixas(argv[1], fx, MKV_MAX_FAIXAS);
  ok(n == 5, "5 TrackEntry (video, audio, 3 legendas)");
  ok(n == 5 && fx[2].numero == 3 && fx[3].numero == 4 && fx[4].numero == 5,
     "legendas com TrackNumber 3, 4, 5");
  ok(n == 5 && !strcmp(fx[2].codec, "S_TEXT/ASS") && !strcmp(fx[3].codec, "S_TEXT/UTF8") &&
     !strcmp(fx[4].codec, "S_TEXT/ASS"), "codecs ASS, SRT, ASS");

  printf("\n[2] trackNum da LG (ordinal) -> TrackEntry\n");
  { int tv[3] = { 0, 1, 2 };
    modo = mkv_casar_legendas(fx, n, tv, 3, idx);
    ok(modo == MKV_CASA_ORDINAL, "trackNum 0,1,2 casa por ORDINAL");
    ok(idx[0] == 2 && idx[1] == 3 && idx[2] == 4, "0->TrackNumber 3, 1->4, 2->5");
    ok(!strcmp(fx[idx[0]].codec, "S_TEXT/ASS") && !strcmp(fx[idx[1]].codec, "S_TEXT/UTF8"),
       "a PRIMEIRA legenda leva o selo ASS (era a que ficava sem, o English do #92)"); }
  { int tv[3] = { 2, 0, 1 };     // a TV lista fora de ordem: o ordinal vale, a posicao nao
    modo = mkv_casar_legendas(fx, n, tv, 3, idx);
    ok(modo == MKV_CASA_ORDINAL && idx[0] == 4 && idx[1] == 2 && idx[2] == 3,
       "lista fora de ordem casa pelo valor, nao pela posicao"); }
  { int tv[3] = { 3, 4, 5 };     // leitura antiga, se alguma TV entregar TrackNumber
    modo = mkv_casar_legendas(fx, n, tv, 3, idx);
    ok(modo == MKV_CASA_NUMERO && idx[0] == 2 && idx[2] == 4, "TrackNumber 3,4,5 ainda casa");
  }
  { int tv[2] = { 0, 1 };        // a TV escondeu uma faixa: ordinal nao e confiavel
    modo = mkv_casar_legendas(fx, n, tv, 2, idx);
    ok(modo == MKV_CASA_NADA && idx[0] == -1 && idx[1] == -1, "contagem diferente: nao casa nada");
  }
  { int tv[3] = { 0, 0, 1 };
    ok(mkv_casar_legendas(fx, n, tv, 3, idx) == MKV_CASA_NADA, "ordinal repetido: nao casa nada"); }
  { int tv[3] = { 1, 2, 3 };     // nem ordinal (3 fora) nem TrackNumber (1, 2 nao sao legenda)
    ok(mkv_casar_legendas(fx, n, tv, 3, idx) == MKV_CASA_NADA,
       "trackNum que cairia no video/audio nao casa"); }

  printf("\n[2b] BD/remux: a TV omite as legendas de imagem (PGS)\n");
  { MkvFaixa g[8]; int tv[3], ix[8];
    memset(g, 0, sizeof g);
    // 1 video, 2 audio, 3 PGS, 4 ASS (Full), 5 PGS, 6 ASS (Signs)
    g[0].numero = 1; g[0].tipo = 1;  snprintf(g[0].codec, sizeof g[0].codec, "V_MPEGH/ISO/HEVC");
    g[1].numero = 2; g[1].tipo = 2;  snprintf(g[1].codec, sizeof g[1].codec, "A_FLAC");
    g[2].numero = 3; g[2].tipo = 17; snprintf(g[2].codec, sizeof g[2].codec, "S_HDMV/PGS");
    g[3].numero = 4; g[3].tipo = 17; snprintf(g[3].codec, sizeof g[3].codec, "S_TEXT/ASS");
    g[4].numero = 5; g[4].tipo = 17; snprintf(g[4].codec, sizeof g[4].codec, "S_HDMV/PGS");
    g[5].numero = 6; g[5].tipo = 17; snprintf(g[5].codec, sizeof g[5].codec, "S_TEXT/ASS");
    tv[0] = 0; tv[1] = 1;           // renumeradas entre as de texto
    modo = mkv_casar_legendas(g, 6, tv, 2, ix);
    ok(modo == MKV_CASA_SEM_IMAGEM && ix[0] == 3 && ix[1] == 5, "TV renumera 0,1: casa as duas ASS");
    tv[0] = 1; tv[1] = 3;           // trackNum segue o ordinal do arquivo
    modo = mkv_casar_legendas(g, 6, tv, 2, ix);
    ok(modo == MKV_CASA_SEM_IMAGEM && ix[0] == 3 && ix[1] == 5, "TV mantem o ordinal do arquivo (1,3)");
    tv[0] = 0; tv[1] = 1; tv[2] = 2; // lista inteira (com PGS): continua ordinal puro
    modo = mkv_casar_legendas(g, 6, tv, 3, ix);
    ok(modo == MKV_CASA_NADA, "3 na TV contra 4 no arquivo, 2 de texto: nao casa");
    tv[0] = 0; tv[1] = 0;
    ok(mkv_casar_legendas(g, 6, tv, 2, ix) == MKV_CASA_NADA, "ordinal repetido sem imagem: nao casa");
    tv[0] = 0; tv[1] = 5;
    ok(mkv_casar_legendas(g, 6, tv, 2, ix) == MKV_CASA_NADA, "ordinal fora do arquivo: nao casa");
    tv[0] = 1; tv[1] = 2;           // 1 e ASS, mas 2 aponta uma PGS no arquivo inteiro e passa de 2 no texto
    ok(mkv_casar_legendas(g, 6, tv, 2, ix) == MKV_CASA_NADA, "ordinal que cairia numa PGS: nao casa"); }

  printf("\n[3] mkvass colhe a faixa do ORDINAL pedido\n");
  mkvass_iniciar_ordinal(argv[1], 2);
  ok(colher(30) == MKVASS_COMPLETO, "ordinal 2 (TrackNumber 5, ASS) completa");
  ok(cueCom(0, "Faixa C 0") && cueCom(11, "Faixa C 11"), "falas da Faixa C, primeira e ultima");
  ok(!cueCom(3, "Faixa A"), "nenhuma fala da Faixa A (a vizinha)");
  { int col = 0, tot = 0; mkvass_estatisticas(NULL, NULL, &col, &tot);
    ok(col == 12 && tot == 12, "12 de 12 eventos colhidos"); }
  mkvass_parar(); esperarFio(); legenda_desligar();

  mkvass_iniciar_ordinal(argv[1], 0);
  ok(colher(30) == MKVASS_COMPLETO, "ordinal 0 (TrackNumber 3, ASS) completa");
  ok(cueCom(5, "Faixa A 5") && !cueCom(5, "Faixa C"), "falas da Faixa A, nao da C");
  mkvass_parar(); esperarFio(); legenda_desligar();

  mkvass_iniciar_ordinal(argv[1], 1);
  ok(colher(10) == MKVASS_COMPLETO, "ordinal 1 (SRT) completa pelo overlay (#421)");
  ok(cueCom(1.5, "Faixa B"), "SRT selecionado entrega texto ao overlay");
  mkvass_parar(); esperarFio();

  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "tudo ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
